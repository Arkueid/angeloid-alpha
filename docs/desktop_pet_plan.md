# 桌宠开发计划

## 项目概述

基于现有 `mmd-demo` 项目，开发一个 **PMX 桌宠** 功能。桌宠是一个始终显示在桌面上的小型动画角色，具有以下特性：

- 持续运行，轻量低耗
- 支持骨骼动画（身体动作）
- 支持表情系统（面部表情）
- 可与用户交互（点击、拖拽）
- 可切换不同姿势

---

## 当前项目状态

### 已实现功能

| 模块 | 状态 | 说明 |
|------|------|------|
| PMX 模型加载 | ✅ 完成 | 使用 pymeshio 库 |
| 顶点数据渲染 | ✅ 完成 | 基础 OpenGL 渲染 |
| Toon Shading | ✅ 完成 | 卡通渲染风格 |
| Outline 描边 | ✅ 完成 | 边缘检测描边 |
| 相机控制 | ✅ 完成 | Orbit 相机 |
| 基础 Idle 动画 | ⚠️ 伪实现 | 仅模型矩阵变换，非真骨骼动画 |
| 纹理加载 | ✅ 完成 | 多纹理支持 |
| 多模型切换 | ✅ 完成 | 命令行选择模型 |

### 项目数据规模（以 Ikaros 模型为例）

| 数据类型 | 数量 |
|----------|------|
| 顶点 | 114,294 |
| 骨骼 | 367 |
| Morphs（表情） | 231 |
| 材质 | 43 |
| 纹理 | 28+ |

### 当前顶点数据格式

```
Format: 3f(position) + 3f(normal) + 2f(uv) = 8 floats = 32 bytes/vertex
总计: 114,294 × 32 bytes ≈ 3.6 MB
```

### 当前 Idle 动画（伪实现）

```python
# renderer.py 中的简单矩阵变换
breath[3, 1] = sin(idle_time * 1.5) * 0.005      # 上下呼吸
sway_angle = sin(idle_time * 0.7) * 0.01          # 身体摇摆
model = model @ breath @ sway                       # 矩阵乘法
```

**问题**：这只是整体模型的位移和旋转，不是真正的骨骼驱动，无法实现局部动作（如眨眼、抬手）。

---

## 缺失的核心功能

### 1. 骨骼蒙皮系统 (Skeletal Skinning) ❌

```
当前：顶点直接使用模型矩阵变换
目标：顶点根据骨骼权重进行线性混合
```

### 2. Morph 表情系统 ❌

```
当前：所有表情部件一次性绘制
目标：根据权重混合不同表情的顶点偏移
```

### 3. VPD 姿势加载 ❌

```
当前：无
目标：解析 VPD 文件，应用姿势到骨骼
```

### 4. 骨骼层级与矩阵计算 ❌

```
当前：无
目标：实现骨骼树，计算级联变换矩阵
```

---

## 开发计划

### 阶段一：骨骼蒙皮系统

**目标**：让模型能通过骨骼正确变形

#### 1.1 修改顶点数据格式

```python
# 当前格式
'3f 3f 2f'  # position, normal, uv

# 新格式
'3f 3f 2f 4f 4f 4i'  # position, normal, uv, boneWeights, boneIndices
# = 16 floats = 64 bytes/vertex
```

**改动文件**：`src/renderer.py`

#### 1.2 实现骨骼矩阵计算

```python
# 新建 src/bone_transform.py

class BoneTransform:
    def __init__(self, bones):
        self.bones = bones  # PMX bones
        self.local_matrices = []   # 局部变换矩阵
        self.world_matrices = []    # 世界变换矩阵

    def compute_local_matrix(self, bone):
        """计算单个骨骼的局部变换矩阵"""
        # 位置 + 旋转（四元数转矩阵）
        pass

    def compute_world_matrices(self):
        """从根骨骼开始，递归计算所有骨骼的世界矩阵"""
        # 子骨骼 = 父骨骼世界矩阵 × 子骨骼局部矩阵
        pass

    def get_skinning_matrices(self):
        """返回用于 GPU 的骨骼矩阵数组"""
        # 通常需要 inverse bind pose 矩阵
        pass
```

#### 1.3 修改顶点着色器

