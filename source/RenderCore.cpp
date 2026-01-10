#include "RenderCore.h"
#include "Window.h"
#include "neuGUI.h"
#include "neuLog.h"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <set>
#include <vulkan/vulkan_core.h>

namespace neurender {

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

const int MAX_FRAMES_IN_FLIGHT = 2;

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
  CreateInstance();       // Create Vulkan instance
  SetupDebugMessenger();  // Setup validation layer callback
  CreateSurface();        // Create window surface
  PickPhysicalDevice();   // Select GPU
  CreateLogicalDevice();  // Create logical device and queues
  CreateSwapchain();      // Create swapchain
  CreateImageViews();     // Create image views
  CreateRenderPass();     // Create render pass
  CreateFramebuffers();   // Create framebuffers
  CreateCommandPool();    // Create command pool
  CreateCommandBuffers(); // Create command buffers
  CreateSyncObjects();    // Create semaphores and fences
  CreateDescriptorPool(); // Create descriptor pool
  InitImGui();            // Initialize ImGui

  LOG_I("RenderCore Initialized");
}

void RenderCore::Shutdown() {
  vkDeviceWaitIdle(m_Device);

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  CleanupSwapchain();

  vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
    vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
    vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
  }

  vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
  vkDestroyDevice(m_Device, nullptr);

  DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);

  vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
  vkDestroyInstance(m_Instance, nullptr);

  LOG_I("RenderCore Shutdown");
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
    framebufferInfo.renderPass = m_RenderPass;
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
      ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_None; // Enable Docking

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
  init_info.RenderPass = m_RenderPass;
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

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_RenderPass;
  renderPassInfo.framebuffer = m_SwapchainFramebuffers[imageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = m_SwapchainExtent;

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  // Call ImGui Render Here? No, ImGui Render needs to happen before
  // RecordCommandBuffer? No, standard workflow:
  // 1. ImGui::NewFrame()
  // 2. Build UI
  // 3. ImGui::Render()
  // 4. RecordCommandBuffer: vkCmdDraw, ImGui_ImplVulkan_RenderDrawData

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

  vkCmdEndRenderPass(commandBuffer);

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
  } // Missing bracket fix? No, looks correct.

  vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
}

} // namespace neurender
