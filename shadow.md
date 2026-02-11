
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
6. 在GUI开放

