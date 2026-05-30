# PMX格式规格说明

## PMX规格

PMD编辑器0.1.0.1之后及 PMX编辑器 所对应的 PMX格式(2.0以上)的数据格式及动作规格。

## 使用条件

关于规格与 PMX编辑器等通用。

管理由我方统一进行。擅自修改不予处理(我方不承担任何责任)
※格式公开也由我方管理。

各应用程序等的格式使用,如为非营利可自行承担责任范围内自由使用。

## 版本管理与兼容性

PMX的兼容性由ver严格管理。
非对应ver的情况下请立即中断处理。

另外,PMX1.0为非公开(开头密钥和头部结构也有些不同,请注意不要错误读取)

当前最新ver为 2.1 。

## 数据格式

格式 : 仅二进制
字节序 : 小端序
文本编码 : Unicode(UTF16LE) 及 UTF8 的选择方式

### 基本数据类型和字节大小

byte : 1 - 无符号
sbyte : 1 - 有符号

ushort : 2 - 无符号
short : 2 - 有符号

uint : 4 - 无符号
int : 4 - 有符号(32bit固定)

float : 4 - 单精度实数

float2 : 8 (4 * 2) | Vector2 - X,Y(or U,V) 顺序
float3 : 12 (4 * 3) | Vector3 - X,Y,Z(or R,G,B) 顺序
float4 : 16 (4 * 4) | Vector4 - X,Y,Z,W(or R,G,B,A) 顺序

TextBuf : 可变 | 4 + 缓冲区长度

bitFlag : 1 | 1byte对应8个标志 bit-0:OFF 1:ON

### TextBuf (文本缓冲区)

int : 字节长度
byte * 字节长度 : byte序列 编码格式见PMX头

※缓冲区大小为 4 + n [byte]

## 索引引用字节大小

各数据的索引引用值根据数据总数优化为1,2,4字节。

对应:
- 顶点
- 纹理(内部表管理)
- 材质
- 骨骼
- Morph
- 刚体

各数据大小见PMX头。

### 顶点 - 无符号

字节大小 : 顶点数范围
1 : 0 ~ 255
2 : 256 ~ 65535
4 : 65536 ~ 2147483647 ※注意是有符号

### 骨骼/纹理/材质/Morph/刚体 - 有符号

字节大小 : 数据数范围
1 : 0 ~ 127
2 : 128 ~ 32767
4 : 32768 ~ 2147483647

※骨骼和刚体等以-1作为非引用值。材质由材质Morph将-1视为"全部材质"。

## PMX数据结构概要 - ver2.0用(2.1在下面追加)

-开头-

PMX头
-
模型信息
-
顶点数 [int]
顶点 * 顶点数
-
面数 [int]
引用顶点索引 * 面数
-
纹理数 [int]
纹理路径 * 纹理数
-
材质数 [int]
材质 * 材质数
-
骨骼数 [int]
骨骼 * 骨骼数
-
Morph数 [int]
Morph * Morph数
-
显示框数 [int]
显示框 * 显示框数
-
刚体数 [int]
刚体 * 刚体数
-
Joint数 [int]
Joint * Joint数

-结束-

## PMX头

4 : byte[4] | "PMX " | ASCII为 0x50,0x4d,0x58,0x20 ※注意末尾空格(PMX1.0是"Pmx ")
4 : float | ver (2.0/2.1)

以上为幻数

1 : byte | 后续数据列的字节大小 PMX2.0固定为 8
n : byte[8] | byte * 字节大小

字节列:
- [0] - 编码方式 | 0:UTF16 1:UTF8
- [1] - 追加UV数 | 0~4
- [2] - 顶点索引大小 | 1,2,4 之一
- [3] - 纹理索引大小 | 1,2,4 之一
- [4] - 材质索引大小 | 1,2,4 之一
- [5] - 骨骼索引大小 | 1,2,4 之一
- [6] - Morph索引大小 | 1,2,4 之一
- [7] - 刚体索引大小 | 1,2,4 之一

## 模型信息

- 模型名
- 模型名英
- 注释
- 注释英

## 顶点

### 追加UV

PMX顶点可存储最多4个追加UV(内容为4D向量)。追加数在头中指定。
追加UV数对文件大小影响很大,如不需要最好不要添加。

### 权重

1顶点最多存储4骨骼的权重信息。
存储方法根据对应骨骼数及SDEF相关分为:

- BDEF1 : 仅骨骼
- BDEF2 : 2骨骼+权重(PMD方式)
- BDEF4 : 4骨骼+各自权重
- SDEF : BDEF2+3个float3向量

顶点数据:
- 12 : float3 | 位置(x,y,z)
- 12 : float3 | 法线(x,y,z)
- 8 : float2 | UV(u,v)
- 16 * n : float4[n] | 追加UV(x,y,z,w)
- 1 : byte | 权重变形方式 0:BDEF1 1:BDEF2 2:BDEF4 3:SDEF
- 4 : float | 边缘倍率

## 面

n : 顶点索引大小 | 顶点引用索引

※3点(3顶点索引)为1面
各材质的面数由材质的面(顶点)数管理(同PMD方式)

## 纹理

材质内引用的纹理路径表

4 + n : TextBuf | 纹理路径

※材质通过索引引用。纹理/球体/个别Toon统一使用
※共享Toon -> toon01.bmp~toon10.bmp 不放入纹理表

## 材质

- 材质名
- 材质名英
- 16 : float4 | Diffuse (R,G,B,A)
- 12 : float3 | Specular (R,G,B)
- 4 : float | Specular系数
- 12 : float3 | Ambient (R,G,B)
- 1 : bitFlag | 描画标志(8bit)
  - 0x01:双面描画
  - 0x02:地面阴影
  - 0x04:自投影映射描画
  - 0x08:自阴影描画
  - 0x10:边缘描画