```glsl
# shaders/skinning.vert (新建)

#version 330

// Uniforms
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 boneMatrices[367];  // 骨骼矩阵数组

// Vertex Attributes
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_boneWeights;
layout(location = 4) in ivec4 in_boneIndices;

// Output to fragment shader
out vec3 v_position;
out vec3 v_normal;
out vec2 v_uv;

void main() {
    // 骨骼蒙皮计算
    mat4 skinMatrix =
        boneMatrices[in_boneIndices.x] * in_boneWeights.x +
        boneMatrices[in_boneIndices.y] * in_boneWeights.y +
        boneMatrices[in_boneIndices.z] * in_boneWeights.z +
        boneMatrices[in_boneIndices.w] * in_boneWeights.w;

    vec4 skinnedPosition = skinMatrix * vec4(in_position, 1.0);
    vec4 skinnedNormal = skinMatrix * vec4(in_normal, 0.0);

    gl_Position = projection * view * model * skinnedPosition;
    v_position = (model * skinnedPosition).xyz;
    v_normal = normalize((model * skinnedNormal).xyz);
    v_uv = in_uv;
}
```

#### 1.4 传递骨骼矩阵到 GPU

```python
# 使用 Uniform Buffer Object (UBO) 或 Storage Buffer Object (SSBO)
bone_buffer = ctx.buffer(data=bone_matrices.astype('f4').tobytes())
```

**数据量**：
- 367 骨骼 × 16 floats × 4 bytes = 42 KB（可接受）

#### 1.5 验证

- [ ] T-pose 渲染正确（骨骼绑定到初始位置）
- [ ] 骨骼旋转正确驱动顶点
- [ ] 多骨骼权重正确混合

---

### 阶段二：Morph 表情系统

**目标**：支持面部表情和形状变化

#### 2.1 Morph 数据结构

```python
# pymeshio 已读取，验证数据结构
class Morph:
    name: str
    vertices: list[tuple[int, tuple[float, float, float]]]  # (vertex_index, offset)
    # 例如: [(0, (0.1, -0.2, 0.0)), (5, (-0.1, 0.0, 0.0)), ...]
```

#### 2.2 Morph Shader 实现

```glsl
# shaders/morph.vert (修改)

#version 330

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 boneMatrices[367];

// Morph uniforms
uniform vec3 morphOffsets[MORPH_VERTEX_COUNT];  // Morph 顶点偏移数组
uniform int morphVertexIndices[MORPH_VERTEX_COUNT];  // 对应顶点索引
uniform float morphWeight;  // 当前 Morph 权重 0.0 ~ 1.0

// ... vertex attributes ...

void main() {
    // ... skeletal skinning ...

    // 应用 Morph 偏移
    vec3 finalPosition = skinnedPosition.xyz;
    for (int i = 0; i < MORPH_VERTEX_COUNT; i++) {
        if (gl_VertexID == morphVertexIndices[i]) {
            finalPosition += morphOffsets[i] * morphWeight;
            break;
        }
    }

    gl_Position = projection * view * model * vec4(finalPosition, 1.0);
}
```

**问题**：逐顶点查找 Morph 偏移效率低。更优方案：

#### 2.3 优化的 Morph 实现

```glsl
// 方案1: 将 Morph 偏移预插值到顶点数据中
// 在 CPU 中计算: final_pos = original_pos + offset * weight
// 然后传入 GPU

// 方案2: 多路复用骨骼蒙皮
// 使用 Instanced Rendering 或 Geometry Shader

// 方案3: 分开渲染不同 Morph 的部件
// 适合部件不重叠的情况
```

#### 2.4 Morph 管理器

```python
# 新建 src/morph_controller.py

class MorphController:
    def __init__(self, morphs):
        self.morphs = morphs
        self.current_morphs = {}  # name -> weight
        self.target_weights = {}

    def set_morph(self, morph_name, weight):
        """设置某个表情的权重 (0.0 ~ 1.0)"""
        self.target_weights[morph_name] = weight

    def update(self, delta_time):
        """平滑过渡到目标权重"""
        for name, target in self.target_weights.items():
            current = self.current_morphs.get(name, 0.0)
            # 插值: lerp
            self.current_morphs[name] = lerp(current, target, speed * delta_time)

    def get_morph_data(self):
        """返回当前 Morph 混合后的顶点偏移数组"""
        pass
```

