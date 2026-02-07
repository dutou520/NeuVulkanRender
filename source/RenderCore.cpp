#include "RenderCore.h"
#include "Asset/AssetManager.h"
#include "Nodes/PointLightNode.h"
#include "Project/Project.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Scene.h"
#include "Window.h"
#include "neuGUI.h"
#include "neuLog.h"
#include "tiny_obj_loader.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <set>
#include <vulkan/vulkan_core.h>

namespace neurender {

// ========== 原有静态成员定义 ==========
std::unordered_map<UUID, MeshResource> RenderCore::m_MeshCache;
VkInstance RenderCore::m_Instance = VK_NULL_HANDLE;
VkPhysicalDevice RenderCore::m_PhysicalDevice = VK_NULL_HANDLE;
VkDevice RenderCore::m_Device = VK_NULL_HANDLE;
VkQueue RenderCore::m_GraphicsQueue = VK_NULL_HANDLE;
VkQueue RenderCore::m_PresentQueue = VK_NULL_HANDLE;
VkSurfaceKHR RenderCore::m_Surface = VK_NULL_HANDLE;
VkSwapchainKHR RenderCore::m_Swapchain = VK_NULL_HANDLE;
std::vector<VkImage> RenderCore::m_SwapchainImages;
VkFormat RenderCore::m_SwapchainImageFormat;
VkExtent2D RenderCore::m_SwapchainExtent;
std::vector<VkImageView> RenderCore::m_SwapchainImageViews;
std::vector<VkFramebuffer> RenderCore::m_SwapchainFramebuffers;
VkRenderPass RenderCore::m_RenderPass = VK_NULL_HANDLE;
VkCommandPool RenderCore::m_CommandPool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> RenderCore::m_CommandBuffers;
std::vector<VkSemaphore> RenderCore::m_ImageAvailableSemaphores;
std::vector<VkSemaphore> RenderCore::m_RenderFinishedSemaphores;
std::vector<VkFence> RenderCore::m_InFlightFences;
VkDescriptorPool RenderCore::m_DescriptorPool = VK_NULL_HANDLE;
uint32_t RenderCore::m_CurrentFrame = 0;
bool RenderCore::m_FramebufferResized = false;
VkDebugUtilsMessengerEXT RenderCore::m_DebugMessenger = VK_NULL_HANDLE;

// ========== 延迟渲染系统静态成员定义 ==========
GBuffer RenderCore::m_GBuffer;
VkRenderPass RenderCore::m_GBufferRenderPass = VK_NULL_HANDLE;
VkRenderPass RenderCore::m_CompositionRenderPass = VK_NULL_HANDLE;

VkPipeline RenderCore::m_GeometryPipeline = VK_NULL_HANDLE;
VkPipelineLayout RenderCore::m_GeometryPipelineLayout = VK_NULL_HANDLE;
VkPipeline RenderCore::m_CompositionPipeline = VK_NULL_HANDLE;
VkPipelineLayout RenderCore::m_CompositionPipelineLayout = VK_NULL_HANDLE;

VkDescriptorSetLayout RenderCore::m_GeometryDescriptorSetLayout =
    VK_NULL_HANDLE;
VkDescriptorSetLayout RenderCore::m_CompositionGBufferDescriptorSetLayout =
    VK_NULL_HANDLE;
VkDescriptorSetLayout RenderCore::m_CompositionLightDescriptorSetLayout =
    VK_NULL_HANDLE;

std::vector<VkDescriptorSet> RenderCore::m_GeometryDescriptorSets;
std::vector<VkDescriptorSet> RenderCore::m_CompositionGBufferDescriptorSets;
std::vector<VkDescriptorSet> RenderCore::m_CompositionLightDescriptorSets;

std::vector<VkBuffer> RenderCore::m_UniformBuffers;
std::vector<VkDeviceMemory> RenderCore::m_UniformBuffersMemory;
std::vector<void *> RenderCore::m_UniformBuffersMapped;

std::vector<VkBuffer> RenderCore::m_LightUniformBuffers;
std::vector<VkDeviceMemory> RenderCore::m_LightUniformBuffersMemory;
std::vector<void *> RenderCore::m_LightUniformBuffersMapped;

// 点光源 Uniform Buffers
std::vector<VkBuffer> RenderCore::m_PointLightUniformBuffers;
std::vector<VkDeviceMemory> RenderCore::m_PointLightUniformBuffersMemory;
std::vector<void *> RenderCore::m_PointLightUniformBuffersMapped;

VkBuffer RenderCore::m_VertexBuffer = VK_NULL_HANDLE;
VkDeviceMemory RenderCore::m_VertexBufferMemory = VK_NULL_HANDLE;
VkBuffer RenderCore::m_IndexBuffer = VK_NULL_HANDLE;
VkDeviceMemory RenderCore::m_IndexBufferMemory = VK_NULL_HANDLE;
uint32_t RenderCore::m_IndexCount = 0;

std::vector<VkFramebuffer> RenderCore::m_CompositionFramebuffers;
VkSampler RenderCore::m_GBufferSampler = VK_NULL_HANDLE;

// ========== 相机系统静态成员定义 ==========
Camera RenderCore::m_Camera(glm::vec3(2.0f, 2.0f, 2.0f),
                            glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, -35.0f);
float RenderCore::m_DeltaTime = 0.0f;
float RenderCore::m_LastFrameTime = 0.0f;
bool RenderCore::m_CameraControlEnabled = false;
float RenderCore::m_LastMouseX = 640.0f;
float RenderCore::m_LastMouseY = 360.0f;
bool RenderCore::m_FirstMouse = true;

// ========== 自定义几何体静态成员定义 ==========
std::vector<Vertex> RenderCore::m_CustomVertices;
std::vector<uint32_t> RenderCore::m_CustomIndices;
bool RenderCore::m_UseCustomGeometry = false;

// ========== 工程管理静态成员定义 ==========
std::shared_ptr<Project> RenderCore::m_CurrentProject = nullptr;

// ========== 前向渲染系统静态成员定义 ==========
VkRenderPass RenderCore::m_ForwardRenderPass = VK_NULL_HANDLE;
VkPipeline RenderCore::m_ForwardPipeline = VK_NULL_HANDLE;
VkPipelineLayout RenderCore::m_ForwardPipelineLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout RenderCore::m_ForwardDescriptorSetLayout = VK_NULL_HANDLE;
std::vector<VkDescriptorSet> RenderCore::m_ForwardDescriptorSets;

// ========== 后处理系统静态成员定义 ==========
std::vector<GBufferAttachment> RenderCore::m_SceneColor;

VkRenderPass RenderCore::m_PostProcessRenderPass = VK_NULL_HANDLE;
VkPipeline RenderCore::m_PostProcessPipeline = VK_NULL_HANDLE;
VkPipelineLayout RenderCore::m_PostProcessPipelineLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout RenderCore::m_PostProcessDescriptorSetLayout =
    VK_NULL_HANDLE;
std::vector<VkDescriptorSet> RenderCore::m_PostProcessDescriptorSets;

// Bloom资源
GBufferAttachment RenderCore::m_BloomBrightTexture;
GBufferAttachment RenderCore::m_BloomBlurTexture;
VkFramebuffer RenderCore::m_BloomBrightFramebuffer = VK_NULL_HANDLE;
VkFramebuffer RenderCore::m_BloomBlurFramebuffer = VK_NULL_HANDLE;
VkPipeline RenderCore::m_BloomThresholdPipeline = VK_NULL_HANDLE;
VkPipeline RenderCore::m_BloomBlurPipeline = VK_NULL_HANDLE;
VkPipelineLayout RenderCore::m_BloomPipelineLayout = VK_NULL_HANDLE;

VkRenderPass RenderCore::m_BloomRenderPass = VK_NULL_HANDLE;
VkDescriptorSetLayout RenderCore::m_SingleTextureDescriptorSetLayout =
    VK_NULL_HANDLE;
std::vector<VkDescriptorSet> RenderCore::m_BloomThresholdDescriptorSets;
std::vector<VkDescriptorSet> RenderCore::m_BloomBlurDescriptorSets;

// SSAO资源
GBufferAttachment RenderCore::m_SSAONoise;
VkBuffer RenderCore::m_SSAOKernelBuffer = VK_NULL_HANDLE;
VkDeviceMemory RenderCore::m_SSAOKernelMemory = VK_NULL_HANDLE;

// 相机Uniform Buffer
std::vector<VkBuffer> RenderCore::m_CameraUniformBuffers;
std::vector<VkDeviceMemory> RenderCore::m_CameraUniformBuffersMemory;
std::vector<void *> RenderCore::m_CameraUniformBuffersMapped;

// 后处理设置
RenderCore::PostProcessSettings RenderCore::m_PostProcessSettings;

// ========== 场景对象静态成员定义 ==========
std::vector<RenderCore::RenderObject> RenderCore::m_RenderObjects;

// Global variable for PostProcess Camera Descriptor Sets (avoiding header
// change)
std::vector<VkDescriptorSet> g_PostProcessCameraDescriptorSets;

// Global variable for Forward Framebuffer
std::vector<VkFramebuffer> g_ForwardFramebuffers;

const int MAX_FRAMES_IN_FLIGHT = 3;

// Helper function to check validation layer support
bool CheckValidationLayerSupport() {
  // For simplicity, we assume validation layers if in debug, but user didn't
  // ask explicitly. However, to make it robust, we'll request
  // "VK_LAYER_KHRONOS_validation" if available.
  return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    LOG_W("validation layer: {0}", pCallbackData->pMessage);
  }
  return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

void PopulateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
}

void RenderCore::Init() {
  CreateInstance();      // Create Vulkan instance
  SetupDebugMessenger(); // Setup validation layer callback
  CreateSurface();       // Create window surface
  PickPhysicalDevice();  // Select GPU
  CreateLogicalDevice(); // Create logical device and queues
  CreateSwapchain();     // Create swapchain
  CreateImageViews();    // Create image views

  // ========== 资源初始化 ==========
  CreateCommandPool();    // Create command pool
  CreateUniformBuffers(); // 创建 Uniform Buffers
  CreateDescriptorPool(); // Create descriptor pool

  // ========== 核心资源初始化 (纹理/缓冲) ==========
  CreateGBuffer();           // 创建 GBuffer 资源
  CreateSampler();           // 创建全局采样器
  CreateSceneRenderTarget(); // 创建HDR场景渲染目标
  CreateSSAOResources();     // 创建SSAO资源
  CreateBloomResources();    // 创建Bloom资源

  // ========== 延迟渲染逻辑初始化 ==========
  CreateGBufferRenderPass(); // 创建 GBuffer 渲染通道

  CreateCompositionRenderPass(); // 创建 合成渲染通道
  CreateBloomRenderPass();       // 创建 Bloom 渲染通道
  CreateBloomFramebuffers();     // 创建 Bloom 帧缓冲
  CreateDescriptorSetLayouts();  // 创建描述符集布局
  CreateGeometryPipeline();      // 创建几何管线
  CreateCompositionPipeline();   // 创建合成管线
  CreateDescriptorSets();        // 创建描述符集

  // ========== 前向渲染初始化 ==========
  CreateForwardRenderPass(); // 创建前向渲染通道
  CreateForwardPipeline();   // 创建前向渲染管线

  // ========== 后处理逻辑初始化 ==========
  CreatePostProcessRenderPass(); // 创建后处理渲染通道 (作为最终Pass)

  // 创建最终Swapchain Framebuffers (依赖 PostProcessRenderPass)
  CreateFramebuffers();

  CreatePostProcessPipeline();       // 创建后处理管线
  CreateBloomPipelines();            // 创建Bloom管线
  CreatePostProcessDescriptorSets(); // 创建后处理描述符集

  CreateCommandBuffers(); // Create command buffers (依赖 RenderPass)
  CreateSyncObjects();    // Create semaphores and fences

  // ========== 场景设置 ==========
  // 不再设置默认测试场景，等待用户加载工程
  // SetupBunnyTestScene();

  InitImGui(); // Initialize ImGui

  LOG_I("RenderCore Initialized with Deferred + Forward Rendering & "
        "Post-Processing Pipeline");
}

