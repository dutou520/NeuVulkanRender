#多线程编译
set(CMAKE_BUILD_PARALLEL_LEVEL 40)#多线程编译
cmake_minimum_required( VERSION 3.10)
# 指定 vcpkg 工具链文件，建立 CMake 与 vcpkg 的关联
set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GenerateExportHeader)


project(NeuVulkanRender VERSION 0.1.0)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# 查找依赖
find_package(imgui CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

# 1. NeuLogLib
add_library(NeuLogLib STATIC source/neuLog.cpp)
generate_export_header(NeuLogLib EXPORT_MACRO_NAME NEULOG_API BASE_NAME NeuLog)
target_compile_definitions(NeuLogLib PUBLIC NEULOG_STATIC_DEFINE)
target_include_directories(NeuLogLib PUBLIC 
    "${CMAKE_SOURCE_DIR}/include" 
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(NeuLogLib PUBLIC spdlog::spdlog fmt::fmt)

# 2. NeuWindowLib
add_library(NeuWindowLib STATIC source/Window.cpp)
generate_export_header(NeuWindowLib EXPORT_MACRO_NAME NEUWINDOW_API BASE_NAME NeuWindow)
target_compile_definitions(NeuWindowLib PUBLIC NEUWINDOW_STATIC_DEFINE)
target_include_directories(NeuWindowLib PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(NeuWindowLib PUBLIC SDL3::SDL3 NeuLogLib)

# 3. NeuImGuiBackendLib (Stable backends, rarely change)
add_library(NeuImGuiBackendLib STATIC source/imgui_impl_sdl3.cpp source/imgui_impl_vulkan.cpp)
target_include_directories(NeuImGuiBackendLib PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(NeuImGuiBackendLib PUBLIC imgui::imgui SDL3::SDL3 Vulkan::Vulkan)

# 4. NeuGUILib
add_library(NeuGUILib STATIC source/neuGUI.cpp)
generate_export_header(NeuGUILib EXPORT_MACRO_NAME NEUGUI_API BASE_NAME NeuGUI)
target_compile_definitions(NeuGUILib PUBLIC NEUGUI_STATIC_DEFINE)
target_include_directories(NeuGUILib PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(NeuGUILib PUBLIC imgui::imgui NeuLogLib NeuWindowLib spdlog::spdlog fmt::fmt)

# ThirParty 库配置（在使用前定义）
# 创建一个接口库来管理第三方头文件库
add_library(ThirPartyLib INTERFACE)
target_include_directories(ThirPartyLib INTERFACE 
    "${CMAKE_SOURCE_DIR}/ThirParty/stb"
    "${CMAKE_SOURCE_DIR}/ThirParty/tinyobjloader"
    "${CMAKE_SOURCE_DIR}/ThirParty/nlohmann-json"
    "${CMAKE_SOURCE_DIR}/ThirParty/tinygltf"
)

# 为 stb_image 创建单独的接口库（header-only）
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE "${CMAKE_SOURCE_DIR}/ThirParty/stb")

# 为 tinyobjloader 创建单独的接口库（header-only）
add_library(tinyobjloader INTERFACE)
target_include_directories(tinyobjloader INTERFACE "${CMAKE_SOURCE_DIR}/ThirParty/tinyobjloader")

# 为 nlohmann-json 创建单独的接口库（header-only）
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE "${CMAKE_SOURCE_DIR}/ThirParty/nlohmann-json")

# 为 tinygltf 创建单独的接口库（header-only）
add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE "${CMAKE_SOURCE_DIR}/ThirParty/tinygltf")

# ========== 文件系统相关库 ==========

# NeuCoreLib (Object, UUID 基础类)
add_library(NeuCoreLib STATIC 
    source/Object.cpp 
    source/UUID.cpp
)
generate_export_header(NeuCoreLib EXPORT_MACRO_NAME NEUCORE_API BASE_NAME NeuCore)
target_compile_definitions(NeuCoreLib PUBLIC NEUCORE_STATIC_DEFINE)
target_include_directories(NeuCoreLib PUBLIC 
    "${CMAKE_SOURCE_DIR}/include" 
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(NeuCoreLib PUBLIC glm::glm)

# NeuAssetLib (AssetManager, MetaFile, ModelImporter, TextureResource, MaterialResource)
add_library(NeuAssetLib STATIC 
    source/AssetManager.cpp 
    source/MetaFile.cpp 
    source/ModelImporter.cpp
    source/TextureResource.cpp
    source/MaterialResource.cpp
)
generate_export_header(NeuAssetLib EXPORT_MACRO_NAME NEUASSET_API BASE_NAME NeuAsset)
target_compile_definitions(NeuAssetLib PUBLIC NEUASSET_STATIC_DEFINE)
target_include_directories(NeuAssetLib PUBLIC 
    "${CMAKE_SOURCE_DIR}/include" 
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(NeuAssetLib PUBLIC 
    NeuCoreLib 
    NeuLogLib 
    ThirPartyLib
    glm::glm
    Vulkan::Vulkan
)
# Note: NeuAssetLib and NeuSceneLib have circular dependency due to ModelImporter::ImportToScene

# NeuSceneLib (Scene, Project, Node, MeshNode, LightNode, PointLightNode)
add_library(NeuSceneLib STATIC 
    source/Scene.cpp 
    source/Project.cpp 
    source/Node.cpp
    source/MeshNode.cpp
    source/LightNode.cpp
    source/PointLightNode.cpp
    source/CameraNode.cpp
)
generate_export_header(NeuSceneLib EXPORT_MACRO_NAME NEUSCENE_API BASE_NAME NeuScene)
target_compile_definitions(NeuSceneLib PUBLIC NEUSCENE_STATIC_DEFINE)
target_include_directories(NeuSceneLib PUBLIC 
    "${CMAKE_SOURCE_DIR}/include" 
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(NeuSceneLib PUBLIC 
    NeuCoreLib 
    NeuAssetLib
    NeuLogLib
    ThirPartyLib
    glm::glm
)



# 5. NeuRenderCoreLib
add_library(NeuRenderCoreLib STATIC 
    source/RenderCore.cpp 
    source/GBuffer.cpp 
    source/Camera.cpp
)
generate_export_header(NeuRenderCoreLib EXPORT_MACRO_NAME NEURENDERCORE_API BASE_NAME NeuRenderCore)
target_compile_definitions(NeuRenderCoreLib PUBLIC NEURENDERCORE_STATIC_DEFINE)
target_include_directories(NeuRenderCoreLib PUBLIC 
    "${CMAKE_SOURCE_DIR}/include" 
    "${CMAKE_CURRENT_BINARY_DIR}"
)



target_link_libraries(NeuRenderCoreLib PUBLIC 
    Vulkan::Vulkan 
    SDL3::SDL3 
    imgui::imgui 
    NeuLogLib 
    NeuWindowLib 
    NeuImGuiBackendLib 
    NeuGUILib
    NeuCoreLib
    # Use linker group to handle circular dependency between NeuAssetLib and NeuSceneLib
    $<LINK_GROUP:RESCAN,NeuAssetLib,NeuSceneLib>
    spdlog::spdlog
    fmt::fmt
    glm::glm
    ThirPartyLib
    imguizmo::imguizmo
)
#定义debug模式
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(NeuRenderCoreLib PRIVATE DEBUG)
endif()

# 6. 为兼容性保留 NeuVulkanRenderLib 接口
add_library(NeuVulkanRenderLib INTERFACE)
target_link_libraries(NeuVulkanRenderLib INTERFACE NeuRenderCoreLib)

# 7. 创建主程序并链接
add_executable(NeuVulkanRender source/Main.cpp)
# Note: explicit link to NeuSceneLib after NeuAssetLib to resolve circular dependency
target_link_libraries(NeuVulkanRender PRIVATE 
    NeuRenderCoreLib
    NeuSceneLib  # Explicit re-link to resolve circular dependency with NeuAssetLib
)

# ========== Shader 编译自动化 ==========

# 查找 glslc 编译器
find_program(GLSLC_EXE NAMES glslc HINTS ${Vulkan_BIN_DIR})

if(GLSLC_EXE)
    message(STATUS "Found glslc: ${GLSLC_EXE}")
    
    # 获取所有 GLSL 着色器文件
    file(GLOB_RECURSE GLSL_SOURCE_FILES 
        "${CMAKE_SOURCE_DIR}/resource/shaders/glsl/*.vert"
        "${CMAKE_SOURCE_DIR}/resource/shaders/glsl/*.frag"
    )

    set(SPV_OUTPUT_FILES "")

    foreach(GLSL_FILE ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
        set(SPV_FILE "${CMAKE_SOURCE_DIR}/resource/shaders/compiled/${FILE_NAME}.spv")
        
        # 添加编译命令
        add_custom_command(
            OUTPUT ${SPV_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/resource/shaders/compiled"
            COMMAND ${GLSLC_EXE} ${GLSL_FILE} -o ${SPV_FILE}
            DEPENDS ${GLSL_FILE}
            COMMENT "Compiling shader ${FILE_NAME}"
        )
        list(APPEND SPV_OUTPUT_FILES ${SPV_FILE})
    endforeach()

    # 创建一个伪目标，让所有着色器成为它的依赖
    add_custom_target(CompileShaders ALL DEPENDS ${SPV_OUTPUT_FILES})
    
    # 让主程序依赖于着色器编译
    add_dependencies(NeuVulkanRender CompileShaders)
else()
    message(WARNING "glslc not found! Shaders will not be automatically compiled.")
endif()

# 复制资源文件到构建目录
add_custom_command(TARGET NeuVulkanRender POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/resource"
    "$<TARGET_FILE_DIR:NeuVulkanRender>/resource"
    COMMENT "Copying resource folder to build directory"
)
