### 开发环境
Windows x86_64
gcc编译器
MinGW静态库

### 自由视角控制
在右键按住的情况下
WASD移动，鼠标旋转视角，ctrl/shift下降，space上升


## Albedo渲染系统数据流与实现架构
展示NeuVulkanRender项目中Albedo渲染的完整数据流架构，从材质定义[1a]、GBuffer存储[2a]到最终光照合成[3c]的全过程，以及为纹理渲染准备的UV坐标传递机制[5a]。
### 1. 材质Albedo数据定义与传递
从Material结构体定义到渲染管线的albedo数据传递流程
### 1a. Material结构体定义albedo属性 (`Material.h:29`)
在Material结构体中定义albedo颜色属性，默认为灰色
```text
glm::vec3 albedo = glm::vec3(0.8f, 0.8f, 0.8f); // 反照率颜色
```
### 1b. CreateOpaque方法设置albedo (`Material.h:54`)
创建不透明材质时设置albedo颜色值
```text
mat.albedo = color;
```
### 1c. RenderObject包含Material (`RenderCore.h:309`)
渲染对象结构体包含material成员用于存储材质属性
```text
Material material;
```
### 1d. GBuffer着色器输出albedo (`gbuffer.frag:28`)
几何阶段着色器将顶点颜色作为albedo输出到GBuffer1
```text
outAlbedoFlags = vec4(fragColor.rgb, 0.0);
```
### 2. GBuffer系统中的Albedo存储
GBuffer如何存储和管理albedo数据的多帧缓冲机制
### 2a. GBuffer1布局定义 (`GBuffer.h:25`)
注释说明GBuffer1用于存储albedo颜色和材质标志
```text
// GBuffer1: RGB=Albedo(sRGB), A=MaterialFlags  (VK_FORMAT_R8G8B8A8_SRGB)
```
### 2b. Albedo缓冲区数组 (`GBuffer.h:138`)
为多帧缓冲准备的albedo附件数组
```text
std::vector<GBufferAttachment> m_AlbedoMaterialFlags;
```
### 2c. 创建Albedo附件 (`GBuffer.cpp:25`)
为每帧创建sRGB格式的albedo颜色附件
```text
CreateAttachment(device, physicalDevice, VK_FORMAT_R8G8B8A8_SRGB,
```
### 2d. 获取Albedo附件接口 (`GBuffer.h:67`)
提供获取指定帧albedo附件的访问方法
```text
GBufferAttachment &GetAlbedoFlags(int frameIndex) {
```
### 3. 延迟渲染中的Albedo合成
从GBuffer读取albedo数据并在光照计算中使用的流程
### 3a. 合成着色器绑定GBuffer1 (`composition.frag:4`)
在合成阶段绑定GBuffer1作为albedo纹理采样器
```text
layout(set = 0, binding = 0) uniform sampler2D gbuffer1; // Albedo + MaterialFlags
```
### 3b. 采样Albedo数据 (`composition.frag:155`)
从GBuffer1采样获取albedo颜色和材质标志
```text
vec4 g1 = texture(gbuffer1, fragTexCoord);
```
### 3c. 解析Albedo颜色 (`composition.frag:168`)
从采样结果中提取RGB作为albedo颜色
```text
vec3 albedo = g1.rgb;
```
### 3d. PBR计算使用Albedo (`composition.frag:118`)
在PBR光照计算中使用albedo作为漫反射颜色
```text
vec3 diffuse = kD * albedo / 3.14159265;
```
### 4. 前向渲染中的Albedo处理
透明物体前向渲染管线中的albedo处理逻辑
### 4a. 前向着色器获取Albedo (`forward.frag:66`)
前向渲染直接使用顶点颜色作为albedo
```text
vec3 albedo = fragColor.rgb;
```
### 4b. Fresnel计算使用Albedo (`forward.frag:73`)
根据金属度混合基础反射率和albedo计算F0
```text
vec3 F0 = mix(vec3(0.04), albedo, material.metallic);
```
### 4c. 自发光使用Albedo (`forward.frag:98`)
使用albedo作为自发光颜色基础
```text
vec3 emissive = albedo * material.emissiveIntensity;
```
### 5. 纹理坐标数据流
从顶点数据到着色器的纹理坐标传递机制，为纹理albedo渲染做准备
### 5a. 顶点结构体定义纹理坐标 (`Vertex.h:17`)
Vertex结构体包含texCoord字段用于存储UV坐标
```text
glm::vec2 texCoord; // 纹理坐标
```
### 5b. 纹理坐标属性描述 (`Vertex.h:52`)
定义纹理坐标作为32位浮点数向量的顶点属性格式
```text
attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
```
### 5c. 顶点着色器传递纹理坐标 (`gbuffer.vert:38`)
顶点着色器将输入纹理坐标传递到片段着色器
```text
fragTexCoord = inTexCoord;
```
### 5d. 片段着色器接收纹理坐标 (`gbuffer.frag:6`)
片段着色器接收纹理坐标输入，为纹理采样做准备
```text
layout(location = 2) in vec2 fragTexCoord;
```