void RenderCore::Shutdown() {
  vkDeviceWaitIdle(m_Device);

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  // 1. 销毁 GBuffer 系统资源
  m_GBuffer.Destroy(m_Device);

  // 2. 销毁 场景相关图像资源 (Color, Bloom, SSAO etc.)
  for (auto &sc : m_SceneColor) {
    if (sc.view != VK_NULL_HANDLE) {
      vkDestroyImageView(m_Device, sc.view, nullptr);
      vkDestroyImage(m_Device, sc.image, nullptr);
      vkFreeMemory(m_Device, sc.memory, nullptr);
      sc.view = VK_NULL_HANDLE;
    }
  }
  m_SceneColor.clear();

  if (m_BloomBrightTexture.view != VK_NULL_HANDLE) {
    vkDestroyImageView(m_Device, m_BloomBrightTexture.view, nullptr);
    vkDestroyImage(m_Device, m_BloomBrightTexture.image, nullptr);
    vkFreeMemory(m_Device, m_BloomBrightTexture.memory, nullptr);
    m_BloomBrightTexture.view = VK_NULL_HANDLE;
  }
  if (m_BloomBlurTexture.view != VK_NULL_HANDLE) {
    vkDestroyImageView(m_Device, m_BloomBlurTexture.view, nullptr);
    vkDestroyImage(m_Device, m_BloomBlurTexture.image, nullptr);
    vkFreeMemory(m_Device, m_BloomBlurTexture.memory, nullptr);
    m_BloomBlurTexture.view = VK_NULL_HANDLE;
  }
  if (m_SSAONoise.view != VK_NULL_HANDLE) {
    vkDestroyImageView(m_Device, m_SSAONoise.view, nullptr);
    vkDestroyImage(m_Device, m_SSAONoise.image, nullptr);
    vkFreeMemory(m_Device, m_SSAONoise.memory, nullptr);
    m_SSAONoise.view = VK_NULL_HANDLE;
  }

  // 3. 销毁 帧缓冲
  for (auto fb : m_CompositionFramebuffers) {
    if (fb != VK_NULL_HANDLE)
      vkDestroyFramebuffer(m_Device, fb, nullptr);
  }
  m_CompositionFramebuffers.clear();

  if (m_BloomBrightFramebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(m_Device, m_BloomBrightFramebuffer, nullptr);
    m_BloomBrightFramebuffer = VK_NULL_HANDLE;
  }
  if (m_BloomBlurFramebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(m_Device, m_BloomBlurFramebuffer, nullptr);
    m_BloomBlurFramebuffer = VK_NULL_HANDLE;
  }
  for (auto fb : g_ForwardFramebuffers) {
    if (fb != VK_NULL_HANDLE)
      vkDestroyFramebuffer(m_Device, fb, nullptr);
  }
  g_ForwardFramebuffers.clear();

  // 4. 销毁 管线与管线布局
  auto destroyPipeline = [](VkDevice dev, VkPipeline &p) {
    if (p != VK_NULL_HANDLE) {
      vkDestroyPipeline(dev, p, nullptr);
      p = VK_NULL_HANDLE;
    }
  };
  auto destroyPipelineLayout = [](VkDevice dev, VkPipelineLayout &pl) {
    if (pl != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(dev, pl, nullptr);
      pl = VK_NULL_HANDLE;
    }
  };

  destroyPipeline(m_Device, m_GeometryPipeline);
  destroyPipelineLayout(m_Device, m_GeometryPipelineLayout);
  destroyPipeline(m_Device, m_CompositionPipeline);
  destroyPipelineLayout(m_Device, m_CompositionPipelineLayout);
  destroyPipeline(m_Device, m_ForwardPipeline);
  destroyPipelineLayout(m_Device, m_ForwardPipelineLayout);
  destroyPipeline(m_Device, m_BloomThresholdPipeline);
  destroyPipeline(m_Device, m_BloomBlurPipeline);
  destroyPipelineLayout(m_Device, m_BloomPipelineLayout);
  destroyPipeline(m_Device, m_PostProcessPipeline);
  destroyPipelineLayout(m_Device, m_PostProcessPipelineLayout);

  // 5. 销毁 渲染通道 (m_GBufferRenderPass is destroyed by m_GBuffer.Destroy())
  vkDestroyRenderPass(m_Device, m_CompositionRenderPass, nullptr);
  m_CompositionRenderPass = VK_NULL_HANDLE;
  vkDestroyRenderPass(m_Device, m_ForwardRenderPass, nullptr);
  m_ForwardRenderPass = VK_NULL_HANDLE;
  vkDestroyRenderPass(m_Device, m_BloomRenderPass, nullptr);
  m_BloomRenderPass = VK_NULL_HANDLE;
  vkDestroyRenderPass(m_Device, m_PostProcessRenderPass, nullptr);
  m_PostProcessRenderPass = VK_NULL_HANDLE;

  // 6. 采样器与缓冲区
  if (m_GBufferSampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_Device, m_GBufferSampler, nullptr);
    m_GBufferSampler = VK_NULL_HANDLE;
  }
  if (m_SSAOKernelBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(m_Device, m_SSAOKernelBuffer, nullptr);
    vkFreeMemory(m_Device, m_SSAOKernelMemory, nullptr);
    m_SSAOKernelBuffer = VK_NULL_HANDLE;
  }
  if (m_IndexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
    vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);
    m_IndexBuffer = VK_NULL_HANDLE;
  }
  if (m_VertexBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
    vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
    m_VertexBuffer = VK_NULL_HANDLE;
  }

  for (size_t i = 0; i < m_UniformBuffers.size(); i++) {
    vkDestroyBuffer(m_Device, m_UniformBuffers[i], nullptr);
    vkFreeMemory(m_Device, m_UniformBuffersMemory[i], nullptr);
  }
  for (size_t i = 0; i < m_LightUniformBuffers.size(); i++) {
    vkDestroyBuffer(m_Device, m_LightUniformBuffers[i], nullptr);
    vkFreeMemory(m_Device, m_LightUniformBuffersMemory[i], nullptr);
  }
  for (size_t i = 0; i < m_PointLightUniformBuffers.size(); i++) {
    vkDestroyBuffer(m_Device, m_PointLightUniformBuffers[i], nullptr);
    vkFreeMemory(m_Device, m_PointLightUniformBuffersMemory[i], nullptr);
  }
  for (size_t i = 0; i < m_CameraUniformBuffers.size(); i++) {
    vkDestroyBuffer(m_Device, m_CameraUniformBuffers[i], nullptr);
    vkFreeMemory(m_Device, m_CameraUniformBuffersMemory[i], nullptr);
  }

  // 7. 销毁 描述符池与布局
  vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
  m_DescriptorPool = VK_NULL_HANDLE;

  vkDestroyDescriptorSetLayout(m_Device, m_GeometryDescriptorSetLayout,
                               nullptr);
  vkDestroyDescriptorSetLayout(
      m_Device, m_CompositionGBufferDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(m_Device, m_CompositionLightDescriptorSetLayout,
                               nullptr);
  vkDestroyDescriptorSetLayout(m_Device, m_PostProcessDescriptorSetLayout,
                               nullptr);
  vkDestroyDescriptorSetLayout(m_Device, m_SingleTextureDescriptorSetLayout,
                               nullptr);
  if (m_ForwardDescriptorSetLayout != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(m_Device, m_ForwardDescriptorSetLayout,
                                 nullptr);

  // 8. 清理交换链
  CleanupSwapchain();

  // 9. 销毁 同步对象
  for (size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++) {
    vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
    vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
  }

  // 10. 销毁 命令池与设备、实例
  vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
  vkDestroyDevice(m_Device, nullptr);
  DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
  vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
  vkDestroyInstance(m_Instance, nullptr);

  LOG_I("RenderCore Shutdown cleanly.");
}

// 创建vulkan实例
void RenderCore::CreateInstance() {
  VkApplicationInfo appInfo{}; /*花括号 {} 是 C++ 的值初始化语法（C++11+），
   会将结构体的所有成员默认初始化为「零值」（数值成员为 0，指针成员为
   nullptr）， 避免未初始化的垃圾值导致 Vulkan 驱动报错。*/
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "NeuVulkanRender";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "NeuEngine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_4;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  // SDL Extensions
  uint32_t sdlExtensionCount = 0;
  const char *const *sdlExtensions =
      SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
  std::vector<const char *> extensions(sdlExtensions,
                                       sdlExtensions + sdlExtensionCount);

  // Add debug utils, mostly for validation layers
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  // Validation layers
  const std::vector<const char *> validationLayers = {
      "VK_LAYER_KHRONOS_validation"};

#ifdef DEBUG
  createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
  createInfo.ppEnabledLayerNames = validationLayers.data();

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  PopulateDebugMessengerCreateInfo(debugCreateInfo);
  createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
#else
  createInfo.enabledLayerCount = 0;
  createInfo.pNext = nullptr;
#endif

  if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
    LOG_E("Failed to create Vulkan Instance!");
    throw std::runtime_error("failed to create instance!");
  }
}

void RenderCore::CreateSurface() {
  if (!SDL_Vulkan_CreateSurface(Window::GetNativeWindow(), m_Instance, nullptr,
                                &m_Surface)) {
    LOG_E("Failed to create Window Surface!");
    throw std::runtime_error("failed to create window surface!");
  }
}

void RenderCore::PickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

  // Simple picker: first discrete GPU or just the first one
  for (
      const auto &device :
      devices) { // 遍历所有已检测到的物理设备（devices是VkPhysicalDevice数组/容器）
    VkPhysicalDeviceProperties
        deviceProperties; // 存储设备的属性信息（类型、名称、性能等）
    vkGetPhysicalDeviceProperties(
        device, &deviceProperties); // 调用Vulkan API，获取当前设备的属性

    // 判断设备类型是否为“独立显卡”
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      m_PhysicalDevice = device; // 选中该独立显卡
      // 声明设备属性结构体（VkPhysicalDeviceProperties）
      VkPhysicalDeviceProperties deviceProps;

      // 调用 Vulkan API 获取设备属性
      // 参数1：目标物理设备（m_PhysicalDevice）
      // 参数2：输出参数，接收设备属性的结构体指针
      vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProps);
      LOG_I("Running vulkan on {0}", deviceProps.deviceName);
      break; // 找到目标，退出循环（不再找其他设备）
    }
  }

  // 若循环结束后仍未选中设备（m_PhysicalDevice还是空句柄），说明没有独立显卡
  if (m_PhysicalDevice == VK_NULL_HANDLE) {
    m_PhysicalDevice =
        devices[0]; // 退选第一个检测到的设备（可能是集成显卡、CPU等）
  }
}
/**
 * @brief 创建Vulkan逻辑设备（Logical Device）
 *
 * 逻辑设备是Vulkan应用与物理设备（GPU）交互的核心接口，负责管理GPU队列、启用扩展和功能。
 * 该函数主要完成以下工作：
 * 1. 查询物理设备支持的队列族属性
 * 2. 查找支持图形渲染和表面呈现的队列族
 * 3. 配置队列创建信息（含优先级设置）
 * 4. 指定设备所需扩展（如交换链扩展）
 * 5. 创建逻辑设备并获取对应的图形队列和呈现队列
 */