#### 2.5 验证

- [ ] 单个表情正确显示
- [ ] 多表情混合正确
- [ ] 表情切换平滑过渡

---

### 阶段三：VPD 姿势加载与应用

**目标**：加载并应用 VPD 姿势文件

#### 3.1 VPD 文件解析

```python
# 新建 src/vpd_parser.py

import re

class VpdPose:
    def __init__(self):
        self.bones = {}  # {bone_name: {'translation': (x,y,z), 'quaternion': (x,y,z,w)}}

    @staticmethod
    def from_file(filepath, encoding='cp932'):
        """解析 VPD 文件"""
        pose = VpdPose()
        with open(filepath, 'r', encoding=encoding, errors='ignore') as f:
            content = f.read()

        # VPD 格式: Bone数字{名称\n  tx,ty,tz;\n  qx,qy,qz,qw;\n}
        bone_blocks = re.findall(r'Bone(\d+)\{([^\}]+)\}', content)

        for num, block in bone_blocks:
            lines = block.split('\n')
            bone_name = lines[0].strip()
            numbers = re.findall(r'([-\d.,]+)\s*;', block)
            if len(numbers) >= 2:
                trans = tuple(map(float, numbers[0].split(',')))
                quat = tuple(map(float, numbers[1].split(',')))
                pose.bones[bone_name] = {
                    'translation': trans,
                    'quaternion': quat
                }
        return pose
```

#### 3.2 姿势应用到骨骼

```python
class PoseApplier:
    def __init__(self, bone_transform):
        self.bone_transform = bone_transform

    def apply_pose(self, pose):
        """将 VPD 姿势应用到骨骼"""
        for bone_name, pose_data in pose.bones.items():
            bone_idx = self.find_bone_index(bone_name)
            if bone_idx is not None:
                # 设置骨骼的局部变换
                self.bone_transform.set_local_transform(
                    bone_idx,
                    translation=pose_data['translation'],
                    rotation=pose_data['quaternion']
                )

    def blend_poses(self, pose1, pose2, weight):
        """在两个姿势之间插值"""
        pass
```

#### 3.3 四元数运算

```python
def quaternion_multiply(q1, q2):
    """四元数乘法: q1 * q2"""
    w1, x1, y1, z1 = q1
    w2, x2, y2, z2 = q2
    return (
        w1*w2 - x1*x2 - y1*y2 - z1*z2,
        w1*x2 + x1*w2 + y1*z2 - z1*y2,
        w1*y2 - x1*z2 + y1*w2 + z1*x2,
        w1*z2 + x1*y2 - y1*x2 + z1*w2
    )

def quaternion_slerp(q1, q2, t):
    """四元数球面线性插值"""
    # ...
```

#### 3.4 验证

- [ ] VPD 文件解析正确
- [ ] 姿势正确应用到模型
- [ ] 姿势切换平滑过渡

---

### 阶段四：桌宠窗口与交互

**目标**：实现桌面小部件功能

#### 4.1 窗口设置

```python
# 修改 Renderer 初始化

# 窗口属性
glfw.window_hint(glfw.DECORATED, False)      # 无边框
glfw.window_hint(glfw.TRANSPARENT_FRAMEBUFFER, True)  # 透明背景
glfw.window_hint(glfw.ALWAYS_ON_TOP, True)    # 始终在最前
glfw.window_hint(glfw.SAMPLES, 4)             # 多重采样抗锯齿

# 可选：设定窗口大小和位置
self.window = glfw.create_window(400, 600, title, None, None)
# 移动到屏幕右下角
glfw.set_window_pos(self.window, screen_width - 420, screen_height - 620)
```

#### 4.2 透明背景渲染

```python
# 渲染循环中
self.ctx.clear(0, 0, 0, 0)  # RGBA 全为 0，透明

# 启用混合
self.ctx.enable(moderngl.BLEND)
self.ctx.blend_func = moderngl.SRC_ALPHA, moderngl.ONE_MINUS_SRC_ALPHA
```

#### 4.3 鼠标交互

