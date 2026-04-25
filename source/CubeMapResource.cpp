#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Asset/CubeMapResource.h"
#include "neuLog.h"

#include <stb_image.h>

namespace neurender {

bool CubeMapResource::LoadFromFiles(VkDevice device,
                                    VkPhysicalDevice physicalDevice,
                                    VkCommandPool commandPool,
                                    VkQueue graphicsQueue,
                                    const std::vector<std::string> &paths) {
  if (paths.size() != 6) {
    LOG_E("CubeMap requires exactly 6 texture paths");
    return false;
  }

  std::vector<unsigned char *> pixels(6);
  int texWidth = 0, texHeight = 0, texChannels = 0;

  uint64_t sourceTime = 0;
  std::filesystem::path p0 = std::filesystem::u8path(paths[0]);
  if (!paths.empty() && std::filesystem::exists(p0)) {
    auto ftime = std::filesystem::last_write_time(p0);
    sourceTime = ftime.time_since_epoch().count();
  }
  std::string cachePath = paths[0] + ".ibl_cache";
  LoadIBLCache(cachePath, sourceTime);

  for (int i = 0; i < 6; i++) {
    std::ifstream ifs(std::filesystem::u8path(paths[i]),
                      std::ios::binary | std::ios::ate);
    if (!ifs) {
      LOG_E("Failed to open cube map face file: {}", paths[i]);
      // Cleanup already loaded
      for (int j = 0; j < i; j++)
        stbi_image_free(pixels[j]);
      return false;
    }

    std::streamsize fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(static_cast<size_t>(fileSize));
    ifs.read(reinterpret_cast<char *>(buffer.data()), fileSize);

    int w, h, c;
    pixels[i] =
        stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()),
                              &w, &h, &c, STBI_rgb_alpha);
    if (!pixels[i]) {
      LOG_E("STB failed to decode cube map face: {}", paths[i]);
      for (int j = 0; j < i; j++)
        stbi_image_free(pixels[j]);
      return false;
    }

    if (i == 0) {
      texWidth = w;
      texHeight = h;
      texChannels = c;
    } else if (w != texWidth || h != texHeight) {
      LOG_E("Cube map face sizes do not match: {}", paths[i]);
      for (int j = 0; j <= i; j++)
        stbi_image_free(pixels[j]);
      return false;
    }
  }

  if (!m_hasIBLCache) {
    // 计算球谐系数
    for (int i = 0; i < 9; ++i)
      sh[i] = glm::vec4(0.0f);
    float totalWeight = 0.0f;

    for (int face = 0; face < 6; ++face) {
      for (int y = 0; y < texHeight; ++y) {
        for (int x = 0; x < texWidth; ++x) {
          float u = ((float)x + 0.5f) / (float)texWidth * 2.0f - 1.0f;
          float v = ((float)y + 0.5f) / (float)texHeight * 2.0f - 1.0f;
          v = -v; // Invert V

          glm::vec3 dir;
          switch (face) {
          case 0:
            dir = glm::vec3(1.0f, v, -u);
            break; // PosX
          case 1:
            dir = glm::vec3(-1.0f, v, u);
            break; // NegX
          case 2:
            dir = glm::vec3(u, 1.0f, -v);
            break; // PosY
          case 3:
            dir = glm::vec3(u, -1.0f, v);
            break; // NegY
          case 4:
            dir = glm::vec3(u, v, 1.0f);
            break; // PosZ
          case 5:
            dir = glm::vec3(-u, v, -1.0f);
            break; // NegZ
          }
          dir = glm::normalize(dir);

          float dist = u * u + v * v + 1.0f;
          float dw = 1.0f / (dist * std::sqrt(dist));

          int pixelOffset = (y * texWidth + x) * 4;
          float r = pixels[face][pixelOffset] / 255.0f;
          float g = pixels[face][pixelOffset + 1] / 255.0f;
          float b = pixels[face][pixelOffset + 2] / 255.0f;

          r = std::pow(r, 2.2f);
          g = std::pow(g, 2.2f);
          b = std::pow(b, 2.2f);
          glm::vec3 color(r, g, b);

          float Y[9];
          Y[0] = 0.282095f;
          Y[1] = 0.488603f * dir.y;
          Y[2] = 0.488603f * dir.z;
          Y[3] = 0.488603f * dir.x;
          Y[4] = 1.092548f * dir.x * dir.y;
          Y[5] = 1.092548f * dir.y * dir.z;
          Y[6] = 0.315392f * (3.0f * dir.z * dir.z - 1.0f);
          Y[7] = 1.092548f * dir.x * dir.z;
          Y[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y);

          for (int i = 0; i < 9; ++i) {
            sh[i] += glm::vec4(color * Y[i] * dw, 0.0f);
          }
          totalWeight += dw;
        }
      }
    }

    // Normalize
    float const PI = 3.14159265359f;
    float invWeight = (4.0f * PI) / totalWeight;
    for (int i = 0; i < 9; ++i) {
      sh[i] *= invWeight;
    }

    LOG_I("Calculated Spherical Harmonics for {}x{} CubeMap", texWidth,
          texHeight);
  } else {
    LOG_I("Loaded Spherical Harmonics from cache");
  }

  filePaths = paths;

  bool result = CreateFromData(
      device, physicalDevice, commandPool, graphicsQueue, pixels,
      static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 4);

  for (int i = 0; i < 6; i++) {
    stbi_image_free(pixels[i]);
  }

  if (result) {
    LOG_I("Loaded CubeMap successfully ({}x{})", width, height);
  }

  return result;
}

