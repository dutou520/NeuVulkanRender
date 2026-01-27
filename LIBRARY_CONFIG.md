# 库配置说明

本文档说明了项目中已配置的第三方库及其使用方法。

## 已配置的库

### 1. GLM (OpenGL Mathematics)
- **版本**: 通过vcpkg安装
- **类型**: Header-only数学库
- **用途**: 提供向量、矩阵等图形学相关的数学运算
- **链接方式**: `glm::glm`

#### 使用示例:
```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// 创建向量
glm::vec3 position(0.0f, 0.0f, 0.0f);
glm::vec4 color(1.0f, 0.0f, 0.0f, 1.0f);

// 矩阵变换
glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
glm::mat4 view = glm::lookAt(
    glm::vec3(0.0f, 0.0f, 3.0f),  // 相机位置
    glm::vec3(0.0f, 0.0f, 0.0f),  // 看向的点
    glm::vec3(0.0f, 1.0f, 0.0f)   // 上方向
);
glm::mat4 projection = glm::perspective(
    glm::radians(45.0f),  // FOV
    800.0f / 600.0f,      // 宽高比
    0.1f,                 // 近平面
    100.0f                // 远平面
);
```

### 2. STB Image
- **位置**: `ThirParty/stb/stb_image.h`
- **类型**: Header-only图像加载库
- **用途**: 加载各种格式的图像文件（JPG, PNG, BMP, TGA等）
- **链接方式**: `stb_image` 或 `ThirPartyLib`

#### 使用示例:
```cpp
// 在一个.cpp文件中定义实现
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// 加载图像
int width, height, channels;
unsigned char* data = stbi_load("texture.jpg", &width, &height, &channels, 0);
if (data) {
    // 使用图像数据
    // ...
    
    // 释放内存
    stbi_image_free(data);
} else {
    // 处理加载失败
}

// 强制加载为RGBA格式
unsigned char* rgba_data = stbi_load("texture.png", &width, &height, &channels, STBI_rgb_alpha);
```

### 3. TinyObjLoader
- **位置**: `ThirParty/tinyobjloader/tiny_obj_loader.h`
- **类型**: Header-only OBJ文件加载库
- **用途**: 加载Wavefront OBJ格式的3D模型
- **链接方式**: `tinyobjloader` 或 `ThirPartyLib`

#### 使用示例:
```cpp
// 在一个.cpp文件中定义实现
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

// 加载OBJ模型
tinyobj::attrib_t attrib;
std::vector<tinyobj::shape_t> shapes;
std::vector<tinyobj::material_t> materials;
std::string warn, err;

bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "model.obj");

if (!warn.empty()) {
    std::cout << "警告: " << warn << std::endl;
}

if (!err.empty()) {
    std::cerr << "错误: " << err << std::endl;
}

if (!ret) {
    // 加载失败
    return;
}

// 遍历所有形状
for (const auto& shape : shapes) {
    // 遍历所有面
    size_t index_offset = 0;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
        int fv = shape.mesh.num_face_vertices[f];
        
        // 遍历面的顶点
        for (size_t v = 0; v < fv; v++) {
            tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
            
            // 获取顶点数据
            float vx = attrib.vertices[3 * idx.vertex_index + 0];
            float vy = attrib.vertices[3 * idx.vertex_index + 1];
            float vz = attrib.vertices[3 * idx.vertex_index + 2];
            
            // 获取法线（如果有）
            if (idx.normal_index >= 0) {
                float nx = attrib.normals[3 * idx.normal_index + 0];
                float ny = attrib.normals[3 * idx.normal_index + 1];
                float nz = attrib.normals[3 * idx.normal_index + 2];
            }
            
            // 获取纹理坐标（如果有）
            if (idx.texcoord_index >= 0) {
                float tx = attrib.texcoords[2 * idx.texcoord_index + 0];
                float ty = attrib.texcoords[2 * idx.texcoord_index + 1];
            }
        }
        index_offset += fv;
    }
}
```

## CMake配置详情

### 库目标
项目中创建了以下库目标：

1. **ThirPartyLib** (INTERFACE)
   - 包含所有ThirParty目录下的库
   - 自动添加stb和tinyobjloader的包含路径

2. **stb_image** (INTERFACE)
   - 单独的stb_image库目标
   - 可以单独链接

3. **tinyobjloader** (INTERFACE)
   - 单独的tinyobjloader库目标
   - 可以单独链接

### 如何在新目标中使用这些库

如果你创建了新的库或可执行文件，可以这样链接：

```cmake
# 链接所有ThirParty库
target_link_libraries(你的目标 PRIVATE ThirPartyLib glm::glm)

# 或者只链接特定的库
target_link_libraries(你的目标 PRIVATE stb_image glm::glm)
target_link_libraries(你的目标 PRIVATE tinyobjloader glm::glm)
```

## 注意事项

1. **STB Image和TinyObjLoader的实现宏**
   - 这两个库是header-only的，但需要在**一个且仅一个**.cpp文件中定义实现宏
   - 定义实现宏的格式：
     ```cpp
     #define STB_IMAGE_IMPLEMENTATION
     #include <stb_image.h>
     ```
   - 不要在头文件中定义实现宏
   - 不要在多个.cpp文件中定义实现宏

2. **GLM库**
   - GLM是纯header-only库，可以在任何地方包含使用
   - 不需要定义任何实现宏

3. **包含路径**
   - 所有链接到`NeuRenderCoreLib`的目标都自动获得这些库的访问权限
   - 直接使用`#include <stb_image.h>`和`#include <tiny_obj_loader.h>`即可

## 当前项目结构

```
NeuVulkanRender/
├── ThirParty/
│   ├── stb/
│   │   └── stb_image.h
│   └── tinyobjloader/
│       └── tiny_obj_loader.h
├── include/
│   └── LibraryUsageExamples.h  (使用示例)
└── CMakeLists.txt
```

## 验证配置

CMake配置已经成功通过，你可以通过以下命令重新配置：

```bash
cmake -B build -G "MinGW Makefiles"
```

如果需要编译项目：

```bash
cmake --build build
```
