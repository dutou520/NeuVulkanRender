#pragma once
#include "Camera.h"
#include "GBuffer.h"
#include "Material.h"
#include "Vertex.h"
#include "neurendercore_export.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "Asset/MaterialResource.h"
#include "Asset/MeshResource.h"
#include "Asset/TextureResource.h"
#include "Core/UUID.h"
#include "Project/Project.h"
#include <unordered_map>

namespace neurender {

class Project;

class NEURENDERCORE_API RenderCore {
public:
  static void Init();
  static void Shutdown();
  static void DrawFrame();

  // Getters for external access
  static VkDevice GetDevice() { return m_Device; }
  static VkInstance GetInstance() { return m_Instance; }
  static VkPhysicalDevice GetPhysicalDevice() { return m_PhysicalDevice; }
  static VkCommandPool GetCommandPool() { return m_CommandPool; }
  static VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }

  // ========== 材质管理接口 (Public) ==========
  static UUID CreateMaterial();
  static void DeleteMaterial(const UUID &id);
  static void SaveAllMaterials();
  static std::vector<UUID> GetAllMaterials();
  static void SetMaterialTexture(const UUID &matID, uint32_t binding,
                                 const UUID &texID);
  static MaterialResource *GetMaterialResource(const UUID &materialID);
  static TextureResource *GetTextureResource(const UUID &textureID);
  static ImTextureID GetImGuiTextureID(const UUID &textureID);

private:
  static void CreateInstance();
  static void CreateSurface();
  static void PickPhysicalDevice();
  static void CreateLogicalDevice();
  static void CreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
  static void CreateImageViews();
  static void CreateRenderPass();
  static void CreateSwapchainFramebuffers();
  static void CreateCommandPool();
  static void CreateCommandBuffers();
  static void CreateSyncObjects();
  static void CreateRenderFinishedSemaphores();
  static void CreateDescriptorPool();
  static void InitImGui();
  static void SetupDebugMessenger();

  static void CleanupSwapchain();
  static void RecreateSwapchain();
  static void RecreateRenderResolutionResources(); // 重建分辨率相关资源

