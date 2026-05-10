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


RIGID_SHAPE_SPHERE = 0
RIGID_SHAPE_BOX = 1
RIGID_SHAPE_CAPSULE = 2


class RigidBody:
    def __init__(self, rigid_body, index):
        self._rigid_body = rigid_body
        self.index = index
        self.name = rigid_body.name
        self.english_name = rigid_body.english_name
        self.bone_index = rigid_body.bone_index
        self.collision_group = rigid_body.collision_group
        self.no_collision_group = rigid_body.no_collision_group
        self.shape_type = rigid_body.shape_type
        self.shape_size = np.array([
            rigid_body.shape_size.x,
            rigid_body.shape_size.y,
            rigid_body.shape_size.z
        ], dtype=np.float32)
        self.shape_position = np.array([
            rigid_body.shape_position.x,
            rigid_body.shape_position.y,
            rigid_body.shape_position.z
        ], dtype=np.float32)
        self.shape_rotation = np.array([
            rigid_body.shape_rotation.x,
            rigid_body.shape_rotation.y,
            rigid_body.shape_rotation.z
        ], dtype=np.float32)
        self.mass = rigid_body.param.mass
        self.linear_damping = rigid_body.param.linear_damping
        self.angular_damping = rigid_body.param.angular_damping
        self.restitution = rigid_body.param.restitution
        self.friction = rigid_body.param.friction
        self.mode = rigid_body.mode

    @property
    def is_sphere(self):
        return self.shape_type == RIGID_SHAPE_SPHERE

    @property
    def is_box(self):
        return self.shape_type == RIGID_SHAPE_BOX

    @property
    def is_capsule(self):
        return self.shape_type == RIGID_SHAPE_CAPSULE


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


MORPH_TYPE_GROUP = 0
MORPH_TYPE_VERTEX = 1
MORPH_TYPE_BONE = 2
MORPH_TYPE_UV = 3
MORPH_TYPE_UV_EXT1 = 4
MORPH_TYPE_UV_EXT2 = 5
MORPH_TYPE_UV_EXT3 = 6
MORPH_TYPE_UV_EXT4 = 7
MORPH_TYPE_MATERIAL = 8


class VertexMorphOffset:
    def __init__(self, offset):
        self.vertex_index = offset.vertex_index
        self.position_offset = np.array([
            offset.position_offset.x,
            offset.position_offset.y,
            offset.position_offset.z
        ], dtype=np.float32)


class UVMorphOffset:
    def __init__(self, offset):
        self.vertex_index = offset.vertex_index
        self.uv_offset = np.array([
            offset.uv.x,
            offset.uv.y
        ], dtype=np.float32)


class BoneMorphOffset:
    def __init__(self, offset):
        self.bone_index = offset.bone_index
        self.position_offset = np.array([
            offset.position.x,
            offset.position.y,
            offset.position.z
        ], dtype=np.float32)
        self.rotation = np.array([
            offset.rotation.x,
            offset.rotation.y,
            offset.rotation.z,
            offset.rotation.w
        ], dtype=np.float32)


class MaterialMorphOffset:
    def __init__(self, offset):
        self.material_index = offset.material_index
        self.calc_mode = offset.calc_mode
        self.diffuse = np.array([
            offset.diffuse.r,
            offset.diffuse.g,
            offset.diffuse.b,
            offset.diffuse.a
        ], dtype=np.float32) if offset.diffuse else np.zeros(4, dtype=np.float32)
        self.specular = np.array([
            offset.specular.r,
            offset.specular.g,
            offset.specular.b
        ], dtype=np.float32) if offset.specular else np.zeros(3, dtype=np.float32)
        self.specular_factor = offset.specular_factor
        self.ambient = np.array([
            offset.ambient.r,
            offset.ambient.g,
            offset.ambient.b
        ], dtype=np.float32) if offset.ambient else np.zeros(3, dtype=np.float32)
        self.edge_color = np.array([
            offset.edge_color.r,
            offset.edge_color.g,
            offset.edge_color.b,
            offset.edge_color.a
        ], dtype=np.float32) if offset.edge_color else np.zeros(4, dtype=np.float32)
        self.edge_size = offset.edge_size
        self.texture_factor = np.array([
            offset.texture_factor.r,
            offset.texture_factor.g,
            offset.texture_factor.b,
            offset.texture_factor.a
        ], dtype=np.float32) if offset.texture_factor else np.zeros(4, dtype=np.float32)
        self.sphere_texture_factor = np.array([
            offset.sphere_texture_factor.r,
            offset.sphere_texture_factor.g,
            offset.sphere_texture_factor.b,
            offset.sphere_texture_factor.a
        ], dtype=np.float32) if offset.sphere_texture_factor else np.zeros(4, dtype=np.float32)
        self.toon_texture_factor = np.array([
            offset.toon_texture_factor.r,
            offset.toon_texture_factor.g,
            offset.toon_texture_factor.b,
            offset.toon_texture_factor.a
        ], dtype=np.float32) if offset.toon_texture_factor else np.zeros(4, dtype=np.float32)


class GroupMorphOffset:
    def __init__(self, offset):
        self.morph_index = offset.morph_index
        self.value = offset.value


