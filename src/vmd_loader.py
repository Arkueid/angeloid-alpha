import struct
import numpy as np
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


@dataclass
class BoneKeyframe:
    bone_name: str
    frame: int
    position: np.ndarray
    rotation: np.ndarray
    interpolation: bytes
    
    def __post_init__(self):
        self.position = np.array(self.position, dtype=np.float32)
        self.rotation = np.array(self.rotation, dtype=np.float32)


@dataclass
class MorphKeyframe:
    morph_name: str
    frame: int
    weight: float


@dataclass
class CameraKeyframe:
    frame: int
    distance: float
    position: np.ndarray
    rotation: np.ndarray
    interpolation: bytes
    fov: int
    perspective: bool


@dataclass
class LightKeyframe:
    frame: int
    color: np.ndarray
    position: np.ndarray


@dataclass
class VmdAnimation:
    model_name: str
    bone_keyframes: Dict[str, List[BoneKeyframe]] = field(default_factory=dict)
    morph_keyframes: Dict[str, List[MorphKeyframe]] = field(default_factory=dict)
    camera_keyframes: List[CameraKeyframe] = field(default_factory=list)
    light_keyframes: List[LightKeyframe] = field(default_factory=list)
    
    max_frame: int = 0
    
    def get_bone_keyframes(self, bone_name: str) -> List[BoneKeyframe]:
        return self.bone_keyframes.get(bone_name, [])
    
    def get_morph_keyframes(self, morph_name: str) -> List[MorphKeyframe]:
        return self.morph_keyframes.get(morph_name, [])


class VmdLoader:
    BONE_NAME_SIZE = 15
    MORPH_NAME_SIZE = 15
    
    @staticmethod
    def load(filepath: str, encoding: str = 'shift_jis') -> VmdAnimation:
        with open(filepath, 'rb') as f:
            magic = f.read(30)
            magic_str = magic.decode('ascii', errors='ignore').strip()
            
            if 'Vocaloid Motion Data' not in magic_str:
                raise ValueError(f"Invalid VMD file: {filepath}")
            
            model_name_bytes = f.read(20)
            model_name = model_name_bytes.decode(encoding, errors='ignore').rstrip('\x00')
            
            animation = VmdAnimation(model_name=model_name)
            
            bone_key_count = struct.unpack('<I', f.read(4))[0]
            
            for _ in range(bone_key_count):
                bone_name_bytes = f.read(VmdLoader.BONE_NAME_SIZE)
                bone_name = bone_name_bytes.decode(encoding, errors='ignore').rstrip('\x00')
                
                frame = struct.unpack('<I', f.read(4))[0]
                position = struct.unpack('<fff', f.read(12))
                rotation = struct.unpack('<ffff', f.read(16))
                interpolation = f.read(64)
                
                keyframe = BoneKeyframe(
                    bone_name=bone_name,
                    frame=frame,
                    position=position,
                    rotation=rotation,
                    interpolation=interpolation
                )
                
                if bone_name not in animation.bone_keyframes:
                    animation.bone_keyframes[bone_name] = []
                animation.bone_keyframes[bone_name].append(keyframe)
                
                animation.max_frame = max(animation.max_frame, frame)
            
            morph_key_count = struct.unpack('<I', f.read(4))[0]
            
            for _ in range(morph_key_count):
                morph_name_bytes = f.read(VmdLoader.MORPH_NAME_SIZE)
                morph_name = morph_name_bytes.decode(encoding, errors='ignore').rstrip('\x00')
                
                frame = struct.unpack('<I', f.read(4))[0]
                weight = struct.unpack('<f', f.read(4))[0]
                
                keyframe = MorphKeyframe(
                    morph_name=morph_name,
                    frame=frame,
                    weight=weight
                )
                
                if morph_name not in animation.morph_keyframes:
                    animation.morph_keyframes[morph_name] = []
                animation.morph_keyframes[morph_name].append(keyframe)
                
                animation.max_frame = max(animation.max_frame, frame)
            
            try:
                camera_key_count = struct.unpack('<I', f.read(4))[0]
                
                for _ in range(camera_key_count):
                    frame = struct.unpack('<I', f.read(4))[0]
                    distance = struct.unpack('<f', f.read(4))[0]
                    position = struct.unpack('<fff', f.read(12))
                    rotation = struct.unpack('<fff', f.read(12))
                    interpolation = f.read(24)
                    fov = struct.unpack('<I', f.read(4))[0]
                    perspective = struct.unpack('<B', f.read(1))[0]
                    
                    keyframe = CameraKeyframe(
                        frame=frame,
                        distance=distance,
                        position=np.array(position, dtype=np.float32),
                        rotation=np.array(rotation, dtype=np.float32),
                        interpolation=interpolation,
                        fov=fov,
                        perspective=bool(perspective)
                    )
                    animation.camera_keyframes.append(keyframe)
                    animation.max_frame = max(animation.max_frame, frame)
            except struct.error:
                pass
            
            try:
                light_key_count = struct.unpack('<I', f.read(4))[0]
                
                for _ in range(light_key_count):
                    frame = struct.unpack('<I', f.read(4))[0]
                    color = struct.unpack('<fff', f.read(12))
                    position = struct.unpack('<fff', f.read(12))
                    
                    keyframe = LightKeyframe(
                        frame=frame,
                        color=np.array(color, dtype=np.float32),
                        position=np.array(position, dtype=np.float32)
                    )
                    animation.light_keyframes.append(keyframe)
                    animation.max_frame = max(animation.max_frame, frame)
            except struct.error:
                pass
            
            for bone_name in animation.bone_keyframes:
                animation.bone_keyframes[bone_name].sort(key=lambda k: k.frame)
            
            for morph_name in animation.morph_keyframes:
                animation.morph_keyframes[morph_name].sort(key=lambda k: k.frame)
            
            animation.camera_keyframes.sort(key=lambda k: k.frame)
            animation.light_keyframes.sort(key=lambda k: k.frame)
            
            return animation


