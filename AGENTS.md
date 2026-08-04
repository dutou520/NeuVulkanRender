# NeuVulkanRender — Vulkan 延迟渲染引擎

C++17 实时渲染引擎，基于 Vulkan API，支持延迟渲染、PCSS 软阴影、PBR 着色、Bloom/TAA/SSAO 后处理、IBL、ImGui 场景编辑器。

## Project

- **语言**: C++17，编译器 MSVC (VS 2022)
- **构建**: CMake + vcpkg（工具链: `D:/vcpkg/scripts/buildsystems/vcpkg.cmake`）
- **图形**: Vulkan SDK 1.3+
- **窗口**: SDL3
- **入口**: `source/Main.cpp` → `main()`，命名空间 `neurender`

## Commands

```bash
# 配置（首次或 vcpkg 路径变化后运行）
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake

# 编译
cmake --build build

# 运行（需在构建目录下执行，资源已自动复制）
.\build\NeuVulkanRender.exe
```

- 无 lint / test 命令；项目无测试框架。
- Shader 编译在 CMake 构建阶段自动完成（glslc → `.spv`），无需手动触发。

## Architecture

工程拆分为多个静态库，自底向上依赖：

| 库 | 源文件 | 职责 |
|---|---|---|
| **NeuLogLib** | `source/neuLog.cpp` | spdlog 日志系统，提供 `LOG_I/E/W/D/T` 宏 |
| **NeuWindowLib** | `source/Window.cpp` | SDL3 窗口与输入 |
| **NeuImGuiBackendLib** | `source/imgui_impl_sdl3.cpp`, `imgui_impl_vulkan.cpp` | ImGui ↔ SDL3/Vulkan 胶水 |
| **NeuGUILib** | `source/neuGUI.cpp` | 编辑器 UI（层级面板、Inspector、Content Browser、菜单等） |
| **NeuCoreLib** | `source/Object.cpp`, `UUID.cpp` | 基础类 `Object`（UUID + Name）和 `UUID` |
| **NeuAssetLib** | `source/AssetManager.cpp`, `ModelImporter.cpp`, `TextureResource.cpp`, `MaterialResource.cpp`, `CubeMapResource.cpp`, `DDSLoader.cpp`, `MetaFile.cpp` | 资源管理、GLTF/OBJ/DDS 导入、纹理/材质/CubeMap 资源 |
| **NeuSceneLib** | `source/Scene.cpp`, `Project.cpp`, `Node.cpp`, `MeshNode.cpp`, `LightNode.cpp`, `PointLightNode.cpp`, `CameraNode.cpp` | 场景图、节点系统、工程管理 |
| **NeuRenderCoreLib** | `source/RenderCore.cpp` (~300KB)、`GBuffer.cpp`、`Camera.cpp` | Vulkan 核心：设备/Swapchain/RenderPass/管线/延迟着色/Shadow/后处理/TAA/IBL 全部在此 |

- `NeuAssetLib` ↔ `NeuSceneLib` 存在循环依赖，通过 CMake `LINK_GROUP:RESCAN` 处理。
- 可执行文件 `NeuVulkanRender`（`source/Main.cpp`）链接 `NeuRenderCoreLib`（它已聚合所有子库）。

头文件目录 `include/` 按功能分子目录：`Asset/`、`Core/`、`Nodes/`、`Project/`、`Renderer/`、`Scene/`。

Shader 位于 `resource/shaders/glsl/`（`.vert/.frag/.comp`），编译输出到 `resource/shaders/compiled/`（另有 `resource/shaders/spv/` 备用目录）。

## Conventions

- **命名空间**: 所有代码在 `neurender` 下。
- **头文件防护**: 统一使用 `#pragma once`。
- **命名**: 类 PascalCase（`RenderCore`、`MeshNode`），成员变量 `m_` 前缀（`m_Device`、`m_Name`），静态成员同样 `m_` 前缀。
- **日志**: 使用 `LOG_T`/`LOG_D`/`LOG_I`/`LOG_W`/`LOG_E` 宏，参数用 fmt 风格 `{}` 占位。
- **缩进与括号**: 2 空格缩进，函数/类大括号 K&R 风格（同行开括号）。
- **注释**: Doxygen `@brief` 风格，中英文混用；长文件用 `// ========== Section ==========` 分隔。
- **禁止拷贝**: 资源/管理器类 delete 拷贝构造和赋值（`= delete`），保留移动。
- **智能指针**: 使用 `std::shared_ptr` / `std::unique_ptr` 管理所有权。
- **序列化**: 对象通过 `ToJson()` / `FromJson()` 与 nlohmann/json 互转。
- **Vulkan 封装**: 渲染逻辑集中在 `RenderCore` 的静态方法中，管线资源（`VkPipeline`、`VkDescriptorSet` 等）均为静态成员，按功能分组。
- **资产 ID**: 文件和节点均通过 `UUID` 全局唯一标识，`.meta` 文件维护 GUID 到路径映射。

## Notes

- `RenderCore.cpp` 约 295KB，是核心渲染逻辑的单一集中点；新增渲染特性应在此文件内扩展。
- 项目仅支持 Windows（依赖 MSVC + Vulkan SDK + vcpkg）。
- 无自动化测试：验证靠手动运行并观察渲染结果。
- `ThirParty/` 下为 header-only 第三方库（stb、tinyobjloader、nlohmann-json、tinygltf），通过 CMake INTERFACE 库引入。
- ImGuizmo 已声明但不推荐使用，计划自研简单 Gizmo 替代（见 `documents/架构.md`）。