#include "Asset/ModelImporter.h"
#include "Asset/AssetManager.h"
#include "neuLog.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <glm/glm.hpp>

namespace neurender {

// stb_image_write 回调函数
static void NeuStbiWriteFunc(void *context, void *data, int size) {
  std::ofstream *ofs = static_cast<std::ofstream *>(context);
  ofs->write(static_cast<const char *>(data), size);
}

// 自定义材质读取器，支持 UTF-8 路径 (Windows)
class NeuMaterialFileReader : public tinyobj::MaterialReader {
public:
  explicit NeuMaterialFileReader(const std::string &mtl_basedir)
      : m_mtlBaseDir(mtl_basedir) {}
  virtual bool operator()(const std::string &matId,
                          std::vector<tinyobj::material_t> *materials,
                          std::map<std::string, int> *matMap,
                          std::string *err) override {
    std::string filepath;
    if (!m_mtlBaseDir.empty()) {
      filepath = m_mtlBaseDir + matId;
    } else {
      filepath = matId;
    }

    // 使用 u8path 确保 Windows 下能正确打开包含非 ASCII 字符的文件
    std::ifstream matIStream(std::filesystem::u8path(filepath));
    if (!matIStream) {
      std::stringstream ss;
      ss << "WARN: Material file [ " << filepath << " ] not found."
         << std::endl;
      if (err) {
        (*err) += ss.str();
      }
      return false;
    }

    std::string warning;
    tinyobj::LoadMtl(matMap, materials, &matIStream, &warning);
    if (!warning.empty()) {
      if (err) {
        (*err) += warning;
      }
    }
    return true;
  }

private:
  std::string m_mtlBaseDir;
};

// tinygltf 自定义文件系统回调
static bool NeuFileExists(const std::string &abs_filename, void *user_data) {
  return std::filesystem::exists(std::filesystem::u8path(abs_filename));
}

static std::string NeuExpandFilePath(const std::string &filepath,
                                     void *user_data) {
  return filepath;
}

static bool NeuReadWholeFile(std::vector<unsigned char> *out, std::string *err,
                             const std::string &filepath, void *user_data) {
  std::ifstream ifs(std::filesystem::u8path(filepath),
                    std::ios::binary | std::ios::ate);
  if (!ifs) {
    if (err)
      (*err) = "File not found: " + filepath;
    return false;
  }
  std::streamsize size = ifs.tellg();
  ifs.seekg(0, std::ios::beg);
  out->resize(static_cast<size_t>(size));
  if (!ifs.read(reinterpret_cast<char *>(out->data()), size)) {
    if (err)
      (*err) = "Failed to read file: " + filepath;
    return false;
  }
  return true;
}

static bool NeuGetFileSizeInBytes(size_t *filesize_out, std::string *err,
                                  const std::string &filepath,
                                  void *user_data) {
  try {
    *filesize_out = static_cast<size_t>(
        std::filesystem::file_size(std::filesystem::u8path(filepath)));
    return true;
  } catch (...) {
    if (err)
      (*err) = "Failed to get file size: " + filepath;
    return false;
  }
}

std::string ModelImporter::GetFileExtension(const std::string &filePath) {
  std::filesystem::path path = std::filesystem::u8path(filePath);
  std::string ext = path.extension().u8string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext;
}

std::filesystem::path
ModelImporter::GenerateAssetFileName(const std::string &baseName,
                                     const std::string &extension,
                                     const std::string &assetsDir) {
  std::filesystem::path assetsPathObj = std::filesystem::u8path(assetsDir);
  std::string safeBaseName = baseName;
  // 移除文件名中非法字符
  safeBaseName.erase(std::remove_if(safeBaseName.begin(), safeBaseName.end(),
                                    [](char c) {
                                      return c == '/' || c == '\\' ||
                                             c == ':' || c == '*' || c == '?' ||
                                             c == '\"' || c == '<' ||
                                             c == '>' || c == '|';
                                    }),
                     safeBaseName.end());

  std::string fileName = safeBaseName + extension;
  std::filesystem::path fullPath = assetsPathObj / fileName;

  int counter = 1;
  while (std::filesystem::exists(fullPath)) {
    fileName = safeBaseName + "_" + std::to_string(counter) + extension;
    fullPath = assetsPathObj / fileName;
    counter++;
  }

  return fullPath;
}

ImportResult ModelImporter::Import(const std::string &filePath,
                                   const std::string &assetsDir) {
  std::string ext = GetFileExtension(filePath);

  if (ext == ".gltf" || ext == ".glb") {
    return ImportGLTF(filePath, assetsDir);
  } else if (ext == ".obj") {
    return ImportOBJ(filePath, assetsDir);
  }

  ImportResult result;
  result.success = false;
  result.errorMessage = "Unsupported file format: " + ext;
  return result;
}

ImportResult ModelImporter::ImportToScene(const std::string &filePath,
                                          const std::string &assetsDir,
                                          Scene &scene) {
  ImportResult result = Import(filePath, assetsDir);

  if (result.success) {
    // 将生成的 MeshNode 添加到场景
    for (auto &meshNode : result.meshNodes) {
      scene.AddNode(std::move(meshNode));
    }
    result.meshNodes.clear(); // 已转移所有权
  }

  return result;
}

ImportResult ModelImporter::ImportGLTF(const std::string &filePath,
                                       const std::string &assetsDir) {
  ImportResult result;
  result.success = false;

  try {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    std::string ext = GetFileExtension(filePath);
    bool loadSuccess = false;

    // 设置自定义 FS 回调
    tinygltf::FsCallbacks fsCallbacks;
    fsCallbacks.FileExists = NeuFileExists;
    fsCallbacks.ExpandFilePath = NeuExpandFilePath;
    fsCallbacks.ReadWholeFile = NeuReadWholeFile;
    fsCallbacks.WriteWholeFile = nullptr;
    fsCallbacks.GetFileSizeInBytes = NeuGetFileSizeInBytes;
    fsCallbacks.user_data = nullptr;
    loader.SetFsCallbacks(fsCallbacks);

    std::filesystem::path modelPathObj = std::filesystem::u8path(filePath);
    std::string baseDir = modelPathObj.parent_path().u8string();

    if (ext == ".glb") {
      std::vector<unsigned char> data;
      if (!NeuReadWholeFile(&data, &err, filePath, nullptr)) {
        return result;
      }
      loadSuccess = loader.LoadBinaryFromMemory(
          &model, &err, &warn, data.data(),
          static_cast<unsigned int>(data.size()), baseDir);
    } else {
      std::vector<unsigned char> data;
      if (!NeuReadWholeFile(&data, &err, filePath, nullptr)) {
        return result;
      }
      loadSuccess = loader.LoadASCIIFromString(
          &model, &err, &warn, reinterpret_cast<const char *>(data.data()),
          static_cast<unsigned int>(data.size()), baseDir);
    }

    if (!warn.empty()) {
      LOG_W("GLTF Warning: {}", warn);
    }

    if (!err.empty()) {
      result.errorMessage = err;
      LOG_E("GLTF Error: {}", err);
      return result;
    }

    if (!loadSuccess) {
      result.errorMessage = "Failed to load GLTF file";
      return result;
    }

    // 确保 Assets 目录存在
    std::filesystem::path assetsPathObj = std::filesystem::u8path(assetsDir);
    std::filesystem::create_directories(assetsPathObj);

    // 获取模型基本名称
    std::filesystem::path modelPath = std::filesystem::u8path(filePath);
    std::string baseName = modelPath.stem().u8string();

    // 处理纹理
    for (size_t i = 0; i < model.textures.size(); i++) {
      const tinygltf::Texture &tex = model.textures[i];
      if (tex.source >= 0 &&
          tex.source < static_cast<int>(model.images.size())) {
        const tinygltf::Image &image = model.images[tex.source];

        // 保存纹理到 Assets 目录
        std::string texName = image.name.empty()
                                  ? baseName + "_tex" + std::to_string(i)
                                  : image.name;
        std::filesystem::path texPath =
            GenerateAssetFileName(texName, ".png", assetsDir);

        // 如果图像有数据，保存它
        if (!image.image.empty()) {
          // 使用 std::ofstream 和 u8path 支持 Windows 下的 UTF-8 路径
          std::ofstream ofs(texPath, std::ios::binary);
          if (ofs.is_open()) {
            stbi_write_png_to_func(NeuStbiWriteFunc, &ofs, image.width,
                                   image.height, image.component,
                                   image.image.data(),
                                   image.width * image.component);
            ofs.close();

            // 注册资源 - 使用 path 对象版本，避免 string 转换
            UUID texGUID =
                AssetManager::GetInstance().RegisterAsset(texPath, "texture");
            result.textureIDs.push_back(texGUID);
          } else {
            LOG_E("Failed to save texture: {}", texPath.u8string());
          }
        }
      }
    }

    // 处理材质
    for (size_t i = 0; i < model.materials.size(); i++) {
      const tinygltf::Material &mat = model.materials[i];

      // 保存材质信息到 JSON 文件
      std::string matName =
          mat.name.empty() ? baseName + "_mat" + std::to_string(i) : mat.name;
      std::filesystem::path matPath =
          GenerateAssetFileName(matName, ".mat.json", assetsDir);

      nlohmann::json matJson;
      matJson["name"] = matName;
      matJson["type"] = "Opaque";
      matJson["doubleSided"] = mat.doubleSided;
      matJson["alphaMode"] = mat.alphaMode;
      matJson["alphaCutoff"] = mat.alphaCutoff;

      // PBR 参数
      const auto &pbr = mat.pbrMetallicRoughness;
      matJson["baseColorFactor"] = {
          pbr.baseColorFactor[0], pbr.baseColorFactor[1],
          pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
      matJson["metallicFactor"] = pbr.metallicFactor;
      matJson["roughnessFactor"] = pbr.roughnessFactor;
      matJson["normalScale"] = 1.0f;
      matJson["occlusionStrength"] = 1.0f;

      // 自发光
      matJson["emissiveColor"] = {mat.emissiveFactor[0], mat.emissiveFactor[1],
                                  mat.emissiveFactor[2]};
      matJson["emissiveIntensity"] =
          (mat.emissiveFactor[0] + mat.emissiveFactor[1] +
           mat.emissiveFactor[2]) > 0.0
              ? 1.0f
              : 0.0f;
      matJson["shadingId"] = 0.0f;

      // 纹理引用 - 使用 UUID 而非索引
      if (pbr.baseColorTexture.index >= 0 &&
          pbr.baseColorTexture.index <
              static_cast<int>(result.textureIDs.size())) {
        matJson["baseColorTexture"] =
            result.textureIDs[pbr.baseColorTexture.index].ToString();
      }
      if (pbr.metallicRoughnessTexture.index >= 0 &&
          pbr.metallicRoughnessTexture.index <
              static_cast<int>(result.textureIDs.size())) {
        std::string texIDStr =
            result.textureIDs[pbr.metallicRoughnessTexture.index].ToString();
        matJson["metallicTexture"] = texIDStr;
        matJson["roughnessTexture"] = texIDStr;
      }
      if (mat.normalTexture.index >= 0 &&
          mat.normalTexture.index <
              static_cast<int>(result.textureIDs.size())) {
        matJson["normalTexture"] =
            result.textureIDs[mat.normalTexture.index].ToString();
      }
      if (mat.emissiveTexture.index >= 0 &&
          mat.emissiveTexture.index <
              static_cast<int>(result.textureIDs.size())) {
        matJson["emissiveTexture"] =
            result.textureIDs[mat.emissiveTexture.index].ToString();
      }
      if (mat.occlusionTexture.index >= 0 &&
          mat.occlusionTexture.index <
              static_cast<int>(result.textureIDs.size())) {
        matJson["occlusionTexture"] =
            result.textureIDs[mat.occlusionTexture.index].ToString();
      }

      std::filesystem::path matPathObj(matPath);
      std::ofstream matFile(matPathObj);
      matFile << matJson.dump(2);
      matFile.close();

      UUID matGUID =
          AssetManager::GetInstance().RegisterAsset(matPath, "material");
      result.materialIDs.push_back(matGUID);
    }

    // 处理网格
    for (size_t i = 0; i < model.meshes.size(); i++) {
      const tinygltf::Mesh &mesh = model.meshes[i];

      std::string meshName = mesh.name.empty()
                                 ? baseName + "_mesh" + std::to_string(i)
                                 : mesh.name;

      // 对每个 primitive 创建一个 MeshNode
      for (size_t j = 0; j < mesh.primitives.size(); j++) {
        const tinygltf::Primitive &prim = mesh.primitives[j];

        std::string primName = meshName;
        if (mesh.primitives.size() > 1) {
          primName += "_" + std::to_string(j);
        }

        // 保存网格数据到二进制文件
        std::filesystem::path meshPath =
            GenerateAssetFileName(primName, ".mesh", assetsDir);

        // 提取顶点数据
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> texcoords;
        std::vector<uint32_t> indices;

        // 位置
        auto posIt = prim.attributes.find("POSITION");
        if (posIt != prim.attributes.end()) {
          const tinygltf::Accessor &accessor = model.accessors[posIt->second];
          const tinygltf::BufferView &bufferView =
              model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

          const float *posData = reinterpret_cast<const float *>(
              buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);

          positions.resize(accessor.count * 3);
          for (size_t k = 0; k < accessor.count * 3; k++) {
            positions[k] = posData[k];
          }
        }

        // 法线
        auto normIt = prim.attributes.find("NORMAL");
        if (normIt != prim.attributes.end()) {
          const tinygltf::Accessor &accessor = model.accessors[normIt->second];
          const tinygltf::BufferView &bufferView =
              model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

          const float *normData = reinterpret_cast<const float *>(
              buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);

          normals.resize(accessor.count * 3);
          for (size_t k = 0; k < accessor.count * 3; k++) {
            normals[k] = normData[k];
          }
        }

        // 纹理坐标
        auto texIt = prim.attributes.find("TEXCOORD_0");
        if (texIt != prim.attributes.end()) {
          const tinygltf::Accessor &accessor = model.accessors[texIt->second];
          const tinygltf::BufferView &bufferView =
              model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

          const float *texData = reinterpret_cast<const float *>(
              buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);

          texcoords.resize(accessor.count * 2);
          for (size_t k = 0; k < accessor.count * 2; k++) {
            texcoords[k] = texData[k];
          }
        }

        // 切线
        std::vector<float> tangents;
        auto tanIt = prim.attributes.find("TANGENT");
        if (tanIt != prim.attributes.end()) {
          const tinygltf::Accessor &accessor = model.accessors[tanIt->second];
          const tinygltf::BufferView &bufferView =
              model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

          const float *tanData = reinterpret_cast<const float *>(
              buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);

          tangents.resize(accessor.count * 4);
          for (size_t k = 0; k < accessor.count * 4; k++) {
            tangents[k] = tanData[k];
          }
        }

        // 索引
        if (prim.indices >= 0) {
          const tinygltf::Accessor &accessor = model.accessors[prim.indices];
          const tinygltf::BufferView &bufferView =
              model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

          indices.resize(accessor.count);

          const unsigned char *indexData =
              buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

          if (accessor.componentType ==
              TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t *shortIndices =
                reinterpret_cast<const uint16_t *>(indexData);
            for (size_t k = 0; k < accessor.count; k++) {
              indices[k] = shortIndices[k];
            }
          } else if (accessor.componentType ==
                     TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            const uint32_t *intIndices =
                reinterpret_cast<const uint32_t *>(indexData);
            for (size_t k = 0; k < accessor.count; k++) {
              indices[k] = intIndices[k];
            }
          }
        }

        // 保存网格数据到文件 (简单的二进制格式)
        std::ofstream meshFile(meshPath, std::ios::binary);
        if (!meshFile.is_open()) {
          LOG_E("Failed to create mesh file: {}", meshPath.u8string());
          continue;
        }

        // 写入顶点数量
        uint32_t vertexCount = static_cast<uint32_t>(positions.size() / 3);
        meshFile.write(reinterpret_cast<const char *>(&vertexCount),
                       sizeof(uint32_t));

        // 写入索引数量
        uint32_t indexCount = static_cast<uint32_t>(indices.size());
        meshFile.write(reinterpret_cast<const char *>(&indexCount),
                       sizeof(uint32_t));

        // 写入位置数据
        meshFile.write(reinterpret_cast<const char *>(positions.data()),
                       positions.size() * sizeof(float));

        uint32_t hasNormalsFlag = !normals.empty() ? 1 : 0;
        meshFile.write(reinterpret_cast<const char *>(&hasNormalsFlag),
                       sizeof(uint32_t));
        if (!normals.empty()) {
          meshFile.write(reinterpret_cast<const char *>(normals.data()),
                         normals.size() * sizeof(float));
        }

        uint32_t hasTexcoordsFlag = !texcoords.empty() ? 1 : 0;
        meshFile.write(reinterpret_cast<const char *>(&hasTexcoordsFlag),
                       sizeof(uint32_t));
        if (!texcoords.empty()) {
          meshFile.write(reinterpret_cast<const char *>(texcoords.data()),
                         texcoords.size() * sizeof(float));
        }

        uint32_t hasColorsFlag = 0; // GLTF 导入暂不支持顶点颜色
        meshFile.write(reinterpret_cast<const char *>(&hasColorsFlag),
                       sizeof(uint32_t));

        uint32_t hasTangentsFlag = !tangents.empty() ? 1 : 0;
        meshFile.write(reinterpret_cast<const char *>(&hasTangentsFlag),
                       sizeof(uint32_t));
        if (!tangents.empty()) {
          meshFile.write(reinterpret_cast<const char *>(tangents.data()),
                         tangents.size() * sizeof(float));
        }

        // 写入索引数据
        meshFile.write(reinterpret_cast<const char *>(indices.data()),
                       indices.size() * sizeof(uint32_t));

        meshFile.close();

        // 注册资源
        UUID meshGUID =
            AssetManager::GetInstance().RegisterAsset(meshPath, "mesh");
        result.meshIDs.push_back(meshGUID);

        // 创建 MeshNode
        auto meshNode = std::make_unique<MeshNode>(primName);
        meshNode->SetMeshID(meshGUID);

        // 设置材质
        if (prim.material >= 0 &&
            prim.material < static_cast<int>(result.materialIDs.size())) {
          meshNode->SetMaterialID(result.materialIDs[prim.material]);
        }

        result.meshNodes.push_back(std::move(meshNode));
      }
    }

    result.success = true;
    LOG_I("Imported GLTF: {} ({} meshes, {} materials, {} textures)", filePath,
          result.meshIDs.size(), result.materialIDs.size(),
          result.textureIDs.size());

    return result;
  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = std::string("GLTF Import Exception: ") + e.what();
    LOG_E("{}", result.errorMessage);
    return result;
  }
}

ImportResult ModelImporter::ImportOBJ(const std::string &filePath,
                                      const std::string &assetsDir) {
  ImportResult result;
  result.success = false;

  try {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    // 获取模型目录用于加载 MTL 文件
    std::filesystem::path modelPath = std::filesystem::u8path(filePath);
    std::string baseDir = modelPath.parent_path().u8string();
    if (!baseDir.empty())
      baseDir += "/";
    std::string baseName = modelPath.stem().u8string();

    std::ifstream ifs(modelPath);
    if (!ifs.is_open()) {
      result.errorMessage = "Failed to open OBJ file: " + filePath;
      LOG_E("{}", result.errorMessage);
      return result;
    }

    NeuMaterialFileReader matFileReader(baseDir);
    // LoadObj 签名: (attrib*, shapes*, materials*, err*, instream,
    // matReader, triangulate)
    bool loadSuccess = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
                                        &ifs, &matFileReader, true);

    if (!err.empty()) {
      // err 可能包含警告和错误信息
      LOG_W("OBJ Info: {}", err);
    }

    if (!loadSuccess) {
      result.errorMessage = err.empty() ? "Failed to load OBJ file" : err;
      LOG_E("OBJ Error: {}", result.errorMessage);
      return result;
    }

    // 确保 Assets 目录存在
    std::filesystem::path assetsPathObj = std::filesystem::u8path(assetsDir);
    std::filesystem::create_directories(assetsPathObj);

    // 处理材质
    for (size_t i = 0; i < materials.size(); i++) {
      const tinyobj::material_t &mat = materials[i];

      // 保存材质信息到 JSON 文件
      std::string matName =
          mat.name.empty() ? baseName + "_mat" + std::to_string(i) : mat.name;
      std::filesystem::path matPath =
          GenerateAssetFileName(matName, ".mat.json", assetsDir);

      nlohmann::json matJson;
      matJson["name"] = matName;
      matJson["baseColorFactor"] = {mat.diffuse[0], mat.diffuse[1],
                                    mat.diffuse[2], 1.0f};
      matJson["metallicFactor"] = mat.metallic;
      matJson["roughnessFactor"] = mat.roughness;

      auto importTexture = [&](const std::string &texName) -> std::string {
        if (texName.empty())
          return "";
        std::filesystem::path srcPath =
            std::filesystem::u8path(baseDir) / std::filesystem::u8path(texName);
        if (std::filesystem::exists(srcPath)) {
          std::filesystem::path dstPath = GenerateAssetFileName(
              std::filesystem::u8path(texName).stem().u8string(),
              std::filesystem::u8path(texName).extension().u8string(),
              assetsDir);
          try {
            std::filesystem::copy_file(
                srcPath, dstPath,
                std::filesystem::copy_options::overwrite_existing);
            UUID guid =
                AssetManager::GetInstance().RegisterAsset(dstPath, "texture");
            result.textureIDs.push_back(guid);
            return guid.ToString();
          } catch (...) {
            return "";
          }
        }
        return "";
      };

      // 纹理路径处理
      std::string diffuseTexId = importTexture(mat.diffuse_texname);
      if (!diffuseTexId.empty()) {
        matJson["baseColorTexture"] = diffuseTexId;
      }

      std::string normalTexId = importTexture(mat.bump_texname);
      if (normalTexId.empty()) {
        normalTexId = importTexture(mat.normal_texname);
      }
      if (!normalTexId.empty()) {
        matJson["normalTexture"] = normalTexId;
      }

      std::string specTexId = importTexture(mat.specular_texname);
      if (!specTexId.empty()) {
        matJson["metallicRoughnessTexture"] = specTexId;
      }

      std::ofstream matFile(matPath);
      matFile << matJson.dump(2);
      matFile.close();

      UUID matGUID =
          AssetManager::GetInstance().RegisterAsset(matPath, "material");
      result.materialIDs.push_back(matGUID);
    }

    // 处理形状
    for (size_t s = 0; s < shapes.size(); s++) {
      const tinyobj::shape_t &shape = shapes[s];

      std::string shapeName = shape.name.empty()
                                  ? baseName + "_shape" + std::to_string(s)
                                  : shape.name;
      std::filesystem::path meshPath =
          GenerateAssetFileName(shapeName, ".mesh", assetsDir);

      // 提取顶点数据
      std::vector<float> positions;
      std::vector<float> normals;
      std::vector<float> texcoords;
      std::vector<uint32_t> indices;

      // 遍历面
      size_t indexOffset = 0;
      for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
        size_t fv = shape.mesh.num_face_vertices[f];

        // 提前计算面法线，作为无顶点法线时的回退
        glm::vec3 faceNormal(0.0f, 1.0f, 0.0f);
        if (fv >= 3) {
          tinyobj::index_t idx0 = shape.mesh.indices[indexOffset + 0];
          tinyobj::index_t idx1 = shape.mesh.indices[indexOffset + 1];
          tinyobj::index_t idx2 = shape.mesh.indices[indexOffset + 2];

          glm::vec3 v0(attrib.vertices[3 * idx0.vertex_index + 0],
                       attrib.vertices[3 * idx0.vertex_index + 1],
                       attrib.vertices[3 * idx0.vertex_index + 2]);
          glm::vec3 v1(attrib.vertices[3 * idx1.vertex_index + 0],
                       attrib.vertices[3 * idx1.vertex_index + 1],
                       attrib.vertices[3 * idx1.vertex_index + 2]);
          glm::vec3 v2(attrib.vertices[3 * idx2.vertex_index + 0],
                       attrib.vertices[3 * idx2.vertex_index + 1],
                       attrib.vertices[3 * idx2.vertex_index + 2]);

          glm::vec3 edge1 = v1 - v0;
          glm::vec3 edge2 = v2 - v0;
          if (glm::length(edge1) > 1e-6f && glm::length(edge2) > 1e-6f) {
            faceNormal = glm::normalize(glm::cross(edge1, edge2));
          }
        }

        for (size_t v = 0; v < fv; v++) {
          tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

          // 位置 (必有)
          positions.push_back(attrib.vertices[3 * idx.vertex_index + 0]);
          positions.push_back(attrib.vertices[3 * idx.vertex_index + 1]);
          positions.push_back(attrib.vertices[3 * idx.vertex_index + 2]);

          // 法线 - 如果没有或法线为0则使用面法线
          bool validNormal = false;
          if (idx.normal_index >= 0) {
            float nx = attrib.normals[3 * idx.normal_index + 0];
            float ny = attrib.normals[3 * idx.normal_index + 1];
            float nz = attrib.normals[3 * idx.normal_index + 2];

            if (std::abs(nx) > 1e-6f || std::abs(ny) > 1e-6f ||
                std::abs(nz) > 1e-6f) {
              normals.push_back(nx);
              normals.push_back(ny);
              normals.push_back(nz);
              validNormal = true;
            }
          }

          if (!validNormal) {
            normals.push_back(faceNormal.x);
            normals.push_back(faceNormal.y);
            normals.push_back(faceNormal.z);
          }

          // 纹理坐标 - 必须翻转 V 以适应 Vulkan
          if (idx.texcoord_index >= 0) {
            texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 0]);
            texcoords.push_back(1.0f -
                                attrib.texcoords[2 * idx.texcoord_index + 1]);
          } else {
            texcoords.push_back(0.0f);
            texcoords.push_back(0.0f);
          }

          indices.push_back(static_cast<uint32_t>(indexOffset + v));
        }

        indexOffset += fv;
      }

