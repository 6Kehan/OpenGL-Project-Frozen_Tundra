# Frozen Tundra

基于 OpenGL 3.3 的交互式 3D 冰雪场景，包含雪山地形、极光天空盒、带有骨骼动画的可控霸王龙、动态降雪粒子系统和实时阴影映射。

使用 **FreeGLUT**、**GLEW**、**GLM** 和 **Assimp** 构建。

## 演示视频

[最终版演示](https://youtu.be/OSV0oSyVEPM)

## 场景组成

| 元素 | 来源 |
|------|------|
| 雪山地形 | `Models/mountain.quads.obj` |
| 极光夜空天空盒 | `Models/aurora-sky.gltf` |
| 霸王龙（带动画） | `Models/Tyranno/Tyranno_Walk.glb` |
| 雪花粒子 | 程序化生成 |

## 功能特性

### 层次骨骼动画
- 通过 Assimp 加载模型骨骼，递归遍历节点树
- 提取骨骼权重和偏移矩阵，上传 GPU 进行蒙皮计算
- 基于行走周期参数（`0.0`–`1.0`）实时计算骨骼姿态
- 父骨骼变换逐级传递给子骨骼，保证层次正确性

### 相机控制
- **W/A/S/D** — 前后左右移动
- **方向键上/下** — 垂直移动
- **鼠标** — 环视（欧拉角俯仰/偏航）
- 基于 `deltaTime` 实现帧率无关的平滑移动

### 恐龙角色控制
- **L** — 向左旋转
- **K** — 向右旋转
- 恐龙沿面朝方向自动前进，旋转角度平滑插值
- 通过三角函数根据旋转角计算前方向量

### 粒子系统 — 动态降雪
- 6000 个雪花粒子，使用 `GL_POINTS` 绘制
- 根据距离相机远近自动缩放（`gl_PointSize`）
- 模拟风力效果，粒子水平方向偏移
- 无限循环：粒子落至地面以下或超出视野范围后自动重置

### Phong 光照模型
- 固定点光源位于 `(0, 50, 20)`
- 环境光、漫反射、镜面高光三要素
- 支持法线贴图增强地形细节
- TBN 矩阵将切线空间法线转换至世界空间

### 实时阴影映射
- 从光源正交视角渲染深度帧缓冲
- 3×3 PCF（百分比渐近滤波）实现软阴影边缘
- 根据表面法线与光线夹角动态计算偏移量，减少阴影失真

## 操作键位

| 按键 | 功能 |
|------|------|
| W / S | 相机前进 / 后退 |
| A / D | 相机左移 / 右移 |
| 上 / 下 | 相机上移 / 下移 |
| 鼠标 | 环视 |
| L | 恐龙左转 |
| K | 恐龙右转 |

## 构建要求

- **OpenGL 3.3** Core Profile
- **FreeGLUT**（位于 `freeglut/`）
- **GLEW**（位于 `glew-1.10.0/`）
- **GLM**（位于 `glm/`）
- **Assimp**（位于 `assimp/`）
- Visual Studio（提供 `.sln` / `.vcxproj`）

## 项目结构

```
Frozen_Tundra/
├── main.cpp                  # 入口程序、着色器源码、渲染循环
├── Shader.h / Shader.cpp     # 着色器编译与 uniform 管理
├── Camera.h / Camera.cpp     # 欧拉角相机控制
├── Model.h / Model.cpp       # Assimp 模型加载
├── Mesh.h / Mesh.cpp         # 网格数据与 OpenGL 缓冲区管理
├── Bone.h                    # 骨骼数据结构
├── Animation.h               # 动画数据与节点层次
├── Animator.h / Animator.cpp # 骨骼动画运行时
├── stb_image.h               # 图片加载（stb）
├── stb_impl.cpp              # stb 实现
├── Models/                   # 3D 模型和纹理资源
├── assimp/                   # Assimp 库头文件
├── glm/                      # GLM 数学库
├── glew-1.10.0/              # GLEW 库
├── freeglut/                 # FreeGLUT 库
└── Frozen_Tundra.sln         # Visual Studio 解决方案
```
