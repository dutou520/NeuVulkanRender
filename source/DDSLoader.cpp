#include "Asset/DDSLoader.h"
#include "neuLog.h"
#include <cstring>
#include <filesystem>
#include <fstream>

namespace neurender {

// DDS 最小结构定义
constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

// DDPF flags
constexpr uint32_t DDPF_FOURCC = 0x4;
constexpr uint32_t DDPF_RGB = 0x40;
constexpr uint32_t DDPF_ALPHA = 0x1;
constexpr uint32_t DDPF_ALPHAPIXELS = 0x1;

// D3DFORMAT 旧式 FourCC 值
constexpr uint32_t D3DFMT_A16B16G16R16F = 113; // RGBA16F
constexpr uint32_t D3DFMT_G16R16F = 112;       // RG16F (G=高位, R=低位)
constexpr uint32_t D3DFMT_R16F = 111;          // R16F

// DXGI_FORMAT 枚举片段（DX10 头）
constexpr uint32_t DXGI_FORMAT_R32G32B32A32_FLOAT = 2;
constexpr uint32_t DXGI_FORMAT_R16G16B16A16_FLOAT = 10;
constexpr uint32_t DXGI_FORMAT_R16G16_FLOAT = 34;
constexpr uint32_t DXGI_FORMAT_R32G32_FLOAT = 16;
constexpr uint32_t DXGI_FORMAT_R8G8B8A8_UNORM = 28;
constexpr uint32_t DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29;

struct DDS_PIXELFORMAT {
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwFourCC;
  uint32_t dwRGBBitCount;
  uint32_t dwRBitMask;
  uint32_t dwGBitMask;
  uint32_t dwBBitMask;
  uint32_t dwABitMask;
};

struct DDS_HEADER {
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwHeight;
  uint32_t dwWidth;
  uint32_t dwPitchOrLinearSize;
  uint32_t dwDepth;
  uint32_t dwMipMapCount;
  uint32_t dwReserved1[11];
  DDS_PIXELFORMAT ddspf;
  uint32_t dwCaps;
  uint32_t dwCaps2;
  uint32_t dwCaps3;
  uint32_t dwCaps4;
  uint32_t dwReserved2;
};

struct DDS_HEADER_DXT10 {
  uint32_t dxgiFormat;
  uint32_t resourceDimension;
  uint32_t miscFlag;
  uint32_t arraySize;
  uint32_t miscFlags2;
};

bool DDSLoader::Load(const std::string &path, DDSImage &outImage) {
  std::ifstream ifs(std::filesystem::u8path(path),
                    std::ios::binary | std::ios::ate);
  if (!ifs) {
    LOG_E("Failed to open DDS file: {}", path);
    return false;
  }

  std::streamsize fileSize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  if (fileSize < 128) {
    LOG_E("DDS file too small: {}", path);
    return false;
  }

  uint32_t magic = 0;
  ifs.read(reinterpret_cast<char *>(&magic), 4);
  if (magic != DDS_MAGIC) {
    LOG_E("Invalid DDS magic in file: {}", path);
    return false;
  }

  DDS_HEADER header;
  ifs.read(reinterpret_cast<char *>(&header), sizeof(DDS_HEADER));

  outImage.width = header.dwWidth;
  outImage.height = header.dwHeight;
  outImage.mipMapCount = (header.dwMipMapCount == 0) ? 1 : header.dwMipMapCount;

  bool isDX10 = false;
  DDS_HEADER_DXT10 headerDX10{};

  // 检查 FourCC 是否为 'DX10'
  constexpr uint32_t fourCC_DX10 = 0x30315844; // "DX10" in little-endian
  if ((header.ddspf.dwFlags & DDPF_FOURCC) &&
      header.ddspf.dwFourCC == fourCC_DX10) {
    isDX10 = true;
    ifs.read(reinterpret_cast<char *>(&headerDX10), sizeof(DDS_HEADER_DXT10));
  }

  if (isDX10) {
    // ---- DX10 扩展头格式 ----
    switch (headerDX10.dxgiFormat) {
    case DXGI_FORMAT_R16G16_FLOAT:
      outImage.format = VK_FORMAT_R16G16_SFLOAT;
      break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      outImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;
      break;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
      outImage.format = VK_FORMAT_R32G32B32A32_SFLOAT;
      break;
    case DXGI_FORMAT_R32G32_FLOAT:
      outImage.format = VK_FORMAT_R32G32_SFLOAT;
      break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      outImage.format = VK_FORMAT_R8G8B8A8_UNORM;
      break;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      outImage.format = VK_FORMAT_R8G8B8A8_SRGB;
      break;
    default:
      LOG_E("Unsupported DXGI format {} in DDS: {}", headerDX10.dxgiFormat,
            path);
      return false;
    }
  } else if (header.ddspf.dwFlags & DDPF_FOURCC) {
    // ---- 旧式 FourCC 格式 (非 DX10) ----
    switch (header.ddspf.dwFourCC) {
    case D3DFMT_G16R16F: // 112: RG16 float
      outImage.format = VK_FORMAT_R16G16_SFLOAT;
      break;
    case D3DFMT_A16B16G16R16F: // 113: RGBA16 float
      outImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;
      break;
    case D3DFMT_R16F: // 111: R16 float
      outImage.format = VK_FORMAT_R16_SFLOAT;
      break;
    default:
      LOG_E("Unsupported legacy DDS FourCC {} in: {}", header.ddspf.dwFourCC,
            path);
      return false;
    }
  } else {
    // ---- 无 FourCC，通过位数判断 (旧式 uncompressed) ----
    if (header.ddspf.dwRGBBitCount == 32 &&
        (header.ddspf.dwFlags & DDPF_ALPHAPIXELS)) {
      outImage.format = VK_FORMAT_R8G8B8A8_UNORM;
    } else if (header.ddspf.dwRGBBitCount == 32) {
      outImage.format = VK_FORMAT_R8G8B8A8_UNORM;
    } else {
      LOG_E("Unsupported pixel format (RGBBitCount={}) in DDS: {}",
            header.ddspf.dwRGBBitCount, path);
      return false;
    }
  }

  uint32_t dataOffset =
      isDX10 ? (4 + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10))
             : (4 + sizeof(DDS_HEADER));
  size_t dataSize = static_cast<size_t>(fileSize) - dataOffset;

  outImage.data.resize(dataSize);
  ifs.seekg(dataOffset, std::ios::beg);
  ifs.read(reinterpret_cast<char *>(outImage.data.data()), dataSize);

  LOG_I("Loaded DDS: {} ({}x{}, Mips:{}, Format:{}, FourCC:{})", path,
        outImage.width, outImage.height, outImage.mipMapCount,
        static_cast<int>(outImage.format), header.ddspf.dwFourCC);
  return true;
}

} // namespace neurender