void RenderCore::CreateLogicalDevice() {
  // 1. 查询物理设备支持的队列族数量
  uint32_t queueFamilyCount = 0;
  // 第一次调用：仅获取队列族数量（第二个参数传入nullptr）
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount,
                                           nullptr);

  // 2. 分配内存并获取所有队列族的详细属性
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  // 第二次调用：填充队列族属性数组（包含队列类型、数量等信息）
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount,
                                           queueFamilies.data());

  // 存储找到的队列族索引：-1表示未找到
  int graphicsFamily = -1; // 图形队列族（支持VK_QUEUE_GRAPHICS_BIT）
  int presentFamily = -1;  // 呈现队列族（支持与表面交换图像）

  // 3. 遍历所有队列族，查找所需的队列族
  for (int i = 0; i < queueFamilies.size(); i++) {
    // 检查当前队列族是否支持图形操作（必备功能）
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphicsFamily = i; // 记录图形队列族索引
    }

    // 检查当前队列族是否支持表面呈现（与窗口系统交互必备）
    VkBool32 presentSupport = false;
    // 查询队列族i是否支持当前表面m_Surface的呈现操作
    vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface,
                                         &presentSupport);
    if (presentSupport) {
      presentFamily = i; // 记录呈现队列族索引
    }

    // 若同时找到图形队列族和呈现队列族，提前退出循环（无需继续查找）
    if (graphicsFamily != -1 && presentFamily != -1) {
      break;
    }
  }

  // 4. 配置队列创建信息（避免重复创建同一队列族的队列）
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  // 使用set自动去重：若图形队列族和呈现队列族是同一索引，仅保留一个
  std::set<uint32_t> uniqueQueueFamilies = {
      static_cast<uint32_t>(graphicsFamily),
      static_cast<uint32_t>(presentFamily)};

  float queuePriority = 1.0f; // 队列优先级（0.0~1.0，1.0为最高）
  // 为每个唯一的队列族创建队列配置
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo
        queueCreateInfo{}; // 队列创建信息结构体（初始化清零）
    queueCreateInfo.sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; // 结构体类型标识
    queueCreateInfo.queueFamilyIndex = queueFamily; // 目标队列族索引
    queueCreateInfo.queueCount = 1; // 每个队列族创建1个队列（足够基础使用）
    queueCreateInfo.pQueuePriorities = &queuePriority; // 队列优先级指针
    queueCreateInfos.push_back(queueCreateInfo);       // 添加到配置列表
  }

  // 5. 配置设备启用的物理功能（此处使用默认配置，无额外启用功能）
  VkPhysicalDeviceFeatures deviceFeatures{}; // 所有功能默认禁用

  // 6. 构建逻辑设备创建信息结构体
  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; // 结构体类型标识
  // 队列创建信息数量和数据指针
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  // 设备启用的物理功能（此处为默认配置）
  createInfo.pEnabledFeatures = &deviceFeatures;

  // 7. 指定设备需要启用的扩展（此处仅启用交换链扩展，用于图像显示）
  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // 交换链扩展（KHR标准扩展）
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size()); // 扩展数量
  createInfo.ppEnabledExtensionNames =
      deviceExtensions.data(); // 扩展名称数组指针

  // 8. 创建逻辑设备
  if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) !=
      VK_SUCCESS) {
    // 创建失败时抛出异常（终止程序并提示错误）
    throw std::runtime_error("failed to create logical device!");
  }

  // 9. 获取创建好的队列句柄（后续用于提交渲染/呈现命令）
  // 从逻辑设备中获取图形队列（队列族索引graphicsFamily，队列索引0）
  vkGetDeviceQueue(m_Device, graphicsFamily, 0, &m_GraphicsQueue);
  // 从逻辑设备中获取呈现队列（队列族索引presentFamily，队列索引0）
  vkGetDeviceQueue(m_Device, presentFamily, 0, &m_PresentQueue);
}

void RenderCore::CreateSwapchain() {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface,
                                            &capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface,
                                       &formatCount, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface,
                                       &formatCount, formats.data());

  VkSurfaceFormatKHR surfaceFormat = formats[0];
  for (const auto &availableFormat : formats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surfaceFormat = availableFormat;
      break;
    }
  }

  VkExtent2D extent;
  if (capabilities.currentExtent.width != UINT32_MAX) {
    extent = capabilities.currentExtent;
  } else {
    int width, height;
    SDL_GetWindowSizeInPixels(Window::GetNativeWindow(), &width, &height);

    extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height =
        std::clamp(extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
  }

  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_Surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  // Assuming same queue family for simplicity. If different, need
  // SHARING_MODE_CONCURRENT.
  createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  createInfo.preTransform = capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode =
      VK_PRESENT_MODE_FIFO_KHR; // Guaranteed to be available
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
  m_SwapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount,
                          m_SwapchainImages.data());

  m_SwapchainImageFormat = surfaceFormat.format;
  m_SwapchainExtent = extent;
}

void RenderCore::CreateImageViews() {
  m_SwapchainImageViews.resize(m_SwapchainImages.size());
  for (size_t i = 0; i < m_SwapchainImages.size(); i++) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = m_SwapchainImages[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = m_SwapchainImageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device, &createInfo, nullptr,
                          &m_SwapchainImageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create image views!");
    }
  }
}

void RenderCore::CreateRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = m_SwapchainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void RenderCore::CreateFramebuffers() {
  m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());
  for (size_t i = 0; i < m_SwapchainImageViews.size(); i++) {
    VkImageView attachments[] = {m_SwapchainImageViews[i]};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_PostProcessRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_SwapchainExtent.width;
    framebufferInfo.height = m_SwapchainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr,
                            &m_SwapchainFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
  }
}

void RenderCore::CreateCommandPool() {
  // Find graphics queue family... (assuming we found it in
  // CreateLogicalDevice). For simplicity, re-query or store it. Storing it in
  // CreateLogicalDevice would be better but local vars. Re-query:
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount,
                                           queueFamilies.data());
  int graphicsFamily = -1;
  for (int i = 0; i < queueFamilies.size(); i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphicsFamily = i;
      break;
    }
  }

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = graphicsFamily;

  if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create command pool!");
  }
}

void RenderCore::CreateCommandBuffers() {
  m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_CommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

  if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void RenderCore::CreateSyncObjects() {
  m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr,
                          &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr,
                          &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) !=
            VK_SUCCESS) {
      throw std::runtime_error(
          "failed to create synchronization objects for a frame!");
    }
  }
}

void RenderCore::CreateDescriptorPool() {
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
  pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  if (vkCreateDescriptorPool(m_Device, &pool_info, nullptr,
                             &m_DescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

void RenderCore::InitImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
#ifdef IMGUI_HAS_DOCK
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
#endif

  ImGui::StyleColorsDark();

  ImGui_ImplSDL3_InitForVulkan(Window::GetNativeWindow());

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = m_Instance;
  init_info.PhysicalDevice = m_PhysicalDevice;
  init_info.Device = m_Device;
  init_info.QueueFamily = 0; // Assuming graphics queue family index is 0 if we
                             // didn't store it perfectly (TODO fix if needed)
  init_info.Queue = m_GraphicsQueue;
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPool = m_DescriptorPool;
  init_info.RenderPass = m_PostProcessRenderPass;
  init_info.Subpass = 0;
  init_info.MinImageCount = 2;
  init_info.ImageCount = m_SwapchainImages.size();
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = nullptr;

  // We need to pass the correct queue family index.
  // Re-querying again to be safe:
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount,
                                           queueFamilies.data());
  for (int i = 0; i < queueFamilies.size(); i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      init_info.QueueFamily = i;
      break;
    }
  }

  ImGui_ImplVulkan_Init(
      &init_info); // Using dynamic rendering not passed? Wait, passing
                   // RenderPass so it's normal rendering.

  // Initialize EditorGUI
  EditorGUI::Initialize();
}
// 验证层
void RenderCore::SetupDebugMessenger() {
#ifndef DEBUG
  return;
#endif

  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  PopulateDebugMessengerCreateInfo(createInfo);

  if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr,
                                   &m_DebugMessenger) != VK_SUCCESS) {
    throw std::runtime_error("failed to set up debug messenger!");
  }
}

