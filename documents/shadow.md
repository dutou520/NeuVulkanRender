
# PCSS阴影
## 1. 核心参数

| **参数类别**  | **变量名**            | **说明**                       |
| --------- | ------------------ | ---------------------------- |
| **光源属性**  | `u_LightDir`       | 方向光的方向向量。                    |
|           | `u_LightSize`      | **关键参数**：光源的物理大小（控制阴影变软的速率）。 |
| **相机参数**  | `u_LightNear/Far`  | 光源相机的近/远平面。                  |
|           | `u_LightVP`        | 光源空间的 View-Projection 矩阵。    |
| **算法精度**  | `u_BlockerSamples` | 遮挡物搜索采样数（推荐 16 - 25）。        |
|           | `u_PCFSamples`     | 滤波采样数（推荐 32 - 64）。           |
| **硬件/纹理** | `u_ShadowMapRes`   | 阴影贴图分辨率（1024 / 2048 / 4096）。 |
| **阴影距离**  | `u_shadowDistance` |                              |

---

## 2. 光源相机设置（稳定化方案）

为彻底避免阴影抖动，按以下逻辑构建 `u_LightVP`：

1. **确定包围球**：
    
    - 获取视锥体（根据 `MaxShadowDistance` 裁剪后）的 8 个顶点。
        
    - 计算这 8 个顶点的**中心点 $C$** 和**外接球半径 $R$**。
        
2. **构建 View 矩阵**：
    
    - $Position = C - u\_LightDir \times R+ShadowDistance$
        
    - $LookAt = C$
        
3. **构建 Ortho 投影矩阵**：
    
    - $Left, Bottom = -R$，$Right, Top = R$。
        
4. **像素对齐（Texel Snapping）**：
    
    - 在 CPU 端将 View 矩阵的平移分量 $x, y$ 锁定到像素单位：
        
    - `world_units_per_texel = (2.0 * R) / u_ShadowMapRes`
        
    - `view_space_pos.xy = floor(view_space_pos.xy / unit) * unit`
        

---

## 3. Shader 实现逻辑（PCSS 三步曲）

### 第一步：遮挡物搜索 (Blocker Search)

在当前像素对应的 Shadow Map 区域内采样，计算所有**遮挡物（深度小于当前点）**的平均深度值 $d_{avg}$。

- **搜索半径**：由 `u_LightSize` 和当前点到光源的距离决定。
    
- **输出**：若无遮挡，直接渲染为光照区；若有遮挡，记录 $d_{avg}$。
    

### 第二步：半影半径计算 (Penumbra Estimation)

利用相似三角形原理，确定模糊半径：

$$w_{penumbra} = \frac{(d_{receiver} - d_{avg}) \times u\_LightSize}{d_{avg}}$$

### 第三步：滤波采样 (Filtering / PCF)

使用第 2 步得到的 $w_{penumbra}$ 作为半径，进行 **Poisson Disk（泊松圆盘）** 采样。

- **随机化**：使用一个随机旋转角度（基于像素位置生成的 Noise）旋转采样圆盘，将锯齿转化为高频噪声，视觉上更平滑。


## 另外：.像素对齐 (Texel Snapping) —— 消除抖动

即使矩阵大小固定了，如果相机随玩家移动，阴影边缘仍会在像素间跳变。

- **计算纹理单位**：$TexelSize = \frac{2 \times R}{ShadowMapResolution}$。
    
- **坐标对齐**：在计算观察矩阵（View Matrix）的平移部分时，将其锁定到 $TexelSize$ 的整数倍。
    
    C++
    
    ```
    // 伪代码示例
    shadowViewMatrix[3].x = floor(shadowViewMatrix[3].x / texelSize) * texelSize;
    shadowViewMatrix[3].y = floor(shadowViewMatrix[3].y / texelSize) * texelSize;
    ```
    
    这样当摄像机移动时，阴影贴图在世界空间中的采样点也是“一格一格”跳动的，边缘就不会闪烁。

## 工程步骤：
1. 创建Shadow Pass
2. 创建深度附件VK_FORMAT_D32_SFLOAT
3. 视锥体剔除、背面剔除（只需要考虑不透明物体）
4. 构建变换矩阵$$ShadowMatrix = P_{ortho} \times V_{light}$$
5. 把Shadow Pass生成的Shadow Map 传入延迟渲染Pass 、向前半透明物体渲染pass的片元着色器，进行深度比较。
6. 处理 Shadow Map 的深度偏移 (Depth Bias)

为了解决 **Shadow Acne（阴影粉刺）**（由于阴影贴图分辨率有限导致的自遮挡黑斑），在绘制 Shadow Map 时需要添加偏移。

- **Vulkan 实现**：在 `VkPipelineRasterizationStateCreateInfo` 中设置 `depthBiasEnable = VK_TRUE`。
    
- **动态状态**：建议将 `VK_DYNAMIC_STATE_DEPTH_BIAS` 加入流水线，以便在运行时根据光源角度动态调整 `vkCmdSetDepthBias`。
    

7. 配置阴影采样器 (Sampler Setup)

PCSS 需要在 Shader 中手动进行多次采样，因此采样器的配置至关重要：

- **Filter**：设置为 `VK_FILTER_LINEAR`。
    
- **Address Mode**：设置为 `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER`。
    
- **Border Color**：设置为 `VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE`（确保超出阴影贴图范围的区域被视为“未遮挡”，深度值为 1.0）。
    

8. 显存同步与布局转换 (Barrier & Synchronization)

这是 Vulkan 的核心。你需要确保阴影图在被读取前已经完全写入。

- **写入时**：`oldLayout = VK_IMAGE_LAYOUT_UNDEFINED`, `newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL`。
    
- **读取前**：在 Shadow Pass 结束和 Lighting Pass 开始之间插入一个 **Image Memory Barrier**：
    
    - `srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT`
        
    - `dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT`
        
    - `newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
        

9. 实现 Poisson Disk 采样与噪声注入

PCSS 的精髓在于随机性。

- **采样点生成**：在 C++ 端生成一组随机分布在单位圆内的点坐标（Poisson Disk），通过 Uniform Buffer 或常量数组传入 Shader。
- **噪声纹理**：加载一张小尺寸（如 64x64）的蓝噪声（Blue Noise）贴图resource\textures\noise-texture-64x64.png。在 Shader 中使用 `gl_FragCoord` 采样该噪声，得到一个随机旋转角度来旋转采样圆盘。


10. Shader 中的坐标变换 (Projective Coordinates)

在片元着色器中，将世界空间坐标变换到阴影空间：

1. `shadowCoords = u_LightVP * vec4(worldPos, 1.0)`
    
2. **归一化设备坐标 (NDC) 转换**：
    
    - $uv = shadowCoords.xy \times 0.5 + 0.5$
        
    - **注意**：在 Vulkan 中，Y 轴是反向的，如果你的投影矩阵没有处理这一点，UV 转换可能需要改为 `uv.y = 1.0 - uv.y`。
        
3. 深度对比：$d_{receiver} = shadowCoords.z$。