```python
# 拖拽移动窗口
def _on_mouse_button(self, window, button, action, mods):
    if button == glfw.MOUSE_BUTTON_LEFT:
        if action == glfw.PRESS:
            # 记录点击位置
            self.dragging = True
            self.drag_start = glfw.get_cursor_pos(window)
        elif action == glfw.RELEASE:
            self.dragging = False

def _on_cursor_pos(self, window, xpos, ypos):
    if self.dragging:
        # 计算移动量
        dx = xpos - self.drag_start[0]
        dy = ypos - self.drag_start[1]
        # 移动窗口
        x, y = glfw.get_window_pos(window)
        glfw.set_window_pos(window, x + dx, y - dy)  # 注意 Y 轴方向
        self.drag_start = (xpos, ypos)
```

#### 4.4 点击检测（可选）

```python
# 点击模型部件检测
def _on_click(self, window, xpos, ypos):
    # OpenGL 拾取 / Ray casting
    # 或者：使用 2D 碰撞检测（简化版）
    pass
```

#### 4.5 验证

- [ ] 窗口无边框透明
- [ ] 始终在最前
- [ ] 可拖拽移动
- [ ] 点击有响应

---

### 阶段五：桌宠行为与动画

**目标**：实现自然的小宠物行为

#### 5.1 行为状态机

```python
# 新建 src/pet_behavior.py

class PetState(Enum):
    IDLE = auto()        # 站立发呆
    BLINKING = auto()    # 眨眼
    LOOKING = auto()     # 看向鼠标
    WAVE = auto()        # 挥手
    HAPPY = auto()       # 开心
    SLEEPING = auto()    # 睡觉

class PetBehavior:
    def __init__(self):
        self.state = PetState.IDLE
        self.state_timer = 0
        self.transitions = {
            PetState.IDLE: [PetState.BLINKING, PetState.LOOKING],
            # ...
        }

    def update(self, delta_time, mouse_pos):
        self.state_timer += delta_time

        # 状态切换逻辑
        if self.should_transition():
            self.transition_to_random()

        # 执行当前状态的动作
        self.execute_state(mouse_pos)

    def execute_state(self, mouse_pos):
        if self.state == PetState.IDLE:
            # 播放 idle 骨骼动画
            pass
        elif self.state == PetState.LOOKING:
            # 头部跟随鼠标
            self.look_at(mouse_pos)
        # ...
```

#### 5.2 眨眼动画

```python
# 使用 Morph 系统
self.morph_controller.set_morph("eye_face.f00_winkl_op", blink_weight)
self.morph_controller.set_morph("eye_face.f00_winkr_op", blink_weight)
```

#### 5.3 头部跟随

```python
# 修改头部骨骼旋转
head_bone_idx = self.find_bone("头")
# 计算朝向鼠标的方向
# 应用四元数旋转到头部骨骼
```

#### 5.4 随机行为

```python
import random

def maybe_trigger_action(self):
    if random.random() < 0.001:  # 0.1% 概率
        self.trigger_action(random.choice([
            Action.WAVE,
            Action.JUMP,
            Action.BOW,
            # ...
        ]))
```

#### 5.5 验证

- [ ] Idle 动画流畅自然
- [ ] 眨眼频率合理
- [ ] 鼠标跟随头部
- [ ] 随机行为触发

---

### 阶段六：性能优化

**目标**：确保桌宠低资源占用

#### 6.1 帧率控制

```python
# 限制为 30fps 足够流畅且省电
target_fps = 30
frame_time = 1.0 / target_fps

while not glfw.window_should_close(self.window):
    # ...
    if delta_time < frame_time:
        time.sleep(frame_time - delta_time)
```

#### 6.2 条件渲染

```python
# 窗口最小化时不渲染
if glfw.get_window_attrib(window, glfw.ICONIFIED):
    glfw.wait_events()  # 等待事件，不做渲染
    continue
```

#### 6.3 降功耗模式

```python
# 检测用户空闲，自动降低帧率
if user_idle_time > 60:  # 1分钟无操作
    target_fps = 10  # 降低到 10fps
```

#### 6.4 验证

- [ ] CPU 占用 < 5%（平均）
- [ ] 内存占用 < 200MB
- [ ] 帧率稳定

---

## 文件结构