void RenderCore::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                     uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  // ========== Pass 1: GBuffer Geometry Pass ==========
  {
    VkRenderPassBeginInfo gbufferPassInfo{};
    gbufferPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    gbufferPassInfo.renderPass = m_GBufferRenderPass;
    gbufferPassInfo.framebuffer = m_GBuffer.GetFramebuffer(m_CurrentFrame);
    gbufferPassInfo.renderArea.offset = {0, 0};
    gbufferPassInfo.renderArea.extent = m_SwapchainExtent;

    // 5 clear values: 4 color + 1 depth
    std::array<VkClearValue, 5> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Albedo
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Specular
    clearValues[2].color = {{0.5f, 0.5f, 1.0f, 0.0f}}; // Normal (default up)
    clearValues[3].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // ShadingID
    clearValues[4].depthStencil = {1.0f, 0};           // Depth

    gbufferPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    gbufferPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &gbufferPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    // Bind geometry pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_GeometryPipeline);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_SwapchainExtent.width);
    viewport.height = static_cast<float>(m_SwapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_SwapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Bind descriptor set (UBO)
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_GeometryPipelineLayout, 0, 1,
                            &m_GeometryDescriptorSets[m_CurrentFrame], 0,
                            nullptr);

    // Push constants structure (Must match shader layout)
    struct PushConstantData {
      glm::mat4 model;
      float metallic;
      float roughness;
      float shadingId;
      float emissiveIntensity;
    };

    // Render opaque objects
    for (const auto &obj : m_RenderObjects) {
      if (obj.material.IsTransparent())
        continue;

      PushConstantData pcData{};
      pcData.model = obj.modelMatrix;
      pcData.metallic = obj.material.metallic;
      pcData.roughness = obj.material.roughness;
      pcData.shadingId = obj.material.shadingId;
      pcData.emissiveIntensity = obj.material.emissiveIntensity;

      vkCmdPushConstants(commandBuffer, m_GeometryPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(PushConstantData), &pcData);

      VkBuffer vertexBuffers[] = {obj.vertexBuffer};
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
      vkCmdBindIndexBuffer(commandBuffer, obj.indexBuffer, 0,
                           VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(commandBuffer, obj.indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);
  }

  // ========== Pass 2: Composition Pass (Deferred Shading) ==========
  {
    VkRenderPassBeginInfo compositionPassInfo{};
    compositionPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    compositionPassInfo.renderPass = m_CompositionRenderPass;
    compositionPassInfo.framebuffer =
        m_CompositionFramebuffers[m_CurrentFrame]; // SCENE color
    compositionPassInfo.renderArea.offset = {0, 0};
    compositionPassInfo.renderArea.extent = m_SwapchainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    compositionPassInfo.clearValueCount = 1;
    compositionPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &compositionPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_CompositionPipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_CompositionPipelineLayout, 0, 1,
                            &m_CompositionGBufferDescriptorSets[m_CurrentFrame],
                            0, nullptr);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_CompositionPipelineLayout, 1, 1,
                            &m_CompositionLightDescriptorSets[m_CurrentFrame],
                            0, nullptr);

    glm::vec2 viewportSize =
        glm::vec2(m_SwapchainExtent.width, m_SwapchainExtent.height);
    vkCmdPushConstants(commandBuffer, m_CompositionPipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec2),
                       &viewportSize);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
  }

  // ========== Pass 3: Forward Pass (Transparent) ==========
  {
    VkRenderPassBeginInfo forwardPassInfo{};
    forwardPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    forwardPassInfo.renderPass = m_ForwardRenderPass;
    forwardPassInfo.framebuffer = g_ForwardFramebuffers[m_CurrentFrame];
    forwardPassInfo.renderArea.offset = {0, 0};
    forwardPassInfo.renderArea.extent = m_SwapchainExtent;
    forwardPassInfo.clearValueCount = 0; // Load Op

    vkCmdBeginRenderPass(commandBuffer, &forwardPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_ForwardPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_SwapchainExtent.width);
    viewport.height = static_cast<float>(m_SwapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_SwapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ForwardPipelineLayout,
        0, 1, &m_GeometryDescriptorSets[m_CurrentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ForwardPipelineLayout,
        1, 1, &m_CompositionLightDescriptorSets[m_CurrentFrame], 0, nullptr);

    struct ForwardPushConstants {
      glm::mat4 model;
      float metallic;
      float roughness;
      float alpha;
      float emissiveIntensity;
    };

    for (const auto &obj : m_RenderObjects) {
      if (!obj.material.IsTransparent())
        continue;

      ForwardPushConstants pcData{};
      pcData.model = obj.modelMatrix;
      pcData.metallic = obj.material.metallic;
      pcData.roughness = obj.material.roughness;
      pcData.alpha = obj.material.alpha;
      pcData.emissiveIntensity = obj.material.emissiveIntensity;

      vkCmdPushConstants(commandBuffer, m_ForwardPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(ForwardPushConstants), &pcData);

      VkBuffer vertexBuffers[] = {obj.vertexBuffer};
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
      vkCmdBindIndexBuffer(commandBuffer, obj.indexBuffer, 0,
                           VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(commandBuffer, obj.indexCount, 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);
  }

  // ========== Pass 3.5: Bloom Pass ==========
  if (m_PostProcessSettings.enableBloom) {
    VkRenderPassBeginInfo bloomPassInfo{};
    bloomPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    bloomPassInfo.renderPass = m_BloomRenderPass;
    bloomPassInfo.renderArea.offset = {0, 0};
    bloomPassInfo.renderArea.extent = {m_SwapchainExtent.width / 2,
                                       m_SwapchainExtent.height / 2};
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    bloomPassInfo.clearValueCount = 1;
    bloomPassInfo.pClearValues = &clearColor;

    // 1. Threshold (Scene -> Bright)
    bloomPassInfo.framebuffer = m_BloomBrightFramebuffer;
    vkCmdBeginRenderPass(commandBuffer, &bloomPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_BloomThresholdPipeline);

    VkViewport viewport{};
    viewport.width = (float)m_SwapchainExtent.width / 2.0f;
    viewport.height = (float)m_SwapchainExtent.height / 2.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = {m_SwapchainExtent.width / 2,
                      m_SwapchainExtent.height / 2};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BloomPipelineLayout,
        0, 1, &m_BloomThresholdDescriptorSets[m_CurrentFrame], 0, nullptr);

    struct {
      float threshold;
      float softThreshold;
      glm::vec2 pad;
    } dbParams;
    dbParams.threshold = m_PostProcessSettings.bloomThreshold;
    dbParams.softThreshold = 0.5f;
    vkCmdPushConstants(commandBuffer, m_BloomPipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(dbParams),
                       &dbParams);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    // 2. Blur Horizontal (Bright -> Blur)
    bloomPassInfo.framebuffer = m_BloomBlurFramebuffer;
    vkCmdBeginRenderPass(commandBuffer, &bloomPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_BloomBlurPipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BloomPipelineLayout,
        0, 1, &m_BloomBlurDescriptorSets[m_CurrentFrame * 2 + 0], 0, nullptr);

    struct BlurParams {
      glm::vec2 direction;
      glm::vec2 texelSize;
    } blurParams;
    blurParams.direction = {1.0f, 0.0f};
    blurParams.texelSize = {1.0f / (m_SwapchainExtent.width / 2.0f),
                            1.0f / (m_SwapchainExtent.height / 2.0f)};
    vkCmdPushConstants(commandBuffer, m_BloomPipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(blurParams),
                       &blurParams);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    // 3. Blur Vertical (Blur -> Bright)
    bloomPassInfo.framebuffer = m_BloomBrightFramebuffer;
    vkCmdBeginRenderPass(commandBuffer, &bloomPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_BloomBlurPipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BloomPipelineLayout,
        0, 1, &m_BloomBlurDescriptorSets[m_CurrentFrame * 2 + 1], 0, nullptr);

    blurParams.direction = {0.0f, 1.0f};
    vkCmdPushConstants(commandBuffer, m_BloomPipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(blurParams),
                       &blurParams);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
  }

  // ========== Pass 4: PostProcess Pass (Final Output) ==========
  {
    VkRenderPassBeginInfo ppPassInfo{};
    ppPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    ppPassInfo.renderPass = m_PostProcessRenderPass;
    ppPassInfo.framebuffer = m_SwapchainFramebuffers[imageIndex];
    ppPassInfo.renderArea.offset = {0, 0};
    ppPassInfo.renderArea.extent = m_SwapchainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    ppPassInfo.clearValueCount = 1;
    ppPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &ppPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_PostProcessPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_SwapchainExtent.width);
    viewport.height = static_cast<float>(m_SwapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_SwapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PostProcessPipelineLayout, 0, 1,
                            &m_PostProcessDescriptorSets[m_CurrentFrame], 0,
                            nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PostProcessPipelineLayout, 1, 1,
                            &g_PostProcessCameraDescriptorSets[m_CurrentFrame],
                            0, nullptr);

    vkCmdPushConstants(commandBuffer, m_PostProcessPipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(m_PostProcessSettings), &m_PostProcessSettings);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0); // Fullscreen triangle

    // ImGui on top
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    vkCmdEndRenderPass(commandBuffer);
  }

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

void RenderCore::DrawFrame() {
  vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE,
                  UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      m_Device, m_Swapchain, UINT64_MAX,
      m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // RecreateSwapchain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // NOTE: If we used per-image fences (tutorial style), we would wait for them
  // here. But since we use MAX_FRAMES_IN_FLIGHT fences and wait at the top,
  // it should be safe for non-swapchain resources.
  // For the semaphore reuse warning, we follow the advice to use separate
  // semaphores if possible, or ensure the presentation is done.

  vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

  // ImGui New Frame
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  // Draw UI
  neuGUI::Render(); // Call the UI render function

  ImGui::Render();

  vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame],
                       /*VkCommandBufferResetFlagBits*/ 0);

  // 处理输入（相机控制）
  ProcessInput();

  // Update uniform buffers (MVP matrices and light data)
  UpdateUniformBuffer(m_CurrentFrame);

  // Dynamic Scene Rendering: Collect objects from scene
  CollectSceneRenderables();

  // Record command buffer (Standard Vulkan draw calls)
  RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

  VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[m_CurrentFrame]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo,
                    m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapchains[] = {m_Swapchain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      m_FramebufferResized) {
    m_FramebufferResized = false;
    // RecreateSwapchain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void RenderCore::CleanupSwapchain() {
  for (auto framebuffer : m_SwapchainFramebuffers) {
    vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
  }

  for (auto imageView : m_SwapchainImageViews) {
    vkDestroyImageView(m_Device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
}

// ========== 辅助函数实现 ==========

std::vector<char> RenderCore::ReadShaderFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open shader file: " + filename);
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

VkShaderModule RenderCore::CreateShaderModule(const std::vector<char> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create shader module!");
  }

  return shaderModule;
}

uint32_t RenderCore::FindMemoryType(uint32_t typeFilter,
                                    VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("Failed to find suitable memory type!");
}

void RenderCore::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags properties,
                              VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate buffer memory!");
  }

  vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
}

void RenderCore::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                            VkDeviceSize size) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_CommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_GraphicsQueue);

  vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
}

// ========== GBuffer 系统实现 ==========

void RenderCore::CreateGBuffer() {
  m_GBuffer.Create(m_Device, m_PhysicalDevice, m_SwapchainExtent.width,
                   m_SwapchainExtent.height, MAX_FRAMES_IN_FLIGHT);
  LOG_I("GBuffer created successfully with {} frames", MAX_FRAMES_IN_FLIGHT);
}

void RenderCore::CreateGBufferRenderPass() {
  // 5 个附件: 4 个颜色 + 1 个深度
  std::array<VkAttachmentDescription, 5> attachments{};

  // GBuffer1: Albedo + MaterialFlags (SRGB)
  attachments[0].format = VK_FORMAT_R8G8B8A8_SRGB;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // GBuffer2-4: UNORM
  for (int i = 1; i <= 3; i++) {
    attachments[i].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  // Depth attachment
  attachments[4].format = VK_FORMAT_D32_SFLOAT;
  attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[4].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // Color attachment references
  std::array<VkAttachmentReference, 4> colorAttachmentRefs{};
  for (uint32_t i = 0; i < 4; i++) {
    colorAttachmentRefs[i].attachment = i;
    colorAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  // Depth attachment reference
  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 4;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // Subpass
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount =
      static_cast<uint32_t>(colorAttachmentRefs.size());
  subpass.pColorAttachments = colorAttachmentRefs.data();
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  // 子通道依赖：处理 GBuffer 写入与外部读取的同步
  std::array<VkSubpassDependency, 2> dependencies{};

  // 1. 外部 -> GBuffer 内容写入：确保之前的读取已完成
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  // 2. GBuffer 写入 -> 外部读取 (Composition Pass)：确保布局转换完成且写入可见
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  // Create render pass
  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr,
                         &m_GBufferRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create GBuffer render pass!");
  }

  // Create GBuffer framebuffers (Double Buffered)
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    std::vector<VkImageView> gbufferAttachments =
        m_GBuffer.GetColorAttachmentViews(i);
    gbufferAttachments.push_back(m_GBuffer.GetDepthView(i));

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_GBufferRenderPass;
    framebufferInfo.attachmentCount =
        static_cast<uint32_t>(gbufferAttachments.size());
    framebufferInfo.pAttachments = gbufferAttachments.data();
    framebufferInfo.width = m_SwapchainExtent.width;
    framebufferInfo.height = m_SwapchainExtent.height;
    framebufferInfo.layers = 1;

    VkFramebuffer fb;
    if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &fb) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create GBuffer framebuffer!");
    }
    m_GBuffer.SetFramebuffer(i, fb);
  }

  m_GBuffer.renderPass = m_GBufferRenderPass;
  LOG_I("GBuffer render pass and framebuffers created successfully");
}

void RenderCore::CreateCompositionRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR Format
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  colorAttachment.finalLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Continue to Forward Pass

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  // 依赖项：合成阶段的读操作必须等 GBuffer 阶段的写操作完成
  std::array<VkSubpassDependency, 2> dependencies{};

  // 1. 等待 GBuffer 写入
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  // 2. 合成完毕后的输出
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr,
                         &m_CompositionRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create composition render pass!");
  }

  // Create composition framebuffers (Double Buffered)
  m_CompositionFramebuffers.resize(MAX_FRAMES_IN_FLIGHT);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkImageView attachments[] = {m_SceneColor[i].view};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_CompositionRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_SwapchainExtent.width;
    framebufferInfo.height = m_SwapchainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr,
                            &m_CompositionFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create composition framebuffer!");
    }
  }

  LOG_I("Composition render pass and framebuffers created successfully");
}

