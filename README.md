# Frozen Tundra

An OpenGL 3.3 interactive 3D scene featuring a snowy mountain terrain, aurora skybox, controllable Tyrannosaurus Rex with skeletal animation, dynamic snowfall particle system, and real-time shadow mapping.

Built with **FreeGLUT**, **GLEW**, **GLM**, and **Assimp**.

## Demo

- [Deliverable 2 Demo](https://youtu.be/AMMPC5atXBA)
- [Final Report Demo](https://youtu.be/OSV0oSyVEPM)

## Scene Composition

| Element | Source |
|---------|--------|
| Snow-capped mountain terrain | `Models/mountain.quads.obj` |
| Aurora night skybox | `Models/aurora-sky.gltf` |
| Tyrannosaurus Rex (animated) | `Models/Tyranno/Tyranno_Walk.glb` |
| Snowflake particles | Procedurally generated |

## Features

### Hierarchical Skeletal Animation
- Loads model skeletons via Assimp with recursive node tree traversal
- Extracts bone weights and offset matrices for GPU skinning
- Real-time bone pose calculation using a walk cycle parameter (`0.0`–`1.0`)
- Hierarchical transformation propagates parent bone motion to children

### Camera Control
- **W/A/S/D** — move forward/backward/left/right
- **Arrow keys** — move up/down
- **Mouse** — look around (pitch/yaw via Euler angles)
- Movement is frame-rate independent using `deltaTime`

### Dinosaur Character Control
- **L** — rotate left
- **K** — rotate right
- The dinosaur moves in the direction it faces with smooth rotation interpolation
- Trigonometric calculation of forward vector from rotation angle

### Particle System — Dynamic Snowfall
- 6000 snowflake particles rendered as `GL_POINTS`
- Perspective scaling via `gl_PointSize` (near = larger, far = smaller)
- Wind simulation with horizontal velocity offsets
- Infinite looping: particles reset when they fall below ground or exit the camera's horizontal range

### Phong Lighting
- Fixed point light source at `(0, 50, 20)`
- Ambient, diffuse, and specular components
- Normal mapping support for terrain detail
- TBN matrix transformation for tangent-space normals

### Real-Time Shadow Mapping
- Depth framebuffer rendered from light's orthographic perspective
- 3x3 PCF (Percentage Closer Filtering) for soft shadow edges
- Dynamic bias calculation to reduce shadow acne based on surface angle

## Controls

| Key | Action |
|-----|--------|
| W / S | Move camera forward / backward |
| A / D | Move camera left / right |
| Up / Down | Move camera up / down |
| Mouse | Look around |
| L | Rotate dinosaur left |
| K | Rotate dinosaur right |

## Build Requirements

- **OpenGL 3.3** core profile
- **FreeGLUT** (included in `freeglut/`)
- **GLEW** (included in `glew-1.10.0/`)
- **GLM** (included in `glm/`)
- **Assimp** (included in `assimp/`)
- Visual Studio (`.sln` / `.vcxproj` provided)

## Project Structure

```
Frozen_Tundra/
├── main.cpp                  # Entry point, shader sources, render loop
├── Shader.h / Shader.cpp     # Shader compilation and uniform management
├── Camera.h / Camera.cpp     # Camera with Euler angle control
├── Model.h / Model.cpp       # Model loading via Assimp
├── Mesh.h / Mesh.cpp         # Mesh data and OpenGL buffer management
├── Bone.h                    # Bone data structure
├── Animation.h               # Animation data with node hierarchy
├── Animator.h / Animator.cpp # Skeletal animation runtime
├── stb_image.h               # Image loading (stb)
├── stb_impl.cpp              # stb implementation
├── Models/                   # 3D models and textures
├── assimp/                   # Assimp library headers
├── glm/                      # GLM math library
├── glew-1.10.0/              # GLEW library
├── freeglut/                 # FreeGLUT library
└── Frozen_Tundra.sln         # Visual Studio solution
```
