from pymeshio.pmx import reader
import numpy as np


def next_pow2(x):
    """返回大于等于x的最小2的幂次"""
    return 1 if x == 0 else 2**(x - 1).bit_length()


def compute_bone_world_matrices(bones):
    """计算骨骼的世界变换矩阵（绑定姿态）
    只包含位置，不包含初始旋转
    """
    num_bones = len(bones)
    world_mats = np.zeros((num_bones, 4, 4), dtype=np.float32)
    
    for i, bone in enumerate(bones):
        world_mats[i] = np.eye(4, dtype=np.float32)
        world_mats[i, 0, 3] = bone.position[0]
        world_mats[i, 1, 3] = bone.position[1]
        world_mats[i, 2, 3] = bone.position[2]
    
    return world_mats


def compute_bind_pose_matrices(bones, debug_scale=1.0):
    """计算绑定姿态矩阵 - 返回恒等矩阵，因为PMX顶点已在模型空间
    debug_scale: 调试缩放因子，用于验证蒙皮是否工作（>1时可见变形）
    """
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
    """
    将4x4矩阵打包到RGBA32F纹理中
    每个矩阵需要4个RGBA纹素（每个纹素存储矩阵的一列）
    布局：[m00, m10, m20, m30] [m01, m11, m21, m31] [m02, m12, m22, m32] [m03, m13, m23, m33]
    """
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


class VpdPose:
    """VPD 姿势数据类"""
    def __init__(self, bone_name, tx, ty, tz, qx, qy, qz, qw):
        self.bone_name = bone_name
        self.position = np.array([tx, ty, tz], dtype=np.float32)
        self.quaternion = np.array([qx, qy, qz, qw], dtype=np.float32)

    def to_matrix(self):
        """将四元数和位置转换为 4x4 变换矩阵"""
        qx, qy, qz, qw = self.quaternion
        xx, yy, zz = qx*qx, qy*qy, qz*qz
        xy, yz, xz = qx*qy, qy*qz, qx*qz
        wx, wy, wz = qw*qx, qw*qy, qw*qz

        matrix = np.eye(4, dtype=np.float32)
        matrix[0, 0] = 1 - 2*(yy + zz)
        matrix[0, 1] = 2*(xy - wz)
        matrix[0, 2] = 2*(xz + wy)
        matrix[1, 0] = 2*(xy + wz)
        matrix[1, 1] = 1 - 2*(xx + zz)
        matrix[1, 2] = 2*(yz - wx)
        matrix[2, 0] = 2*(xz - wy)
        matrix[2, 1] = 2*(yz + wx)
        matrix[2, 2] = 1 - 2*(xx + yy)
        matrix[:3, 3] = self.position
        return matrix


class VpdLoader:
    """VPD 姿势文件加载器"""
    @staticmethod
    def load(file_path):
        """加载 VPD 文件并返回姿势数据字典 {bone_name: VpdPose}"""
        with open(file_path, 'rb') as f:
            raw = f.read()

        try:
            text = raw.decode('cp932')
        except:
            text = raw.decode('utf-8', errors='ignore')

        lines = text.split('\n')
        poses = {}

        i = 0
        while i < len(lines):
            line = lines[i].strip()
            if line.startswith('Bone'):
                parts = line.replace('Bone', '').split('{')
                bone_idx = int(parts[0])
                bone_name = parts[1].replace('}', '')

                i += 1
                trans_line = lines[i].split('//')[0].replace(';', '').strip()
                trans_parts = [p.strip() for p in trans_line.split(',')]
                tx, ty, tz = float(trans_parts[0]), float(trans_parts[1]), float(trans_parts[2])

                i += 1
                quat_line = lines[i].split('//')[0].replace(';', '').strip()
                quat_parts = [p.strip() for p in quat_line.split(',')]
                qx, qy, qz, qw = float(quat_parts[0]), float(quat_parts[1]), float(quat_parts[2]), float(quat_parts[3])

                poses[bone_name] = VpdPose(bone_name, tx, ty, tz, qx, qy, qz, qw)
            i += 1

        return poses