bool CubeMapResource::CreateFromData(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
    VkQueue graphicsQueue, const std::vector<unsigned char *> &pixels,
    uint32_t texWidth, uint32_t texHeight, uint32_t texChannels) {
  width = texWidth;
  height = texHeight;
  channels = texChannels;

  mipLevels =
      static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

  VkDeviceSize layerSize = width * height * 4;
  VkDeviceSize imageSize = layerSize * 6;

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = imageSize;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) !=
      VK_SUCCESS) {
    LOG_E("Failed to create staging buffer for CubeMap");
    return false;
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory) !=
      VK_SUCCESS) {
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    LOG_E("Failed to allocate staging buffer memory for CubeMap");
    return false;
  }

  vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

  void *data;
  vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
  for (int i = 0; i < 6; i++) {
    memcpy(static_cast<unsigned char *>(data) + (layerSize * i), pixels[i],
           static_cast<size_t>(layerSize));
  }
  vkUnmapMemory(device, stagingBufferMemory);

  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB; // Standard for albedo/skybox
  if (!CreateImage(device, physicalDevice, width, height, mipLevels, format,
                   VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    return false;
  }

  TransitionImageLayout(device, commandPool, graphicsQueue,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  CopyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer);

  GenerateMipmaps(device, physicalDevice, commandPool, graphicsQueue, format);

  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);

  if (!CreateImageView(device, format))
    return false;
  if (!CreateSampler(device, physicalDevice))
    return false;

  isLoaded = true;
  return true;
}
bool CubeMapResource::LoadIBLCache(const std::string &path,
                                   uint64_t sourceTime) {
  std::ifstream ifs(std::filesystem::u8path(path), std::ios::binary);
  if (!ifs)
    return false;

  uint32_t magic = 0;
  ifs.read(reinterpret_cast<char *>(&magic), sizeof(magic));
  if (magic != 0x434C4249)
    return false; // "IBLC"

  uint64_t cacheTime = 0;
  ifs.read(reinterpret_cast<char *>(&cacheTime), sizeof(cacheTime));
  if (cacheTime != sourceTime)
    return false; // Outdated

  ifs.read(reinterpret_cast<char *>(sh), sizeof(sh));

  uint32_t dataSize = 0;
  ifs.read(reinterpret_cast<char *>(&dataSize), sizeof(dataSize));

  if (dataSize > 0) {
    m_prefilterCacheData.resize(dataSize);
    ifs.read(reinterpret_cast<char *>(m_prefilterCacheData.data()), dataSize);
    if (!ifs) {
      m_prefilterCacheData.clear();
      return false;
    }
  }

  m_hasIBLCache = true;
  return true;
}