class VmdInterpolator:
    @staticmethod
    def bezier(t: float, p0: float, p1: float, p2: float, p3: float) -> float:
        u = 1 - t
        return u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3
    
    @staticmethod
    def interpolate_bezier(t: float, interp_data: bytes, axis: int) -> float:
        if len(interp_data) < 64:
            return t
        
        idx = axis * 16
        
        ax = interp_data[idx] / 127.0
        ay = interp_data[idx + 4] / 127.0
        bx = interp_data[idx + 8] / 127.0
        by = interp_data[idx + 12] / 127.0
        
        if abs(ax - ay) < 0.001 and abs(bx - by) < 0.001:
            return t
        
        x = VmdInterpolator._solve_bezier_x(t, ax, bx)
        return VmdInterpolator.bezier(x, 0.0, ay, by, 1.0)
    
    @staticmethod
    def _solve_bezier_x(target_x: float, ax: float, bx: float, iterations: int = 16) -> float:
        low = 0.0
        high = 1.0
        
        for _ in range(iterations):
            mid = (low + high) * 0.5
            x = VmdInterpolator.bezier(mid, 0.0, ax, bx, 1.0)
            
            if x < target_x:
                low = mid
            else:
                high = mid
        
        return (low + high) * 0.5
    
    @staticmethod
    def lerp(a: float, b: float, t: float) -> float:
        return a + (b - a) * t
    
    @staticmethod
    def lerp_vec3(a: np.ndarray, b: np.ndarray, t: float) -> np.ndarray:
        return a + (b - a) * t
    
    @staticmethod
    def slerp_quat(q1: np.ndarray, q2: np.ndarray, t: float) -> np.ndarray:
        q1 = np.array(q1, dtype=np.float64)
        q2 = np.array(q2, dtype=np.float64)
        
        dot = np.dot(q1, q2)
        
        if dot < 0:
            q2 = -q2
            dot = -dot
        
        if dot > 0.9995:
            result = q1 + t * (q2 - q1)
            return result / np.linalg.norm(result)
        
        theta_0 = np.arccos(np.clip(dot, -1.0, 1.0))
        theta = theta_0 * t
        
        sin_theta = np.sin(theta)
        sin_theta_0 = np.sin(theta_0)
        
        s1 = np.cos(theta) - dot * sin_theta / sin_theta_0
        s2 = sin_theta / sin_theta_0
        
        result = s1 * q1 + s2 * q2
        return result / np.linalg.norm(result)


