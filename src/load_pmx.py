from pymeshio.pmx import reader
import numpy as np


def next_pow2(x):
    """返回大于等于x的最小2的幂次"""
    return 1 if x == 0 else 2**(x - 1).bit_length()


def compute_bone_world_matrices(bones):
    """计算骨骼的世界变换矩阵（用于绑定姿态显示）"""
    num_bones = len(bones)
    world_mats = np.zeros((num_bones, 4, 4), dtype=np.float32)
    
    for i, bone in enumerate(bones):
        world_mat = np.eye(4, dtype=np.float32)
        world_mat[:3, 3] = bone.position
        
        if bone.parent_index >= 0:
            world_mats[i] = world_mats[bone.parent_index] @ world_mat
        else:
            world_mats[i] = world_mat
    
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

    def get_bone_texture_data(self, debug_scale=1.0):
        """获取打包后的骨骼纹理数据
        debug_scale: 调试缩放因子
        """
        bind_pose_mats = self.get_bind_pose_matrices(debug_scale)
        return pack_matrices_to_texture(bind_pose_mats)

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