bool CubeMapResource::SaveIBLCache(const std::string &path, uint64_t sourceTime,
                                   const uint8_t *prefilterData,
                                   size_t prefilterSize) {
  std::ofstream ofs(std::filesystem::u8path(path), std::ios::binary);
  if (!ofs)
    return false;

  uint32_t magic = 0x434C4249; // "IBLC"
  ofs.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
  ofs.write(reinterpret_cast<const char *>(&sourceTime), sizeof(sourceTime));
  ofs.write(reinterpret_cast<const char *>(sh), sizeof(sh));

  uint32_t dataSize = static_cast<uint32_t>(prefilterSize);
  ofs.write(reinterpret_cast<const char *>(&dataSize), sizeof(dataSize));
  if (dataSize > 0 && prefilterData) {
    ofs.write(reinterpret_cast<const char *>(prefilterData), dataSize);
  }

  return true;
}

void CubeMapResource::Destroy(VkDevice device) {
  if (sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, sampler, nullptr);
    sampler = VK_NULL_HANDLE;
  }
  if (imageView != VK_NULL_HANDLE) {
    vkDestroyImageView(device, imageView, nullptr);
    imageView = VK_NULL_HANDLE;
  }
  if (image != VK_NULL_HANDLE) {
    vkDestroyImage(device, image, nullptr);
    image = VK_NULL_HANDLE;
  }
  if (memory != VK_NULL_HANDLE) {
    vkFreeMemory(device, memory, nullptr);
    memory = VK_NULL_HANDLE;
  }
  // 预过滤贴图
  if (prefilteredSampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, prefilteredSampler, nullptr);
    prefilteredSampler = VK_NULL_HANDLE;
  }
  if (prefilteredImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(device, prefilteredImageView, nullptr);
    prefilteredImageView = VK_NULL_HANDLE;
  }
  if (prefilteredImage != VK_NULL_HANDLE) {
    vkDestroyImage(device, prefilteredImage, nullptr);
    prefilteredImage = VK_NULL_HANDLE;
  }
  if (prefilteredMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, prefilteredMemory, nullptr);
    prefilteredMemory = VK_NULL_HANDLE;
  }
  isLoaded = false;
}

// ===========================================================
// 预过滤环境贴图生成 (CPU 重要性采样, GGX)
// ===========================================================

