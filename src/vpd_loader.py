import numpy as np


class VpdPose:
    def __init__(self, bone_name, tx, ty, tz, qx, qy, qz, qw):
        self.bone_name = bone_name
        self.position = np.array([tx, ty, tz], dtype=np.float32)
        self.quaternion = np.array([qx, qy, qz, qw], dtype=np.float32)

    def to_matrix(self):
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
    @staticmethod
    def load(file_path):
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
                int(parts[0])
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
