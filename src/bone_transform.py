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


def create_rigid_body_lines(rigid_body, color=(1.0, 1.0, 0.0)):
    if rigid_body.is_box:
        sx, sy, sz = rigid_body.shape_size * 0.5
        vertices = np.array([
            [-sx, -sy, -sz], [sx, -sy, -sz],
            [sx, -sy, -sz], [sx, sy, -sz],
            [sx, sy, -sz], [-sx, sy, -sz],
            [-sx, sy, -sz], [-sx, -sy, -sz],
            [-sx, -sy, sz], [sx, -sy, sz],
            [sx, -sy, sz], [sx, sy, sz],
            [sx, sy, sz], [-sx, sy, sz],
            [-sx, sy, sz], [-sx, -sy, sz],
            [-sx, -sy, -sz], [-sx, -sy, sz],
            [sx, -sy, -sz], [sx, -sy, sz],
            [sx, sy, -sz], [sx, sy, sz],
            [-sx, sy, -sz], [-sx, sy, sz],
        ], dtype=np.float32)
    elif rigid_body.is_sphere:
        r = rigid_body.shape_size[0]
        vertices = []
        for i in range(8):
            angle = i * np.pi / 4
            x1 = r * np.cos(angle)
            y1 = r * np.sin(angle)
            x2 = r * np.cos(angle + np.pi / 4)
            y2 = r * np.sin(angle + np.pi / 4)
            vertices.extend([[x1, y1, 0], [x2, y2, 0]])
        for i in range(8):
            angle = i * np.pi / 4
            x1 = r * np.cos(angle)
            z1 = r * np.sin(angle)
            x2 = r * np.cos(angle + np.pi / 4)
            z2 = r * np.sin(angle + np.pi / 4)
            vertices.extend([[x1, 0, z1], [x2, 0, z2]])
        for i in range(8):
            angle = i * np.pi / 4
            y1 = r * np.cos(angle)
            z1 = r * np.sin(angle)
            y2 = r * np.cos(angle + np.pi / 4)
            z2 = r * np.sin(angle + np.pi / 4)
            vertices.extend([[0, y1, z1], [0, y2, z2]])
        vertices = np.array(vertices, dtype=np.float32)
    elif rigid_body.is_capsule:
        r = rigid_body.shape_size[0]
        h_total = rigid_body.shape_size[1] * 0.5
        h_cylinder = h_total - r
        if h_cylinder < 0:
            h_cylinder = 0
        vertices = []
        num_theta = 6
        num_phi = 4
        for i in range(num_phi):
            phi1 = np.pi * i / num_phi
            phi2 = np.pi * (i + 1) / num_phi
            y1_top = h_cylinder + r * np.cos(phi1)
            y2_top = h_cylinder + r * np.cos(phi2)
            y1_bottom = -h_cylinder - r * np.cos(phi1)
            y2_bottom = -h_cylinder - r * np.cos(phi2)
            for j in range(num_theta):
                theta = j * 2 * np.pi / num_theta
                x1 = r * np.sin(phi1) * np.cos(theta)
                z1 = r * np.sin(phi1) * np.sin(theta)
                x2 = r * np.sin(phi2) * np.cos(theta)
                z2 = r * np.sin(phi2) * np.sin(theta)
                vertices.extend([[x1, y1_top, z1], [x2, y2_top, z2]])
                vertices.extend([[x1, y1_bottom, z1], [x2, y2_bottom, z2]])
                theta2 = (j + 1) * 2 * np.pi / num_theta
                x1_next = r * np.sin(phi1) * np.cos(theta2)
                z1_next = r * np.sin(phi1) * np.sin(theta2)
                x2_next = r * np.sin(phi2) * np.cos(theta2)
                z2_next = r * np.sin(phi2) * np.sin(theta2)
                vertices.extend([[x1, y1_top, z1], [x1_next, y1_top, z1_next]])
                vertices.extend([[x1, y1_bottom, z1], [x1_next, y1_bottom, z1_next]])
        for j in range(num_theta):
            theta = j * 2 * np.pi / num_theta
            x1 = r * np.cos(theta)
            z1 = r * np.sin(theta)
            x2 = r * np.cos(theta + 2 * np.pi / num_theta)
            z2 = r * np.sin(theta + 2 * np.pi / num_theta)
            if h_cylinder > 0.01:
                vertices.extend([[x1, h_cylinder, z1], [x2, h_cylinder, z2]])
                vertices.extend([[x1, -h_cylinder, z1], [x1, h_cylinder, z1]])
        vertices = np.array(vertices, dtype=np.float32)
    else:
        return np.array([], dtype=np.float32).reshape(0, 3)

    rx, ry, rz = rigid_body.shape_rotation
    cx, cy, cz = np.cos(rx), np.cos(ry), np.cos(rz)
    sx, sy, sz = np.sin(rx), np.sin(ry), np.sin(rz)

    rot_x = np.array([
        [1, 0, 0],
        [0, cx, -sx],
        [0, sx, cx]
    ], dtype=np.float32)
    rot_y = np.array([
        [cy, 0, sy],
        [0, 1, 0],
        [-sy, 0, cy]
    ], dtype=np.float32)
    rot_z = np.array([
        [cz, -sz, 0],
        [sz, cz, 0],
        [0, 0, 1]
    ], dtype=np.float32)
    rotation = rot_z @ rot_y @ rot_x

    vertices = vertices @ rotation.T
    px, py, pz = rigid_body.shape_position
    vertices += np.array([px, py, pz], dtype=np.float32)

    colors = np.tile(color, (len(vertices), 1)).astype(np.float32)
    return vertices, colors


