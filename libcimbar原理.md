# libcimbar 编码原理详解

libcimbar 是一个将任意二进制文件编码为彩色二维码风格图像（或实时视频）的工具，由 `cimbar.org` 项目开发。核心思想是**利用颜色通道增加 QR 码的信息密度**，配合**喷泉码（Fountain Code）** 和 **Reed-Solomon 纠错码**实现高容错的数传输。

---

## 整体流水线

```
原始文件（任意二进制，最大约 33 MB）
    │
    ▼
[1. Zstandard 压缩]         —  zstd level 16，内嵌文件名头，不足则填充
    │
    ▼
[2. Wirehair 喷泉编码器]     —  将压缩数据分割为 fountain 块，生成无限多的纠删块
    │
    ▼
[3. Reed-Solomon ECC]       —  RS(155,125)，每 155 字节加 30 字节校验
    │
    ▼
[4. 比特拆分器]              —  分两遍交织：先写 symbol 位，后写 color 位
    │
    ▼
[5. CimbWriter / 像素贴图]   —  按交织顺序将 6-bit 值映射为彩色瓷砖，粘贴到画布
    │
    ▼
[6. 输出]                   —  PNG 帧序列 / GLFW 实时窗口 / WebGL 画布
```

---

## 1. Zstandard 压缩（`src/lib/compression/zstd_compressor.h`）

- 使用 **Zstandard** 压缩，默认 level 16。
- 压缩前，在数据流最前方插入一个 **skippable zstd frame** 作为文件头，内容为 `0x01 + 文件名`（`write_header()`）。
- 如果压缩后数据小于 fountain 块大小，则填充 **skippable null frame** 至所需大小 (`pad()`)。
- 可通过 `compression_level=0` 关闭压缩。

**关键函数：**
```cpp
bool compress(INSTREAM& raw, int compression_level=0);  // 逐块压缩
size_t write_header(const char* data, unsigned len);     // 写文件名头
size_t pad(unsigned len);                                 // 填充到最小长度
```

---

## 2. Wirehair 喷泉码（`src/third_party_lib/wirehair/`）

### 什么是喷泉码
喷泉码是一种**无码率纠删码（rateless erasure code）**。发送端可以从原始数据中生成**无限多个**编码块，接收端只要收到**略多于原始数据量**的任意编码块（不要求顺序），就能恢复全部原始数据。

### Wirehair 的特性
- 由 Christopher A. Taylor 开发的开源喷泉码库。
- **时间复杂度 O(N)**（而非 O(N log N) 或 O(N²)），适合大数据量。
- 支持块数 N 范围为 **2 ~ 64000**。
- 不是理想 MDS 码，平均需要 **N + 0.02** 个块即可恢复全部数据。
- **前 N 个块（block_id < N）就是原始数据本身**（systematic 特性），允许发送端在编解码器初始化同时直接发送原始数据。

### 在 libcimbar 中的应用

**创建编码器**（`src/lib/fountain/FountainEncoder.h`）：
```cpp
_codec = wirehair_encoder_create(nullptr, data, length, packet_size);
```
- `data`：zstd 压缩后的完整数据
- `length`：数据长度
- `packet_size`：每个 fountain 块大小（不含 6 字节头）

**生成新块**（`src/lib/fountain/fountain_encoder_stream.h:encode_new_block()`）：
```cpp
size_t res = _encoder.encode(_block++, data, block_size());
FountainMetadata::to_uint8_arr(_encodeId, _data.size(), block, _buffer.data());
```
- 每调用一次生成一个新的编码块
- block_id 从 0 开始递增，无上限

**Fountain 块头格式**（`FountainMetadata.h`，共 6 字节）：
```
Byte 0: [7-bit encode_id | 1-bit 文件大小高位 (bit 17)]
Byte 1-3: [24-bit 文件总大小]
Byte 4-5: [16-bit block_id]
```

### Fountain 块和帧的关系

在 **Conf8x8（默认模式）**中：
- `fountain_chunks_per_frame = 12`（6-bit 模式下：2 × bits_per_cell）
- `fountain_chunk_size = 750 字节`
- 每帧包含 12 个 fountain 块，每个块前 6 字节为头，后 744 字节为有效载荷
- 每帧 fountain 有效数据 = 744 × 12 = **8928 字节**

`fountain_chunk_size` 的计算公式（`GridConf.h:63-71`）：
```
chunk_size = capacity(bits_per_cell) × (ecc_block_size - ecc_bytes) / ecc_block_size / fountain_chunks_per_frame
```
代入默认值：
```
chunk_size = 9300 × (155 - 30) / 155 / 12 = 9300 × 125 / 155 / 12 = 7500 / 12 = 750 × 125/155 = 750
```

