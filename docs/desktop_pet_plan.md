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
| 纹理加载 | ✅ 完成 | 多纹理支持 |
| 多模型切换 | ✅ 完成 | 命令行选择模型 |
| **骨骼蒙皮系统** | ✅ 完成 | GPU 纹理驱动蒙皮 |
| **VPD 姿势加载** | ✅ 完成 | 支持姿势切换 |
| **坐标系转换** | ✅ 完成 | MMD 左手 → OpenGL 右手 |

### 项目数据规模（以 Ikaros 模型为例）

| 数据类型 | 数量 |
|----------|------|
| 顶点 | 114,294 |
| 骨骼 | 367 |
| Morphs（表情） | 231 |
| 材质 | 43 |
| 纹理 | 28+ |
| 刚体 | 255 |
| 关节 | 377 |

### 当前顶点数据格式

```
蒙皮格式: 3f(position) + 3f(normal) + 2f(uv) + 4f(bone_weights) + 4i(bone_indices)
= 20 floats = 80 bytes/vertex
```

### 快捷键

| 按键 | 功能 |
|------|------|
| K | 切换骨骼蒙皮渲染 |
| P | 切换 VPD 姿势 |
| T | 切换 Toon 着色 |

---

## 缺失的核心功能

### 1. Morph 表情系统 ❌

```
当前：所有表情部件一次性绘制
目标：根据权重混合不同表情的顶点偏移
用途：眨眼、表情变化、翅膀切换等
```

### 2. VMD 动画加载 ❌

```
当前：仅支持静态姿势 (VPD)
目标：解析 VMD 文件，播放关键帧动画
难点：贝塞尔曲线插值、多骨骼同步
```

### 3. 物理效果 ❌

```
当前：无物理模拟
目标：头发、衣服等物理飘动
数据：255 刚体 + 377 关节
```

### 4. 桌宠窗口与交互 ❌

```
当前：普通窗口
目标：无边框透明窗口、拖拽移动
```

### 5. 桌宠行为系统 ❌

```
当前：无行为逻辑
目标：状态机、眨眼、头部跟随鼠标等
```

---

## 开发计划

### ~~阶段一：骨骼蒙皮系统~~ ✅ 已完成

**目标**：让模型能通过骨骼正确变形

#### 已实现功能

- [x] 顶点数据格式修改（添加骨骼权重和索引）
- [x] 骨骼矩阵计算（逆绑定矩阵、世界矩阵）
- [x] GPU 纹理驱动蒙皮（Texture-based GPU Skinning）
- [x] 骨骼蒙皮着色器 (`skinned.vert`)
- [x] 蒙皮描边着色器 (`outline_skinned.vert`)
- [x] 坐标系转换（MMD 左手 → OpenGL 右手）

#### 技术实现

```python
# 骨骼矩阵存储到纹理
bone_texture = ctx.texture((bone_texture_width, texture_height), 4, dtype='f4')
bone_texture.write(matrices.astype('f4').tobytes())

# Shader 中采样骨骼矩阵
mat4 fetch_bone_matrix(int bone_index) {
    int texels_per_matrix = 4;
    int global_texel_idx = bone_index * texels_per_matrix;
    int row = global_texel_idx / tex_width;
    int col_start = global_texel_idx % tex_width;
    
    vec4 col0 = texelFetch(bone_texture, ivec2(col_start, row), 0);
    vec4 col1 = texelFetch(bone_texture, ivec2(col_start + 1, row), 0);
    vec4 col2 = texelFetch(bone_texture, ivec2(col_start + 2, row), 0);
    vec4 col3 = texelFetch(bone_texture, ivec2(col_start + 3, row), 0);
    
    return mat4(col0, col1, col2, col3);
}
```

---

### ~~阶段三：VPD 姿势加载与应用~~ ✅ 已完成

**目标**：加载并应用 VPD 姿势文件

#### 已实现功能

- [x] VPD 文件解析
- [x] 四元数到矩阵转换
- [x] 姿势应用到骨骼
- [x] 骨骼层级变换计算

#### 技术实现

```python
class VpdLoader:
    @staticmethod
    def load(filepath, encoding='cp932'):
        poses = {}
        with open(filepath, 'r', encoding=encoding, errors='ignore') as f:
            content = f.read()
        
        bone_blocks = re.findall(r'Bone(\d+)\{([^\}]+)\}', content)
        for num, block in bone_blocks:
            lines = block.split('\n')
            bone_name = lines[0].strip()
            numbers = re.findall(r'([-\d.,]+)\s*;', block)
            if len(numbers) >= 2:
                trans = tuple(map(float, numbers[0].split(',')))
                quat = tuple(map(float, numbers[1].split(',')))
                poses[bone_name] = VpdPose(bone_name, trans, quat)
        return poses
```

---

### 阶段二：Morph 表情系统

**目标**：支持面部表情和形状变化

#### 2.1 Morph 类型