def create_all_rigid_body_lines(rigidbodies):
    all_vertices = []
    all_colors = []
    colors = [
        (1.0, 1.0, 0.0),
        (1.0, 0.5, 0.0),
        (0.0, 1.0, 1.0),
        (1.0, 0.0, 1.0),
    ]
    for i, rb in enumerate(rigidbodies):
        color = colors[i % len(colors)]
        verts, cols = create_rigid_body_lines(rb, color)
        if len(verts) > 0:
            all_vertices.append(verts)
            all_colors.append(cols)

    if not all_vertices:
        return np.array([], dtype=np.float32).reshape(0, 3), np.array([], dtype=np.float32).reshape(0, 3)

    all_vertices = np.vstack(all_vertices)
    all_colors = np.vstack(all_colors)
    return all_vertices, all_colors


def create_rigid_body_local_data(rigidbodies):
    colors = [
        (1.0, 1.0, 0.0),
        (1.0, 0.5, 0.0),
        (0.0, 1.0, 1.0),
        (1.0, 0.0, 1.0),
    ]
    result = []
    for i, rb in enumerate(rigidbodies):
        color = colors[i % len(colors)]
        verts, _ = create_rigid_body_lines(rb, color)
        if len(verts) > 0:
            cols = np.tile(color, (len(verts), 1)).astype(np.float32)
            result.append({
                'vertices': verts,
                'colors': cols,
                'bone_index': rb.bone_index,
                'count': len(verts)
            })
    return result


def create_rigid_body_lines_with_bones(rigidbodies, bone_matrices, transform_params=None):
    all_vertices = []
    all_colors = []
    colors = [
        (1.0, 1.0, 0.0),
        (1.0, 0.5, 0.0),
        (0.0, 1.0, 1.0),
        (1.0, 0.0, 1.0),
    ]
    for i, rb in enumerate(rigidbodies):
        color = colors[i % len(colors)]
        verts, cols = create_rigid_body_lines(rb, color, None)
        if len(verts) > 0:
            bone_idx = rb.bone_index
            if bone_matrices is not None and bone_idx >= 0 and bone_idx < len(bone_matrices):
                bone_mat = bone_matrices[bone_idx]
                transformed = np.zeros_like(verts)
                for j in range(len(verts)):
                    v = np.array([verts[j][0], verts[j][1], verts[j][2], 1.0])
                    result = bone_mat @ v
                    transformed[j] = [result[0], result[1], result[2]]
                all_vertices.append(transformed)
                all_colors.append(cols)
            else:
                if transform_params:
                    center = transform_params['center']
                    min_y = transform_params['min_y']
                    scale = transform_params['scale']
                    for j in range(len(verts)):
                        vx, vy, vz = verts[j]
                        vx = (vx - center[0]) * scale
                        vy = (vy - min_y) * scale
                        vz = (vz - center[2]) * scale
                        verts[j] = [vx, vy, vz]
                all_vertices.append(verts)
                all_colors.append(cols)

    if not all_vertices:
        return np.array([], dtype=np.float32).reshape(0, 3), np.array([], dtype=np.float32).reshape(0, 3)

    all_vertices = np.vstack(all_vertices)
    all_colors = np.vstack(all_colors)
    return all_vertices, all_colors