  static void RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                  uint32_t imageIndex);

  // 阴影相关
  static glm::mat4 m_LightVP;

  // ========== 延迟渲染方法 ==========
  static void CreateGBuffer();
  static void CreateGBufferRenderPass();
  static void CreateCompositionRenderPass();
  static void CreateGeometryPipeline();
  static void CreateCompositionPipeline();
  static void CreateDescriptorSetLayouts();
  static void CreateUniformBuffers();
  static void CreateDescriptorSets();
  static void UpdateUniformBuffer(uint32_t currentImage);
  static void CreateSampler();

  // ========== 前向渲染管线方法 ==========
  static void CreateForwardRenderPass();
  static void CreateForwardPipeline();
  static void SortTransparentObjects();

  // ========== Shadow Pass方法 ==========
  static void CreateShadowPass();
  static void CreateShadowRenderPass();
  static void CreateShadowPipeline();
  static void CreateShadowMap();
  static void CreateShadowSampler();
  static void CreatePCSSParamsBuffers();
  static void CreateShadowUniformBuffers(); // 新增
  static void CreateShadowDescriptorSets(); // 新增
  static void GeneratePoissonDisk();
  static void LoadNoiseTexture();
  static void UpdateLightCamera();
  static void CalculateFrustumBoundingSphere(const Camera &camera,
                                             float maxDistance,
                                             glm::vec3 &outCenter,
                                             float &outRadius);

  // Shadow Pass 成员变量
  static std::vector<VkBuffer> m_ShadowUniformBuffers;
  static std::vector<VkDeviceMemory> m_ShadowUniformBuffersMemory;
  static std::vector<void *> m_ShadowUniformBuffersMapped;

  // ========== 后处理管线方法 ==========
  static void CreateSceneRenderTarget();
  static void CreatePostProcessRenderPass();
  static void CreatePostProcessPipeline();
  static void CreateSSAOResources();
  static void CreateBloomResources();
  static void CreateBloomRenderPass();
  static void CreateBloomFramebuffers();
  static void CreateBloomPipelines();
  static void CreatePostProcessDescriptorSets();
  static void CreateGBufferFramebuffers();
  static void CreateCompositionFramebuffers();
  static void CreateForwardFramebuffers();

  // ========== 辅助函数 ==========
  static VkShaderModule CreateShaderModule(const std::vector<char> &code);
  static std::vector<char> ReadShaderFile(const std::string &filename);
  static void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties, VkBuffer &buffer,
                           VkDeviceMemory &bufferMemory);
  static uint32_t FindMemoryType(uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties);
  static VkCommandBuffer CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                                    VkDeviceSize size,
                                    VkFence fence = VK_NULL_HANDLE);

  // Vulkan Members

  // ============================== Mesh Resource Cache
  // ==============================
  static std::unordered_map<UUID, MeshResource> m_MeshCache;
  static bool LoadMeshResource(const UUID &meshID);

  // ============================== Texture Resource Cache
  // ==============================
  static std::unordered_map<UUID, TextureResource> m_TextureCache;
  static bool LoadTextureResource(const UUID &textureID);

  // ============================== Material Resource Cache
  // ==============================
  static std::unordered_map<UUID, MaterialResource> m_MaterialCache;
  static bool LoadMaterialResource(const UUID &materialID);
  static void CreateMaterialDescriptorSet(MaterialResource *material);

  // ============================== Default Resources
  // ==============================
  static TextureResource m_DefaultWhiteTexture;  // 默认白色纹理
  static TextureResource m_DefaultNormalTexture; // 默认法线纹理 (0.5, 0.5, 1.0)
  static TextureResource m_DefaultBlackTexture;  // 默认黑色纹理
  static MaterialResource m_DefaultMaterial;     // 默认材质
  static void CreateDefaultTextures();
  static void CreateDefaultMaterial();

  // Material descriptor set layout
  static VkDescriptorSetLayout m_MaterialDescriptorSetLayout;

  // 场景渲染收集
  static void CollectSceneRenderables();
  static void CollectSceneLights();

  // ============================== Vulkan 实例与调试相关
  // ============================== Vulkan 实例：是 Vulkan API 的入口点，所有
  // Vulkan 操作的基础 负责加载 Vulkan 函数、初始化 API
  // 环境，是与驱动程序交互的根对象
  static VkInstance m_Instance;

  // 调试信使：Vulkan 调试工具扩展对象，用于捕获 API
  // 调用中的警告、错误等调试信息
  // 仅在调试模式下启用，帮助开发者定位内存泄漏、参数错误、管线状态异常等问题
  static VkDebugUtilsMessengerEXT m_DebugMessenger;

  // ============================== 设备与队列相关
  // ============================== 物理设备：代表实际的 GPU 硬件（或其他 Vulkan
  // 兼容硬件）
  // 用于查询硬件能力（如支持的特性、内存类型、队列族等），是逻辑设备的创建基础
  static VkPhysicalDevice m_PhysicalDevice;

  // 逻辑设备：应用程序与物理设备交互的主要接口
  // 所有 GPU
  // 操作（绘制、计算、资源拷贝等）都通过逻辑设备发起，需指定启用的扩展和特性
  static VkDevice m_Device;

  // 图形队列：用于执行图形相关命令（如绘制、管线状态设置、顶点数据处理等）
  // 属于特定的队列族，需支持图形操作能力，是渲染管线的核心执行载体
  static VkQueue m_GraphicsQueue;

  // 呈现队列：用于将渲染完成的图像提交到交换链（Surface）显示
  // 需支持呈现操作能力，可能与图形队列属于同一队列族（提高性能）或不同队列族
  static VkQueue m_PresentQueue;

  // 表面（Surface）：与窗口系统关联的抽象对象，代表渲染目标窗口/屏幕
  // 由平台相关 API（如 GLFW、SDL）创建，是交换链与实际显示设备的桥梁
  static VkSurfaceKHR m_Surface;

  // ============================== 交换链与帧缓冲相关
  // ============================== 交换链：管理用于呈现的图像缓冲队列，实现 GPU
  // 渲染与屏幕显示的同步 自动处理图像的交换、显示时序（如垂直同步），是 Vulkan
  // 渲染输出的核心组件
  static VkSwapchainKHR m_Swapchain;

  // 交换链图像：交换链管理的实际图像资源，存储渲染后的像素数据
  // 图像数量由交换链创建时指定（通常为 2-3
  // 个，实现双缓冲/三缓冲），不可直接修改，需通过视图访问
  static std::vector<VkImage> m_SwapchainImages;

  // 交换链图像格式：指定交换链图像的像素格式（如
  // VK_FORMAT_B8G8R8A8_UNORM）、颜色空间 需与 Surface
  // 支持的格式匹配，决定了像素数据的存储方式和色彩范围
  static VkFormat m_SwapchainImageFormat;

  // 交换链图像范围：指定交换链图像的尺寸（宽/高），通常与窗口尺寸一致
  // 若窗口 resize，需重新创建交换链并更新该尺寸
  static VkExtent2D m_SwapchainExtent;

  // 渲染分辨率 (用于GBuffer和TAA输入)
  static VkExtent2D m_RenderExtent;
  static float m_SuperResolutionScale; // 1.0 = Native, 2.0 = Half Res Render
  static bool m_TAAEnabled;
  static float m_TAAFeedbackFactor;

  // TAA 资源
  static GBufferAttachment m_TAAHistoryTexture; // High Res (Presentation Size)
  static VkImage m_TAAHistoryImage;
  static VkDeviceMemory m_TAAHistoryMemory;
  static VkImageView m_TAAHistoryView;

  // Jitter
  static uint32_t m_FrameCount;
  static glm::mat4 m_PrevViewProj; // 上一帧的 VP 矩阵

  // 交换链图像视图：VkImage
  // 的"视图"封装，定义了图像的访问方式（如采样方式、通道掩码） Vulkan
  // 中不能直接使用 VkImage，必须通过 ImageView 绑定到管线或帧缓冲
  static std::vector<VkImageView> m_SwapchainImageViews;

  // 交换链帧缓冲：渲染目标的集合，包含交换链图像视图（颜色附件）及可能的深度/模板附件
  // 与 RenderPass 绑定使用，定义了渲染过程中使用的所有附件资源
  static std::vector<VkFramebuffer> m_SwapchainFramebuffers;

  // ============================== 渲染管线与命令相关
  // ==============================
  // 渲染通道：定义渲染过程中的附件布局转换、子通道划分、依赖关系
  // 描述了从开始渲染到呈现的完整流程（如颜色缓冲清除、深度测试启用、布局切换时机）
  static VkRenderPass m_RenderPass;

  // 命令池：命令缓冲的内存分配器，所有命令缓冲必须从命令池创建
  // 与特定队列族关联，决定了命令缓冲可提交到哪些队列，负责管理命令缓冲的内存生命周期
  static VkCommandPool m_CommandPool;

  // 命令缓冲：存储一系列 Vulkan 命令（如绑定管线、设置顶点缓冲、绘制、拷贝等）
  // 命令需先记录到命令缓冲，再提交到队列执行，支持一次记录多次提交（或动态重录）
  static std::vector<VkCommandBuffer> m_CommandBuffers;

  // ============================== 同步相关 ==============================
  // 图像可用信号量：用于同步"交换链获取图像"与"渲染开始"的时机
  // 当交换链成功返回可渲染的图像时，该信号量触发，允许渲染命令开始执行
  static std::vector<VkSemaphore> m_ImageAvailableSemaphores;

  // 渲染完成信号量：用于同步"渲染结束"与"图像呈现"的时机
  // 当渲染命令执行完成后，该信号量触发，允许交换链将图像提交到屏幕显示
  static std::vector<VkSemaphore> m_RenderFinishedSemaphores;

  // 飞行中围栏：用于同步 CPU 与 GPU 操作，确保当前帧的 GPU
  // 命令执行完成后再进行下一帧准备
  // 防止帧重叠导致的资源竞争（如重复使用未释放的缓冲/图像），支持等待和重置操作
  static std::vector<VkFence> m_InFlightFences;

  // ============================== 资源描述符相关
  // ============================== 描述符池：用于分配描述符集（Descriptor
  // Set）的内存池
  // 预分配描述符池大小，避免频繁创建/销毁描述符集导致的性能开销，需指定支持的描述符类型和数量
  static VkDescriptorPool m_DescriptorPool;

  // ============================== 帧状态相关 ==============================
  // 当前帧索引：标识当前正在处理的帧（用于多缓冲同步，如双缓冲时 0/1 切换）
  // 配合信号量、围栏数组使用，实现帧间资源隔离，避免资源竞争
  static uint32_t m_CurrentFrame;

  // 帧缓冲大小是否已调整：标记窗口是否被 resize，触发交换链重建
  // 窗口尺寸变化时，需重新创建交换链、帧缓冲等与尺寸相关的资源，该标志用于触发重建逻辑
  static bool m_FramebufferResized;

  // ============================== 延迟渲染系统 ==============================
  // GBuffer 系统
  static GBuffer m_GBuffer;

  // 渲染通道
  static VkRenderPass m_GBufferRenderPass;
  static VkRenderPass m_CompositionRenderPass;

  // 图形管线
  static VkPipeline m_GeometryPipeline;
  static VkPipelineLayout m_GeometryPipelineLayout;
  static VkPipeline m_CompositionPipeline;
  static VkPipelineLayout m_CompositionPipelineLayout;

  // 描述符布局
  static VkDescriptorSetLayout m_GeometryDescriptorSetLayout;
  static VkDescriptorSetLayout m_CompositionGBufferDescriptorSetLayout;
  static VkDescriptorSetLayout m_CompositionLightDescriptorSetLayout;

  // 描述符集
  static std::vector<VkDescriptorSet> m_GeometryDescriptorSets;
  static std::vector<VkDescriptorSet> m_CompositionGBufferDescriptorSets;
  static std::vector<VkDescriptorSet> m_CompositionLightDescriptorSets;

  // Uniform Buffers (MVP矩阵)
  static std::vector<VkBuffer> m_UniformBuffers;
  static std::vector<VkDeviceMemory> m_UniformBuffersMemory;
  static std::vector<void *> m_UniformBuffersMapped;

  // 光照 Uniform Buffers (方向光)
  static std::vector<VkBuffer> m_LightUniformBuffers;
  static std::vector<VkDeviceMemory> m_LightUniformBuffersMemory;
  static std::vector<void *> m_LightUniformBuffersMapped;

  // 点光源 Uniform Buffers
  static std::vector<VkBuffer> m_PointLightUniformBuffers;
  static std::vector<VkDeviceMemory> m_PointLightUniformBuffersMemory;
  static std::vector<void *> m_PointLightUniformBuffersMapped;

  // 测试几何体 (立方体)
  static VkBuffer m_VertexBuffer;
  static VkDeviceMemory m_VertexBufferMemory;
  static VkBuffer m_IndexBuffer;
  static VkDeviceMemory m_IndexBufferMemory;
  static uint32_t m_IndexCount;

  // Composition Framebuffer (用于最终合成)
  static std::vector<VkFramebuffer> m_CompositionFramebuffers;

  // GBuffer Sampler
  static VkSampler m_GBufferSampler;

  // ============================== Shadow Pass系统
  // ============================== Shadow Pass渲染通道
  static VkRenderPass m_ShadowRenderPass;
  static VkPipeline m_ShadowPipeline;
  static VkPipelineLayout m_ShadowPipelineLayout;
  static VkDescriptorSetLayout m_ShadowDescriptorSetLayout;
  static std::vector<VkDescriptorSet> m_ShadowDescriptorSets;
  static std::vector<VkFramebuffer> m_ShadowFramebuffers;

  // Shadow Map深度附件
  static GBufferAttachment m_ShadowMap;

  // Shadow采样器
  static VkSampler m_ShadowSampler;

  // 噪声纹理(64x64蓝噪声)
  static TextureResource m_NoiseTexture;

  // TAA Pipeline
  static VkPipeline m_TAAPipeline;
  static VkPipelineLayout m_TAAPipelineLayout;
  static VkDescriptorSetLayout m_TAADescriptorSetLayout;
  static std::vector<VkDescriptorSet>
      m_TAADescriptorSets; // Per frame (ping-pong history?) or simpler

  // TAA 需要两个 High Res 纹理。
  static GBufferAttachment
      m_TAAHistoryTextures[2]; // 0: History, 1: Result (Ping-Pong)

  static void CreateTAAResources();
  static void CreateTAAPipeline();
  static void CreateTAADescriptorSets();
  static void RecordTAAPass(VkCommandBuffer commandBuffer, uint32_t imageIndex);
  static void UpdateFrameDescriptors();

  // PCSS参数Uniform Buffer
  static std::vector<VkBuffer> m_PCSSParamsBuffers;
  static std::vector<VkDeviceMemory> m_PCSSParamsMemory;
  static std::vector<void *> m_PCSSParamsMapped;

  // Poisson Disk采样点
  static std::vector<glm::vec2> m_PoissonDisk;

  // ============================== 前向渲染系统 (半透明物体)
  // ==============================
  static VkRenderPass m_ForwardRenderPass;
  static VkPipeline m_ForwardPipeline;
  static VkPipelineLayout m_ForwardPipelineLayout;
  static VkDescriptorSetLayout m_ForwardDescriptorSetLayout;
  static std::vector<VkDescriptorSet> m_ForwardDescriptorSets;

  // ============================== 后处理系统 ==============================
  // 场景HDR渲染目标
  static std::vector<GBufferAttachment> m_SceneColor;

  // 后处理通道
  static VkRenderPass m_PostProcessRenderPass;
  static VkPipeline m_PostProcessPipeline;
  static VkPipelineLayout m_PostProcessPipelineLayout;
  static VkDescriptorSetLayout m_PostProcessDescriptorSetLayout;
  static std::vector<VkDescriptorSet> m_PostProcessDescriptorSets;

  // Bloom资源
  static GBufferAttachment m_BloomBrightTexture;
  static GBufferAttachment m_BloomBlurTexture;
  static VkFramebuffer m_BloomBrightFramebuffer;
  static VkFramebuffer m_BloomBlurFramebuffer;
  static VkPipeline m_BloomThresholdPipeline;
  static VkPipeline m_BloomBlurPipeline;
  static VkPipelineLayout m_BloomPipelineLayout;
  static VkRenderPass m_BloomRenderPass;

  static VkDescriptorSetLayout m_SingleTextureDescriptorSetLayout;
  static std::vector<VkDescriptorSet> m_BloomThresholdDescriptorSets;
  static std::vector<VkDescriptorSet>
      m_BloomBlurDescriptorSets; // [0]: Read Bright, [1]: Read Blur

  // SSAO资源
  static GBufferAttachment m_SSAONoise;
  static VkBuffer m_SSAOKernelBuffer;
  static VkDeviceMemory m_SSAOKernelMemory;

  // 相机Uniform Buffer (用于后处理)
  static std::vector<VkBuffer> m_CameraUniformBuffers;
  static std::vector<VkDeviceMemory> m_CameraUniformBuffersMemory;
  static std::vector<void *> m_CameraUniformBuffersMapped;

  // 后处理设置
  struct PostProcessSettings {
    uint32_t enableSSAO = 1;
    uint32_t enableBloom = 1;
    uint32_t enableToneMapping = 1;
    uint32_t enableGamma = 1;
    float bloomIntensity = 0.5f;
    float bloomThreshold = 0.8f;
    float ssaoRadius = 0.5f;
    float ssaoStrength = 1.5f;
    uint32_t debugMode =
        0; // 0=Shaded, 1=Wireframe, 2=Albedo, 3=Normal, 4=Depth, 5=Smoothness,
           // 6=Specular, 7=Occlusion, 8=MaterialFlags, 9=ShadingID, 10=Emission
  };
  static PostProcessSettings m_PostProcessSettings;

  // ============================== 场景对象管理 ==============================
  // 渲染对象
  struct RenderObject {
    glm::mat4 modelMatrix;
    Material material;
    MaterialResource *pMaterialResource = nullptr;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    uint32_t indexCount;
    float distanceToCamera; // 用于透明物体排序
    bool isInViewFrustum = false;
    bool isInLightFrustum = false;
  };

  static std::vector<RenderObject> m_RenderObjects;

  // ============================== 相机系统 ==============================
  static Camera m_Camera;
  static float m_DeltaTime;
  static float m_LastFrameTime;
  static bool m_CameraControlEnabled;
  static float m_LastMouseX;
  static float m_LastMouseY;
  static bool m_FirstMouse;