```
mmd-demo/
├── src/
│   ├── __init__.py
│   ├── renderer.py          # 主渲染器（修改）
│   ├── camera.py            # 相机控制
│   ├── load_pmx.py          # PMX 加载
│   ├── bone_transform.py    # 新增：骨骼变换计算
│   ├── morph_controller.py  # 新增：表情控制器
│   ├── vpd_parser.py        # 新增：VPD 姿势解析
│   ├── pose_applier.py      # 新增：姿势应用器
│   └── pet_behavior.py      # 新增：桌宠行为
├── resources/
│   ├── shaders/
│   │   ├── main.vert        # 修改：支持骨骼蒙皮
│   │   ├── main.frag
│   │   ├── skinning.vert    # 新增：骨骼蒙皮着色器
│   │   └── ...
│   ├── vpd/                 # 姿势文件
│   └── models/              # PMX 模型
├── docs/
│   └── desktop_pet_plan.md  # 本文档
└── run.py
```

---

## 实施顺序

```
第一阶段 (骨骼蒙皮)
    │
    ├─ 1.1 修改顶点格式
    ├─ 1.2 骨骼矩阵计算
    ├─ 1.3 Shader 实现
    ├─ 1.4 数据传输
    └─ 1.5 验证 T-pose

第二阶段 (Morph 表情)
    │
    ├─ 2.1 数据结构确认
    ├─ 2.2 Morph Shader
    ├─ 2.3 Morph 管理器
    └─ 2.4 验证

第三阶段 (VPD 姿势)
    │
    ├─ 3.1 VPD 解析
    ├─ 3.2 姿势应用
    ├─ 3.3 平滑过渡
    └─ 3.4 验证

第四阶段 (窗口交互)
    │
    ├─ 4.1 窗口设置
    ├─ 4.2 透明渲染
    ├─ 4.3 鼠标交互
    └─ 4.4 验证

第五阶段 (行为动画)
    │
    ├─ 5.1 状态机
    ├─ 5.2 眨眼/跟随
    ├─ 5.3 随机行为
    └─ 5.4 验证

第六阶段 (性能优化)
    │
    ├─ 6.1 帧率控制
    ├─ 6.2 条件渲染
    ├─ 6.3 降功耗
    └─ 6.4 验证
```

---

## 技术要点

### 骨骼蒙皮矩阵计算

```python
# 骨骼世界矩阵 = 父骨骼世界矩阵 × 骨骼局部矩阵
def compute_world_matrix(bone, parent_matrix):
    local = compute_local_matrix(bone)
    return parent_matrix @ local
```

### 四元数到旋转矩阵

```python
import numpy as np

def quaternion_to_matrix(q):
    w, x, y, z = q
    return np.array([
        [1-2*y*y-2*z*z,    2*x*y-2*w*z,      2*x*z+2*w*y,      0],
        [2*x*y+2*w*z,      1-2*x*x-2*z*z,    2*y*z-2*w*x,      0],
        [2*x*z-2*w*y,      2*y*z+2*w*x,      1-2*x*x-2*y*y,    0],
        [0,                0,                0,                1]
    ], dtype='f4')
```

### Morph 顶点偏移预计算

```python
# CPU 端预计算 Morph 混合结果
def compute_morphed_vertices(original_vertices, morphs, weights):
    result = original_vertices.copy()
    for morph_name, weight in weights.items():
        for vertex_idx, offset in morphs[morph_name].vertices:
            result[vertex_idx] += offset * weight
    return result
```

---

## 注意事项

1. **数据兼容性**：不同模型的骨骼数量不同（Ikaros: 367，其他模型可能不同）
2. **骨骼名称**：VPD 使用日文名，PMX 骨骼也使用日文名，需要准确匹配
3. **坐标系**：MMD 使用 Y-up 右手坐标系，需确认与 OpenGL 一致
4. **权重归一化**：确保所有骨骼权重和为 1.0
5. **Shader 版本**：确保 GPU 支持 OpenGL 3.3+ (GLSL 330)

---

## 参考资料

- [PMX 格式规范](http://www.geocities.jp/higuchu4/index.html)
- [MMD 骨骼蒙皮原理](https://en.wikipedia.org/wiki/Skeletal_skinning)
- [GLSL 骨骼蒙皮教程](https://ogldev.org/www/tutorial38/tutorial38.html)
- [四元数与旋转](https://www.3dgep.com/understanding-quaternions/)
