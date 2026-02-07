#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace neurender {

/**
 * @brief 顶点结构体，用于渲染管线
 *
 * 包含位置、法线、纹理坐标和颜色信息
 */
struct Vertex {
  glm::vec3 position; // 顶点位置
  glm::vec3 normal;   // 顶点法线
  glm::vec2 texCoord; // 纹理坐标
  glm::vec4 color;    // 顶点颜色

  /**
   * @brief 获取顶点输入绑定描述
   */
  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
  }

  /**
   * @brief 获取顶点属性描述数组
   */
  static std::array<VkVertexInputAttributeDescription, 4>
  getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    // Normal
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    // TexCoord
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    // Color
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, color);

    return attributeDescriptions;
  }

  bool operator==(const Vertex &other) const {
    return position == other.position && normal == other.normal &&
           texCoord == other.texCoord && color == other.color;
  }
};

/**
 * @brief Uniform Buffer Object - MVP矩阵
 */
struct UniformBufferObject {
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

/**
 * @brief 光照数据 Uniform Buffer (方向光)
 */
struct LightDataUBO {
  alignas(16) glm::vec3 lightDir;
  alignas(16) glm::vec3 lightColor;
  alignas(16) glm::vec3 viewPos;
};

/**
 * @brief 点光源最大数量
 */
constexpr uint32_t MAX_POINT_LIGHTS = 128;

/**
 * @brief 单个点光源数据 (用于Shader传输)
 */
struct PointLight {
  alignas(16) glm::vec3 position;
  float radius;
  alignas(16) glm::vec3 color;
  float intensity;
};

/**
 * @brief 点光源数组 Uniform Buffer
 */
struct PointLightsUBO {
  PointLight lights[MAX_POINT_LIGHTS];
  uint32_t count;
  float _pad[3];
};

} // namespace neurender
