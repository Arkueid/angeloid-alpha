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


import numpy as np

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


JOINT_TYPE_SPRING = 0
JOINT_TYPE_HINGE = 1
JOINT_TYPE_BALL = 2
JOINT_TYPE_SLIDER = 3
JOINT_TYPE_CONE_TWIST = 4


def create_joint_marker_lines(joint, color=(0.0, 1.0, 0.0), size=0.05):
    vertices = []
    x, y, z = joint.position
    rx, ry, rz = joint.rotation

    if joint.joint_type == JOINT_TYPE_SPRING:
        vertices.extend([
            [x - size, y, z], [x + size, y, z],
            [x, y - size, z], [x, y + size, z],
            [x, y, z - size], [x, y, z + size],
        ])
    elif joint.joint_type == JOINT_TYPE_HINGE:
        cx, cy, cz = np.cos(rx), np.cos(ry), np.cos(rz)
        sx, sy, sz = np.sin(rx), np.sin(ry), np.sin(rz)
        axis_len = size * 2
        vertices.extend([
            [x - cx * axis_len, y - cy * axis_len, z - cz * axis_len],
            [x + cx * axis_len, y + cy * axis_len, z + cz * axis_len],
        ])
    elif joint.joint_type == JOINT_TYPE_BALL:
        for i in range(8):
            angle = i * np.pi / 4
            x1 = x + size * np.cos(angle)
            y1 = y + size * np.sin(angle)
            x2 = x + size * np.cos(angle + np.pi / 4)
            y2 = y + size * np.sin(angle + np.pi / 4)
            vertices.extend([[x1, y1, z], [x2, y2, z]])
        for i in range(8):
            angle = i * np.pi / 4
            x1 = x + size * np.cos(angle)
            z1 = z + size * np.sin(angle)
            x2 = x + size * np.cos(angle + np.pi / 4)
            z2 = z + size * np.sin(angle + np.pi / 4)
            vertices.extend([[x1, y, z1], [x2, y, z2]])
    elif joint.joint_type == JOINT_TYPE_SLIDER:
        cx, cy, cz = np.cos(rx), np.cos(ry), np.cos(rz)
        sx, sy, sz = np.sin(rx), np.sin(ry), np.sin(rz)
        axis_len = size * 2
        vertices.extend([
            [x - cx * axis_len, y - cy * axis_len, z - cz * axis_len],
            [x + cx * axis_len, y + cy * axis_len, z + cz * axis_len],
        ])
        perp1 = np.array([-sy, sx, 0])
        perp2 = np.array([sz, 0, -sx])
        vertices.extend([
            [x, y, z],
            [x + perp1[0] * size * 0.5, y + perp1[1] * size * 0.5, z + perp1[2] * size * 0.5],
        ])
        vertices.extend([
            [x, y, z],
            [x + perp2[0] * size * 0.5, y + perp2[1] * size * 0.5, z + perp2[2] * size * 0.5],
        ])
    elif joint.joint_type == JOINT_TYPE_CONE_TWIST:
        for i in range(6):
            angle = i * np.pi / 3
            x1 = x + size * np.cos(angle)
            z1 = z + size * np.sin(angle)
            x2 = x + size * np.cos(angle + np.pi / 3)
            z2 = z + size * np.sin(angle + np.pi / 3)
            vertices.extend([[x1, y, z1], [x2, y, z2]])
        vertices.extend([[x, y, z], [x, y + size, z]])
    else:
        vertices.extend([
            [x - size, y, z], [x + size, y, z],
            [x, y - size, z], [x, y + size, z],
            [x, y, z - size], [x, y, z + size],
        ])

    vertices = np.array(vertices, dtype=np.float32)
    colors = np.tile(color, (len(vertices), 1)).astype(np.float32)
    return vertices, colors


