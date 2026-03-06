### 开发环境
Windows x86_64
gcc编译器
MinGW静态库

### 自由视角控制
在右键按住的情况下
WASD移动，鼠标旋转视角，ctrl/shift下降，space上升

- **GUI**: ImGui (启用 Docking 分支，支持窗口拖拽停靠)
    
- 模型加载: `tinygltf` (配合 Vulkan 封装)
    
- 纹理加载: `stb_image`
    
- 数学库: `glm` (注意：需处理 Vulkan Y轴向下 vs GLM Y轴向上的差异)
    
- JSON库: `nlohmann/json` (现代C++标准库首选)
    
- Gizmo: 在视口支持三轴拖拽（ `ImGuizmo` 库）。
    
- **实时编辑流** 即时模式（IM）哲学设计:
    
- 类似 Blender/Godot，选中物体即时修改属性。
    
- **材质系统**: 材质作为独立资源，修改材质参数（颜色、金属度等）优先通过 **Push Constants** 更新以实现即时反馈，纹理更换则通过 DescriptorSet。