      // 保存网格数据
      std::ofstream meshFile(meshPath, std::ios::binary);

      uint32_t vertexCount = static_cast<uint32_t>(positions.size() / 3);
      meshFile.write(reinterpret_cast<const char *>(&vertexCount),
                     sizeof(uint32_t));

      uint32_t indexCount = static_cast<uint32_t>(indices.size());
      meshFile.write(reinterpret_cast<const char *>(&indexCount),
                     sizeof(uint32_t));

      meshFile.write(reinterpret_cast<const char *>(positions.data()),
                     positions.size() * sizeof(float));

      uint32_t hasNormalsFlag = 1; // 总是包含法线（可能是生成的）
      meshFile.write(reinterpret_cast<const char *>(&hasNormalsFlag),
                     sizeof(uint32_t));
      meshFile.write(reinterpret_cast<const char *>(normals.data()),
                     normals.size() * sizeof(float));

      uint32_t hasTexcoordsFlag = 1;
      meshFile.write(reinterpret_cast<const char *>(&hasTexcoordsFlag),
                     sizeof(uint32_t));
      meshFile.write(reinterpret_cast<const char *>(texcoords.data()),
                     texcoords.size() * sizeof(float));

      uint32_t hasColorsFlag = 0; // 不支持顶点颜色，写为0
      meshFile.write(reinterpret_cast<const char *>(&hasColorsFlag),
                     sizeof(uint32_t));

