import numpy as np
from typing import Dict
from pmx_model import MORPH_TYPE_GROUP, MORPH_TYPE_VERTEX, MORPH_TYPE_MATERIAL, MORPH_TYPE_UV, MORPH_TYPE_BONE


class MorphController:
    def __init__(self):
        self.pmx_model = None
        self.morph_vbo = None
        self.uv_morph_vbo = None
        self.bone_texture = None
        self.model_scale = 1.0
        
        self.morph_weights: Dict[str, float] = {}
        self.active_morph_index = -1
        self.morph_weight_value = 0.0
        
        self.original_material_alphas: Dict[int, float] = {}
        self.material_morph_alphas: Dict[int, float] = {}
        self.bone_morph_transforms: Dict[int, Dict] = {}

    def set_model(self, pmx_model, morph_vbo, uv_morph_vbo, bone_texture, model_scale, original_material_alphas):
        self.pmx_model = pmx_model
        self.morph_vbo = morph_vbo
        self.uv_morph_vbo = uv_morph_vbo
        self.bone_texture = bone_texture
        self.model_scale = model_scale
        self.original_material_alphas = original_material_alphas

    def set_morph_weight(self, morph_name: str, weight: float):
        self.morph_weights[morph_name] = weight
        self._update_morph_offsets()

    def set_morph_weights(self, weights: Dict[str, float], skip_bone_morphs=False):
        self.morph_weights = weights
        self._update_morph_offsets(skip_bone_morphs)

    def apply_morph(self, morph_index: int, weight: float):
        if self.pmx_model is None:
            return
        morph = self.pmx_model.get_morph(morph_index)
        if morph:
            self.morph_weights[morph.name] = weight
            self._update_morph_offsets()

    def clear_morphs(self):
        self.morph_weights.clear()
        self._update_morph_offsets()

    def _update_morph_offsets(self, skip_bone_morphs=False):
        if self.pmx_model is None or self.morph_vbo is None:
            return
        
        vertex_count = self.pmx_model.vertex_count
        morph_offsets = np.zeros(vertex_count * 3, dtype='f4')
        uv_morph_offsets = np.zeros(vertex_count * 2, dtype='f4')
        self.material_morph_alphas.clear()
        self.bone_morph_transforms.clear()
        
        for morph_name, weight in self.morph_weights.items():
            if weight == 0.0:
                continue
            morph = self.pmx_model.get_morph_by_name(morph_name)
            if morph is None:
                continue
            self._apply_morph_recursive(morph, weight, morph_offsets, uv_morph_offsets, vertex_count, skip_bone_morphs)
        
        self.morph_vbo.write(morph_offsets.tobytes())
        if self.uv_morph_vbo:
            self.uv_morph_vbo.write(uv_morph_offsets.tobytes())
        
        if not skip_bone_morphs and self.bone_morph_transforms:
            self._apply_bone_morphs_to_texture()

    def _apply_morph_recursive(self, morph, weight, morph_offsets, uv_morph_offsets, vertex_count, skip_bone_morphs=False):
        if morph.morph_type == MORPH_TYPE_GROUP:
            for offset in morph.offsets:
                child_morph = self.pmx_model.get_morph(offset.morph_index)
                if child_morph:
                    child_weight = offset.value * weight
                    self._apply_morph_recursive(child_morph, child_weight, morph_offsets, uv_morph_offsets, vertex_count, skip_bone_morphs)
        
        elif morph.morph_type == MORPH_TYPE_VERTEX:
            for offset in morph.offsets:
                v_idx = offset.vertex_index
                if 0 <= v_idx < vertex_count:
                    morph_offsets[v_idx * 3] += offset.position_offset[0] * weight * self.model_scale
                    morph_offsets[v_idx * 3 + 1] += offset.position_offset[1] * weight * self.model_scale
                    morph_offsets[v_idx * 3 + 2] += offset.position_offset[2] * weight * self.model_scale
        
        elif morph.morph_type == MORPH_TYPE_UV:
            for offset in morph.offsets:
                v_idx = offset.vertex_index
                if 0 <= v_idx < vertex_count:
                    uv_morph_offsets[v_idx * 2] += offset.uv_offset[0] * weight
                    uv_morph_offsets[v_idx * 2 + 1] += offset.uv_offset[1] * weight
        
        elif morph.morph_type == MORPH_TYPE_MATERIAL:
            for offset in morph.offsets:
                mat_idx = offset.material_index
                if mat_idx < 0:
                    continue
                current_alpha = self.material_morph_alphas.get(mat_idx, self.original_material_alphas.get(mat_idx, 1.0))
                if offset.calc_mode == 0:
                    new_alpha = current_alpha * (offset.diffuse[3] * weight + (1 - weight))
                else:
                    new_alpha = current_alpha + offset.diffuse[3] * weight
                self.material_morph_alphas[mat_idx] = max(0.0, min(1.0, new_alpha))
        
        elif morph.morph_type == MORPH_TYPE_BONE:
            for offset in morph.offsets:
                bone_idx = offset.bone_index
                if bone_idx < 0:
                    continue
                if bone_idx not in self.bone_morph_transforms:
                    self.bone_morph_transforms[bone_idx] = {
                        'translation': np.zeros(3, dtype=np.float32),
                        'rotation': np.array([0, 0, 0, 1], dtype=np.float32)
                    }
                self.bone_morph_transforms[bone_idx]['translation'] += offset.position_offset * weight
                current_rot = self.bone_morph_transforms[bone_idx]['rotation']
                new_rot = self._quaternion_slerp(current_rot, offset.rotation, weight)
                self.bone_morph_transforms[bone_idx]['rotation'] = new_rot

    def _apply_bone_morphs_to_texture(self):
        if self.pmx_model is None or self.bone_texture is None:
            return
        
        matrices = self.pmx_model.get_bind_pose_matrices(1.0)
        
        for bone_idx, transform in self.bone_morph_transforms.items():
            if bone_idx >= len(matrices):
                continue
            
            translation = transform['translation'] * self.model_scale
            rotation = transform['rotation']
            
            rot_mat = self._quaternion_to_matrix(rotation)
            
            morph_mat = np.eye(4, dtype=np.float32)
            morph_mat[:3, :3] = rot_mat
            morph_mat[:3, 3] = translation
            
            matrices[bone_idx] = morph_mat @ matrices[bone_idx]
        
        from bone_math import pack_matrices_to_texture
        bone_tex_data, _, _ = pack_matrices_to_texture(matrices)
        self.bone_texture.write(bone_tex_data.tobytes())

    @staticmethod
    def _quaternion_slerp(q1, q2, t):
        if t <= 0:
            return q1
        if t >= 1:
            return q2.copy()
        
        dot = np.dot(q1, q2)
        if dot < 0:
            q2 = -q2
            dot = -dot
        
        if dot > 0.9995:
            result = q1 + t * (q2 - q1)
            return result / np.linalg.norm(result)
        
        theta_0 = np.arccos(np.clip(dot, -1, 1))
        theta = theta_0 * t
        
        sin_theta = np.sin(theta)
        sin_theta_0 = np.sin(theta_0)
        
        s1 = np.cos(theta) - dot * sin_theta / sin_theta_0
        s2 = sin_theta / sin_theta_0
        
        result = s1 * q1 + s2 * q2
        return result / np.linalg.norm(result)

    @staticmethod
    def _quaternion_to_matrix(q):
        x, y, z, w = q
        return np.array([
            [1 - 2*y*y - 2*z*z, 2*x*y - 2*z*w, 2*x*z + 2*y*w],
            [2*x*y + 2*z*w, 1 - 2*x*x - 2*z*z, 2*y*z - 2*x*w],
            [2*x*z - 2*y*w, 2*y*z + 2*x*w, 1 - 2*x*x - 2*y*y]
        ], dtype=np.float32)

    def get_material_alpha(self, material_index: int) -> float:
        if material_index in self.material_morph_alphas:
            return self.material_morph_alphas[material_index]
        return self.original_material_alphas.get(material_index, 1.0)