---

## 3. Reed-Solomon ECC（`src/lib/encoder/ReedSolomon.h`）

- 使用 `libcorrect` 库的 RS 编码器。
- 本原多项式：`correct_rs_primitive_polynomial_8_7_2_1_0`（即 x⁸ + x⁷ + x² + x + 1）。
- 交错深度（interleaving）：1，根间距：1。
- **默认配置（Conf8x8）**：RS(155, 125)，即每 155 字节中包含 30 字节校验。

### Reed-Solomon 流封装（`reed_solomon_stream.h`）

编码时，底层流先被读到 buffer 中（长度 = `ecc_block_size - parity` = 125 字节），然后 RS 编码器将其扩展为 155 字节输出：
```cpp
_stream.read(data, length - _rs.parity());  // 读 125 字节原始数据
_rs.encode(data, bytes, data);              // 编码为 155 字节
```

解码时，读取 155 字节并进行 RS 纠错，输出 125 字节的修复数据：
```cpp
ssize_t bytes = _rs.decode(data, _buffer.size(), _buffer.data());
```

### 后 RS 的帧容量

- 每帧原始（裸数据）容量：`112 × 112 - 4 × 6 × 6 = 12400` 个 cells
- 每个 cell 6 bits → 12400 × 6 / 8 = **9300 字节/帧**
- RS 效率：125/155 ≈ 0.806
- 有效数据/帧：9300 × 125/155 = **7500 字节**

---

## 4. 符号映射与颜色编码（`CimbEncoder`）

每帧有 **12400** 个 data cells，每个 cell 编码 6 bits：
- **低 4 bits**：选择 16 种瓷砖图案之一（symbol）
- **高 2 bits**：选择 4 种颜色之一（color）

### 瓷砖图案

16 种 8×8 像素的双色 PNG 瓷砖内嵌在代码中（`bitmaps.h`），以 base91 编码的字符串形式存储。加载时：
1. base91 解码
2. STB image 解析为 RGBA 像素矩阵
3. 转换为 RGB 矩阵

### 颜色映射（`Common.cpp`）

默认 4 色调色板（mode B, color_mode=1）：

| Index | 颜色 | RGB |
|-------|------|-----|
| 0 | 绿 | `(0, 255, 0)` |
| 1 | 青 | `(0, 255, 255)` |
| 2 | 黄 | `(255, 255, 0)` |
| 3 | 品红 | `(255, 0, 255)` |

此外还有：
- 旧版调色板（color_mode=0）：索引顺序不同
- 增强调色板（color_mode>0x100）：品红改为 `(255, 0x55, 255)`，并配有对应底色

### 瓷砖着色过程（`Common.cpp:getTile()`）

```cpp
// 白色像素视为背景，其余视为前景
if (c != background)   // background = (255, 255, 255)
    c = {r, g, b};     // 替换为所选颜色
else
    c = {bgr, bgg, bgb}; // 替换为底色（通常黑色或暗色变体）
```

### 编码模式：解耦模式 vs 耦合模式

**解耦模式（现代默认，`_coupled=false`）**：

解码时符号位和颜色位分开处理——先写所有 symbol bits，再写所有 color bits。这样颜色通道出错不影响符号通道的解码。

编码流程（`Encoder.h:encode_next()`）：
1. **第一遍（symbol pass）**：从 RS 流中读取 `_bitsPerSymbol`（4）位，以步长 `bits_per_op`（6）写入 bitbuffer
2. **第二遍（color pass）**：填充 color bits 到步长间隙中

**耦合模式（legacy）**：
符号位和颜色位交织在一起，同时从 RS 流读取 6 位直接写入。好处是某个通道的纠错能力可以"帮助"另一个通道，缺点是"一损俱损"。

---

## 5. 细胞位置与交织（`CellPositions.cpp`, `Interleave.h`）

### 网格布局

- 画布大小：**1024 × 1024 像素**（Conf8x8）
- 瓷砖大小：**8×8 像素**
- 间隔：**9 像素**（8+1）
- 网格：**112 × 112** = 12544 个格子
- 四角各去掉 **6×6 = 36** 个格子（用于 anchor 标记）
- 实际 data cells：**12400**

### 位置计算（`CellPositions::compute_linear()`）

位置分三个区域计算：顶部（anchor 下方的带状区）、中部（主体）、底部（anchor 上方的带状区）。这是为了避开四角的 anchor 标记。

### 交织（`Interleave.h:interleave_indices()`）

为了防止局部图像损坏导致连续数据丢失，cell 位置被重新排列为交织顺序：