      uint32_t hasTangentsFlag = 0; // OBJ 导入暂不支持生成切线
      meshFile.write(reinterpret_cast<const char *>(&hasTangentsFlag),
                     sizeof(uint32_t));

      meshFile.write(reinterpret_cast<const char *>(indices.data()),
                     indices.size() * sizeof(uint32_t));

      meshFile.close();

      // 注册资源
      UUID meshGUID =
          AssetManager::GetInstance().RegisterAsset(meshPath, "mesh");
      result.meshIDs.push_back(meshGUID);

      // 创建 MeshNode
      auto meshNode = std::make_unique<MeshNode>(shapeName);
      meshNode->SetMeshID(meshGUID);

      // 设置材质 (使用第一个面的材质)
      if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids[0] >= 0) {
        int matIdx = shape.mesh.material_ids[0];
        if (matIdx < static_cast<int>(result.materialIDs.size())) {
          meshNode->SetMaterialID(result.materialIDs[matIdx]);
        }
      }

      result.meshNodes.push_back(std::move(meshNode));
    }

    result.success = true;
    LOG_I("Imported OBJ: {} ({} shapes, {} materials)", filePath,
          result.meshIDs.size(), result.materialIDs.size());

    return result;
  } catch (const std::exception &e) {
    result.success = false;
    result.errorMessage = std::string("OBJ Import Exception: ") + e.what();
    LOG_E("{}", result.errorMessage);
    return result;
  }
}

} // namespace neurender