public:
  // 相机控制接口
  static void ProcessInput();
  static Camera &GetCamera() { return m_Camera; }
  static void SetCameraControlEnabled(bool enabled) {
    m_CameraControlEnabled = enabled;
  }
  static bool IsCameraControlEnabled() { return m_CameraControlEnabled; }

  // ========== 后处理设置接口 ==========
  static PostProcessSettings &GetPostProcessSettings() {
    return m_PostProcessSettings;
  }

  // ========== 工程管理接口 ==========
  static void SetCurrentProject(std::shared_ptr<Project> project) {
    m_CurrentProject = project;
  }
  static std::shared_ptr<Project> GetCurrentProject() {
    return m_CurrentProject;
  }

  // ========== 性能设置接口 ==========
  static void SetVSync(bool enabled) {
    if (m_VSync != enabled) {
      m_VSync = enabled;
      m_FramebufferResized = true; // 触发交换链重建以应用新的呈现模式
    }
  }
  static bool IsVSyncEnabled() { return m_VSync; }
  static void SetTargetFPS(int fps) { m_TargetFPS = fps; }
  static int GetTargetFPS() { return m_TargetFPS; }

  // ========== PCSS阴影设置接口 ==========
  struct PCSSSettings {
    glm::vec3 lightDirection = glm::normalize(glm::vec3(0.5f, 0.8f, 0.3f));
    float lightSize = 15.0f;
    float shadowDistance = 30.0f;
    float bias = 0.000001f;     // Shadow Bias参数
    float minFilterSize = 0.2f; // 基础模糊半径(像素单位)
    uint32_t blockerSamples = 16;
    uint32_t pcfSamples = 32;
    uint32_t shadowMapRes = 2048;
    bool enableDirectionalLight = true;
    bool enableShadow = true;
  };
  static PCSSSettings &GetPCSSSettings() { return m_PCSSSettings; }
  static void SetShadowMapResolution(uint32_t res);

  // ========== TAA & Super Resolution 接口 ==========
  static void SetSuperResolutionScale(float scale);
  static float GetSuperResolutionScale() { return m_SuperResolutionScale; }
  static void SetTAAEnabled(bool enabled) { m_TAAEnabled = enabled; }
  static bool IsTAAEnabled() { return m_TAAEnabled; }
  static void ApplyResolutionChanges(); // 触发重建
  static void SetTAAFeedbackFactor(float factor) {
    m_TAAFeedbackFactor = factor;
  }
  static float GetTAAFeedbackFactor() { return m_TAAFeedbackFactor; }

  // ========== 模型加载方法 ==========
  /**
   * @brief 从OBJ文件加载模型
   * @param path 模型文件路径
   * @param outVertices 输出顶点数据
   * @param outIndices 输出索引数据
   * @return 加载成功返回true
   */
  static bool LoadModelFromFile(const std::string &path,
                                std::vector<Vertex> &outVertices,
                                std::vector<uint32_t> &outIndices);

  // ========== 自定义几何体接口 ==========
  /**
   * @brief 设置自定义几何体数据
   * @param vertices 顶点数据
   * @param indices 索引数据
   * @note 必须在 RenderCore::Init() 之前调用
   */
  static void SetGeometryData(const std::vector<Vertex> &vertices,
                              const std::vector<uint32_t> &indices);

  /**
   * @brief 检查是否已设置自定义几何体
   */
  static bool HasCustomGeometry() { return m_UseCustomGeometry; }

private:
  // 自定义几何体数据
  // 自定义几何体数据
  static std::vector<Vertex> m_CustomVertices;
  static std::vector<uint32_t> m_CustomIndices;
  static bool m_UseCustomGeometry;

  // Async Command Buffer Tracking
  static std::vector<std::pair<VkFence, VkCommandBuffer>>
      m_ActiveAsyncCommandBuffers;
  static void CleanupAsyncCommandBuffers();

  // 工程管理
  static std::shared_ptr<Project> m_CurrentProject;

  // 性能控制
  static bool m_VSync;
  static int m_TargetFPS;

  // PCSS阴影设置
  static PCSSSettings m_PCSSSettings;
};
} // namespace neurender