```
参数：interleave_blocks = 155, interleave_partitions = 2
```

算法：
1. 将 12400 个位置分为 2 个分区，每分区 6200 个
2. 每分区内，分为 155 个块（`num_chunks`）
3. 遍历块 0..154，每个块内跳跃式取 cell: `i = chunk, chunk+155, chunk+310...`
   即 `for chunk in 0..155: for i in chunk..6200 step 155: indices.push(i)`

这样 RS 块内的连续字节会被散布到图像中的不同区域，提升抗局部损坏能力。

---

## 6. Anchor 标记与引导条

- **四角设 3 个同心的矩形 anchor**，用于定位和透视校正
- **四边中点设引导条**（guide bar），用于精确定位
- 图像生成时先绘制黑色（或指定色）背景，然后粘贴 anchor 和 guide bar，最后依次粘贴数据瓷砖

---

## 7. 输出生成

### PNG 帧（`EncoderPlus.h:encode_fountain()`）

```
frames_required = blocks_required × redundancy / fountain_chunks_per_frame
```
- `blocks_required`：fountain 块总数 (= ceil(压缩数据大小 / fountain_chunk_size))
- `redundancy`：默认 1.2（额外发送 20% 的帧）
- 每帧生成后检查"扫描性"（`Scanner::will_it_scan()`），连续 5 帧无法扫描则报错

### 实时视频（`src/exe/cimbar_send/send.cpp`）

- 使用 GLFW 窗口渲染 1024×1024 图像
- 帧率控制：默认 15 fps
- 编码器生成 `blocks_required × 8` 个块后循环复用
- 支持 "shakycam" 效果：亚像素抖动帮助解码器检测帧切换
- 吞吐量约 **850 kbit/s（~106 KB/s）**

---

## 8. 完整帧容量计算（Conf8x8 默认值）

| 参数 | 计算 | 结果 |
|------|------|------|
| 网格 | 112 × 112 | 12544 cells |
| 四角 anchor 扣除 | 4 × 6 × 6 | −144 cells |
| 实际 data cells | | 12400 cells |
| bits/cell | 4(symbol) + 2(color) | 6 bits |
| 裸容量 | 12400 × 6 / 8 | 9300 bytes |
| RS(155,125) 后 | 9300 × 125/155 | 7500 bytes |
| fountain 块/帧 | 2 × 6 | 12 块 |
| fountain 块大小 | 7500 / 12 | 750 bytes |
| fountain 有效载荷 | 750 − 6(头) | 744 bytes |
| 帧有效数据 | 744 × 12 | 8928 bytes |
| 最大文件大小 | Wirehair 限制 N ≤ 64000 | ~33 MB |
| 吞吐量（15fps） | 7500 × 15 | ~106 KB/s |

---

## 9. 解码流程（逆向）

解码器工作流程：
1. 在图像中检测 4 个 anchor → 透视校正
2. 扫描 grid 中每个 cell 位置，提取瓷砖图案和颜色
3. 反交织还原数据顺序
4. RS 解码纠错
5. 收集 fountain 块直到数量 ≥ N → 用 Wirehair 恢复原始压缩数据
6. zstd 解压缩 → 还原原始文件和文件名

---

## 参考文件

| 文件 | 功能 |
|------|------|
| `GridConf.h` | grid 几何配置（Conf8x8, Conf5x5 等） |
| `Config.h` | 运行时配置适配器 |
| `Encoder.h` | 编码主流程（两遍编码、RS 流包装） |
| `EncoderPlus.h` | 高层接口（fountain 编码、PNG 输出） |
| `ReedSolomon.h` | RS 编解码器封装 |
| `reed_solomon_stream.h` | RS 流（自动分块编解码） |
| `FountainEncoder.h` | Wirehair 喷泉编码器封装 |
| `fountain_encoder_stream.h` | 喷泉流（自动生成带头的块） |
| `FountainMetadata.h` | 6 字节喷泉块头格式 |
| `zstd_compressor.h` | Zstandard 压缩流 |
| `CimbEncoder.h/.cpp` | 瓷砖和颜色的映射 |
| `CimbWriter.h/.cpp` | 画布输出（粘贴瓷砖） |
| `CellPositions.cpp` | cell 位置计算 |
| `Interleave.h` | 交织/反交织算法 |
| `Common.cpp` | 瓷砖加载、颜色表 |
| `bitmaps.h` | 16 种瓷砖图案 PNG（base91 编码） |
| `wirehair/` | Wirehair 喷泉码库 |
| `send.cpp` | 实时视频发送 |
| `cimbar_js.cpp` | WASM API 封装 |