| 类型 | 名称 | 用途 | Ikaros 示例 |
|------|------|------|-------------|
| 0 | Group Morph | 组合多个 Morph | まばたき、change wings |
| 1 | Vertex Morph | 顶点变形 | 眼睛、嘴巴表情 |
| 3 | UV Morph | UV 变形 | 眼睛方向 |
| 8 | Material Morph | 材质变形 | 翅膀透明度切换 |

#### 2.2 实现方案

```python
class MorphController:
    def __init__(self, morphs):
        self.morphs = {m.name: m for m in morphs}
        self.weights = {}  # morph_name -> weight (0.0 ~ 1.0)
    
    def set_weight(self, morph_name, weight):
        self.weights[morph_name] = weight
    
    def apply_vertex_morphs(self, vertices):
        result = vertices.copy()
        for name, weight in self.weights.items():
            morph = self.morphs.get(name)
            if morph and morph.morph_type == 1:
                for offset in morph.offsets:
                    result[offset.vertex_index] += offset.position_offset * weight
        return result
```

#### 2.3 验证

- [ ] 单个表情正确显示
- [ ] 多表情混合正确
- [ ] 表情切换平滑过渡
- [ ] 翅膀切换功能

---

### 阶段四：VMD 动画加载

**目标**：加载并播放 VMD 动画文件

#### 4.1 VMD 文件结构

```
VMD 格式:
├─ 文件头 (签名 + 模型名)
├─ 骨骼动画关键帧
│   ├─ 骨骼名
│   ├─ 帧号
│   ├─ 位置
│   ├─ 旋转 (四元数)
│   └─ 插值曲线 (贝塞尔控制点)
├─ 表情动画关键帧
├─ 相机动画
└─ 灯光动画
```

#### 4.2 主要难点

| 难点 | 说明 |
|------|------|
| 贝塞尔插值 | 每个关键帧有贝塞尔曲线控制点 |
| 关键帧查找 | 给定时间，找到前后关键帧 |
| 多骨骼同步 | 所有骨骼动画同步播放 |

#### 4.3 验证

- [ ] VMD 文件解析正确
- [ ] 动画播放流畅
- [ ] 循环播放正常

---

### 阶段五：物理效果

**目标**：实现头发、衣服等物理飘动

#### 5.1 物理数据

```
Ikaros 模型:
- 刚体数量: 255
- 关节数量: 377
```

#### 5.2 实现方案

**方案 A：完整物理引擎**
- 集成 PyBullet 或 Bullet
- 工作量：约 30%

**方案 B：简化物理**
- 仅对特定骨骼做弹簧阻尼模型
- 工作量：约 5%

```python
class SimplePhysics:
    def __init__(self, bone, stiffness=100, damping=10):
        self.bone = bone
        self.stiffness = stiffness
        self.damping = damping
        self.velocity = 0
    
    def update(self, target_rotation, dt):
        current = self.bone.rotation
        force = (target_rotation - current) * self.stiffness
        self.velocity += force * dt
        self.velocity *= (1 - self.damping * dt)
        self.bone.rotation += self.velocity * dt
```

#### 5.3 验证

- [ ] 头发飘动自然
- [ ] 衣服物理正常
- [ ] 性能可接受

---

### 阶段六：桌宠窗口与交互

**目标**：实现桌面小部件功能

#### 6.1 窗口设置

```python
glfw.window_hint(glfw.DECORATED, False)
glfw.window_hint(glfw.TRANSPARENT_FRAMEBUFFER, True)
glfw.window_hint(glfw.ALWAYS_ON_TOP, True)
```

#### 6.2 验证

- [ ] 窗口无边框透明
- [ ] 始终在最前
- [ ] 可拖拽移动

---

### 阶段七：桌宠行为与动画

**目标**：实现自然的小宠物行为

#### 7.1 行为状态机

```python
class PetState(Enum):
    IDLE = auto()
    BLINKING = auto()
    LOOKING = auto()
    WAVE = auto()
    HAPPY = auto()
    SLEEPING = auto()
```

#### 7.2 验证

- [ ] Idle 动画流畅
- [ ] 眨眼频率合理
- [ ] 鼠标跟随头部

---

### 阶段八：性能优化

**目标**：确保桌宠低资源占用

#### 8.1 帧率控制

```python
target_fps = 30
if delta_time < frame_time:
    time.sleep(frame_time - delta_time)
```

#### 8.2 验证

- [ ] CPU 占用 < 5%
- [ ] 内存占用 < 200MB

---

## 文件结构

```
mmd-demo/
├── src/
│   ├── __init__.py
│   ├── renderer.py          # 主渲染器
│   ├── camera.py            # 相机控制
│   ├── load_pmx.py          # PMX 加载 + 骨骼矩阵 + VPD
│   ├── morph_controller.py  # TODO: 表情控制器
│   ├── vmd_loader.py        # TODO: VMD 动画加载
│   ├── physics.py           # TODO: 物理效果
│   └── pet_behavior.py      # TODO: 桌宠行为
├── resources/
│   ├── shaders/
│   │   ├── main.vert
│   │   ├── main.frag
│   │   ├── toon.vert
│   │   ├── toon.frag
│   │   ├── skinned.vert         # ✅ 骨骼蒙皮着色器
│   │   ├── outline.vert
│   │   ├── outline.frag
│   │   └── outline_skinned.vert # ✅ 蒙皮描边着色器
│   ├── vpd/                     # 姿势文件
│   └── models/                  # PMX 模型
├── docs/
│   └── desktop_pet_plan.md
└── run.py
```