class BoneDeform:
    def __init__(self, deform):
        self._deform = deform
        self.type = type(deform).__name__

    @property
    def bone_indices(self):
        if self.type == 'Bdef1':
            return (self._deform.index0,)
        elif self.type == 'Bdef2':
            return (self._deform.index0, self._deform.index1)
        elif self.type == 'Bdef4':
            return (self._deform.index0, self._deform.index1,
                    self._deform.index2, self._deform.index3)
        return ()

    @property
    def weights(self):
        if self.type == 'Bdef1':
            return (1.0,)
        elif self.type == 'Bdef2':
            w0 = self._deform.weight0
            return (w0, 1.0 - w0)
        elif self.type == 'Bdef4':
            return (self._deform.weight0, self._deform.weight1,
                    self._deform.weight2, self._deform.weight3)
        return ()


class IkLink:
    def __init__(self, ik_link):
        self.bone_index = ik_link.bone_index
        self.limit_angle = ik_link.limit_angle
        self.limit_min = ik_link.limit_min if ik_link.limit_angle else None
        self.limit_max = ik_link.limit_max if ik_link.limit_angle else None


class IkData:
    def __init__(self, ik):
        self.target_index = ik.target_index
        self.loop = ik.loop
        self.limit_radian = ik.limit_radian
        self.links = [IkLink(link) for link in ik.link]


class Bone:
    def __init__(self, bone, index):
        self._bone = bone
        self.index = index
        self.name = bone.name
        self.english_name = bone.english_name
        self.position = np.array([bone.position.x, bone.position.y, bone.position.z], dtype='f')
        self.parent_index = bone.parent_index
        self.layer = bone.layer
        self.flag = bone.flag
        self.tail_position = np.array([bone.tail_position.x, bone.tail_position.y, bone.tail_position.z], dtype='f') if bone.tail_position else None
        self.tail_index = bone.tail_index
        self.effect_index = bone.effect_index
        self.effect_factor = bone.effect_factor
        self.fixed_axis = np.array([bone.fixed_axis.x, bone.fixed_axis.y, bone.fixed_axis.z], dtype='f') if bone.fixed_axis else None
        self.local_x_vector = np.array([bone.local_x_vector.x, bone.local_x_vector.y, bone.local_x_vector.z], dtype='f') if bone.local_x_vector else None
        self.local_z_vector = np.array([bone.local_z_vector.x, bone.local_z_vector.y, bone.local_z_vector.z], dtype='f') if bone.local_z_vector else None
        self.external_key = bone.external_key
        self.ik = IkData(bone.ik) if bone.ik else None

    @property
    def has_ik(self) -> bool:
        return self.ik is not None

    @property
    def is_visible(self) -> bool:
        return bool(self._bone.getVisibleFlag())

    @property
    def is_manipulatable(self) -> bool:
        return bool(self._bone.getManipulatable())

    @property
    def is_rotatable(self) -> bool:
        return bool(self._bone.getRotatable())

    @property
    def is_translatable(self) -> bool:
        return bool(self._bone.getTranslatable())

    @property
    def has_local_coordinate(self) -> bool:
        return bool(self._bone.getLocalCoordinateFlag())

    @property
    def has_fixed_axis(self) -> bool:
        return bool(self._bone.getFixedAxisFlag())

    @property
    def has_external_rotation(self) -> bool:
        return bool(self._bone.getExternalRotationFlag())

    @property
    def has_external_translation(self) -> bool:
        return bool(self._bone.getExternalTranslationFlag())

    @property
    def has_external_parent_deform(self) -> bool:
        return bool(self._bone.getExternalParentDeformFlag())

    @property
    def is_after_physics_deform(self) -> bool:
        return bool(self._bone.getAfterPhysicsDeformFlag())

    def get_world_position(self, parent_world_pos=None):
        if parent_world_pos is None:
            return self.position.copy()
        return parent_world_pos + self.position

    def get_init_rotation_matrix(self):
        """获取骨骼的初始旋转矩阵（从骨骼局部坐标系到世界坐标系）"""
        if self.local_x_vector is not None and self.local_z_vector is not None:
            x_norm = np.linalg.norm(self.local_x_vector)
            z_norm = np.linalg.norm(self.local_z_vector)
            
            if x_norm > 0.001 and z_norm > 0.001:
                local_x = self.local_x_vector
                local_z = self.local_z_vector
                local_y = np.cross(local_z, local_x)
                
                init_rot = np.eye(3, dtype=np.float32)
                init_rot[:, 0] = local_x
                init_rot[:, 1] = local_y
                init_rot[:, 2] = local_z
                return init_rot
        
        return np.eye(3, dtype=np.float32)


