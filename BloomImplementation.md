# Bloom Implementation in NeuVulkanRender

本文档详细描述了本项目中 Bloom（泛光）效果的实现原理和技术细节。

## 1. 概述 (Overview)

Bloom 是一种后处理效果，用于模拟强光物体在摄像机镜头或人眼中产生的辉光现象。它能显著增强高动态范围（HDR）渲染的视觉效果，使明亮的区域看起来更具“光感”。

本项目采用经典的 **Threshold + Gaussian Blur (Ping-Pong) + Additive Blending** 流程来实现 Bloom 效果。

## 2. 渲染管线架构 (Pipeline Architecture)

Bloom 效果作为后处理的一部分，插入在场景主渲染（Forward/Deferred）之后，最终合成（Tone Mapping）之前。

**整体流程：**
1.  **Geometry Pass (Deferred)**: 渲染 GBuffer。
2.  **Composition Pass (Deferred)**: 计算光照，输出 HDR 场景颜色到 `m_SceneColor`。
3.  **Forward Pass**: 渲染半透明物体，叠加到 `m_SceneColor`。
4.  **Bloom Pass**:
    *   **Step 1: Threshold (阈值提取)**: 从 `m_SceneColor` 提取高亮区域，输出到 `m_BloomBrightTexture`。
    *   **Step 2: Horizontal Blur (水平模糊)**: 对 `m_BloomBrightTexture` 进行水平高斯模糊，输出到 `m_BloomBlurTexture`。
    *   **Step 3: Vertical Blur (垂直模糊)**: 对 `m_BloomBlurTexture` 进行垂直高斯模糊，输出回 `m_BloomBrightTexture`。
5.  **Post-Process Pass**: 将模糊后的 `m_BloomBrightTexture` 与原始 `m_SceneColor` 叠加，并进行 Tone Mapping 和 Gamma 校正，最终输出到 Swapchain。

## 3. 核心资源 (Core Resources)

Bloom 操作在半分辨率（1/2 Swapchain 尺寸）下进行，以提高性能并获得更大的模糊半径。

### 3.1 纹理 (Textures)
*   **`m_BloomBrightTexture`**: 
    *   用途：存储亮度阈值提取的结果，以及最终的垂直模糊结果。
    *   格式：`VK_FORMAT_R16G16B16A16_SFLOAT` (HDR)。
*   **`m_BloomBlurTexture`**: 
    *   用途：存储水平模糊产生的中间结果（Ping-Pong 中转）。
    *   格式：`VK_FORMAT_R16G16B16A16_SFLOAT` (HDR)。

### 3.2 渲染通道 (Render Pass)
*   **`m_BloomRenderPass`**: 
    *   配置为单颜色附件输出。
    *   `LoadOp = CLEAR`: 每次使用前清空。
    *   `StoreOp = STORE`: 保存结果供后续采样。
    *   `FinalLayout = SHADER_READ_ONLY_OPTIMAL`: 并在结束后转换为着色器只读布局。

### 3.3 帧缓冲 (Framebuffers)
*   **`m_BloomBrightFramebuffer`**: 绑定 `m_BloomBrightTexture` 视图。
*   **`m_BloomBlurFramebuffer`**: 绑定 `m_BloomBlurTexture` 视图。

## 4. 着色器实现 (Shader Implementation)

### 4.1 阈值提取 (`bloom_threshold.frag`)
负责筛选出亮度超过特定阈值的像素。

```glsl
// 伪代码逻辑
vec3 color = texture(sceneColor, uv).rgb;
float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722)); // 计算亮度
if(brightness > threshold)
    outColor = vec4(color, 1.0);
else
    outColor = vec4(0.0, 0.0, 0.0, 1.0);
```

### 4.2 高斯模糊 (`bloom_blur.frag`)
使用两步高斯模糊（Two-Pass Gaussian Blur）来近似昂贵的二维高斯模糊。通过 Push Constants 控制模糊方向（水平/垂直）。

```glsl
// Push Constants 控制方向
layout(push_constant) uniform PushConstants {
    vec2 direction; // (1,0) 水平 或 (0,1) 垂直
    vec2 texelSize;
} params;

// 权重及偏移预计算（5x5 核或更大）
void main() {
    // 采样周围像素并加权平均
    // ...
}
```

### 4.3 合成 (`postprocess.frag`)
在最终后处理步骤中，将 Bloom 结果叠加回原图。

```glsl
vec3 hdrColor = texture(sceneColor, uv).rgb;
vec3 bloomColor = texture(bloomTexture, uv).rgb; // 采样最终的 m_BloomBrightTexture
hdrColor += bloomColor * settings.bloomIntensity; // 叠加
// 之后进行 Tone Mapping ...
```

## 5. C++ 命令录制逻辑 (Command Recording)

在 `RenderCore::RecordCommandBuffer` 中，Bloom Pass 被插入在 Forward Pass 之后：

```cpp
// 1. Threshold Pass
// Input: SceneColor (Set 0)
// Output: m_BloomBrightFramebuffer
vkCmdBeginRenderPass(..., m_BloomBrightFramebuffer);
vkCmdBindPipeline(..., m_BloomThresholdPipeline);
vkCmdDraw(...);

// 2. Horizontal Blur Pass
// Input: m_BloomBrightTexture (Set 0)
// Output: m_BloomBlurFramebuffer
vkCmdBeginRenderPass(..., m_BloomBlurFramebuffer);
vkCmdBindPipeline(..., m_BloomBlurPipeline);
// PushConstant: direction = {1, 0}
vkCmdPushConstants(...);
vkCmdDraw(...);

// 3. Vertical Blur Pass
// Input: m_BloomBlurTexture (Set 1 - 指向 BloomBlurTexture 的描述符)
// Output: m_BloomBrightFramebuffer (写回 Bright Buffer，作为最终结果)
vkCmdBeginRenderPass(..., m_BloomBrightFramebuffer);
vkCmdBindPipeline(..., m_BloomBlurPipeline);
// PushConstant: direction = {0, 1}
vkCmdPushConstants(...);
vkCmdDraw(...);
```

## 6. 描述符集管理 (Descriptor Sets)

为了支持 Ping-Pong 渲染，我们需要特定的描述符集结构：

*   **`m_SingleTextureDescriptorSetLayout`**: 简单的单纹理采样布局 (Binding 0)。
*   **`m_BloomThresholdDescriptorSets`**: 绑定主场景图 (`m_SceneColor`)。
*   **`m_BloomBlurDescriptorSets`**:
    *   **Set A (偶数索引)**: 绑定 `m_BloomBrightTexture` (用于水平模糊输入)。
    *   **Set B (奇数索引)**: 绑定 `m_BloomBlurTexture` (用于垂直模糊输入)。
*   **PostProcess Descriptor Set**: Binding 3 绑定 `m_BloomBrightTexture`（因为它是垂直模糊后的最终输出）。

## 7. 配置参数 (Configuration)

可以通过 `PostProcessSettings` 结构体实时调整 Bloom 效果：
*   **`enableBloom`**: 开关。
*   **`bloomThreshold`**: 亮度阈值（默认 0.8），越低泛光越多。
*   **`bloomIntensity`**: 泛光强度，控制叠加时的亮度乘数。
