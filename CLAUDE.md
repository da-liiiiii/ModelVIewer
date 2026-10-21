# ModelViewer

Qt6 + OpenGL 3.3 Core Profile 的 3D 模型查看器，支持加载 OBJ/MTL 模型、多模型场景、拾取选中、拖拽移动。

## 构建

- 依赖：Qt6（Widgets / OpenGL / OpenGLWidgets）+ OpenGL
- CMake ≥ 3.16，C++17

```bash
cmake -B build
cmake --build build
```

（若 Qt6 不在默认搜索路径，需加 `-DCMAKE_PREFIX_PATH=<Qt6 安装路径>`）

## 文件职责

| 文件 | 职责 |
|---|---|
| main.cpp / mainwindow.* | 主窗口与 UI（"打开模型"/"添加模型"按钮） |
| glwidget.* | QOpenGLWidget：渲染、轨道相机、光照、拾取、移动 |
| model.* | 场景模型：position/rotation/scale + 一组 Mesh + AABB |
| mesh.* | 单材质子网格：VAO/VBO/纹理 + draw |
| objloader.* | OBJ 解析（按 usemtl 分组为 SubMesh） |
| mtlloader.* | MTL 材质解析（Material：Kd、diffuseMap 等） |

## 渲染管线要点

- 顶点：交错布局，每顶点 8 float = 位置 xyz + 法线 xyz + UV xy
- 着色器 uniform：`uVP`（投影·视图，每帧一次）、`uM`（模型矩阵，每模型一次）
- 模型矩阵 `M = T·R·S`，在 `Model::getMatrix()` 里由 position/rotation/scale 现算（"参数即状态"，避免累积漂移）
- 光照：世界空间 Blinn-Phong（环境 + 漫反射 + 高光）
- 选中高亮：`uSelected` uniform，把结果混入橙色

## 交互

- 左键：点击拾取选中；已选中时左键拖拽 = 移动该模型（相机朝向平面）
- 右键拖拽：旋转相机（轨道）
- 中键拖拽：平移相机
- 滚轮：缩放（改 m_distance）

## 拾取（Picking）

1. 屏幕像素 → NDC（y 翻转）→ 世界射线（`(projection·view).inverted()`）
2. 射线与每个模型求交，取最近命中
3. 当前用写死的包围球半径（radius=1.0），正在替换为真实 AABB：
   - 从顶点算局部空间 AABB（`computeAabb`）
   - 把射线逆变换到模型局部空间，slab 求交

## 约定 / 注意事项

- `Mesh` 持有 `QOpenGLBuffer`/VAO，不可拷贝，用 `std::unique_ptr` + `std::vector` 管理（不能用 QVector，隐式共享会触发拷贝）
- QMatrix4x4 是后乘语义；每帧循环要先 `setToIdentity()`
- 加载/添加模型前要 `makeCurrent()`，结束 `doneCurrent()`
- 终端中文乱码：Windows 控制台 GBK vs Qt UTF-8，调试输出用英文

## 进度

- 已完成：多模型、拾取选中高亮、拖拽移动
- 进行中：用 AABB 替换写死的拾取半径
- TODO：透明度、`resetCameraStatus` 自动取景（按模型真实包围盒）