class PmxModel:
    def __init__(self, path: str):
        self._model = reader.read_from_file(path)

    @property
    def name(self) -> str:
        return self._model.name

    @property
    def english_name(self) -> str:
        return self._model.english_name

    @property
    def comment(self) -> str:
        return self._model.comment

    @property
    def vertices(self):
        return self._model.vertices

    @property
    def vertex_count(self) -> int:
        return len(self._model.vertices)

    @property
    def indices(self):
        return self._model.indices

    @property
    def face_count(self) -> int:
        return len(self._model.indices) // 3

    @property
    def textures(self):
        return self._model.textures

    @property
    def texture_count(self) -> int:
        return len(self._model.textures)

    @property
    def materials(self):
        return self._model.materials

    @property
    def material_count(self) -> int:
        return len(self._model.materials)

    @property
    def bones(self):
        return self._model.bones

    @property
    def bone_count(self) -> int:
        return len(self._model.bones)

    @property
    def morphs(self):
        return self._model.morphs

    @property
    def morph_count(self) -> int:
        return len(self._model.morphs)

    @property
    def display_slots(self):
        return self._model.display_slots

    def get_bones(self):
        return [Bone(bone, i) for i, bone in enumerate(self._model.bones)]

    def get_bone(self, index):
        if 0 <= index < self.bone_count:
            return Bone(self._model.bones[index], index)
        return None

    def get_deforms(self):
        return [BoneDeform(v.deform) for v in self._model.vertices]

    def get_deform(self, vertex_index):
        if 0 <= vertex_index < self.vertex_count:
            return BoneDeform(self._model.vertices[vertex_index].deform)
        return None

    def get_bone_weights_array(self):
        weights = np.zeros((self.vertex_count, 4), dtype='f')
        for i, v in enumerate(self._model.vertices):
            deform = BoneDeform(v.deform)
            indices = deform.bone_indices
            w = deform.weights
            for j, idx in enumerate(indices):
                weights[i, j] = idx
            weights[i, 3] = len(indices)
        return weights

    def get_skinning_data(self):
        bone_indices = np.zeros((self.vertex_count, 4), dtype='i')
        bone_weights = np.zeros((self.vertex_count, 4), dtype='f')
        for i, v in enumerate(self._model.vertices):
            deform = BoneDeform(v.deform)
            indices = deform.bone_indices
            weights = deform.weights
            for j in range(4):
                if j < len(indices):
                    bone_indices[i, j] = indices[j]
                    bone_weights[i, j] = weights[j]
                else:
                    bone_indices[i, j] = 0
                    bone_weights[i, j] = 0.0
        return bone_indices, bone_weights

    def get_bone_world_matrices(self):
        """计算并返回骨骼世界变换矩阵"""
        bones = self.get_bones()
        return compute_bone_world_matrices(bones)

    def get_bind_pose_matrices(self, debug_scale=1.0):
        """计算并返回绑定姿态矩阵
        debug_scale: 调试缩放因子，用于验证蒙皮是否工作
        """
        bones = self.get_bones()
        return compute_bind_pose_matrices(bones, debug_scale)

    def get_bone_texture_data(self, debug_scale=1.0, transform_params=None):
        """获取打包后的骨骼纹理数据
        debug_scale: 调试缩放因子
        transform_params: 顶点变换参数 {'center': [x,y,z], 'min_y': float, 'scale': float}
        """
        matrices = self.get_bind_pose_matrices(debug_scale)
        
        if transform_params:
            center = transform_params['center']
            min_y = transform_params['min_y']
            scale = transform_params['scale']
            
            offset = np.array([center[0], min_y, center[2]])
            offset_scaled = scale * offset
            
            for i in range(len(matrices)):
                M = matrices[i]
                R = M[:3, :3]
                t = M[:3, 3]
                
                t_new = t * scale + (R - np.eye(3)) @ offset_scaled
                matrices[i, :3, 3] = t_new
        
        return pack_matrices_to_texture(matrices)

    def get_bone_matrices_with_pose(self, vpd_poses, debug_scale=1.0, transform_params=None):
        """根据 VPD 姿势计算骨骼矩阵
        vpd_poses: VpdLoader.load() 返回的姿势字典 {bone_name: VpdPose}
        transform_params: 顶点变换参数 {'center': [x,y,z], 'min_y': float, 'scale': float}
        """
        bones = self.get_bones()
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

    def get_all_skinning_vertex_data(self):
        """获取完整的蒙皮顶点数据（位置、法线、UV、骨骼索引、骨骼权重）"""
        positions = []
        normals = []
        uvs = []
        bone_indices = []
        bone_weights = []
        
        for v in self._model.vertices:
            positions.extend([v.position[0], v.position[1], v.position[2]])
            normals.extend([v.normal[0], v.normal[1], v.normal[2]])
            uvs.extend([v.uv[0], v.uv[1]])
            
            deform = BoneDeform(v.deform)
            indices = deform.bone_indices
            weights = deform.weights
            
            for j in range(4):
                if j < len(indices):
                    bone_indices.append(indices[j])
                    bone_weights.append(weights[j])
                else:
                    bone_indices.append(0)
                    bone_weights.append(0.0)
        
        return {
            'positions': np.array(positions, dtype='f4'),
            'normals': np.array(normals, dtype='f4'),
            'uvs': np.array(uvs, dtype='f4'),
            'bone_indices': np.array(bone_indices, dtype='i4'),
            'bone_weights': np.array(bone_weights, dtype='f4')
        }

    def __repr__(self):
        return (f"PmxModel(name={self.name}, vertices={self.vertex_count}, "
                f"faces={self.face_count}, bones={self.bone_count})")