- 16 : float4 | 边缘色 (R,G,B,A)
- 4 : float | 边缘大小
- n : 纹理索引大小 | 通常纹理
- n : 纹理索引大小 | 球体纹理
- 1 : byte | 球体模式 0:无效 1:乘法(sph) 2:加法(spa) 3:子纹理(追加UV1的x,y作UV引用进行通常纹理描画)
- 1 : byte | 共享Toon标志 0:个别Toon 1:共享Toon

共享Toon标志:0 时
- n : 纹理索引大小 | Toon纹理

共享Toon标志:1 时
- 1 : byte | 共享Toon纹理[0~9] -> 分别对应 toon01.bmp~toon10.bmp

- 4 + n : TextBuf | 备注
- 4 : int | 材质对应的面(顶点)数(必须为3的倍数)

## 骨骼

- 骨骼名
- 骨骼名英
- 12 : float3 | 位置
- n : 骨骼索引大小 | 父骨骼索引
- 4 : int | 变形层级

### 骨骼标志(16bit)

- 0x0001 : 连接目标 -> 0:坐标偏移指定 1:骨骼指定
- 0x0002 : 可旋转
- 0x0004 : 可移动
- 0x0008 : 显示
- 0x0010 : 可操作
- 0x0020 : IK
- 0x0080 : 本地赋予
- 0x0100 : 旋转赋予
- 0x0200 : 移动赋予
- 0x0400 : 轴固定
- 0x0800 : 本地轴
- 0x1000 : 物理后变形
- 0x2000 : 外部亲变形

## Morph

### Morph种类

- 顶点Morph
- UVMorph
- 骨骼Morph
- 材质Morph
- 组Morph

UVMorph进一步分类为UV/追加UV1~4共5种。

### 顶点Morph

n : 顶点索引大小 | 顶点索引
12 : float3 | 坐标偏移量(x,y,z)

### UVMorph

n : 顶点索引大小 | 顶点索引
16 : float4 | UV偏移量(x,y,z,w)

### 骨骼Morph

n : 骨骼索引大小 | 骨骼索引
12 : float3 | 移动量(x,y,z)
16 : float4 | 旋转量-四元数(x,y,z,w)

### 材质Morph

n : 材质索引大小 | 材质索引 -> -1:全部材质
1 : 偏移运算形式 | 0:乘法, 1:加法
16 : float4 | Diffuse (R,G,B,A)
12 : float3 | Specular (R,G,B)
4 : float | Specular系数
12 : float3 | Ambient (R,G,B)
16 : float4 | 边缘色 (R,G,B,A)
4 : float | 边缘大小
16 : float4 | 纹理系数 (R,G,B,A)
16 : float4 | 球体纹理系数 (R,G,B,A)
16 : float4 | Toon纹理系数 (R,G,B,A)

### 组Morph

n : Morph索引大小 | Morph索引
4 : float | Morph率

## 显示框

骨骼/Morph统一存储

- 框名
- 框名英
- 1 : byte | 特殊框标志 - 0:通常框 1:特殊框
- 4 : int | 框内元素数

框内元素:
- 1 : byte | 元素对象 0:骨骼 1:Morph

## 刚体

- 刚体名
- 刚体名英
- n : 骨骼索引大小 | 关联骨骼索引 - 无关联为-1
- 1 : byte | 组
- 2 : ushort | 非碰撞组标志
- 1 : byte | 形状 - 0:球 1:箱 2:胶囊
- 12 : float3 | 大小(x,y,z)
- 12 : float3 | 位置(x,y,z)
- 12 : float3 | 旋转(x,y,z) -> 弧度角
- 4 : float | 质量
- 4 : float | 移动衰减
- 4 : float | 旋转衰减
- 4 : float | 反发力
- 4 : float | 摩擦力
- 1 : byte | 刚体物理运算 - 0:骨骼追随(static) 1:物理运算(dynamic) 2:物理运算+Bone位置对应

## Joint

- Joint名
- Joint名英
- 1 : byte | Joint种类 - 0:弹簧6DOF | PMX2.0仅0(扩展用)

Joint种类:0 时

- n : 刚体索引大小 | 关联刚体A索引 - 无关联为-1
- n : 刚体索引大小 | 关联刚体B索引 - 无关联为-1
- 12 : float3 | 位置(x,y,z)
- 12 : float3 | 旋转(x,y,z) -> 弧度角
- 12 : float3 | 移动限制-下限(x,y,z)
- 12 : float3 | 移动限制-上限(x,y,z)
- 12 : float3 | 旋转限制-下限(x,y,z) -> 弧度角
- 12 : float3 | 旋转限制-上限(x,y,z) -> 弧度角
- 12 : float3 | 弹簧常数-移动(x,y,z)
- 12 : float3 | 弹簧常数-旋转(x,y,z)

## PMX2.1扩展/变更项目

### Joint种类 - 新增

- 0 : 弹簧6DOF (同PMX2.0/PMD)
- 1 : 6DOF
- 2 : P2P
- 3 : ConeTwist
- 4 : Slider
- 5 : Hinge

### 新增: Flip Morph

Flip Morph是从多个注册的Morph中根据Flip Morph的Morph值只选择一个的Morph。

转换方式:
index = (int)((count + 1) * value) - 1;

### 新增: Impulse Morph

- 刚体索引
- 本地标志 0:OFF 1:ON
- 12 : float3 | 移动速度 (x,y,z)
- 12 : float3 | 旋转扭矩 (x,y,z)

### 新增: SoftBody (PMX2.1)

- 形状 - 0:TriMesh 1:Rope
- 关联材质索引
- 组和非碰撞组标志
- 各种物理参数

---

极北P (PMX规格 ver2.1)