---

## 实施顺序

```
✅ 第一阶段 (骨骼蒙皮) - 已完成
    │
    ├─ ✅ 修改顶点格式
    ├─ ✅ 骨骼矩阵计算
    ├─ ✅ Shader 实现
    ├─ ✅ 纹理驱动数据传输
    └─ ✅ 坐标系转换

✅ 第三阶段 (VPD 姿势) - 已完成
    │
    ├─ ✅ VPD 解析
    ├─ ✅ 姿势应用
    └─ ✅ 骨骼层级变换

⬜ 第二阶段 (Morph 表情)
    │
    ├─ ⬜ 数据结构确认
    ├─ ⬜ Vertex Morph 实现
    ├─ ⬜ Material Morph 实现
    └─ ⬜ 验证

⬜ 第四阶段 (VMD 动画)
    │
    ├─ ⬜ VMD 解析
    ├─ ⬜ 贝塞尔插值
    └─ ⬜ 动画播放

⬜ 第五阶段 (物理效果) - 可选
    │
    ├─ ⬜ 简化物理实现
    └─ ⬜ 验证

⬜ 第六阶段 (窗口交互)
    │
    ├─ ⬜ 窗口设置
    ├─ ⬜ 透明渲染
    └─ ⬜ 鼠标交互

⬜ 第七阶段 (行为动画)
    │
    ├─ ⬜ 状态机
    ├─ ⬜ 眨眼/跟随
    └─ ⬜ 随机行为

⬜ 第八阶段 (性能优化)
    │
    ├─ ⬜ 帧率控制
    ├─ ⬜ 条件渲染
    └─ ⬜ 降功耗
```

---

## 进度统计

| 阶段 | 功能 | 状态 | 工作量 |
|------|------|------|--------|
| 阶段一 | 骨骼蒙皮系统 | ✅ 完成 | 25% |
| 阶段三 | VPD 姿势加载 | ✅ 完成 | 15% |
| 阶段二 | Morph 表情系统 | ❌ 待开发 | 15% |
| 阶段四 | VMD 动画加载 | ❌ 待开发 | 18% |
| 阶段五 | 物理效果 | ❌ 待开发 | 5~30% |
| 阶段六 | 桌宠窗口 | ❌ 待开发 | 15% |
| 阶段七 | 行为系统 | ❌ 待开发 | 20% |
| 阶段八 | 性能优化 | ❌ 待开发 | 10% |

**总体进度：约 40% 完成**

---

## 技术要点

### 骨骼蒙皮矩阵计算

```python
# 绑定姿势世界矩阵
bind_world[i] = parent_world @ local_matrix

# 蒙皮矩阵 = 姿势世界矩阵 × 逆绑定矩阵
skin_matrix[i] = pose_world[i] @ inv(bind_world[i])
```

### 坐标系转换

```python
# MMD 左手坐标系 → OpenGL 右手坐标系
coord_convert = np.diag([-1, 1, 1, 1]).astype('f4')
model = coord_convert @ model

# 三角形缠绕顺序调整
ctx.front_face = 'cw'
```

### 四元数到旋转矩阵

```python
def quaternion_to_matrix(q):
    w, x, y, z = q
    return np.array([
        [1-2*y*y-2*z*z,    2*x*y-2*w*z,      2*x*z+2*w*y,      0],
        [2*x*y+2*w*z,      1-2*x*x-2*z*z,    2*y*z-2*w*x,      0],
        [2*x*z-2*w*y,      2*y*z+2*w*x,      1-2*x*x-2*y*y,    0],
        [0,                0,                0,                1]
    ], dtype='f4')
```

---

## 注意事项

1. **数据兼容性**：不同模型的骨骼数量不同（Ikaros: 367）
2. **骨骼名称**：VPD 使用日文名，PMX 骨骼也使用日文名，需要准确匹配
3. **坐标系**：MMD 使用左手坐标系，OpenGL 使用右手坐标系，需要转换
4. **权重归一化**：确保所有骨骼权重和为 1.0
5. **Shader 版本**：确保 GPU 支持 OpenGL 3.3+ (GLSL 330)

---

## 参考资料

- [PMX 格式规范](http://www.geocities.jp/higuchu4/index.html)
- [MMD 骨骼蒙皮原理](https://en.wikipedia.org/wiki/Skeletal_skinning)
- [GLSL 骨骼蒙皮教程](https://ogldev.org/www/tutorial38/tutorial38.html)
- [四元数与旋转](https://www.3dgep.com/understanding-quaternions/)