class VmdPlayer:
    def __init__(self, animation: VmdAnimation, fps: float = 30.0):
        self.animation = animation
        self.fps = fps
        self.current_frame = 0.0
        self.playing = False
        self.loop = True
        self.speed = 1.0
        
        self._bone_cache: Dict[str, Tuple[int, int]] = {}
        self._morph_cache: Dict[str, Tuple[int, int]] = {}
    
    def play(self):
        self.playing = True
    
    def pause(self):
        self.playing = False
    
    def stop(self):
        self.playing = False
        self.current_frame = 0.0
    
    def set_frame(self, frame: float):
        self.current_frame = max(0.0, min(frame, self.animation.max_frame))
    
    def update(self, delta_time: float):
        if not self.playing:
            return
        
        self.current_frame += delta_time * self.fps * self.speed
        
        if self.current_frame >= self.animation.max_frame:
            if self.loop:
                self.current_frame = 0.0
            else:
                self.current_frame = self.animation.max_frame
                self.playing = False
    
    def get_bone_transform(self, bone_name: str) -> Optional[Tuple[np.ndarray, np.ndarray]]:
        keyframes = self.animation.get_bone_keyframes(bone_name)
        if not keyframes:
            return None
        
        if len(keyframes) == 1:
            kf = keyframes[0]
            return kf.position.copy(), kf.rotation.copy()
        
        frame = self.current_frame
        
        left_idx = 0
        right_idx = len(keyframes) - 1
        
        for i in range(len(keyframes) - 1):
            if keyframes[i].frame <= frame <= keyframes[i + 1].frame:
                left_idx = i
                right_idx = i + 1
                break
        else:
            if frame < keyframes[0].frame:
                return keyframes[0].position.copy(), keyframes[0].rotation.copy()
            else:
                return keyframes[-1].position.copy(), keyframes[-1].rotation.copy()
        
        kf_left = keyframes[left_idx]
        kf_right = keyframes[right_idx]
        
        frame_diff = kf_right.frame - kf_left.frame
        if frame_diff == 0:
            return kf_left.position.copy(), kf_left.rotation.copy()
        
        t = (frame - kf_left.frame) / frame_diff
        
        tx = VmdInterpolator.interpolate_bezier(t, kf_left.interpolation, 0)
        ty = VmdInterpolator.interpolate_bezier(t, kf_left.interpolation, 1)
        tz = VmdInterpolator.interpolate_bezier(t, kf_left.interpolation, 2)
        tr = VmdInterpolator.interpolate_bezier(t, kf_left.interpolation, 3)
        
        position = np.array([
            VmdInterpolator.lerp(kf_left.position[0], kf_right.position[0], tx),
            VmdInterpolator.lerp(kf_left.position[1], kf_right.position[1], ty),
            VmdInterpolator.lerp(kf_left.position[2], kf_right.position[2], tz)
        ], dtype=np.float32)
        
        rotation = VmdInterpolator.slerp_quat(kf_left.rotation, kf_right.rotation, tr)
        
        return position, rotation
    
    def get_morph_weight(self, morph_name: str) -> Optional[float]:
        keyframes = self.animation.get_morph_keyframes(morph_name)
        if not keyframes:
            return None
        
        if len(keyframes) == 1:
            return keyframes[0].weight
        
        frame = self.current_frame
        
        for i in range(len(keyframes) - 1):
            if keyframes[i].frame <= frame <= keyframes[i + 1].frame:
                kf_left = keyframes[i]
                kf_right = keyframes[i + 1]
                
                frame_diff = kf_right.frame - kf_left.frame
                if frame_diff == 0:
                    return kf_left.weight
                
                t = (frame - kf_left.frame) / frame_diff
                return VmdInterpolator.lerp(kf_left.weight, kf_right.weight, t)
        
        if frame < keyframes[0].frame:
            return keyframes[0].weight
        else:
            return keyframes[-1].weight
    
    def get_active_morphs(self) -> Dict[str, float]:
        result = {}
        for morph_name in self.animation.morph_keyframes:
            weight = self.get_morph_weight(morph_name)
            if weight is not None and abs(weight) > 0.001:
                result[morph_name] = weight
        return result