void RenderCore::CreateDescriptorSetLayouts() {
  // Geometry pass: UBO for MVP matrices
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  uboLayoutBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &uboLayoutBinding;

  if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr,
                                  &m_GeometryDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create geometry descriptor set layout!");
  }

  // Composition pass: GBuffer samplers (5 total: 4 color + 1 depth)
  std::array<VkDescriptorSetLayoutBinding, 5> gbufferBindings{};
  for (uint32_t i = 0; i < 5; i++) {
    gbufferBindings[i].binding = i;
    gbufferBindings[i].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    gbufferBindings[i].descriptorCount = 1;
    gbufferBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    gbufferBindings[i].pImmutableSamplers = nullptr;
  }

  VkDescriptorSetLayoutCreateInfo gbufferLayoutInfo{};
  gbufferLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  gbufferLayoutInfo.bindingCount =
      static_cast<uint32_t>(gbufferBindings.size());
  gbufferLayoutInfo.pBindings = gbufferBindings.data();

  if (vkCreateDescriptorSetLayout(m_Device, &gbufferLayoutInfo, nullptr,
                                  &m_CompositionGBufferDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create composition GBuffer descriptor set layout!");
  }

  // Single Texture Descriptor Set Layout (For Bloom)
  VkDescriptorSetLayoutBinding singleTextureBinding{};
  singleTextureBinding.binding = 0;
  singleTextureBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  singleTextureBinding.descriptorCount = 1;
  singleTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  singleTextureBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo singleTextureLayoutInfo{};
  singleTextureLayoutInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  singleTextureLayoutInfo.bindingCount = 1;
  singleTextureLayoutInfo.pBindings = &singleTextureBinding;

  if (vkCreateDescriptorSetLayout(m_Device, &singleTextureLayoutInfo, nullptr,
                                  &m_SingleTextureDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create single texture descriptor set layout!");
  }

  // Post-Process Descriptor Set Layout (Set 0)
  // Binding 0: SceneColor
  // Binding 1: DepthBuffer
  // Binding 2: NormalBuffer
  // Binding 3: BloomTexture
  // Binding 4: SSAONoise
  // Binding 5: SSAOKernel
  std::vector<VkDescriptorSetLayoutBinding> ppBindings;
  for (int i = 0; i < 6; i++) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = i;
    binding.descriptorType = (i == 5)
                                 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                 : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount =
        (i == 5) ? 64 : 1; // 64 vec4 samples for kernel? No, usually uniform
                           // buffer array or just 1 UBO. Shader: uniform
                           // SSAOKernel { vec4 samples[64]; } -> 1 UBO.
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    ppBindings.push_back(binding);
  }

  VkDescriptorSetLayoutCreateInfo ppLayoutInfo{};
  ppLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  ppLayoutInfo.bindingCount = static_cast<uint32_t>(ppBindings.size());
  ppLayoutInfo.pBindings = ppBindings.data();

  if (vkCreateDescriptorSetLayout(m_Device, &ppLayoutInfo, nullptr,
                                  &m_PostProcessDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create post-process descriptor set layout!");
  }

  // Composition pass: Light data UBO (binding 0: directional, binding 1: point
  // lights)
  std::array<VkDescriptorSetLayoutBinding, 2> lightBindings{};

  // Binding 0: Directional light
  lightBindings[0].binding = 0;
  lightBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  lightBindings[0].descriptorCount = 1;
  lightBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  lightBindings[0].pImmutableSamplers = nullptr;

  // Binding 1: Point lights array
  lightBindings[1].binding = 1;
  lightBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  lightBindings[1].descriptorCount = 1;
  lightBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  lightBindings[1].pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo lightLayoutInfo{};
  lightLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  lightLayoutInfo.bindingCount = static_cast<uint32_t>(lightBindings.size());
  lightLayoutInfo.pBindings = lightBindings.data();

  if (vkCreateDescriptorSetLayout(m_Device, &lightLayoutInfo, nullptr,
                                  &m_CompositionLightDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create composition light descriptor set layout!");
  }

  LOG_I("Descriptor set layouts created successfully");
}

void RenderCore::CreateGeometryPipeline() {
  auto vertShaderCode =
      ReadShaderFile("resource/shaders/compiled/gbuffer.vert.spv");
  auto fragShaderCode =
      ReadShaderFile("resource/shaders/compiled/gbuffer.frag.spv");

  VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  // Vertex input
  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_SwapchainExtent.width;
  viewport.height = (float)m_SwapchainExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_SwapchainExtent;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  // 4 color blend attachments for MRT
  std::array<VkPipelineColorBlendAttachmentState, 4> colorBlendAttachments{};
  for (auto &attachment : colorBlendAttachments) {
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    attachment.blendEnable = VK_FALSE;
  }

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount =
      static_cast<uint32_t>(colorBlendAttachments.size());
  colorBlending.pAttachments = colorBlendAttachments.data();

  // Push constants for Model Matrix (Vertex) + Material data (Fragment)
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size =
      sizeof(glm::mat4) +
      sizeof(float) * 4; // 64 (Mat4) + 16 (4 floats) = 80 bytes

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_GeometryDescriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr,
                             &m_GeometryPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create geometry pipeline layout!");
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();
  pipelineInfo.pDynamicState = &dynamicState;

  pipelineInfo.layout = m_GeometryPipelineLayout;
  pipelineInfo.renderPass = m_GBufferRenderPass;
  pipelineInfo.subpass = 0;

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &m_GeometryPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create geometry pipeline!");
  }

  vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
  vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);

  LOG_I("Geometry pipeline created successfully");
}

void RenderCore::CreateCompositionPipeline() {
  auto vertShaderCode =
      ReadShaderFile("resource/shaders/compiled/composition.vert.spv");
  auto fragShaderCode =
      ReadShaderFile("resource/shaders/compiled/composition.frag.spv");

  VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  // No vertex input - fullscreen triangle generated in vertex shader
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_SwapchainExtent.width;
  viewport.height = (float)m_SwapchainExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_SwapchainExtent;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // Push constants for viewport size
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(float) * 2; // viewportSize

  std::array<VkDescriptorSetLayout, 2> setLayouts = {
      m_CompositionGBufferDescriptorSetLayout,
      m_CompositionLightDescriptorSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr,
                             &m_CompositionPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create composition pipeline layout!");
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();
  pipelineInfo.pDynamicState = &dynamicState;

  pipelineInfo.layout = m_CompositionPipelineLayout;
  pipelineInfo.renderPass = m_CompositionRenderPass;
  pipelineInfo.subpass = 0;

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr,
                                &m_CompositionPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create composition pipeline!");
  }

  vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
  vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);

  LOG_I("Composition pipeline created successfully");
}

void RenderCore::CreateSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_GBufferSampler) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create GBuffer sampler!");
  }
}

void RenderCore::CreateUniformBuffers() {
  VkDeviceSize bufferSize = sizeof(UniformBufferObject);

  m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  m_UniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
  m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_UniformBuffers[i], m_UniformBuffersMemory[i]);

    vkMapMemory(m_Device, m_UniformBuffersMemory[i], 0, bufferSize, 0,
                &m_UniformBuffersMapped[i]);
  }

  // Create light uniform buffers
  VkDeviceSize lightBufferSize = sizeof(LightDataUBO);

  m_LightUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  m_LightUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
  m_LightUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    CreateBuffer(lightBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_LightUniformBuffers[i], m_LightUniformBuffersMemory[i]);

    vkMapMemory(m_Device, m_LightUniformBuffersMemory[i], 0, lightBufferSize, 0,
                &m_LightUniformBuffersMapped[i]);
  }

  // Create point light uniform buffers
  VkDeviceSize pointLightBufferSize = sizeof(PointLightsUBO);

  m_PointLightUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  m_PointLightUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
  m_PointLightUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    CreateBuffer(pointLightBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_PointLightUniformBuffers[i],
                 m_PointLightUniformBuffersMemory[i]);

    vkMapMemory(m_Device, m_PointLightUniformBuffersMemory[i], 0,
                pointLightBufferSize, 0, &m_PointLightUniformBuffersMapped[i]);
  }

  // Create Camera Uniform Buffers (for PostProcess)
  // Size: 4 mat4 (256) + vec2 (8) + 2 floats (8) = 272 bytes. Aligned to 16?
  // Yes.
  VkDeviceSize cameraBufferSize = sizeof(glm::mat4) * 4 + sizeof(float) * 4;

  m_CameraUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  m_CameraUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
  m_CameraUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    CreateBuffer(cameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_CameraUniformBuffers[i], m_CameraUniformBuffersMemory[i]);
    vkMapMemory(m_Device, m_CameraUniformBuffersMemory[i], 0, cameraBufferSize,
                0, &m_CameraUniformBuffersMapped[i]);
  }

  LOG_I("Uniform buffers created successfully");
}

