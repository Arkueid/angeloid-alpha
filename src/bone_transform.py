import numpy as np


def next_pow2(x):
    return 1 if x == 0 else 2**(x - 1).bit_length()


def compute_bone_world_matrices(bones):
    num_bones = len(bones)
    world_mats = np.zeros((num_bones, 4, 4), dtype=np.float32)
    
    for i, bone in enumerate(bones):
        world_mats[i] = np.eye(4, dtype=np.float32)
        world_mats[i, 0, 3] = bone.position[0]
        world_mats[i, 1, 3] = bone.position[1]
        world_mats[i, 2, 3] = bone.position[2]
    
    return world_mats


def compute_bind_pose_matrices(bones, debug_scale=1.0):
    num_bones = len(bones)
    identity_mats = np.zeros((num_bones, 4, 4), dtype=np.float32)
    
    for i in range(num_bones):
        identity_mats[i] = np.eye(4, dtype=np.float32)
        if debug_scale != 1.0:
            identity_mats[i, 0, 0] = debug_scale
            identity_mats[i, 1, 1] = debug_scale
            identity_mats[i, 2, 2] = debug_scale
    
    return identity_mats


def pack_matrices_to_texture(matrices):
    num_mats = len(matrices)
    texels_per_matrix = 4
    total_texels = num_mats * texels_per_matrix
    
    tex_width = next_pow2(int(np.ceil(np.sqrt(total_texels))))
    tex_height = tex_width
    
    texture_data = np.zeros((tex_height, tex_width, 4), dtype=np.float32)
    
    for mat_idx, matrix in enumerate(matrices):
        global_texel_idx = mat_idx * texels_per_matrix
        row = global_texel_idx // tex_width
        col_start = global_texel_idx % tex_width
        
        if col_start + 3 < tex_width:
            texture_data[row, col_start] = matrix[:, 0]
            texture_data[row, col_start + 1] = matrix[:, 1]
            texture_data[row, col_start + 2] = matrix[:, 2]
            texture_data[row, col_start + 3] = matrix[:, 3]
    
    return texture_data, tex_width, tex_height


def get_bone_matrices_with_pose(bones, vpd_poses, transform_params=None):
    num_bones = len(bones)
    
    bind_world = compute_bone_world_matrices(bones)
    
    pose_world = np.zeros((num_bones, 4, 4), dtype=np.float32)
    
    for i in range(num_bones):
        bone = bones[i]
        local_mat = np.eye(4, dtype=np.float32)
        
        if bone.parent_index >= 0:
            parent_pos = bones[bone.parent_index].position
            local_pos = bone.position - parent_pos
            local_mat[0, 3] = local_pos[0]
            local_mat[1, 3] = local_pos[1]
            local_mat[2, 3] = local_pos[2]
            
            if bone.name in vpd_poses:
                pose = vpd_poses[bone.name]
                rot_mat = pose.to_matrix()[:3, :3]
                local_mat[:3, :3] = rot_mat
            
            pose_world[i] = pose_world[bone.parent_index] @ local_mat
        else:
            if bone.name in vpd_poses:
                pose = vpd_poses[bone.name]
                rot_mat = pose.to_matrix()[:3, :3]
                local_mat[:3, :3] = rot_mat
            pose_world[i] = local_mat
    
    matrices = np.zeros((num_bones, 4, 4), dtype=np.float32)
    for i in range(num_bones):
        inv_bind = np.linalg.inv(bind_world[i])
        matrices[i] = pose_world[i] @ inv_bind

    if transform_params:
        center = transform_params['center']
        min_y = transform_params['min_y']
        scale = transform_params['scale']
        
        offset = np.array([center[0], min_y, center[2]])
        offset_scaled = scale * offset
        
        for i in range(num_bones):
            M = matrices[i]
            R = M[:3, :3]
            t = M[:3, 3]
            
            t_new = t * scale + (R - np.eye(3)) @ offset_scaled
            
            matrices[i, :3, 3] = t_new

    return matrices