namespace {

// 畅炴序列低差分 (Van der Corput)
float RadicalInverse_VdC(uint32_t bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

// Hammersley 点集
glm::vec2 Hammersley(uint32_t i, uint32_t N) {
  return glm::vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// GGX NDF 重要性采样
// 返回值: H(世界空间半向量), 同时输出 cosTheta 供 PDF 计算
glm::vec3 ImportanceSampleGGX(glm::vec2 Xi, glm::vec3 N, float roughness,
                              float *outCosTheta = nullptr) {
  constexpr float PI = 3.14159265359f;
  // 至少保留一个最小粗糙度，防止 roughness=0 时分母退化产生 NaN
  float a = std::max(roughness * roughness, 1e-4f);
  float phi = 2.0f * PI * Xi.x;
  // 防止 Xi.y 极端时分母为零
  float denom = 1.0f + (a * a - 1.0f) * Xi.y;
  float cosTheta = (denom > 0.0f) ? std::sqrt((1.0f - Xi.y) / denom) : 1.0f;
  cosTheta = std::min(cosTheta, 1.0f); // clamp 浮点误差
  float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
  if (outCosTheta)
    *outCosTheta = cosTheta;

  glm::vec3 H;
  H.x = std::cos(phi) * sinTheta;
  H.y = std::sin(phi) * sinTheta;
  H.z = cosTheta;

  // 将 TBN 空间转世界空间
  glm::vec3 up =
      std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
  glm::vec3 tangent = glm::normalize(glm::cross(up, N));
  glm::vec3 bitangent = glm::cross(N, tangent);
  return glm::normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// GGX PDF: D(h) * NdotH / (4 * VdotH)
float GGX_PDF(float NdotH, float VdotH, float roughness) {
  constexpr float PI = 3.14159265359f;
  float a = std::max(roughness * roughness, 1e-4f);
  float a2 = a * a;
  float denom = (NdotH * NdotH * (a2 - 1.0f) + 1.0f);
  float D = a2 / (PI * denom * denom + 1e-7f);
  return (D * NdotH) / (4.0f * std::max(VdotH, 1e-4f));
}

// 将世界空间方向映射到 Cubemap面索引+UV
// 遵循 GLSL samplerCube 规范:
//  +X: sc=-rz/|rx|, tc=-ry/|rx|
//  -X: sc=+rz/|rx|, tc=-ry/|rx|
//  +Y: sc=+rx/|ry|, tc=+rz/|ry|
//  -Y: sc=+rx/|ry|, tc=-rz/|ry|
//  +Z: sc=+rx/|rz|, tc=-ry/|rz|
//  -Z: sc=-rx/|rz|, tc=-ry/|rz|
void DirToFaceUV(const glm::vec3 &dir, int &face, float &u, float &v) {
  glm::vec3 d = glm::abs(dir);
  if (d.x >= d.y && d.x >= d.z) {
    if (dir.x > 0) {
      face = 0; // +X
      u = -dir.z / dir.x;
      v = -dir.y / dir.x;
    } else {
      face = 1; // -X
      u = dir.z / (-dir.x);
      v = -dir.y / (-dir.x);
    }
  } else if (d.y >= d.x && d.y >= d.z) {
    if (dir.y > 0) {
      face = 2; // +Y
      u = dir.x / dir.y;
      v = dir.z / dir.y; // tc = +rz/|ry|
    } else {
      face = 3; // -Y
      u = dir.x / (-dir.y);
      v = -dir.z / (-dir.y); // tc = -rz/|ry|
    }
  } else {
    if (dir.z > 0) {
      face = 4; // +Z
      u = dir.x / dir.z;
      v = -dir.y / dir.z;
    } else {
      face = 5; // -Z
      u = -dir.x / (-dir.z);
      v = -dir.y / (-dir.z);
    }
  }
}

// 双线性采样原始 CubeMap (浮点精度, 支持 HDR)
glm::vec3
SampleCubemapBilinearF(const std::vector<std::vector<float>> &faceData,
                       int faceW, int faceH, const glm::vec3 &dir) {
  int face;
  float fu, fv;
  DirToFaceUV(dir, face, fu, fv);

  float texU = (fu * 0.5f + 0.5f) * (faceW - 1);
  float texV = (fv * 0.5f + 0.5f) * (faceH - 1);
  int x0 = std::clamp((int)texU, 0, faceW - 1);
  int y0 = std::clamp((int)texV, 0, faceH - 1);
  int x1 = std::min(x0 + 1, faceW - 1);
  int y1 = std::min(y0 + 1, faceH - 1);
  float tx = texU - x0;
  float ty = texV - y0;

  auto getPixel = [&](int x, int y) -> glm::vec3 {
    int idx = (y * faceW + x) * 4;
    return {faceData[face][idx], faceData[face][idx + 1],
            faceData[face][idx + 2]};
  };

  glm::vec3 c00 = getPixel(x0, y0);
  glm::vec3 c10 = getPixel(x1, y0);
  glm::vec3 c01 = getPixel(x0, y1);
  glm::vec3 c11 = getPixel(x1, y1);
  return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
}

} // anonymous namespace

bool CubeMapResource::GeneratePrefilteredMap(VkDevice device,
                                             VkPhysicalDevice physicalDevice,
                                             VkCommandPool commandPool,
                                             VkQueue graphicsQueue) {
  if (!isLoaded) {
    LOG_E("GeneratePrefilteredMap: CubeMap not loaded");
    return false;
  }

  // 每面计算尺寸和 mip
  const uint32_t baseSize = 128; // 预过滤贴图 Mip0 大小
  const uint32_t numSamples = 512;
  constexpr float PI = 3.14159265359f;

  // 为每个 mip level
  const uint32_t NUM_MIPS = PREFILTER_MIP_LEVELS;

  // 1. 创建 Vulkan 图像 (Cube, R16G16B16A16_SFLOAT, NUM_MIPS mips)
  VkImageCreateInfo imgInfo{};
  imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgInfo.imageType = VK_IMAGE_TYPE_2D;
  imgInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  imgInfo.extent = {baseSize, baseSize, 1};
  imgInfo.mipLevels = NUM_MIPS;
  imgInfo.arrayLayers = 6;
  imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (vkCreateImage(device, &imgInfo, nullptr, &prefilteredImage) !=
      VK_SUCCESS) {
    LOG_E("Failed to create prefilteredMap image");
    return false;
  }

  VkMemoryRequirements memReq;
  vkGetImageMemoryRequirements(device, prefilteredImage, &memReq);
  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(physicalDevice, memReq.memoryTypeBits,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (vkAllocateMemory(device, &allocInfo, nullptr, &prefilteredMemory) !=
      VK_SUCCESS) {
    LOG_E("Failed to allocate prefilteredMap memory");
    return false;
  }
  vkBindImageMemory(device, prefilteredImage, prefilteredMemory, 0);

  // 2. 读取原始 cubemap 面数据 (float, 支持 HDR)
  if (filePaths.empty() || filePaths.size() != 6) {
    LOG_E("GeneratePrefilteredMap: no valid face paths");
    return false;
  }

  std::vector<std::vector<float>> faceData(6);
  int faceW = 0, faceH = 0;
  for (int f = 0; f < 6; ++f) {
    int w, h, c;
    std::string ext = std::filesystem::path(filePaths[f]).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".hdr" || ext == ".exr") {
      // HDR: stbi_loadf 直接返回线性浮点值
      float *raw = stbi_loadf(filePaths[f].c_str(), &w, &h, &c, 4);
      if (!raw) {
        LOG_E("Prefilter: failed to load HDR face {}", f);
        return false;
      }
      if (f == 0) {
        faceW = w;
        faceH = h;
      }
      faceData[f].assign(raw, raw + (size_t)w * h * 4);
      stbi_image_free(raw);
    } else {
      // LDR: stbi_load + 手动 sRGB->线性
      unsigned char *raw = stbi_load(filePaths[f].c_str(), &w, &h, &c, 4);
      if (!raw) {
        LOG_E("Prefilter: failed to reload face {}", f);
        return false;
      }
      if (f == 0) {
        faceW = w;
        faceH = h;
      }
      size_t n = (size_t)w * h * 4;
      faceData[f].resize(n);
      for (size_t i = 0; i < n; ++i) {
        faceData[f][i] = std::pow(raw[i] / 255.0f, 2.2f);
      }
      stbi_image_free(raw);
    }
  }

  // 3. 计算总缓冲大小 (R16G16B16A16_SFLOAT = 8 bytes/pixel)
  const VkFormat PREFILTER_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
  const size_t BYTES_PER_PIXEL = 8; // 4 channels × float16
  std::vector<uint32_t> mipPixels(NUM_MIPS);
  size_t totalBytes = 0;
  for (uint32_t m = 0; m < NUM_MIPS; m++) {
    uint32_t mipW = std::max(1u, baseSize >> m);
    uint32_t mipH = std::max(1u, baseSize >> m);
    mipPixels[m] = mipW * mipH;
    totalBytes += (size_t)mipPixels[m] * 6 * BYTES_PER_PIXEL;
  }

  VkBuffer stagingBuf;
  VkDeviceMemory stagingMem;
  VkBufferCreateInfo bufInfo{};
  bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufInfo.size = totalBytes;
  bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vkCreateBuffer(device, &bufInfo, nullptr, &stagingBuf);
  VkMemoryRequirements stagingReq;
  vkGetBufferMemoryRequirements(device, stagingBuf, &stagingReq);
  VkMemoryAllocateInfo sAllocInfo{};
  sAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  sAllocInfo.allocationSize = stagingReq.size;
  sAllocInfo.memoryTypeIndex =
      FindMemoryType(physicalDevice, stagingReq.memoryTypeBits,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vkAllocateMemory(device, &sAllocInfo, nullptr, &stagingMem);
  vkBindBufferMemory(device, stagingBuf, stagingMem, 0);

  uint8_t *stagingPtr = nullptr;
  vkMapMemory(device, stagingMem, 0, totalBytes, 0, (void **)&stagingPtr);
  uint16_t *stagingF16 = reinterpret_cast<uint16_t *>(stagingPtr);

  if (m_hasIBLCache && m_prefilterCacheData.size() == totalBytes) {
    memcpy(stagingPtr, m_prefilterCacheData.data(), totalBytes);
    m_prefilterCacheData.clear();
    LOG_I("Loaded PrefilteredMap from cache");
  } else {
    // IEEE 754 float -> float16 helper（NaN/Inf 安全版）
    auto toF16 = [](float v) -> uint16_t {
      // 先处理 NaN/Inf：若不是有限值则输出 0
      if (!std::isfinite(v))
        return 0;
      // clamp 到 float16 最大正值 (~65504)，并 clamp 负值为
      // 0（线性颜色值不应为负）
      v = std::max(0.0f, std::min(v, 65504.0f));
      uint32_t bits;
      memcpy(&bits, &v, 4);
      uint16_t sign = (bits >> 16) & 0x8000;
      int32_t exp = ((bits >> 23) & 0xFF) - 127 + 15;
      uint32_t mant = bits & 0x7FFFFF;
      if (exp <= 0)
        return sign; // flush to zero
      if (exp >= 31)
        return sign | 0x7C00; // clamp to max (avoid inf)
      return (uint16_t)(sign | (exp << 10) | (mant >> 13));
    };

    size_t offsetPixels = 0; // in uint16_t-element units (4 channels per pixel)

    for (uint32_t m = 0; m < NUM_MIPS; m++) {
      float roughness = (float)m / (float)(NUM_MIPS - 1);
      uint32_t mipW = std::max(1u, baseSize >> m);
      uint32_t mipH = std::max(1u, baseSize >> m);

      for (int face = 0; face < 6; face++) {
        for (uint32_t y = 0; y < mipH; y++) {
          for (uint32_t x = 0; x < mipW; x++) {
            float fu = ((float)x + 0.5f) / mipW * 2.0f - 1.0f;
            float fv = ((float)y + 0.5f) / mipH * 2.0f - 1.0f;
            fv = -fv;

            glm::vec3 N;
            switch (face) {
            case 0:
              N = glm::normalize(glm::vec3(1.0f, fv, -fu));
              break;
            case 1:
              N = glm::normalize(glm::vec3(-1.0f, fv, fu));
              break;
            case 2:
              N = glm::normalize(glm::vec3(fu, 1.0f, -fv));
              break;
            case 3:
              N = glm::normalize(glm::vec3(fu, -1.0f, fv));
              break;
            case 4:
              N = glm::normalize(glm::vec3(fu, fv, 1.0f));
              break;
            case 5:
              N = glm::normalize(glm::vec3(-fu, fv, -1.0f));
              break;
            }
            glm::vec3 R = N;
            glm::vec3 V = R;

            glm::vec3 prefilteredColor(0.0f);
            float totalWeight = 0.0f;

            constexpr float PI2 = 3.14159265359f;
            // 计算 mip0 采样的立体角（用于 LOD 偏置）
            // saTexel = 4*PI / (6 * faceW * faceH)  （近似每个纹素的立体角）
            float saTexel = 4.0f * PI2 / (6.0f * (float)(faceW * faceH));

            for (uint32_t i = 0; i < numSamples; ++i) {
              glm::vec2 Xi = Hammersley(i, numSamples);
              float cosTheta;
              glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness, &cosTheta);
              float NdotH = std::max(cosTheta, 0.0f);
              glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);
              float NdotL = std::max(glm::dot(N, L), 0.0f);
              if (NdotL > 0.0f) {
                // 计算当前样本的 PDF，用于 LOD 选择以避免采到高频极端值
                float VdotH = std::max(glm::dot(V, H), 0.0f);
                float pdf = GGX_PDF(NdotH, VdotH, std::max(roughness, 1e-4f));
                // 每个样本覆盖的立体角
                float saSample = 1.0f / (float(numSamples) * pdf + 1e-7f);
                // LOD = log2(saSample / saTexel) * 0.5，并 clamp 到合理范围
                float lod =
                    (roughness == 0.0f)
                        ? 0.0f
                        : std::max(0.0f, 0.5f * std::log2(saSample / saTexel));
                // 限制最大 lod，避免高 roughness 时采到过低分辨率产生色块
                lod = std::min(lod, (float)(mipLevels - 1));
                // 当 lod 较小时直接用 bilinear（当前只有
                // mip0），可以视现有接口忽略 lod 实际已通过 pdf
                // 权重隐式压低极端值（高斯罗棱传对高频样本给较高 pdf，
                // 对应更大的 lod 意味着在低分辨率采样，等效避免单像素极端值）
                // 为简化实现，直接用 NdotL 但对 极端亮度 做裁剪：
                glm::vec3 sampleColor =
                    SampleCubemapBilinearF(faceData, faceW, faceH, L);
                // 对每个通道做 firefly 抑制：限制单次样本最大亮度贡献
                // 用当前已累积平均亮度的倍数做 clamp，防止一个极端值主导结果
                // 第一次迭代没有参考值，先不 clamp；后续用历史均值 * 8 作为上限
                if (totalWeight > 0.0f) {
                  glm::vec3 curAvg = prefilteredColor / totalWeight;
                  float avgLum = 0.2126f * curAvg.r + 0.7152f * curAvg.g +
                                 0.0722f * curAvg.b;
                  float sampleLum = 0.2126f * sampleColor.r +
                                    0.7152f * sampleColor.g +
                                    0.0722f * sampleColor.b;
                  // 如果样本亮度超过当前均值 8 倍则按比例压制
                  float maxLum = std::max(avgLum * 8.0f, 0.1f);
                  if (sampleLum > maxLum && sampleLum > 0.0f)
                    sampleColor *= (maxLum / sampleLum);
                }
                prefilteredColor += sampleColor * NdotL;
                totalWeight += NdotL;
              }
            }
            if (totalWeight > 0.0f)
              prefilteredColor /= totalWeight;
            // 最终 clamp，防止残留 NaN/Inf 写入
            prefilteredColor.r =
                std::isfinite(prefilteredColor.r) ? prefilteredColor.r : 0.0f;
            prefilteredColor.g =
                std::isfinite(prefilteredColor.g) ? prefilteredColor.g : 0.0f;
            prefilteredColor.b =
                std::isfinite(prefilteredColor.b) ? prefilteredColor.b : 0.0f;

            // 写入 staging (R16G16B16A16_SFLOAT, 线性空间)
            size_t pixBase = offsetPixels + ((size_t)y * mipW + x) * 4;
            stagingF16[pixBase + 0] = toF16(prefilteredColor.r);
            stagingF16[pixBase + 1] = toF16(prefilteredColor.g);
            stagingF16[pixBase + 2] = toF16(prefilteredColor.b);
            stagingF16[pixBase + 3] = toF16(1.0f);
          }
        }
        offsetPixels += (size_t)mipW * mipH * 4; // advance by pixels×channels
      }
    }

    uint64_t sourceTime = 0;
    std::filesystem::path p0 = std::filesystem::u8path(filePaths[0]);
    if (!filePaths.empty() && std::filesystem::exists(p0)) {
      auto ftime = std::filesystem::last_write_time(p0);
      sourceTime = ftime.time_since_epoch().count();
    }
    std::string cachePath = filePaths[0] + ".ibl_cache";
    SaveIBLCache(cachePath, sourceTime, stagingPtr, totalBytes);
  }
  vkUnmapMemory(device, stagingMem);

  // 4. 转换布局 UNDEFINED -> TRANSFER_DST
  auto beginCmd = [&]() -> VkCommandBuffer {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = commandPool;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(device, &ai, &cb);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
  };
  auto endCmd = [&](VkCommandBuffer cb) {
    vkEndCommandBuffer(cb);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &cb);
  };

  {
    VkCommandBuffer cb = beginCmd();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = prefilteredImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, NUM_MIPS, 0, 6};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    endCmd(cb);
  }