void RenderCore::CreateDescriptorSets() {
  // Allocate geometry descriptor sets
  std::vector<VkDescriptorSetLayout> geometryLayouts(
      MAX_FRAMES_IN_FLIGHT, m_GeometryDescriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_DescriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = geometryLayouts.data();

  m_GeometryDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_Device, &allocInfo,
                               m_GeometryDescriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate geometry descriptor sets!");
  }

  // Update geometry descriptor sets
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_UniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_GeometryDescriptorSets[i];
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
  }

  // Allocate composition GBuffer descriptor sets
  std::vector<VkDescriptorSetLayout> gbufferLayouts(
      MAX_FRAMES_IN_FLIGHT, m_CompositionGBufferDescriptorSetLayout);

  allocInfo.pSetLayouts = gbufferLayouts.data();
  m_CompositionGBufferDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_Device, &allocInfo,
                               m_CompositionGBufferDescriptorSets.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to allocate composition GBuffer descriptor sets!");
  }

  // Update composition GBuffer descriptor sets
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    std::array<VkDescriptorImageInfo, 5> imageInfos{};

    // GBuffer1
    imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[0].imageView = m_GBuffer.GetAlbedoFlags(i).view;
    imageInfos[0].sampler = m_GBuffer.GetAlbedoFlags(i).sampler;

    // GBuffer2
    imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[1].imageView = m_GBuffer.GetSpecularOcclusion(i).view;
    imageInfos[1].sampler = m_GBuffer.GetSpecularOcclusion(i).sampler;

    // GBuffer3
    imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[2].imageView = m_GBuffer.GetNormalSmoothness(i).view;
    imageInfos[2].sampler = m_GBuffer.GetNormalSmoothness(i).sampler;

    // GBuffer4
    imageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[3].imageView = m_GBuffer.GetShadingEmissive(i).view;
    imageInfos[3].sampler = m_GBuffer.GetShadingEmissive(i).sampler;

    // Depth
    imageInfos[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[4].imageView = m_GBuffer.GetDepth(i).view;
    imageInfos[4].sampler = m_GBuffer.GetDepth(i).sampler;

    std::array<VkWriteDescriptorSet, 5> descriptorWrites{};
    for (uint32_t j = 0; j < 5; j++) {
      descriptorWrites[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[j].dstSet = m_CompositionGBufferDescriptorSets[i];
      descriptorWrites[j].dstBinding = j;
      descriptorWrites[j].dstArrayElement = 0;
      descriptorWrites[j].descriptorType =
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[j].descriptorCount = 1;
      descriptorWrites[j].pImageInfo = &imageInfos[j];
    }

    vkUpdateDescriptorSets(m_Device,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }

  // Allocate composition light descriptor sets
  std::vector<VkDescriptorSetLayout> lightLayouts(
      MAX_FRAMES_IN_FLIGHT, m_CompositionLightDescriptorSetLayout);

  allocInfo.pSetLayouts = lightLayouts.data();
  m_CompositionLightDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_Device, &allocInfo,
                               m_CompositionLightDescriptorSets.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to allocate composition light descriptor sets!");
  }

  // Update composition light descriptor sets
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    // Directional light buffer info (binding 0)
    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = m_LightUniformBuffers[i];
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = sizeof(LightDataUBO);

    // Point lights buffer info (binding 1)
    VkDescriptorBufferInfo pointLightBufferInfo{};
    pointLightBufferInfo.buffer = m_PointLightUniformBuffers[i];
    pointLightBufferInfo.offset = 0;
    pointLightBufferInfo.range = sizeof(PointLightsUBO);

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    // Binding 0: Directional light
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_CompositionLightDescriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &lightBufferInfo;

    // Binding 1: Point lights
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_CompositionLightDescriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &pointLightBufferInfo;

    vkUpdateDescriptorSets(m_Device,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }

  LOG_I("Descriptor sets created successfully");

  // Allocate Bloom descriptor sets
  std::vector<VkDescriptorSetLayout> bloomLayouts(
      MAX_FRAMES_IN_FLIGHT, m_SingleTextureDescriptorSetLayout);

  // Threshold Sets
  allocInfo.pSetLayouts = bloomLayouts.data();
  m_BloomThresholdDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_Device, &allocInfo,
                               m_BloomThresholdDescriptorSets.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to allocate bloom threshold descriptor sets!");
  }

  // Blur Sets (2 sets per frame: one for H, one for V)
  std::vector<VkDescriptorSetLayout> blurLayouts(
      MAX_FRAMES_IN_FLIGHT * 2, m_SingleTextureDescriptorSetLayout);
  allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT * 2;
  allocInfo.pSetLayouts = blurLayouts.data();

  m_BloomBlurDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT * 2);
  if (vkAllocateDescriptorSets(m_Device, &allocInfo,
                               m_BloomBlurDescriptorSets.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate bloom blur descriptor sets!");
  }

  // Update Bloom sets
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    // Threshold Set: Input SceneColor
    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    sceneInfo.imageView = m_SceneColor[i].view;
    sceneInfo.sampler = m_GBufferSampler;

    VkWriteDescriptorSet sceneWrite{};
    sceneWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sceneWrite.dstSet = m_BloomThresholdDescriptorSets[i];
    sceneWrite.dstBinding = 0;
    sceneWrite.dstArrayElement = 0;
    sceneWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sceneWrite.descriptorCount = 1;
    sceneWrite.pImageInfo = &sceneInfo;

    vkUpdateDescriptorSets(m_Device, 1, &sceneWrite, 0, nullptr);

    // Blur Set 0 (Horizontal): Input Bright
    VkDescriptorImageInfo brightInfo{};
    brightInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    brightInfo.imageView = m_BloomBrightTexture.view;
    brightInfo.sampler = m_GBufferSampler;

    VkWriteDescriptorSet brightWrite{};
    brightWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    brightWrite.dstSet = m_BloomBlurDescriptorSets[i * 2 + 0];
    brightWrite.dstBinding = 0;
    brightWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    brightWrite.descriptorCount = 1;
    brightWrite.pImageInfo = &brightInfo;

    vkUpdateDescriptorSets(m_Device, 1, &brightWrite, 0, nullptr);

    // Blur Set 1 (Vertical): Input Blur
    VkDescriptorImageInfo blurInfo{};
    blurInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    blurInfo.imageView = m_BloomBlurTexture.view;
    blurInfo.sampler = m_GBufferSampler;

    VkWriteDescriptorSet blurWrite{};
    blurWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    blurWrite.dstSet = m_BloomBlurDescriptorSets[i * 2 + 1];
    blurWrite.dstBinding = 0;
    blurWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    blurWrite.descriptorCount = 1;
    blurWrite.pImageInfo = &blurInfo;

    vkUpdateDescriptorSets(m_Device, 1, &blurWrite, 0, nullptr);
  }
}

void RenderCore::UpdateUniformBuffer(uint32_t currentImage) {
  // 使用相机获取视图和投影矩阵
  float aspectRatio =
      (float)m_SwapchainExtent.width / (float)m_SwapchainExtent.height;

  static auto startTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  UniformBufferObject ubo{};
  // 使用相机的视图和投影矩阵
  ubo.view = m_Camera.GetViewMatrix();
  ubo.proj = m_Camera.GetProjectionMatrix(aspectRatio);

  memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));

  // Update light data (directional)
  LightDataUBO lightData{};
  lightData.lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
  lightData.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
  lightData.viewPos = m_Camera.GetPosition(); // 使用相机位置

  memcpy(m_LightUniformBuffersMapped[currentImage], &lightData,
         sizeof(lightData));

  // Update point lights data by traversing scene
  PointLightsUBO pointLightsData{};
  pointLightsData.count = 0;

  // Get current scene from EditorGUI
  auto currentScene = EditorGUI::GetCurrentScene();
  if (currentScene) {
    currentScene->TraverseNodes([&pointLightsData](Node *node) {
      // Check if this is an active PointLightNode
      if (node->IsActive() && node->GetNodeType() == "PointLightNode") {
        auto *pointLight = static_cast<PointLightNode *>(node);

        if (pointLightsData.count < MAX_POINT_LIGHTS) {
          uint32_t idx = pointLightsData.count;

          // Use position from the node (local for now, world transform TODO)
          pointLightsData.lights[idx].position = pointLight->GetPosition();
          pointLightsData.lights[idx].radius = pointLight->GetRadius();
          pointLightsData.lights[idx].color = pointLight->GetColor();
          pointLightsData.lights[idx].intensity = pointLight->GetIntensity();

          pointLightsData.count++;
        }
      }
    });
  }

  memcpy(m_PointLightUniformBuffersMapped[currentImage], &pointLightsData,
         sizeof(pointLightsData));

  // Update Camera UBO (PostProcess)
  struct CameraDataUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 invView;
    glm::mat4 invProj;
    glm::vec2 viewportSize;
    float nearPlane;
    float farPlane;
  } cameraData{};

  cameraData.view = ubo.view;
  cameraData.proj = ubo.proj;
  cameraData.invView = glm::inverse(ubo.view);
  cameraData.invProj = glm::inverse(ubo.proj);
  cameraData.viewportSize =
      glm::vec2(m_SwapchainExtent.width, m_SwapchainExtent.height);
  cameraData.nearPlane = 0.1f;
  cameraData.farPlane = 100.0f; // Assuming standard range

  memcpy(m_CameraUniformBuffersMapped[currentImage], &cameraData,
         sizeof(cameraData));
}

void RenderCore::ProcessInput() {
  // 计算 delta time
  float currentTime = SDL_GetTicks() / 1000.0f;
  m_DeltaTime = currentTime - m_LastFrameTime;
  m_LastFrameTime = currentTime;

  // 检查是否按下右键启用相机控制
  float mouseXf, mouseYf;
  Uint32 mouseButtons = SDL_GetMouseState(&mouseXf, &mouseYf);

  bool rightMousePressed = (mouseButtons & SDL_BUTTON_RMASK) != 0;

  // 仅当 ImGui 没有捕获鼠标时才处理相机控制
  if (ImGui::GetIO().WantCaptureMouse) {
    m_CameraControlEnabled = false;
    m_FirstMouse = true;
    return;
  }

  if (rightMousePressed) {
    if (!m_CameraControlEnabled) {
      m_CameraControlEnabled = true;
      m_FirstMouse = true;
      SDL_SetWindowRelativeMouseMode(Window::GetNativeWindow(), true);
    }

    // 使用相对鼠标模式时获取相对移动
    float relX, relY;
    SDL_GetRelativeMouseState(&relX, &relY);
    if (!m_FirstMouse) {
      m_Camera.ProcessMouseMovement(relX, -relY);
    }
    m_FirstMouse = false;

    // 处理键盘输入
    const bool *keyState = SDL_GetKeyboardState(nullptr);

    if (keyState[SDL_SCANCODE_W]) {
      m_Camera.ProcessKeyboard(Camera::Movement::FORWARD, m_DeltaTime);
    }
    if (keyState[SDL_SCANCODE_S]) {
      m_Camera.ProcessKeyboard(Camera::Movement::BACKWARD, m_DeltaTime);
    }
    if (keyState[SDL_SCANCODE_A]) {
      m_Camera.ProcessKeyboard(Camera::Movement::LEFT, m_DeltaTime);
    }
    if (keyState[SDL_SCANCODE_D]) {
      m_Camera.ProcessKeyboard(Camera::Movement::RIGHT, m_DeltaTime);
    }
    if (keyState[SDL_SCANCODE_SPACE]) {
      m_Camera.ProcessKeyboard(Camera::Movement::UP, m_DeltaTime);
    }
    if (keyState[SDL_SCANCODE_LCTRL] || keyState[SDL_SCANCODE_LSHIFT]) {
      m_Camera.ProcessKeyboard(Camera::Movement::DOWN, m_DeltaTime);
    }
    if (keyState[SDL_SCANCODE_R]) {
      m_Camera.Reset();
    }
  } else {
    if (m_CameraControlEnabled) {
      m_CameraControlEnabled = false;
      SDL_SetWindowRelativeMouseMode(Window::GetNativeWindow(), false);
    }
  }
}

void RenderCore::SetGeometryData(const std::vector<Vertex> &vertices,
                                 const std::vector<uint32_t> &indices) {
  m_CustomVertices = vertices;
  m_CustomIndices = indices;
  m_UseCustomGeometry = true;
  LOG_I("Custom geometry data set: {} vertices, {} indices ({} triangles)",
        vertices.size(), indices.size(), indices.size() / 3);
}

// ========== 前向渲染管线实现 ==========

void RenderCore::CreateForwardRenderPass() {
  // 前向渲染使用场景HDR颜色缓冲作为渲染目标
  // 假设composition pass已经渲染到了m_SceneColor
  // 前向渲染在其上叠加透明物体

  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR格式
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // 保留之前的内容
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = VK_FORMAT_D32_SFLOAT;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // 使用GBuffer的深度
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  std::array<VkAttachmentDescription, 2> attachments = {colorAttachment,
                                                        depthAttachment};

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr,
                         &m_ForwardRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create forward render pass!");
  }

  // Create Forward Framebuffers (Double Buffered)
  g_ForwardFramebuffers.resize(MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    std::array<VkImageView, 2> fbAttachments = {m_SceneColor[i].view,
                                                m_GBuffer.GetDepthView(i)};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_ForwardRenderPass;
    framebufferInfo.attachmentCount =
        static_cast<uint32_t>(fbAttachments.size());
    framebufferInfo.pAttachments = fbAttachments.data();
    framebufferInfo.width = m_SwapchainExtent.width;
    framebufferInfo.height = m_SwapchainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr,
                            &g_ForwardFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create forward framebuffer!");
    }
  }

  LOG_I("Forward render pass and framebuffers created successfully");
}

void RenderCore::CreateForwardPipeline() {
  auto vertShaderCode =
      ReadShaderFile("resource/shaders/compiled/forward.vert.spv");
  auto fragShaderCode =
      ReadShaderFile("resource/shaders/compiled/forward.frag.spv");

  VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  // 顶点输入
  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE; // 双面渲染用于透明物体
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // 深度测试开启，深度写入关闭（透明物体）
  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_FALSE; // 不写入深度
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  // Alpha混合
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  // Push constants for Model (Vertex) + Material (Fragment)
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(glm::mat4) + sizeof(float) * 4; // 80 bytes

  // 使用现有的描述符布局
  VkDescriptorSetLayout setLayouts[] = {m_GeometryDescriptorSetLayout,
                                        m_CompositionLightDescriptorSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 2;
  pipelineLayoutInfo.pSetLayouts = setLayouts;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr,
                             &m_ForwardPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create forward pipeline layout!");
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = m_ForwardPipelineLayout;
  pipelineInfo.renderPass = m_ForwardRenderPass;
  pipelineInfo.subpass = 0;

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &m_ForwardPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create forward graphics pipeline!");
  }

  vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
  vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);

  LOG_I("Forward pipeline created successfully");
}

