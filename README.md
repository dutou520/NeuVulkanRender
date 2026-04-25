# NeuVulkanRender

![Vulkan](https://img.shields.io/badge/Graphics-Vulkan-red.svg?style=for-the-badge&logo=vulkan)
![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg?style=for-the-badge&logo=c%2B%2B)
![Build](https://img.shields.io/badge/Build-CMake-brightgreen.svg?style=for-the-badge&logo=cmake)

**NeuVulkanRender** 是一款基于 Vulkan API 开发的高性能、模块化实时渲染引擎。项目采用延迟渲染架构（Deferred Rendering），旨在实现电影级的物理着色效果、软阴影以及丰富的后处理特效。

---

## 🚀 核心渲染特性

### 1. 渲染管线架构
*   **延迟渲染管线 (Deferred Rendering)**：
    *   **几何阶段**：将场景属性（Albedo, Normal, Specular, Smoothness, Shading ID 等）渲染至高精度的多附件 G-Buffer。
    *   **着色阶段**：支持多种着色模型，通过 G-Buffer 中的着色 ID 实现高效的分支着色。
*   **向前渲染 (Forward Pass)**：专门用于处理半透明物体，支持 Alpha 混合。
*   **深度附件**：独立配置 `VK_FORMAT_D32_SFLOAT` 深度缓冲，确保深度精度。

### 2. 高质量阴影系统 (PCSS)
*   **PCSS (Percentage Closer Soft Shadows)**：模拟真实世界中随距离变软的半影效果。
*   **泊松圆盘采样 (Poisson Disk Sampling)**：结合蓝噪声旋转采样圆盘，消除阴影锯齿。
*   **阴影稳定化**：
    *   **包围球策略**：基于视锥体构建稳定的光源观察矩阵。
    *   **像素对齐 (Texel Snapping)**：锁定像素单位偏移，彻底消除摄像机移动时的阴影闪烁。

### 3. PBR 物理着色
*   全场景统一使用物理基础渲染（PBR）材质。
*   支持自发光（Emissive）、环境光遮蔽（Occlusion）以及细致的材质参数控制。

### 4. 后处理管线
*   **Bloom**：采用多级降采样卷积实现柔和的辉光效果。
*   **Tonemapping**：支持 HDR 到 SDR 的映射及伽马校正。
*   **大气雾效**：内置距离雾与高度雾系统。
*   **SSAO**：屏幕空间环境光遮蔽（开发中）。

---

## 🎨 场景编辑与交互

*   **实时编辑流**：基于即时模式（IM）哲学设计，选中物体即可在 UI 中实时修改属性。
*   **材质系统**：材质作为独立资源，修改参数（颜色、金属度等）优先通过 **Push Constants** 更新实现即时反馈。
*   **Gizmo 操作**：集成 `ImGuizmo` 支持在视口中通过三轴拖拽移动/旋转物体。
*   **GUI 架构**：基于 ImGui Docking 分支，支持灵活的窗口拖拽与布局停靠。

---

## 🏗️ 架构设计

项目采用高度模块化的设计，将核心功能拆分为多个专用的静态库：

| 模块名称 | 职责描述 |
| :--- | :--- |
| **NeuRenderCore** | 渲染核心，管理 Vulkan 设备、交换链及 RenderPass。 |
| **NeuAssetLib** | 资源管理系统，支持 GLTF、OBJ 模型及 DDS、STB 纹理加载。 |
| **NeuSceneLib** | 场景树与节点系统（MeshNode, LightNode, CameraNode）。 |
| **NeuGUILib** | 基于 ImGui 的 UI 系统，集成 ImGuizmo 实现场景编辑器功能。 |
| **NeuWindowLib** | 跨平台窗口与输入管理（基于 SDL3）。 |
| **NeuLogLib** | 基于 spdlog 的全局日志系统。 |

---

## 🛠️ 技术栈与依赖

*   **图形 API**: Vulkan SDK (1.3+)
*   **窗口系统**: SDL3
*   **数学库**: GLM (已处理 Vulkan Y 轴坐标差异)
*   **UI 系统**: Dear ImGui (Docking) + ImGuizmo
*   **资源加载**: tinygltf, tinyobjloader, stb_image
*   **序列化**: nlohmann/json
*   **构建工具**: CMake + vcpkg

---

## 🔨 构建与运行

### 环境要求
*   **系统**: Windows 10/11
*   **编译器**: 支持 C++17 的 MSVC (建议 VS 2022)
*   **SDK**: 已安装 Vulkan SDK

### 获取代码
```bash
git clone https://github.com/dutou520/NeuVulkanRender.git
cd NeuVulkanRender
```
