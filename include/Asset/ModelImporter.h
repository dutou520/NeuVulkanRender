#pragma once
#include "Core/UUID.h"
#include "Nodes/MeshNode.h"
#include "Scene/Scene.h"
#include <memory>
#include <string>
#include <vector>


namespace neurender {

/**
 * @brief 模型导入结果
 */
struct ImportResult {
  bool success = false;
  std::string errorMessage;
  std::vector<UUID> meshIDs;                        // 导入的 Mesh 资源 GUID
  std::vector<UUID> materialIDs;                    // 导入的 Material 资源 GUID
  std::vector<UUID> textureIDs;                     // 导入的 Texture 资源 GUID
  std::vector<std::unique_ptr<MeshNode>> meshNodes; // 生成的 MeshNode
};

/**
 * @brief ModelImporter 模型导入器
 * 支持 GLTF/GLB/OBJ 格式
 */
class ModelImporter {
public:
  ModelImporter() = default;
  ~ModelImporter() = default;

  /**
   * @brief 导入模型文件
   * @param filePath 模型文件路径 (支持 .gltf, .glb, .obj)
   * @param assetsDir Assets 目录路径，用于存放解构的资源
   * @return 导入结果
   */
  ImportResult Import(const std::string &filePath,
                      const std::string &assetsDir);

  /**
   * @brief 导入模型并添加到场景
   * @param filePath 模型文件路径
   * @param assetsDir Assets 目录路径
   * @param scene 目标场景
   * @return 导入结果
   */
  ImportResult ImportToScene(const std::string &filePath,
                             const std::string &assetsDir, Scene &scene);

private:
  // GLTF/GLB 导入
  ImportResult ImportGLTF(const std::string &filePath,
                          const std::string &assetsDir);

  // OBJ 导入
  ImportResult ImportOBJ(const std::string &filePath,
                         const std::string &assetsDir);

  // 获取文件扩展名 (小写)
  std::string GetFileExtension(const std::string &filePath);

  // 生成唯一的资源文件名
  std::string GenerateAssetFileName(const std::string &baseName,
                                    const std::string &extension,
                                    const std::string &assetsDir);
};

} // namespace neurender