## NeuVulkanRender GUI系统架构与数据流
展示NeuVulkanRender项目中基于ImGui的完整GUI系统架构，包括初始化集成[1a]、每帧渲染循环[2c]、Dock布局系统[3c]、四大核心面板（场景层级[4b]、属性面板[4c]、资产浏览器[4d]、菜单栏[4a]）、节点交互逻辑[5b]、属性编辑系统[6d]、文件操作流程[7c]和渲染模式控制[8a]的完整实现。
### 1. GUI初始化与集成流程
从RenderCore启动到ImGui集成的完整初始化流程
### 1a. RenderCore调用ImGui初始化 (`RenderCore.cpp:277`)
在渲染系统初始化过程中调用ImGui初始化
```text
InitImGui(); // Initialize ImGui
```
### 1b. 创建ImGui上下文 (`RenderCore.cpp:923`)
建立ImGui的运行时环境
```text
ImGui::CreateContext();
```
### 1c. 启用Dock布局支持 (`RenderCore.cpp:931`)
配置ImGui支持窗口停靠功能
```text
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
```
### 1d. 设置GUI字体缩放 (`neuGUI.cpp:38`)
调整GUI界面字体大小
```text
ImGui::GetIO().FontGlobalScale = 1.5f; // 全局 UI 字体放大 1.2-1.5 倍
```
### 2. 每帧渲染循环中的GUI调用
GUI在主渲染循环中的调用位置和执行流程
### 2a. SDL3新帧处理 (`RenderCore.cpp:1400`)
处理SDL3输入事件和新帧准备
```text
ImGui_ImplSDL3_NewFrame();
```
### 2b. ImGui新帧开始 (`RenderCore.cpp:1401`)
开始新的ImGui渲染帧
```text
ImGui::NewFrame();
```
### 2c. 调用GUI渲染主函数 (`RenderCore.cpp:1404`)
执行所有GUI界面的渲染
```text
neuGUI::Render(); // Call the UI render function
```
### 2d. ImGui渲染结束 (`RenderCore.cpp:1406`)
结束ImGui帧并准备绘制
```text
ImGui::Render();
```
### 3. Dock布局系统初始化
GUI窗口Dock布局的创建和配置过程
### 3a. 设置Dock空间 (`neuGUI.cpp:51`)
在GUI渲染开始时设置窗口停靠空间
```text
SetupDockSpace();
```
### 3b. 获取主视口 (`neuGUI.cpp:83`)
获取整个窗口的视口信息用于布局
```text
ImGuiViewport *viewport = ImGui::GetMainViewport();
```
### 3c. 分割左侧面板 (`neuGUI.cpp:117`)
创建左侧20%宽度的面板区域
```text
auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
```
### 3d. 停靠场景层级窗口 (`neuGUI.cpp:125`)
将场景层级面板停靠到左侧区域
```text
ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
```
### 3e. 停靠视口窗口 (`neuGUI.cpp:128`)
将渲染视口停靠到中央区域
```text
ImGui::DockBuilderDockWindow("Viewport", dockspace_id);
```
### 4. 主界面面板渲染流程
四个主要GUI面板的渲染调用顺序和内容
### 4a. 渲染菜单栏 (`neuGUI.cpp:54`)
绘制顶部菜单栏（文件、创建、Debug）
```text
RenderMenuBar();
```
### 4b. 渲染场景层级 (`neuGUI.cpp:73`)
绘制左侧场景树和节点层级结构
```text
RenderSceneHierarchy();
```
### 4c. 渲染属性面板 (`neuGUI.cpp:74`)
绘制右侧节点属性编辑面板
```text
RenderInspector();
```
### 4d. 渲染资产浏览器 (`neuGUI.cpp:75`)
绘制底部文件和资源管理面板
```text
RenderContentBrowser();
```
### 5. 场景层级面板交互逻辑
场景树节点的选择、编辑和上下文菜单处理
### 5a. 开始场景层级窗口 (`neuGUI.cpp:359`)
创建场景层级面板窗口
```text
ImGui::Begin("场景层级");
```
### 5b. 节点选择逻辑 (`neuGUI.cpp:502`)
点击节点时更新选中状态
```text
if (ImGui::IsItemClicked()) { s_SelectedNode = node; }
```
### 5c. 递归渲染子节点 (`neuGUI.cpp:434`)
递归绘制节点的所有子节点
```text
RenderNodeTree(child.get());
```
### 5d. 右键菜单处理 (`neuGUI.cpp:527`)
处理节点的右键上下文菜单
```text
if (ImGui::BeginPopupContextItem()) { if (ImGui::MenuItem("删除")) { DeleteSelectedNode(); } }
```
### 6. Inspector属性面板编辑系统
根据选中节点类型动态渲染对应的属性编辑界面
### 6a. 开始属性窗口 (`neuGUI.cpp:548`)
创建Inspector面板窗口
```text
ImGui::Begin("详细信息");
```
### 6b. Transform选项卡 (`neuGUI.cpp:603`)
创建Transform编辑选项卡
```text
if (VerticalTab("Transform", s_CurrentInspectorTab == 1, ImVec2(100.0f, 40.0f))) { s_CurrentInspectorTab = 1; }
```
### 6c. 渲染Transform编辑器 (`neuGUI.cpp:643`)
根据选中Tab渲染对应的属性编辑器
```text
RenderTransformEditor(s_SelectedNode);
```
### 6d. 位置属性编辑 (`neuGUI.cpp:691`)
实时编辑节点位置并更新到场景
```text
if (ImGui::DragFloat3("Position", &position.x, 0.1f)) { node->SetPosition(position); }
```
### 7. 资产浏览器文件操作流程
文件浏览、选择和拖拽操作的完整处理流程
### 7a. 开始资产浏览器 (`neuGUI.cpp:812`)
创建文件管理面板窗口
```text
ImGui::Begin("资产浏览器");
```
### 7b. 文件选择逻辑 (`neuGUI.cpp:945`)
处理文件的选择和双击事件
```text
if (ImGui::Selectable(name.c_str(), s_SelectedFile == filename, ImGuiSelectableFlags_AllowDoubleClick)) { s_SelectedFile = filename; }
```
### 7c. 拖拽源设置 (`neuGUI.cpp:1095`)
将文件设置为可拖拽对象
```text
if (ImGui::BeginDragDropSource()) { ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", fullPath.c_str(), fullPath.size() + 1); }
```
### 7d. 拖拽目标处理 (`neuGUI.cpp:958`)
处理文件拖拽到文件夹的移动操作
```text
if (ImGui::BeginDragDropTarget()) { if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) { std::filesystem::rename(src, dst); } }
```
### 8. Debug菜单渲染模式控制
通过Debug菜单切换不同渲染模式的实现
### 8a. Albedo渲染模式 (`neuGUI.cpp:300`)
切换到Albedo颜色调试模式
```text
if (ImGui::MenuItem("Albedo", nullptr, s_RenderMode == RenderMode::Albedo)) { s_RenderMode = RenderMode::Albedo; settings.debugMode = 2; }
```
### 8b. 线框渲染模式 (`neuGUI.cpp:294`)
切换到线框显示模式
```text
if (ImGui::MenuItem("Wireframe", nullptr, s_RenderMode == RenderMode::Wireframe)) { s_RenderMode = RenderMode::Wireframe; settings.debugMode = 1; }
```
### 8c. 获取后处理设置 (`neuGUI.cpp:286`)
获取渲染系统的后处理参数引用
```text
auto &settings = RenderCore::GetPostProcessSettings();
```
### 8d. 获取当前渲染模式 (`neuGUI.h:109`)
提供外部访问当前渲染模式的接口
```text
static RenderMode GetRenderMode() { return s_RenderMode; }
```