def create_all_joint_markers(joints, rigidbodies=None):
    all_vertices = []
    all_colors = []
    colors = [
        (1.0, 0.0, 0.0),
        (0.0, 1.0, 0.0),
        (0.0, 0.0, 1.0),
        (1.0, 1.0, 0.0),
        (1.0, 0.0, 1.0),
        (0.0, 1.0, 1.0),
        (1.0, 0.5, 0.0),
        (0.5, 1.0, 0.0),
        (0.0, 1.0, 0.5),
        (0.5, 0.0, 1.0),
        (1.0, 0.0, 0.5),
        (0.0, 0.5, 1.0),
    ]
    pair_to_color = {}
    color_index = 0
    for joint in joints:
        rb_a_bone = -1
        rb_b_bone = -1
        if rigidbodies is not None:
            if 0 <= joint.rigidbody_index_a < len(rigidbodies):
                rb_a_bone = rigidbodies[joint.rigidbody_index_a].bone_index
            if 0 <= joint.rigidbody_index_b < len(rigidbodies):
                rb_b_bone = rigidbodies[joint.rigidbody_index_b].bone_index
        if rb_a_bone == rb_b_bone and rb_a_bone >= 0:
            key = ('bone', rb_a_bone)
        else:
            key = (joint.rigidbody_index_a, joint.rigidbody_index_b)
        if key not in pair_to_color:
            pair_to_color[key] = colors[color_index % len(colors)]
            color_index += 1
        color = pair_to_color[key]
        verts, cols = create_joint_marker_lines(joint, color)
        if len(verts) > 0:
            all_vertices.append(verts)
            all_colors.append(cols)

    if not all_vertices:
        return np.array([], dtype=np.float32).reshape(0, 3), np.array([], dtype=np.float32).reshape(0, 3)

    all_vertices = np.vstack(all_vertices)
    all_colors = np.vstack(all_colors)
    return all_vertices, all_colors


def create_joint_local_data(joints, rigidbodies=None, joint_size=0.05):
    colors = [
        (1.0, 0.0, 0.0),
        (0.0, 1.0, 0.0),
        (0.0, 0.0, 1.0),
        (1.0, 1.0, 0.0),
        (1.0, 0.0, 1.0),
        (0.0, 1.0, 1.0),
        (1.0, 0.5, 0.0),
        (0.5, 1.0, 0.0),
        (0.0, 1.0, 0.5),
        (0.5, 0.0, 1.0),
        (1.0, 0.0, 0.5),
        (0.0, 0.5, 1.0),
    ]
    pair_to_color = {}
    color_index = 0
    result = []
    for joint in joints:
        rb_a_bone = -1
        rb_b_bone = -1
        if rigidbodies is not None:
            if 0 <= joint.rigidbody_index_a < len(rigidbodies):
                rb_a_bone = rigidbodies[joint.rigidbody_index_a].bone_index
            if 0 <= joint.rigidbody_index_b < len(rigidbodies):
                rb_b_bone = rigidbodies[joint.rigidbody_index_b].bone_index
        if rb_a_bone == rb_b_bone and rb_a_bone >= 0:
            key = ('bone', rb_a_bone)
        else:
            key = (joint.rigidbody_index_a, joint.rigidbody_index_b)
        if key not in pair_to_color:
            pair_to_color[key] = colors[color_index % len(colors)]
            color_index += 1
        color = pair_to_color[key]
        verts, _ = create_joint_marker_lines(joint, color, joint_size)
        if len(verts) > 0:
            bone_index = rb_a_bone if rb_a_bone >= 0 else (rb_b_bone if rb_b_bone >= 0 else -1)
            cols = np.tile(color, (len(verts), 1)).astype(np.float32)
            result.append({
                'vertices': verts,
                'colors': cols,
                'bone_index': bone_index,
                'count': len(verts)
            })
    return result


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


def create_rigid_body_batched(rigidbodies):
    rb_data = create_rigid_body_local_data(rigidbodies)
    if not rb_data:
        empty_v = np.array([], dtype=np.float32).reshape(0, 3)
        return empty_v, empty_v, np.array([], dtype='i4')
    all_verts = np.vstack([d['vertices'] for d in rb_data])
    all_cols = np.vstack([d['colors'] for d in rb_data])
    all_bone_idx = np.concatenate([np.full(d['count'], d['bone_index'], dtype='i4') for d in rb_data])
    return all_verts, all_cols, all_bone_idx


def create_joint_batched(joints, rigidbodies=None, joint_size=0.05):
    jd_list = create_joint_local_data(joints, rigidbodies, joint_size)
    if not jd_list:
        empty_v = np.array([], dtype=np.float32).reshape(0, 3)
        return empty_v, empty_v, np.array([], dtype='i4')
    all_verts = np.vstack([d['vertices'] for d in jd_list])
    all_cols = np.vstack([d['colors'] for d in jd_list])
    all_bone_idx = np.concatenate([np.full(d['count'], d['bone_index'], dtype='i4') for d in jd_list])
    return all_verts, all_cols, all_bone_idx


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