class VmdMixer:
    def __init__(self, fps: float = 30.0):
        self.fps = fps
        self.players: List[VmdPlayer] = []
        self.playing = False
        self.loop = True
        self.speed = 1.0
        self._max_frame = 0.0
    
    def add_vmd(self, animation: VmdAnimation) -> VmdPlayer:
        player = VmdPlayer(animation, self.fps)
        self.players.append(player)
        self._update_max_frame()
        return player
    
    def remove_vmd(self, index: int):
        if 0 <= index < len(self.players):
            del self.players[index]
            self._update_max_frame()
    
    def clear(self):
        self.players.clear()
        self._max_frame = 0.0
    
    def _update_max_frame(self):
        self._max_frame = max((p.animation.max_frame for p in self.players), default=0.0)
    
    @property
    def max_frame(self) -> float:
        return self._max_frame
    
    @property
    def current_frame(self) -> float:
        if self.players:
            return self.players[0].current_frame
        return 0.0
    
    def play(self):
        self.playing = True
        for player in self.players:
            player.play()
    
    def pause(self):
        self.playing = False
        for player in self.players:
            player.pause()
    
    def stop(self):
        self.playing = False
        for player in self.players:
            player.stop()
    
    def set_frame(self, frame: float):
        for player in self.players:
            player.set_frame(frame)
    
    def update(self, delta_time: float):
        if not self.playing:
            return
        
        for player in self.players:
            player.loop = self.loop
            player.speed = self.speed
            player.update(delta_time)
        
        if self.loop and self.players:
            if all(p.current_frame >= p.animation.max_frame for p in self.players):
                for player in self.players:
                    player.current_frame = 0.0
    
    def get_bone_transform(self, bone_name: str) -> Optional[Tuple[np.ndarray, np.ndarray]]:
        combined_pos = np.zeros(3, dtype=np.float32)
        combined_rot = None
        has_transform = False
        
        for player in self.players:
            transform = player.get_bone_transform(bone_name)
            if transform is not None:
                pos, rot = transform
                combined_pos += pos
                if combined_rot is None:
                    combined_rot = rot.astype(np.float64)
                else:
                    combined_rot = VmdInterpolator.slerp_quat(combined_rot, rot, 0.5)
                has_transform = True
        
        if not has_transform:
            return None
        
        if combined_rot is None:
            combined_rot = np.array([0.0, 0.0, 0.0, 1.0], dtype=np.float64)
        
        return combined_pos.astype(np.float32), combined_rot.astype(np.float32)
    
    def get_morph_weight(self, morph_name: str) -> Optional[float]:
        total_weight = 0.0
        has_weight = False
        
        for player in self.players:
            weight = player.get_morph_weight(morph_name)
            if weight is not None:
                total_weight += weight
                has_weight = True
        
        if not has_weight:
            return None
        
        return min(1.0, max(0.0, total_weight))
    
    def get_active_morphs(self) -> Dict[str, float]:
        result = {}
        
        for player in self.players:
            morphs = player.get_active_morphs()
            for morph_name, weight in morphs.items():
                if morph_name in result:
                    result[morph_name] = min(1.0, result[morph_name] + weight)
                else:
                    result[morph_name] = weight
        
        return {k: v for k, v in result.items() if abs(v) > 0.001}