  // 5. 每个 mip level 复制
  {
    VkCommandBuffer cb = beginCmd();
    size_t bufOffset = 0;
    for (uint32_t m = 0; m < NUM_MIPS; m++) {
      uint32_t mipW = std::max(1u, baseSize >> m);
      uint32_t mipH = std::max(1u, baseSize >> m);
      for (int face = 0; face < 6; face++) {
        VkBufferImageCopy region{};
        region.bufferOffset = bufOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m, (uint32_t)face,
                                   1};
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {mipW, mipH, 1};
        vkCmdCopyBufferToImage(cb, stagingBuf, prefilteredImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);
        bufOffset += (size_t)mipW * mipH * 8; // 8 bytes/pixel = RGBA16F
      }
    }
    endCmd(cb);
  }

  // 6. TRANSFER_DST -> SHADER_READ_ONLY
  {
    VkCommandBuffer cb = beginCmd();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = prefilteredImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, NUM_MIPS, 0, 6};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
    endCmd(cb);
  }

  vkDestroyBuffer(device, stagingBuf, nullptr);
  vkFreeMemory(device, stagingMem, nullptr);

  // 7. 创建 ImageView
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = prefilteredImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, NUM_MIPS, 0, 6};
  if (vkCreateImageView(device, &viewInfo, nullptr, &prefilteredImageView) !=
      VK_SUCCESS) {
    LOG_E("Failed to create prefiltered image view");
    return false;
  }

  // 8. 创建 Sampler
  VkSamplerCreateInfo sampInfo{};
  sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampInfo.magFilter = VK_FILTER_LINEAR;
  sampInfo.minFilter = VK_FILTER_LINEAR;
  sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.mipLodBias = 0.0f;
  sampInfo.anisotropyEnable = VK_FALSE;
  sampInfo.compareEnable = VK_FALSE;
  sampInfo.minLod = 0.0f;
  sampInfo.maxLod = (float)NUM_MIPS;
  sampInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  if (vkCreateSampler(device, &sampInfo, nullptr, &prefilteredSampler) !=
      VK_SUCCESS) {
    LOG_E("Failed to create prefiltered sampler");
    return false;
  }

  LOG_I("Generated PrefilteredMap ({}x{}, {} mips)", baseSize, baseSize,
        NUM_MIPS);
  return true;
}

