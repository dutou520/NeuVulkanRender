#include "Asset/ModelImporter.h"
#include "Asset/AssetManager.h"
#include "neuLog.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace neurender {

std::string ModelImporter::GetFileExtension(const std::string &filePath) {
  std::filesystem::path path(filePath);
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext;
}

std::string ModelImporter::GenerateAssetFileName(const std::string &baseName,
                                                 const std::string &extension,
                                                 const std::string &assetsDir) {
  std::string fileName = baseName + extension;
  std::string fullPath = assetsDir + "/" + fileName;

  int counter = 1;
  while (std::filesystem::exists(fullPath)) {
    fileName = baseName + "_" + std::to_string(counter) + extension;
    fullPath = assetsDir + "/" + fileName;
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

  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err, warn;

  std::string ext = GetFileExtension(filePath);
  bool loadSuccess = false;

  if (ext == ".glb") {
    loadSuccess = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
  } else {
    loadSuccess = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);
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
  std::filesystem::create_directories(assetsDir);

  // 获取模型基本名称
  std::filesystem::path modelPath(filePath);
  std::string baseName = modelPath.stem().string();

  // 处理纹理
  for (size_t i = 0; i < model.textures.size(); i++) {
    const tinygltf::Texture &tex = model.textures[i];
    if (tex.source >= 0 && tex.source < static_cast<int>(model.images.size())) {
      const tinygltf::Image &image = model.images[tex.source];

      // 保存纹理到 Assets 目录
      std::string texName = image.name.empty()
                                ? baseName + "_tex" + std::to_string(i)
                                : image.name;
      std::string texPath = GenerateAssetFileName(texName, ".png", assetsDir);

      // 如果图像有数据，保存它
      if (!image.image.empty()) {
        stbi_write_png(texPath.c_str(), image.width, image.height,
                       image.component, image.image.data(),
                       image.width * image.component);

        // 注册资源
        UUID texGUID =
            AssetManager::GetInstance().RegisterAsset(texPath, "texture");
        result.textureIDs.push_back(texGUID);
      }
    }
  }

  // 处理材质
  for (size_t i = 0; i < model.materials.size(); i++) {
    const tinygltf::Material &mat = model.materials[i];

    // 保存材质信息到 JSON 文件
    std::string matName =
        mat.name.empty() ? baseName + "_mat" + std::to_string(i) : mat.name;
    std::string matPath =
        GenerateAssetFileName(matName, ".mat.json", assetsDir);

    nlohmann::json matJson;
    matJson["name"] = matName;
    matJson["doubleSided"] = mat.doubleSided;
    matJson["alphaMode"] = mat.alphaMode;
    matJson["alphaCutoff"] = mat.alphaCutoff;

    // PBR 参数
    const auto &pbr = mat.pbrMetallicRoughness;
    matJson["baseColorFactor"] = {
        pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2],
        pbr.baseColorFactor[3]};
    matJson["metallicFactor"] = pbr.metallicFactor;
    matJson["roughnessFactor"] = pbr.roughnessFactor;

    // 纹理引用
    if (pbr.baseColorTexture.index >= 0) {
      matJson["baseColorTextureIndex"] = pbr.baseColorTexture.index;
    }
    if (pbr.metallicRoughnessTexture.index >= 0) {
      matJson["metallicRoughnessTextureIndex"] =
          pbr.metallicRoughnessTexture.index;
    }
    if (mat.normalTexture.index >= 0) {
      matJson["normalTextureIndex"] = mat.normalTexture.index;
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

    std::string meshName =
        mesh.name.empty() ? baseName + "_mesh" + std::to_string(i) : mesh.name;

    // 对每个 primitive 创建一个 MeshNode
    for (size_t j = 0; j < mesh.primitives.size(); j++) {
      const tinygltf::Primitive &prim = mesh.primitives[j];

      std::string primName = meshName;
      if (mesh.primitives.size() > 1) {
        primName += "_" + std::to_string(j);
      }

      // 保存网格数据到二进制文件
      std::string meshPath =
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

      // 索引
      if (prim.indices >= 0) {
        const tinygltf::Accessor &accessor = model.accessors[prim.indices];
        const tinygltf::BufferView &bufferView =
            model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

        indices.resize(accessor.count);

        const unsigned char *indexData =
            buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
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

      // 写入法线数据
      bool hasNormals = !normals.empty();
      meshFile.write(reinterpret_cast<const char *>(&hasNormals), sizeof(bool));
      if (hasNormals) {
        meshFile.write(reinterpret_cast<const char *>(normals.data()),
                       normals.size() * sizeof(float));
      }

      // 写入纹理坐标
      bool hasTexcoords = !texcoords.empty();
      meshFile.write(reinterpret_cast<const char *>(&hasTexcoords),
                     sizeof(bool));
      if (hasTexcoords) {
        meshFile.write(reinterpret_cast<const char *>(texcoords.data()),
                       texcoords.size() * sizeof(float));
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
}

ImportResult ModelImporter::ImportOBJ(const std::string &filePath,
                                      const std::string &assetsDir) {
  ImportResult result;
  result.success = false;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;

  // 获取模型目录用于加载 MTL 文件
  std::filesystem::path modelPath(filePath);
  std::string baseDir = modelPath.parent_path().string() + "/";
  std::string baseName = modelPath.stem().string();

  // LoadObj 签名: (attrib*, shapes*, materials*, err*, filename, mtl_basedir,
  // triangulate)
  bool loadSuccess = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
                                      filePath.c_str(), baseDir.c_str(), true);

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
  std::filesystem::create_directories(assetsDir);

  // 处理材质
  for (size_t i = 0; i < materials.size(); i++) {
    const tinyobj::material_t &mat = materials[i];

    std::string matName =
        mat.name.empty() ? baseName + "_mat" + std::to_string(i) : mat.name;
    std::string matPath =
        GenerateAssetFileName(matName, ".mat.json", assetsDir);

    nlohmann::json matJson;
    matJson["name"] = matName;
    matJson["baseColorFactor"] = {mat.diffuse[0], mat.diffuse[1],
                                  mat.diffuse[2], 1.0f};
    matJson["metallicFactor"] = mat.metallic;
    matJson["roughnessFactor"] = mat.roughness;

    // 纹理路径
    if (!mat.diffuse_texname.empty()) {
      matJson["diffuseTexture"] = mat.diffuse_texname;
    }
    if (!mat.normal_texname.empty()) {
      matJson["normalTexture"] = mat.normal_texname;
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
    std::string meshPath = GenerateAssetFileName(shapeName, ".mesh", assetsDir);

    // 提取顶点数据
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<uint32_t> indices;

    // 遍历面
    size_t indexOffset = 0;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      size_t fv = shape.mesh.num_face_vertices[f];

      for (size_t v = 0; v < fv; v++) {
        tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

        // 位置
        positions.push_back(attrib.vertices[3 * idx.vertex_index + 0]);
        positions.push_back(attrib.vertices[3 * idx.vertex_index + 1]);
        positions.push_back(attrib.vertices[3 * idx.vertex_index + 2]);

        // 法线
        if (idx.normal_index >= 0) {
          normals.push_back(attrib.normals[3 * idx.normal_index + 0]);
          normals.push_back(attrib.normals[3 * idx.normal_index + 1]);
          normals.push_back(attrib.normals[3 * idx.normal_index + 2]);
        }

        // 纹理坐标
        if (idx.texcoord_index >= 0) {
          texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 0]);
          texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 1]);
        }

        indices.push_back(static_cast<uint32_t>(indexOffset + v));
      }

      indexOffset += fv;
    }

    // 保存网格数据
    std::filesystem::path meshPathObj(meshPath);
    std::ofstream meshFile(meshPathObj, std::ios::binary);

    uint32_t vertexCount = static_cast<uint32_t>(positions.size() / 3);
    meshFile.write(reinterpret_cast<const char *>(&vertexCount),
                   sizeof(uint32_t));

    uint32_t indexCount = static_cast<uint32_t>(indices.size());
    meshFile.write(reinterpret_cast<const char *>(&indexCount),
                   sizeof(uint32_t));

    meshFile.write(reinterpret_cast<const char *>(positions.data()),
                   positions.size() * sizeof(float));

    bool hasNormals = !normals.empty();
    meshFile.write(reinterpret_cast<const char *>(&hasNormals), sizeof(bool));
    if (hasNormals) {
      meshFile.write(reinterpret_cast<const char *>(normals.data()),
                     normals.size() * sizeof(float));
    }

    bool hasTexcoords = !texcoords.empty();
    meshFile.write(reinterpret_cast<const char *>(&hasTexcoords), sizeof(bool));
    if (hasTexcoords) {
      meshFile.write(reinterpret_cast<const char *>(texcoords.data()),
                     texcoords.size() * sizeof(float));
    }

    meshFile.write(reinterpret_cast<const char *>(indices.data()),
                   indices.size() * sizeof(uint32_t));

    meshFile.close();

    // 注册资源
    UUID meshGUID = AssetManager::GetInstance().RegisterAsset(meshPath, "mesh");
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
}

} // namespace neurender