void RenderCore::SortTransparentObjects() {
  glm::vec3 cameraPos = m_Camera.GetPosition();

  // 1. Calculate distances for all objects
  for (auto &obj : m_RenderObjects) {
    glm::vec3 objectCenter = glm::vec3(obj.modelMatrix[3]);
    obj.distanceToCamera = glm::length(objectCenter - cameraPos);
  }

  // 2. Partition: Opaque objects first, Transparent objects second
  auto transparentStart = std::stable_partition(
      m_RenderObjects.begin(), m_RenderObjects.end(),
      [](const RenderObject &obj) { return !obj.material.IsTransparent(); });

  // 3. Sort Transparent objects (back-to-front)
  std::sort(transparentStart, m_RenderObjects.end(),
            [](const RenderObject &a, const RenderObject &b) {
              return a.distanceToCamera > b.distanceToCamera;
            });
}

// ========== 后处理管线实现 ==========

void RenderCore::CreateSceneRenderTarget() {
  // 创建HDR场景颜色缓冲 (Double Buffered)
  m_SceneColor.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_SwapchainExtent.width;
    imageInfo.extent.height = m_SwapchainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_SceneColor[i].image) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create scene color image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Device, m_SceneColor[i].image,
                                 &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(
        memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr,
                         &m_SceneColor[i].memory) != VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate scene color image memory!");
    }

    vkBindImageMemory(m_Device, m_SceneColor[i].image, m_SceneColor[i].memory,
                      0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_SceneColor[i].image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device, &viewInfo, nullptr,
                          &m_SceneColor[i].view) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create scene color image view!");
    }
    m_SceneColor[i].format = VK_FORMAT_R16G16B16A16_SFLOAT;
  }

  // Transition all SceneColor images to SHADER_READ_ONLY_OPTIMAL layout
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_CommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_SceneColor[i].image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
  }

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_GraphicsQueue);

  vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);

  LOG_I("Scene HDR render targets created: {}x{} (x{})",
        m_SwapchainExtent.width, m_SwapchainExtent.height,
        MAX_FRAMES_IN_FLIGHT);
}

void RenderCore::CreatePostProcessRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = m_SwapchainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  // 依赖项：后处理需要等之前的 HDR 颜色和 Bloom/SSAO 写入完成
  std::array<VkSubpassDependency, 2> dependencies{};

  // 1. 等待之前的所有写入 (FB, Bloom, GBuffer etc.)
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  // 2. 最终输出到交换链
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = 0;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr,
                         &m_PostProcessRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create post-process render pass!");
  }
  LOG_I("Post-process render pass created successfully");
}

void RenderCore::CreatePostProcessPipeline() {
  // 1. 加载后处理着色器 (顶点和片段)
  auto vertShaderCode =
      ReadShaderFile("resource/shaders/compiled/postprocess.vert.spv");
  auto fragShaderCode =
      ReadShaderFile("resource/shaders/compiled/postprocess.frag.spv");

  VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

  // 2. 配置着色器阶段
  VkPipelineShaderStageCreateInfo vertStageInfo{};
  vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStageInfo.module = vertShaderModule;
  vertStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragStageInfo{};
  fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStageInfo.module = fragShaderModule;
  fragStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo,
                                                    fragStageInfo};

  // 3. 顶点输入状态: 后处理通常在着色器内生成全屏三角形，不需要显式顶点缓冲区
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  // 4. 输入装配: 绘制三角形列表
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // 5. 初始视口和剪裁 (实际渲染时由动态状态覆盖)
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_SwapchainExtent.width;
  viewport.height = (float)m_SwapchainExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_SwapchainExtent;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  // 6. 光栅化: 禁用剔除以确保全屏覆盖
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  // 7. 多重采样: 禁用
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // 8. 深度测试: 后处理是在最后进行的，不需要深度测试
  VkPipelineDepthStencilStateCreateInfo depthState{};
  depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthState.depthTestEnable = VK_FALSE;
  depthState.depthWriteEnable = VK_FALSE;
  depthState.stencilTestEnable = VK_FALSE;

  // 9. 颜色混合: 直接覆盖
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // 10. 推送常量: 用于传递调节参数 (PostProcessSettings)
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(PostProcessSettings);

  // 11. 描述符布局: Set 0 (各种纹理/SSAO核), Set 1 (相机 UBO)
  VkDescriptorSetLayout setLayouts[] = {m_PostProcessDescriptorSetLayout,
                                        m_CompositionLightDescriptorSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 2;
  pipelineLayoutInfo.pSetLayouts = setLayouts;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr,
                             &m_PostProcessPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create post-process pipeline layout!");
  }

  // 12. 开启动态状态: 视口和剪裁
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthState;
  pipelineInfo.pColorBlendState = &colorBlending;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();
  pipelineInfo.pDynamicState = &dynamicState;

  pipelineInfo.layout = m_PostProcessPipelineLayout;
  pipelineInfo.renderPass = m_PostProcessRenderPass;
  pipelineInfo.subpass = 0;

  // 13. 创建最终管线
  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr,
                                &m_PostProcessPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create post-process pipeline!");
  }

  // 14. 清理: 创建完成后销毁着色器模块
  vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
  vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);

  LOG_I("Post-process pipeline created successfully");
}

void RenderCore::CreateSSAOResources() {
  // 1. 创建SSAO噪声纹理数据 (4x4)
  std::vector<glm::vec4> ssaoNoise;
  srand(static_cast<unsigned int>(time(nullptr)));
  for (int i = 0; i < 16; i++) {
    // 噪声在 XY 平面旋转
    glm::vec4 noise(static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f,
                    static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f, 0.0f,
                    0.0f);
    ssaoNoise.push_back(noise);
  }

  // 2. 创建噪声图像资源
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = 4;
  imageInfo.extent.height = 4;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_SSAONoise.image) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create SSAO noise image!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(m_Device, m_SSAONoise.image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = FindMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_SSAONoise.memory) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate SSAO noise image memory!");
  }

  vkBindImageMemory(m_Device, m_SSAONoise.image, m_SSAONoise.memory, 0);

  // 3. 上传噪声数据
  VkDeviceSize imageSize = sizeof(glm::vec4) * 16;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void *mappedData;
  vkMapMemory(m_Device, stagingBufferMemory, 0, imageSize, 0, &mappedData);
  memcpy(mappedData, ssaoNoise.data(), imageSize);
  vkUnmapMemory(m_Device, stagingBufferMemory);

  // 转换布局并拷贝
  {
    VkCommandBufferAllocateInfo cbAllocInfo{};
    cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAllocInfo.commandPool = m_CommandPool;
    cbAllocInfo.commandBufferCount = 1;

    VkCommandBuffer tempBuffer;
    vkAllocateCommandBuffers(m_Device, &cbAllocInfo, &tempBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(tempBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_SSAONoise.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(tempBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {4, 4, 1};

    vkCmdCopyBufferToImage(tempBuffer, stagingBuffer, m_SSAONoise.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(tempBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    vkEndCommandBuffer(tempBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &tempBuffer;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &tempBuffer);
  }

  vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
  vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

  // 4. 创建噪声视图
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_SSAONoise.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SSAONoise.view) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create SSAO noise image view!");
  }

  // 5. 创建SSAO采样核
  std::vector<glm::vec4> ssaoKernel;
  for (int i = 0; i < 64; i++) {
    glm::vec3 sample(static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f,
                     static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f,
                     static_cast<float>(rand()) / RAND_MAX);
    sample = glm::normalize(sample);
    sample *= static_cast<float>(rand()) / RAND_MAX;

    float scale = static_cast<float>(i) / 64.0f;
    scale = 0.1f + scale * scale * (1.0f - 0.1f);
    sample *= scale;

    ssaoKernel.push_back(glm::vec4(sample, 0.0f));
  }

  // 6. 创建采样核UBO
  VkDeviceSize bufferSize = sizeof(glm::vec4) * 64;
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               m_SSAOKernelBuffer, m_SSAOKernelMemory);

  void *data;
  vkMapMemory(m_Device, m_SSAOKernelMemory, 0, bufferSize, 0, &data);
  memcpy(data, ssaoKernel.data(), bufferSize);
  vkUnmapMemory(m_Device, m_SSAOKernelMemory);

  LOG_I("SSAO resources created (Kernel + 4x4 Noise)");
}

void RenderCore::CreateBloomResources() {
  // 创建半分辨率Bloom纹理
  uint32_t bloomWidth = m_SwapchainExtent.width / 2;
  uint32_t bloomHeight = m_SwapchainExtent.height / 2;

  auto createBloomTexture = [&](GBufferAttachment &attachment) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = bloomWidth;
    imageInfo.extent.height = bloomHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_Device, &imageInfo, nullptr, &attachment.image) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create bloom image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Device, attachment.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(
        memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &attachment.memory) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate bloom image memory!");
    }

    vkBindImageMemory(m_Device, attachment.image, attachment.memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = attachment.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device, &viewInfo, nullptr, &attachment.view) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create bloom image view!");
    }

    attachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  };

  createBloomTexture(m_BloomBrightTexture);
  createBloomTexture(m_BloomBlurTexture);

  LOG_I("Bloom resources created: {}x{}", bloomWidth, bloomHeight);
}

void RenderCore::CreateBloomRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr,
                         &m_BloomRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Bloom render pass!");
  }
}

void RenderCore::CreateBloomFramebuffers() {
  auto createFramebuffer = [&](GBufferAttachment &attachment,
                               VkFramebuffer &framebuffer) {
    VkImageView attachments[] = {attachment.view};
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_BloomRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_SwapchainExtent.width / 2;
    framebufferInfo.height = m_SwapchainExtent.height / 2;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr,
                            &framebuffer) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create bloom framebuffer!");
    }
  };

  createFramebuffer(m_BloomBrightTexture, m_BloomBrightFramebuffer);
  createFramebuffer(m_BloomBlurTexture, m_BloomBlurFramebuffer);
}

void RenderCore::CreateBloomPipelines() {
  auto vertShaderCode =
      ReadShaderFile("resource/shaders/compiled/composition.vert.spv");
  auto thresholdFragCode =
      ReadShaderFile("resource/shaders/compiled/bloom_threshold.frag.spv");
  auto blurFragCode =
      ReadShaderFile("resource/shaders/compiled/bloom_blur.frag.spv");

  VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
  VkShaderModule thresholdFragModule = CreateShaderModule(thresholdFragCode);
  VkShaderModule blurFragModule = CreateShaderModule(blurFragCode);

  VkPipelineShaderStageCreateInfo vertStageInfo{};
  vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStageInfo.module = vertShaderModule;
  vertStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo thresholdFragStageInfo{};
  thresholdFragStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  thresholdFragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  thresholdFragStageInfo.module = thresholdFragModule;
  thresholdFragStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo blurFragStageInfo{};
  blurFragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  blurFragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  blurFragStageInfo.module = blurFragModule;
  blurFragStageInfo.pName = "main";

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_SwapchainExtent.width / 2.0f;
  viewport.height = (float)m_SwapchainExtent.height / 2.0f;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {m_SwapchainExtent.width / 2, m_SwapchainExtent.height / 2};

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(float) * 4;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_SingleTextureDescriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr,
                             &m_BloomPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create bloom pipeline layout!");
  }

  // Create Threshold Pipeline
  VkPipelineShaderStageCreateInfo thresholdStages[] = {vertStageInfo,
                                                       thresholdFragStageInfo};
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = thresholdStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();
  pipelineInfo.pDynamicState = &dynamicState;

  pipelineInfo.layout = m_BloomPipelineLayout;
  pipelineInfo.renderPass = m_BloomRenderPass;
  pipelineInfo.subpass = 0;

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr,
                                &m_BloomThresholdPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create bloom threshold pipeline!");
  }

  // Create Blur Pipeline
  VkPipelineShaderStageCreateInfo blurStages[] = {vertStageInfo,
                                                  blurFragStageInfo};
  pipelineInfo.pStages = blurStages;

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &m_BloomBlurPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create bloom blur pipeline!");
  }

  vkDestroyShaderModule(m_Device, blurFragModule, nullptr);
  vkDestroyShaderModule(m_Device, thresholdFragModule, nullptr);
  vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);

  LOG_I("Bloom pipelines created successfully");
}

