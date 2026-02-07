#pragma once
#include <vulkan/vulkan.h>

namespace neurender {

/**
 * @brief Mesh 资源
 * 包含已加载到 GPU 的顶点和索引缓冲区
 */
struct MeshResource {
  VkBuffer vertexBuffer = VK_NULL_HANDLE;
  VkBuffer indexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
  VkDeviceMemory indexMemory = VK_NULL_HANDLE;
  uint32_t indexCount = 0;
  bool loaded = false;
};

} // namespace neurender
