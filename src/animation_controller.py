import numpy as np
from typing import Dict, Optional
from vpd_loader import VpdLoader
from bone_math import pack_matrices_to_texture
from vmd_player import VmdLoader, VmdMixer


class AnimationController:
    def __init__(self, fps: float = 30.0):
        self.vmd_mixer: Optional[VmdMixer] = None
        self.vmd_playing = False
        self.vmd_loop = True
        self.vmd_fps = fps
        
        self.vpd_poses: Optional[Dict] = None
        self.vpd_pose_applied = False
        
        self.pmx_model = None
        self.bone_texture = None
        self.model_center = [0, 0, 0]
        self.model_min_pos = [0, 0, 0]
        self.model_scale = 1.0
        
        self.bone_morph_transforms: Dict[int, Dict] = {}
        self.current_bone_matrices: Optional[np.ndarray] = None
        self.pose_world_matrices: Optional[np.ndarray] = None

    def set_model(self, pmx_model, bone_texture, model_center, model_min_pos, model_scale):
        self.pmx_model = pmx_model
        self.bone_texture = bone_texture
        self.model_center = model_center
        self.model_min_pos = model_min_pos
        self.model_scale = model_scale

    def load_vpd(self, vpd_path: str) -> bool:
        try:
            self.vpd_poses = VpdLoader.load(vpd_path)
            matched = 0
            if self.pmx_model:
                bone_names = {b.name for b in self.pmx_model.get_bones()}
                matched = len(set(self.vpd_poses.keys()) & bone_names)
            print(f"Loaded VPD poses: {len(self.vpd_poses)} bones, {matched} matched with model")
            return True
        except Exception as e:
            print(f"Failed to load VPD: {e}")
            return False

    def load_vmd(self, vmd_path: str) -> bool:
        try:
            animation = VmdLoader.load(vmd_path)
            
            if self.vmd_mixer is None:
                self.vmd_mixer = VmdMixer(self.vmd_fps)
            
            self.vmd_mixer.add_vmd(animation)
            
            bone_count = len(animation.bone_keyframes)
            morph_count = len(animation.morph_keyframes)
            max_frame = animation.max_frame
            
            print(f"Loaded VMD: {vmd_path}")
            print(f"  Model: {animation.model_name}")
            print(f"  Bone animations: {bone_count}")
            print(f"  Morph animations: {morph_count}")
            print(f"  Duration: {max_frame / self.vmd_fps:.2f}s ({max_frame} frames)")
            print(f"  Total VMD layers: {len(self.vmd_mixer.players)}")
            
            return True
        except Exception as e:
            print(f"Failed to load VMD: {e}")
            return False

    def clear_vmd(self):
        if self.vmd_mixer:
            self.vmd_mixer.clear()
            self.vmd_playing = False

    def play_vmd(self):
        if self.vmd_mixer and self.vmd_mixer.players:
            self.vmd_playing = True
            self.vmd_mixer.play()
            print("VMD animation: PLAYING")

    def pause_vmd(self):
        if self.vmd_mixer and self.vmd_mixer.players:
            self.vmd_playing = False
            self.vmd_mixer.pause()
            print("VMD animation: PAUSED")

    def stop_vmd(self):
        if self.vmd_mixer and self.vmd_mixer.players:
            self.vmd_playing = False
            self.vmd_mixer.stop()
            print("VMD animation: STOPPED")

    def update(self, delta_time: float):
        if self.vmd_playing and self.vmd_mixer:
            self.vmd_mixer.loop = self.vmd_loop
            self.vmd_mixer.update(delta_time)

    def apply_vmd_frame(self, morph_controller=None):
        if self.pmx_model is None or self.vmd_mixer is None:
            return {}
        
        bone_poses = {}
        for bone in self.pmx_model.get_bones():
            transform = self.vmd_mixer.get_bone_transform(bone.name)
            if transform is not None:
                position, rotation = transform
                bone_poses[bone.name] = (position, rotation)
        
        morph_weights = self.vmd_mixer.get_active_morphs()
        if morph_weights and morph_controller:
            morph_controller.set_morph_weights(morph_weights, skip_bone_morphs=True)
        
        if bone_poses:
            self._apply_vmd_bone_transforms(bone_poses, apply_bone_morphs=True)
        
        if int(self.vmd_mixer.current_frame) % 100 == 0 and self.vmd_mixer.current_frame < 1:
            print(f"VMD frame: {self.vmd_mixer.current_frame:.0f}/{self.vmd_mixer.max_frame}, bones: {len(bone_poses)}, morphs: {len(morph_weights)}")
        
        return morph_weights

    def _compute_bone_world_hierarchy(self, bones, bone_poses=None, bone_morphs=False):
        num_bones = len(bones)
        bind_world = [np.eye(4, dtype=np.float32) for _ in range(num_bones)]
        for i, bone in enumerate(bones):
            if bone.parent_index >= 0:
                parent_pos = bones[bone.parent_index].position
                local_pos = bone.position - parent_pos
                local_mat = np.eye(4, dtype=np.float32)
                local_mat[0, 3] = local_pos[0]
                local_mat[1, 3] = local_pos[1]
                local_mat[2, 3] = local_pos[2]
                bind_world[i] = bind_world[bone.parent_index] @ local_mat
            else:
                bind_world[i][0, 3] = bone.position[0]
                bind_world[i][1, 3] = bone.position[1]
                bind_world[i][2, 3] = bone.position[2]
        pose_world = [np.eye(4, dtype=np.float32) for _ in range(num_bones)]
        for i, bone in enumerate(bones):
            local_mat = np.eye(4, dtype=np.float32)
            if bone.parent_index >= 0:
                parent_pos = bones[bone.parent_index].position
                local_pos = bone.position - parent_pos
                local_mat[0, 3] = local_pos[0]
                local_mat[1, 3] = local_pos[1]
                local_mat[2, 3] = local_pos[2]
                if bone_morphs and i in self.bone_morph_transforms:
                    morph_transform = self.bone_morph_transforms[i]
                    morph_rot = self._quaternion_to_matrix(morph_transform['rotation'])
                    morph_mat = np.eye(4, dtype=np.float32)
                    morph_mat[:3, :3] = morph_rot
                    morph_mat[:3, 3] = morph_transform['translation']
                    local_mat = morph_mat @ local_mat
                if bone_poses and bone.name in bone_poses:
                    position, rotation = bone_poses[bone.name]
                    rot_mat = self._quaternion_to_matrix(rotation)
                    local_mat[:3, :3] = rot_mat
                    local_mat[0, 3] += position[0]
                    local_mat[1, 3] += position[1]
                    local_mat[2, 3] += position[2]
                pose_world[i] = pose_world[bone.parent_index] @ local_mat
            else:
                if bone_morphs and i in self.bone_morph_transforms:
                    morph_transform = self.bone_morph_transforms[i]
                    morph_rot = self._quaternion_to_matrix(morph_transform['rotation'])
                    morph_mat = np.eye(4, dtype=np.float32)
                    morph_mat[:3, :3] = morph_rot
                    morph_mat[:3, 3] = morph_transform['translation']
                    local_mat = morph_mat @ local_mat
                if bone_poses and bone.name in bone_poses:
                    position, rotation = bone_poses[bone.name]
                    rot_mat = self._quaternion_to_matrix(rotation)
                    local_mat[:3, :3] = rot_mat
                    local_mat[0, 3] += position[0]
                    local_mat[1, 3] += position[1]
                    local_mat[2, 3] += position[2]
                pose_world[i] = local_mat
        return bind_world, pose_world

    def _compute_pose_world(self, bones, bone_transforms=None):
        """Compute correct hierarchical bone world matrices.
        PMX bone.position is absolute, so child bones must use
        (position - parent_position) as the local translation."""
        n = len(bones)
        world = [np.eye(4, dtype=np.float32) for _ in range(n)]
        for i, bone in enumerate(bones):
            local = np.eye(4, dtype=np.float32)
            if bone.parent_index >= 0:
                parent_pos = bones[bone.parent_index].position
                local_pos = bone.position - parent_pos
                local[0, 3] = local_pos[0]
                local[1, 3] = local_pos[1]
                local[2, 3] = local_pos[2]
            else:
                local[0, 3] = bone.position[0]
                local[1, 3] = bone.position[1]
                local[2, 3] = bone.position[2]
            if bone_transforms and bone.name in bone_transforms:
                tf = bone_transforms[bone.name]
                if hasattr(tf, 'to_matrix'):
                    local[:3, :3] = tf.to_matrix()[:3, :3]
                else:
                    pos, rot = tf
                    local[:3, :3] = self._quaternion_to_matrix(rot)
                    local[:3, 3] += pos
            if bone.parent_index >= 0:
                world[i] = world[bone.parent_index] @ local
            else:
                world[i] = local
        return np.array([m.copy() for m in world], dtype=np.float32)

    def _apply_vmd_bone_transforms(self, bone_poses: dict, apply_bone_morphs=False):
        if self.pmx_model is None or self.bone_texture is None:
            return

        transform_params = {
            'center': self.model_center,
            'min_y': self.model_min_pos[1],
            'scale': self.model_scale
        }

        bones = self.pmx_model.get_bones()
        num_bones = len(bones)

        bind_world, _ = self._compute_bone_world_hierarchy(
            bones, bone_poses, apply_bone_morphs)
        pose_world = self._compute_pose_world(bones, bone_poses)
        self.pose_world_matrices = pose_world

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

        self.current_bone_matrices = matrices.copy()

        from bone_math import pack_matrices_to_texture
        bone_tex_data, _, _ = pack_matrices_to_texture(matrices)
        self.bone_texture.write(bone_tex_data.tobytes())

    def reload_bone_texture(self, debug_scale=1.0, use_pose=False):
        if self.pmx_model is None:
            return
        bones = self.pmx_model.get_bones()
        num_bones = len(bones)
        bind_world, _ = self._compute_bone_world_hierarchy(bones)
        if use_pose and self.vpd_poses:
            pose_world = self._compute_pose_world(bones, self.vpd_poses)
        else:
            pose_world = self._compute_pose_world(bones)
        self.pose_world_matrices = pose_world

        matrices = np.zeros((num_bones, 4, 4), dtype=np.float32)
        for i in range(num_bones):
            inv_bind = np.linalg.inv(bind_world[i])
            matrices[i] = pose_world[i] @ inv_bind
            if debug_scale != 1.0:
                matrices[i, 0, 0] = debug_scale
                matrices[i, 1, 1] = debug_scale
                matrices[i, 2, 2] = debug_scale

        transform_params = {
            'center': self.model_center,
            'min_y': self.model_min_pos[1],
            'scale': self.model_scale
        }
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

        bone_tex_data, tex_width, tex_height = pack_matrices_to_texture(matrices)
        self.bone_texture.write(bone_tex_data.tobytes())
        self.current_bone_matrices = matrices.copy()

    @staticmethod
    def _quaternion_to_matrix(q):
        x, y, z, w = q
        return np.array([
            [1 - 2*y*y - 2*z*z, 2*x*y - 2*z*w, 2*x*z + 2*y*w],
            [2*x*y + 2*z*w, 1 - 2*x*x - 2*z*z, 2*y*z - 2*x*w],
            [2*x*z - 2*y*w, 2*y*z + 2*x*w, 1 - 2*x*x - 2*y*y]
        ], dtype=np.float32)