void RenderCore::CreatePostProcessDescriptorSets() {
  // 1. Set 0: Textures / SSAO Kernel UBO
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                             m_PostProcessDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_DescriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  m_PostProcessDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_Device, &allocInfo,
                               m_PostProcessDescriptorSets.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to allocate post-process descriptor sets (Set 0)!");
  }

  // 2. Set 1: Camera UBO
  // Reuse Composition Light layout as it has Binding 0 UBO compatible layout
  std::vector<VkDescriptorSetLayout> cameraLayouts(
      MAX_FRAMES_IN_FLIGHT, m_CompositionLightDescriptorSetLayout);
  allocInfo.pSetLayouts = cameraLayouts.data();

  g_PostProcessCameraDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_Device, &allocInfo,
                               g_PostProcessCameraDescriptorSets.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to allocate post-process descriptor sets (Set 1)!");
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    // --- Update Set 0 ---
    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    sceneInfo.imageView = m_SceneColor[i].view;
    sceneInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Using depth as shader
                                                  // resource
    depthInfo.imageView = m_GBuffer.GetDepthView(i);
    depthInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    normalInfo.imageView = m_GBuffer.GetNormalSmoothness(i).view;
    normalInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo bloomInfo{};
    bloomInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bloomInfo.imageView =
        m_BloomBrightTexture
            .view; // Use BrightTexture as it's the output of final V-Blur
    bloomInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo ssaoNoiseInfo{};
    ssaoNoiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ssaoNoiseInfo.imageView = m_SSAONoise.view;
    ssaoNoiseInfo.sampler = m_GBufferSampler;

    VkDescriptorBufferInfo kernelInfo{};
    kernelInfo.buffer = m_SSAOKernelBuffer;
    kernelInfo.offset = 0;
    kernelInfo.range = VK_WHOLE_SIZE;

    std::array<VkWriteDescriptorSet, 6> descriptorWrites{};

    auto writeImage = [&](uint32_t binding, VkDescriptorImageInfo *info) {
      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_PostProcessDescriptorSets[i];
      write.dstBinding = binding;
      write.dstArrayElement = 0;
      write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.descriptorCount = 1;
      write.pImageInfo = info;
      return write;
    };

    descriptorWrites[0] = writeImage(0, &sceneInfo);
    descriptorWrites[1] = writeImage(1, &depthInfo);
    descriptorWrites[2] = writeImage(2, &normalInfo);
    descriptorWrites[3] = writeImage(3, &bloomInfo);
    descriptorWrites[4] = writeImage(4, &ssaoNoiseInfo);

    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = m_PostProcessDescriptorSets[i];
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pBufferInfo = &kernelInfo;

    vkUpdateDescriptorSets(m_Device,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);

    // --- Update Set 1 (Camera) ---
    VkDescriptorBufferInfo cameraInfo{};
    cameraInfo.buffer = m_CameraUniformBuffers[i];
    cameraInfo.offset = 0;
    cameraInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet cameraWrite{};
    cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraWrite.dstSet = g_PostProcessCameraDescriptorSets[i];
    cameraWrite.dstBinding = 0;
    cameraWrite.dstArrayElement = 0;
    cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWrite.descriptorCount = 1;
    cameraWrite.pBufferInfo = &cameraInfo;

    vkUpdateDescriptorSets(m_Device, 1, &cameraWrite, 0, nullptr);
  }

  LOG_I("Post-process descriptor sets created successfully");
}

// ========== 模型加载实现 ==========

bool RenderCore::LoadModelFromFile(const std::string &path,
                                   std::vector<Vertex> &outVertices,
                                   std::vector<uint32_t> &outIndices) {
  // 使用tinyobjloader加载OBJ文件
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, path.c_str(),
                        nullptr, true)) {
    LOG_E("Failed to load model from {}: {}", path, warn);
    return false;
  }

  outVertices.clear();
  outIndices.clear();

  // 遍历所有形状并提取顶点数据
  for (const auto &shape : shapes) {
    for (const auto &index : shape.mesh.indices) {
      Vertex vertex{};

      // 位置
      if (index.vertex_index >= 0) {
        vertex.position = {attrib.vertices[3 * index.vertex_index + 0],
                           attrib.vertices[3 * index.vertex_index + 1],
                           attrib.vertices[3 * index.vertex_index + 2]};
      }

      // 法线
      if (index.normal_index >= 0) {
        vertex.normal = {attrib.normals[3 * index.normal_index + 0],
                         attrib.normals[3 * index.normal_index + 1],
                         attrib.normals[3 * index.normal_index + 2]};
      } else {
        vertex.normal = {0.0f, 1.0f, 0.0f}; // 默认法线
      }

      // 纹理坐标
      if (index.texcoord_index >= 0) {
        vertex.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                           1.0f -
                               attrib.texcoords[2 * index.texcoord_index + 1]};
      } else {
        vertex.texCoord = {0.0f, 0.0f};
      }

      // 默认颜色
      vertex.color = {0.8f, 0.8f, 0.8f, 1.0f};

      outIndices.push_back(static_cast<uint32_t>(outVertices.size()));
      outVertices.push_back(vertex);
    }
  }

  LOG_I("Loaded model from {}: {} vertices, {} indices", path,
        outVertices.size(), outIndices.size());
  return true;
}

// ========== Mesh资源加载与场景收集 ==========

bool RenderCore::LoadMeshResource(const UUID &meshID) {
  if (m_MeshCache.find(meshID) != m_MeshCache.end()) {
    return true;
  }

  std::string path = AssetManager::GetInstance().GetAssetPath(meshID);
  if (path.empty()) {
    LOG_W("Mesh resource path not found for UUID: {}", meshID.ToString());
    return false;
  }

  std::filesystem::path meshPath(path);
  std::ifstream file(meshPath, std::ios::binary);
  if (!file.is_open()) {
    LOG_E("Failed to open mesh file: {}", path);
    return false;
  }

  // 读取头部信息
  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;
  file.read(reinterpret_cast<char *>(&vertexCount), sizeof(uint32_t));
  file.read(reinterpret_cast<char *>(&indexCount), sizeof(uint32_t));

  if (vertexCount == 0 || indexCount == 0) {
    LOG_E("Empty mesh data in file: {}", path);
    return false;
  }

  std::vector<float> positions(vertexCount * 3);
  file.read(reinterpret_cast<char *>(positions.data()),
            positions.size() * sizeof(float));

  bool hasNormals = false;
  file.read(reinterpret_cast<char *>(&hasNormals), sizeof(bool));
  std::vector<float> normals;
  if (hasNormals) {
    normals.resize(vertexCount * 3);
    file.read(reinterpret_cast<char *>(normals.data()),
              normals.size() * sizeof(float));
  }

  bool hasTexcoords = false;
  file.read(reinterpret_cast<char *>(&hasTexcoords), sizeof(bool));
  std::vector<float> texcoords;
  if (hasTexcoords) {
    texcoords.resize(vertexCount * 2);
    file.read(reinterpret_cast<char *>(texcoords.data()),
              texcoords.size() * sizeof(float));
  }

  std::vector<uint32_t> indices(indexCount);
  file.read(reinterpret_cast<char *>(indices.data()),
            indices.size() * sizeof(uint32_t));

  // 构建 Vertex 数组
  std::vector<Vertex> vertices(vertexCount);
  for (uint32_t i = 0; i < vertexCount; i++) {
    vertices[i].position = glm::vec3(positions[i * 3 + 0], positions[i * 3 + 1],
                                     positions[i * 3 + 2]);

    if (hasNormals) {
      vertices[i].normal =
          glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
    } else {
      vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    if (hasTexcoords) {
      vertices[i].texCoord =
          glm::vec2(texcoords[i * 2 + 0], texcoords[i * 2 + 1]);
    } else {
      vertices[i].texCoord = glm::vec2(0.0f, 0.0f);
    }
  }

  // 创建缓冲区
  MeshResource res;
  res.indexCount = indexCount;

  // Vertex Buffer
  { // Scope for vertex buffer creation variables
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void *data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    CreateBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, res.vertexBuffer,
                 res.vertexMemory);

    CopyBuffer(stagingBuffer, res.vertexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
  }

  // Index Buffer
  { // Scope for index buffer creation variables
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void *data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    CreateBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, res.indexBuffer, res.indexMemory);

    CopyBuffer(stagingBuffer, res.indexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
  }

  res.loaded = true;
  m_MeshCache[meshID] = res;
  LOG_I("Loaded mesh resource: {}", path);
  return true;
}

void RenderCore::CollectSceneRenderables() {
  // 获取当前正在编辑的场景
  auto scene = EditorGUI::GetCurrentScene();
  if (!scene)
    return;

  // 清空现有的渲染对象队列，准备从场景中重新收集
  // 注意：在正式版本中，应通过“场景脏标记”来决定是否重整，此处暂且每帧清理
  m_RenderObjects.clear();

  // 创建一个场景渲染器辅助对象，用于递归遍历场景树并收集渲染指令
  SceneRenderer renderer;
  if (scene->GetRootNode()) {
    // 从根节点开始，递归调用 CollectRenderables，初始变换矩阵为单位阵
    scene->GetRootNode()->CollectRenderables(renderer, glm::mat4(1.0f));
  }

  // 遍历收集到的所有渲染命令
  for (const auto &cmd : renderer.GetRenderCommands()) {
    // 确保网格资源已加载到 GPU
    if (!LoadMeshResource(cmd.meshID)) {
      continue;
    }

    // 从缓存中获取已加载的网格资源（包括顶点缓冲和索引缓冲）
    const auto &meshRes = m_MeshCache[cmd.meshID];

    // 构建渲染对象（RenderObject），这是渲染管线直接处理的结构
    RenderObject obj{};
    obj.modelMatrix = cmd.transform;         // 模型变换矩阵
    obj.vertexBuffer = meshRes.vertexBuffer; // 顶点缓冲区句柄
    obj.indexBuffer = meshRes.indexBuffer;   // 索引缓冲区句柄
    obj.indexCount = meshRes.indexCount;     // 索引数量

    // TODO: 未来应根据 MaterialID 从资产管理器加载实际材质
    // 目前暂时硬编码一组默认 PBR 参数

    // 材质属性设置：基础色、透明度、金属度、粗糙度等
    obj.material.albedo = glm::vec3(1.0f); // 纯白基础色
    obj.material.alpha = 1.0f;             // 不透明
    obj.material.metallic = 0.5f;          // 中等金属感
    obj.material.roughness = 0.5f;         // 中等粗糙度
    obj.material.shadingId = 0.0f;         // 0.0 代表受光照（Lit）
    obj.material.emissiveIntensity = 0.0f; // 无自发光

    // 将组装好的渲染对象放入待渲染列表
    m_RenderObjects.push_back(obj);
  }
}

} // namespace neurender