class Morph:
    def __init__(self, morph, index):
        self._morph = morph
        self.index = index
        self.name = morph.name
        self.english_name = morph.english_name
        self.panel = morph.panel
        self.morph_type = morph.morph_type
        self._offsets = None
    
    @property
    def offsets(self):
        if self._offsets is None:
            self._offsets = self._parse_offsets()
        return self._offsets
    
    def _parse_offsets(self):
        if self.morph_type == MORPH_TYPE_VERTEX:
            if self._morph.offsets is None:
                return []
            return [VertexMorphOffset(o) for o in self._morph.offsets]
        elif self.morph_type == MORPH_TYPE_UV:
            if self._morph.offsets is None:
                return []
            return [UVMorphOffset(o) for o in self._morph.offsets]
        elif self.morph_type == MORPH_TYPE_BONE:
            if self._morph.offsets is None:
                return []
            return [BoneMorphOffset(o) for o in self._morph.offsets]
        elif self.morph_type == MORPH_TYPE_MATERIAL:
            if hasattr(self._morph, 'data') and self._morph.data:
                return [MaterialMorphOffset(o) for o in self._morph.data]
            elif self._morph.offsets:
                return [MaterialMorphOffset(o) for o in self._morph.offsets]
            return []
        elif self.morph_type == MORPH_TYPE_GROUP:
            if self._morph.offsets is None:
                return []
            return [GroupMorphOffset(o) for o in self._morph.offsets]
        else:
            return []
    
    @property
    def is_vertex_morph(self):
        return self.morph_type == MORPH_TYPE_VERTEX
    
    @property
    def is_group_morph(self):
        return self.morph_type == MORPH_TYPE_GROUP
    
    @property
    def is_bone_morph(self):
        return self.morph_type == MORPH_TYPE_BONE
    
    @property
    def is_uv_morph(self):
        return self.morph_type == MORPH_TYPE_UV
    
    @property
    def is_material_morph(self):
        return self.morph_type == MORPH_TYPE_MATERIAL
    
    def __repr__(self):
        return f"Morph(name={self.name}, type={self.morph_type}, offsets={len(self.offsets)})"


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

    @property
    def rigidbodies(self):
        return getattr(self._model, 'rigidbodies', [])

    @property
    def rigidbody_count(self) -> int:
        return len(getattr(self._model, 'rigidbodies', []))

    def get_rigidbodies(self):
        return [RigidBody(rb, i) for i, rb in enumerate(getattr(self._model, 'rigidbodies', []))]

    def get_rigidbody(self, index):
        rigidbodies = getattr(self._model, 'rigidbodies', [])
        if 0 <= index < len(rigidbodies):
            return RigidBody(rigidbodies[index], index)
        return None

    def get_bones(self):
        return [Bone(bone, i) for i, bone in enumerate(self._model.bones)]

    def get_bone(self, index):
        if 0 <= index < self.bone_count:
            return Bone(self._model.bones[index], index)
        return None

    def get_morphs(self):
        return [Morph(morph, i) for i, morph in enumerate(self._model.morphs)]

    def get_morph(self, index):
        if 0 <= index < self.morph_count:
            return Morph(self._model.morphs[index], index)
        return None

    def get_morph_by_name(self, name):
        for i, morph in enumerate(self._model.morphs):
            if morph.name == name or morph.english_name == name:
                return Morph(morph, i)
        return None

    def get_vertex_morphs(self):
        return [Morph(m, i) for i, m in enumerate(self._model.morphs) if m.morph_type == MORPH_TYPE_VERTEX]

    def get_group_morphs(self):
        return [Morph(m, i) for i, m in enumerate(self._model.morphs) if m.morph_type == MORPH_TYPE_GROUP]

    def get_material_morphs(self):
        return [Morph(m, i) for i, m in enumerate(self._model.morphs) if m.morph_type == MORPH_TYPE_MATERIAL]

    def get_available_morphs(self):
        from pmx_reader import MORPH_TYPE_VERTEX, MORPH_TYPE_GROUP, MORPH_TYPE_MATERIAL, MORPH_TYPE_UV, MORPH_TYPE_BONE
        return [Morph(m, i) for i, m in enumerate(self._model.morphs) 
                if m.morph_type in (MORPH_TYPE_VERTEX, MORPH_TYPE_GROUP, MORPH_TYPE_MATERIAL, MORPH_TYPE_UV, MORPH_TYPE_BONE)]

    def get_morph_offsets_data(self, morph_indices, weights=None):
        vertex_offsets = {}
        for i, morph_idx in enumerate(morph_indices):
            if morph_idx < 0 or morph_idx >= self.morph_count:
                continue
            morph = self.get_morph(morph_idx)
            if not morph.is_vertex_morph:
                continue
            weight = weights[i] if weights and i < len(weights) else 1.0
            for offset in morph.offsets:
                v_idx = offset.vertex_index
                if v_idx not in vertex_offsets:
                    vertex_offsets[v_idx] = np.zeros(3, dtype=np.float32)
                vertex_offsets[v_idx] += offset.position_offset * weight
        return vertex_offsets

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
        
        return pack_matrices_to_texture(matrices)

    def get_bone_matrices_with_pose(self, vpd_poses, debug_scale=1.0, transform_params=None):
        bones = self.get_bones()
        return get_bone_matrices_with_pose(bones, vpd_poses, transform_params)

    def __repr__(self):
        return (f"PmxModel(name={self.name}, vertices={self.vertex_count}, "
                f"faces={self.face_count}, bones={self.bone_count})")
