from pymeshio.pmx import reader as pmx_reader
from pymeshio import common
import numpy as np

from bone_transform import (
    compute_bone_world_matrices,
    compute_bind_pose_matrices,
    pack_matrices_to_texture,
    get_bone_matrices_with_pose,
)


class ParseException(Exception):
    def __init__(self, message, value=None):
        self.message = message
        self.value = value


common.ParseException = ParseException


_original_reader_init = pmx_reader.Reader.__init__
_original_read_vertex = pmx_reader.Reader.read_vertex
_pmx = pmx_reader.pmx


def _patched_reader_init(self, ios, text_encoding, extended_uv, vertex_index_size,
                         texture_index_size, material_index_size, bone_index_size,
                         morph_index_size, rigidbody_index_size):
    super(pmx_reader.Reader, self).__init__(ios)
    self.read_text = self.get_read_text(text_encoding)
    self._extended_uv = extended_uv
    if vertex_index_size <= 2:
        self.read_vertex_index = lambda: self.read_uint(vertex_index_size)
    else:
        self.read_vertex_index = lambda: self.read_int(vertex_index_size)
    self.read_texture_index = lambda: self.read_int(texture_index_size)
    self.read_material_index = lambda: self.read_int(material_index_size)
    self.read_bone_index = lambda: self.read_int(bone_index_size)
    self.read_morph_index = lambda: self.read_int(morph_index_size)
    self.read_rigidbody_index = lambda: self.read_int(rigidbody_index_size)


def _patched_read_vertex(self):
    pos = self.read_vector3()
    normal = self.read_vector3()
    uv = self.read_vector2()
    for _ in range(self._extended_uv):
        self.read_vector4()
    deform = self.read_deform()
    edge_factor = self.read_float()
    return _pmx.Vertex(pos, normal, uv, deform, edge_factor)


pmx_reader.Reader.__init__ = _patched_reader_init
pmx_reader.Reader.read_vertex = _patched_read_vertex


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
        self._model = pmx_reader.read_from_file(path)

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

    def get_all_skinning_vertex_data(self):
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

    def get_bone_world_matrices(self):
        bones = self.get_bones()
        return compute_bone_world_matrices(bones)

    def get_bind_pose_matrices(self, debug_scale=1.0):
        bones = self.get_bones()
        return compute_bind_pose_matrices(bones, debug_scale)

    def get_bone_texture_data(self, debug_scale=1.0, transform_params=None):
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
        bones = self.get_bones()
        return get_bone_matrices_with_pose(bones, vpd_poses, transform_params)

    def __repr__(self):
        return (f"PmxModel(name={self.name}, vertices={self.vertex_count}, "
                f"faces={self.face_count}, bones={self.bone_count})")