def main():
    model_path = "resources/models/ikaros-origin/Ikaros.pmx"

    print(f"Loading PMX model: {model_path}")
    model = PmxModel(model_path)

    print(f"\n=== Model Info ===")
    print(f"Name: {model.name}")
    print(f"English Name: {model.english_name}")
    print(f"Comment: {model.comment}")

    print(f"\n=== Vertices ===")
    print(f"Vertex count: {model.vertex_count}")
    if model.vertices:
        v = model.vertices[0]
        print(f"First vertex position: {v.position}")

    print(f"\n=== Indices ===")
    print(f"Triangle count: {model.face_count}")

    print(f"\n=== Textures ===")
    print(f"Texture count: {model.texture_count}")
    for i, tex in enumerate(model.textures):
        print(f"  [{i}] {tex}")

    print(f"\n=== Materials ===")
    print(f"Material count: {model.material_count}")
    for i, mat in enumerate(model.materials):
        print(f"  [{i}] {mat.name}")

    print(f"\n=== Bones ===")
    print(f"Bone count: {model.bone_count}")
    bones = model.get_bones()
    for i, bone in enumerate(bones[:10]):
        print(f"  [{i}] {bone.name}: pos={bone.position}, parent={bone.parent_index}, has_ik={bone.has_ik}")
    if model.bone_count > 10:
        print(f"  ... and {model.bone_count - 10} more bones")

    ik_bones = [b for b in bones if b.has_ik]
    print(f"\n  IK bones: {len(ik_bones)}")
    for bone in ik_bones[:5]:
        print(f"    {bone.name}: target={bone.ik.target_index}, loop={bone.ik.loop}, limit={bone.ik.limit_radian}, links={len(bone.ik.links)}")
    if len(ik_bones) > 5:
        print(f"    ... and {len(ik_bones) - 5} more IK bones")

    print(f"\n=== Skinning Data ===")
    bone_indices, bone_weights = model.get_skinning_data()
    print(f"Bone indices shape: {bone_indices.shape}")
    print(f"Bone weights shape: {bone_weights.shape}")
    print(f"First 5 vertices bone weights:")
    for i in range(5):
        indices = bone_indices[i]
        weights = bone_weights[i]
        print(f"  Vertex {i}: indices={indices}, weights={weights}")

    print(f"\n=== Morphs ===")
    print(f"Morph count: {model.morph_count}")

    print(f"\n{model}")

if __name__ == "__main__":
    main()