bool CubeMapResource::CreateImage(VkDevice device,
                                  VkPhysicalDevice physicalDevice,
                                  uint32_t imgWidth, uint32_t imgHeight,
                                  uint32_t mipCount, VkFormat format,
                                  VkImageTiling tiling, VkImageUsageFlags usage,
                                  VkMemoryPropertyFlags properties) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = imgWidth;
  imageInfo.extent.height = imgHeight;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipCount;
  imageInfo.arrayLayers = 6;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
    LOG_E("Failed to create CubeMap image");
    return false;
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = FindMemoryType(
      physicalDevice, memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
    LOG_E("Failed to allocate CubeMap image memory");
    return false;
  }

  vkBindImageMemory(device, image, memory, 0);
  return true;
}

bool CubeMapResource::CreateImageView(VkDevice device, VkFormat format) {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 6;

  if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
    LOG_E("Failed to create CubeMap image view");
    return false;
  }

  return true;
}

bool CubeMapResource::CreateSampler(VkDevice device,
                                    VkPhysicalDevice physicalDevice) {
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = static_cast<float>(mipLevels);
  samplerInfo.mipLodBias = 0.0f;

  if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
    LOG_E("Failed to create CubeMap sampler");
    return false;
  }

  return true;
}

void CubeMapResource::TransitionImageLayout(VkDevice device,
                                            VkCommandPool commandPool,
                                            VkQueue graphicsQueue,
                                            VkImageLayout oldLayout,
                                            VkImageLayout newLayout) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 6;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void CubeMapResource::CopyBufferToImage(VkDevice device,
                                        VkCommandPool commandPool,
                                        VkQueue graphicsQueue,
                                        VkBuffer buffer) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  std::vector<VkBufferImageCopy> bufferCopyRegions;
  VkDeviceSize layerSize = width * height * 4;
  for (uint32_t i = 0; i < 6; i++) {
    VkBufferImageCopy region{};
    region.bufferOffset = layerSize * i;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = i;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    bufferCopyRegions.push_back(region);
  }

  vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(bufferCopyRegions.size()),
                         bufferCopyRegions.data());

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void CubeMapResource::GenerateMipmaps(VkDevice device,
                                      VkPhysicalDevice physicalDevice,
                                      VkCommandPool commandPool,
                                      VkQueue graphicsQueue, VkFormat format) {
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format,
                                      &formatProperties);

  if (!(formatProperties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    LOG_W("CubeMap format does not support linear blitting!");
    TransitionImageLayout(device, commandPool, graphicsQueue,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return;
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 6;
  barrier.subresourceRange.levelCount = 1;

  int32_t mipWidth = static_cast<int32_t>(width);
  int32_t mipHeight = static_cast<int32_t>(height);

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 6;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                          mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 6;

    vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mipWidth > 1)
      mipWidth /= 2;
    if (mipHeight > 1)
      mipHeight /= 2;
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t CubeMapResource::FindMemoryType(VkPhysicalDevice physicalDevice,
                                         uint32_t typeFilter,
                                         VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  LOG_E("Failed to find suitable memory type for CubeMap");
  return 0;
}

} // namespace neurender
