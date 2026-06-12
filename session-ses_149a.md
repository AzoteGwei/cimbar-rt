# 编码方法讲解

**Session ID:** ses_149a4c06fffekuDteq1woxu3m4
**Created:** 6/11/2026, 7:04:53 PM
**Updated:** 6/11/2026, 7:17:17 PM

---

## User

大概给我讲一下它的编码方法。

---

## Assistant (Build · DeepSeek V4 Flash · 90.2s)

_Thinking:_

The user is asking me to explain the encoding method of libcimbar. Let me explore the codebase to understand what this project does and how it encodes data.

**Tool: task**

**Input:**
```json
{
  "description": "Explore libcimbar encoding",
  "prompt": "Explore the libcimbar codebase at /home/ringo/repositories/azote/libcimbar to understand how it encodes data. This is a research-only task - do not modify any files.\n\nLook at:\n1. README or any docs for high-level description\n2. Source files in src/ - especially anything related to encoding, fountain codes, tile encoding, color encoding\n3. Header files to understand the data flow\n\nReturn a concise explanation of the encoding pipeline:\n- What format is the input?\n- How is it split/packetized?\n- What kind of fountain/error correction codes are used?\n- How are bits mapped to colors/tiles?\n- How does the final output (usually a video/image) get generated?\n\nBe thorough - check multiple files to understand the full pipeline.",
  "subagent_type": "explore"
}
```

**Output:**
```
<task id="ses_149a4aa66ffewyWRlTnnXEnyb5" state="completed">
<task_result>
Now I have a thorough understanding of the full pipeline. Here is a comprehensive explanation:

---

## libcimbar Encoding Pipeline -- Complete Analysis

### 1. What format is the input?

The input is an **arbitrary binary file** (any format -- PDF, image, text, etc.) read from disk as raw bytes. The file is loaded entirely into memory. The pipeline is designed for files up to ~33 MB (the wirehair fountain codec's limit). The executable entry points are:

- **CLI (PNG output):** `src/exe/cimbar/cimbar.cpp` -- uses `EncoderPlus::encode_fountain()` to produce sequential PNG frames.
- **Live video (GLFW window):** `src/exe/cimbar_send/send.cpp` -- uses the `cimbar_js` C API to render frames to a GLFW window in real time.
- **WASM/web:** `src/lib/cimbar_js/cimbar_js.cpp` -- same C API compiled to WebAssembly for `cimbar.org`.

---

### 2. How is it split/packetized? (The three-layer encoding stack)

The pipeline has three distinct layers stacked on top of each other:

#### Layer 1: Zstandard Compression
**File:** `src/lib/compression/zstd_compressor.h`

- The raw input file is **compressed with Zstandard** (zstd) at a configurable level (default level 16).
- A **skippable zstd frame header** is prepended containing the original filename (1-byte type byte + filename string). This allows the decoder to reconstruct the file with its original name.
- If the compressed output is smaller than the fountain chunk size, it is **padded with skippable zstd null frames** to meet the minimum size.
- Compression is optional (level 0 = no compression).

#### Layer 2: Fountain Codes (Wirehair)
**Files:** `src/lib/fountain/FountainEncoder.h`, `fountain_encoder_stream.h`, `FountainMetadata.h`

- The compressed data is fed into a **Wirehair fountain codec** (a rateless erasure code).
- **Wirehair** (`src/third_party_lib/wirehair/`) is a fountain code that can generate an unlimited number of error-correction blocks from the source data. It is O(N) in input size. On average it needs N + 0.02 packets to recover (slightly more than ideal MDS).

- **Chunking:** The compressed data is divided into equal-size blocks. The block size is `fountain_chunk_size()`, computed from the frame capacity and ECC overhead:
  ```
  chunk_size = capacity(bits_per_cell) * (ecc_block_size - ecc_bytes) / ecc_block_size / fountain_chunks_per_frame
  ```
  For default "mode B" (Conf8x8): chunk_size = 750 bytes per fountain block.

- **Fountain metadata header:** Each fountain block has a **6-byte header** (`FountainMetadata.h`):
  - 7-bit `encode_id` (identifies which encoding session; wraps 0-127)
  - 1 bit used for file-size high bit
  - 24-bit total file size (3 bytes)
  - 16-bit block ID (sequence number)

- The fountain encoder produces blocks on demand. Calling `encode_new_block()` increments the block counter and generates the next fountain-encoded packet, regardless of order. The encoder cycles through an unlimited supply of blocks (practical limit: the decoder needs ~N+1 blocks to reconstruct; the encoder generates `blocks_required * 8` blocks before restarting).

#### Layer 3: Reed-Solomon Error Correction
**Files:** `src/lib/encoder/ReedSolomon.h`, `reed_solomon_stream.h`

- After fountain encoding, the data passes through a **Reed-Solomon** byte-level erasure code (via `libcorrect`).
- The stream is divided into blocks of `ecc_block_size` bytes (default 155). For each block:
  - `ecc_block_size - ecc_bytes` bytes are real data (e.g., 155 - 30 = 125 bytes)
  - `ecc_bytes` bytes are parity/ECC (e.g., 30 bytes)
  - The R-S encoder produces `ecc_block_size` total bytes (e.g., 155 bytes)

- The RS codec uses polynomial `0x11D` (CCSDS) with 1 bit of interleaving and 1 byte of "root gap" (`correct_reed_solomon_create(correct_rs_primitive_polynomial_8_7_2_1_0, 1, 1, parity)`).

---

### 3. How are bits mapped to colors and tiles?

Once the RS stream yields bytes, they go into the **CimbWriter** which places tiles on the image canvas.

#### Bit partitioning
**File:** `src/lib/encoder/Encoder.h` (method `encode_next`)

The non-legacy encoder decouples symbol bits from color bits in two passes:
1. **First pass (symbol pass):** Reads `symbol_bits` (default 4) from the RS stream and writes them into a bitbuffer at a stride of `bits_per_op` (symbol_bits + color_bits = 6).
2. **Second pass (color pass):** Reads `color_bits` (default 2) and fills in the gaps in the bitbuffer.

The legacy ("coupled") mode interleaves symbol and color bits together (6 bits at a time).

#### Tile selection
**Files:** `src/lib/cimb_translator/CimbEncoder.h`, `CimbEncoder.cpp`, `Common.cpp`

Each 6-bit value is split:
- **Lower 4 bits** select one of **16 possible tile patterns** (symbols), loaded from embedded PNG bitmaps (`bitmap/4/00.png` through `bitmap/4/0f.png`).
- **Upper 2 bits** select one of **4 colors** to apply to the tile's foreground pixels.

The tile images are 8x8 pixel PNGs stored as base91-encoded strings in `src/lib/cimb_translator/bitmaps.h`. On load, each tile is:
1. Decoded from the embedded bitmap data.
2. Colorized: foreground (non-white) pixels are replaced with the selected color; background (white) pixels get the background color (black or color-specific dim variant).

**Color palettes (4-color mode B default):**
```
Index 0: (0, 255, 0)     -- green
Index 1: (0, 255, 255)   -- cyan
Index 2: (255, 255, 0)   -- yellow
Index 3: (255, 0, 255)   -- magenta
```

There are also 8-color palettes and alternate color modes (e.g., `color_mode > 0x100` enables per-color background colors for better contrast).

#### Cell positions and interleaving
**Files:** `src/lib/cimb_translator/CellPositions.cpp`, `Interleave.h`

The cell positions on the 1024x1024 canvas are computed with interleaving:
1. Linear positions are computed in a grid with 9-pixel spacing (8x8 cell + 1px gap).
2. **Interleaving** reorders the positions: the grid is divided into `interleave_partitions` (2) partitions, each with `interleave_blocks` (155) blocks. Within each partition, cells are visited in a striped pattern (block-by-block, then chunk-by-chunk within each block). This spreads adjacent RS data across distant physical tiles, making the code resistant to localized image damage.

- Total data cells (8x8 mode B): 112 x 112 = 12,544 cells, minus 4 corner anchor zones of 6x6 = 144 cells, giving **12,400 cells**.
- At 6 bits/cell (4 symbol + 2 color): 12,400 * 6 / 8 = **9,300 bytes** per image before ECC.
- With RS 30/155 overhead: 9,300 * 125/155 = **7,500 bytes** per image after ECC.
- With fountain chunk overhead: per frame, `fountain_chunks_per_frame` (default=12 for 6-bit) fountain blocks are packed; each block is 750 bytes with a 6-byte header, giving 744 fountain payload bytes per block = 744 * 12 = 8,928 fountain bytes per frame, which after the 6-byte header overhead per chunk yields roughly 7,500 bytes effective.

#### Image assembly
**File:** `src/lib/cimb_translator/CimbWriter.cpp`

- Creates a 1024x1024 OpenCV `cv::Mat` with black (or white) background.
- Pastes pre-loaded anchor markers at the 4 corners (3 large squares + 1 smaller for orientation).
- Pastes guide bars at midpoints of each edge.
- Iterates through interleaved cell positions, calling `CimbEncoder.encode(bits)` for each position, and pasting the returned colored tile onto the canvas.

---

### 4. How does the final output get generated?

There are two output modes:

#### A. Static PNG frames (`cimbar --encode`)
**File:** `src/lib/encoder/EncoderPlus.h`

- Calls `Encoder::encode_next()` repeatedly until the fountain stream is exhausted.
- Each frame is an OpenCV `cv::Mat` (1024x1024 RGB).
- Converted to BGR and saved as a PNG with an incrementing index: `outputprefix_0.png`, `outputprefix_1.png`, etc.
- The number of frames generated = `blocks_required * redundancy / fountain_chunks_per_frame` (default redundancy = 1.2, so roughly N + 20%).
- Frames that fail a "scanability" check (the scanner cannot find the corner anchors) are skipped (up to 5 consecutive skips).

#### B. Live video via GLFW (`cimbar_send`)
**Files:** `src/exe/cimbar_send/send.cpp`, `src/lib/cimbar_js/cimbar_js.cpp`, `src/lib/gui/window_glfw.h`

- Opens a GLFW window sized to the image dimensions.
- For each file:
  1. Reads the file into memory.
  2. Compresses it with zstd.
  3. Creates a `fountain_encoder_stream`.
  4. In a loop, calls `Encoder::encode_next()` to produce `cv::Mat` frames.
  5. Each frame is uploaded as an OpenGL texture.
  6. Rendered to the screen via a 2D OpenGL display (`gl_2d_display`), with optional "shake" effect (sub-pixel jitter for the `shakycam` feature that helps the decoder detect frame transitions).
  7. Frame pacing is controlled by a target FPS (default 15 fps).
- The fountain encoder generates 8x the required blocks, cycling through them; when block_count exceeds `blocks_required * 8`, the encoder restarts from block 0.
- The `encode_id` increments for each new file.

---

### 5. Summary data flow diagram

```
Raw file (arbitrary binary, up to ~33 MB)
    |
    v
[Zstandard Compression]     -- zstd level 16; embeds filename header; pads if needed
    |
    v
[Fountain Encoder]          -- Wirehair rateless fountain code; produces unlimited blocks
    |                          Each block: 6-byte header (encode_id, file_size, block_id) + payload
    v
[Fountain Encoder Stream]   -- generates blocks on-demand; cycles through block IDs
    |
    v
[Reed-Solomon ECC]          -- libcorrect RS(155,125) with 30 parity bytes per 155-byte block
    |
    v
[Bit Splitter]              -- Decoupled 2-pass: first all symbol bits (4), then all color bits (2)
    |                          Writes to bitbuffer at stride = 6 bits per cell
    v
[CimbWriter / CellPositions]-- Interleaved cell order (2 partitions x 155 blocks)
    |                          Pastes tiles onto 1024x1024 canvas at 9px spacing
    v
[CimbEncoder]               -- Selects tile image from 16 bitmap symbols
    |                          Colors it with 1 of 4 colors (RGB tinting)
    v
[Output]                    -- Either saved as PNG files or rendered via OpenGL/GLFW
                                (or uploaded to WebGL canvas in WASM build)
```

### Key configuration defaults (Mode B / Conf8x8)

| Parameter | Value |
|-----------|-------|
| Image size | 1024 x 1024 px |
| Cell size | 8 x 8 px |
| Cell spacing | 9 px (8+1) |
| Grid dimensions | 112 x 112 |
| Data cells (after corner exclusion) | 12,400 |
| Symbol bits per cell | 4 (16 tile patterns) |
| Color bits per cell | 2 (4 colors) |
| Bits per cell | 6 |
| Raw bytes per frame | 9,300 |
| RS parity bytes | 30 |
| RS block size | 155 bytes |
| RS data per block | 125 bytes |
| Effective bytes per frame after RS | 7,500 |
| Fountain chunks per frame | 12 (for 6-bit mode) |
| Fountain chunk size | 750 bytes |
| Max file size (wirehair limit) | ~33.55 MB |
| Interleave partitions | 2 |
| Interleave blocks (per partition) | 155 |
| Target FPS (video) | 15 fps |
| Sustained throughput | ~850 kbit/s (~106 KB/s) |
</task_result>
</task>
```

---

## Assistant (Build · DeepSeek V4 Flash · 4.8s)

_Thinking:_

The user asked for a brief explanation of the encoding method. The exploration agent returned a very thorough analysis. Let me summarize this concisely for the user.

libcimbar 将任意二进制文件编码为 1024x1024 的彩色二维码风格的图像序列（或视频）。流程如下：

1. **Zstandard 压缩** — 对原始文件进行 zstd 压缩，嵌入文件名
2. **Wirehair 喷泉码** — 将压缩数据分成数据块，用喷泉码生成无限数量的冗余编码块（接收 N+0.02 个块即可恢复全部数据）
3. **Reed-Solomon ECC** — 每 155 字节加 30 字节纠错码，抵抗图像损坏
4. **符号映射** — 每 6 bits 表示一个 cell：低 4 bits 选择 16 种瓷砖图案之一，高 2 bits 选择 4 种颜色（绿/青/黄/紫）之一
5. **图像拼接** — 将彩色瓷砖按交织顺序贴在 112×112 网格上（间距 9px），四角放定位锚点，生成 1024x1024 的 PNG 帧或实时视频流

每帧有效数据约 7500 字节，视频模式 15fps 下吞吐量约 850 kbit/s。

---

## User

用中文写个 libcimbar原理.md 把你读到的一些技术性细节也写进去，我可以频频回顾一下这样。

---

## Assistant (Build · DeepSeek V4 Flash · 4.6s)

_Thinking:_

The user wants me to write a detailed Chinese document about libcimbar's principles, including technical details from the exploration. They want it saved as `libcimbar原理.md`. Let me read a few key source files to get more accurate technical details before writing.

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/encoder/Encoder.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/encoder/Encoder.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "reed_solomon_stream.h"
5: #include "bit_file/bitreader.h"
6: #include "bit_file/bitbuffer.h"
7: #include "cimb_translator/CimbWriter.h"
8: #include "cimb_translator/Config.h"
9: #include "compression/zstd_compressor.h"
10: #include "fountain/fountain_encoder_stream.h"
11: 
12: #include <opencv2/opencv.hpp>
13: #include <optional>
14: #include <string>
15: 
16: class Encoder
17: {
18: public:
19: 	Encoder(unsigned bits_per_symbol=0, int bits_per_color=-1);
20: 	void set_encode_id(uint8_t encode_id); // [0-127] -- the high bit is ignored.
21: 	void set_color_mode(unsigned color_mode);
22: 
23: 	template <typename STREAM>
24: 	std::optional<cv::Mat> encode_next(STREAM& stream, cimbar::vec_xy canvas_size={});
25: 
26: 	template <typename STREAM>
27: 	fountain_encoder_stream::ptr create_fountain_encoder(STREAM& stream, const std::string_view& filename, int compression_level=16);
28: 
29: protected:
30: 	template <typename STREAM>
31: 	std::optional<cv::Mat> encode_next_coupled(STREAM& stream, cimbar::vec_xy canvas_size={});
32: 
33: protected:
34: 	unsigned _eccBytes;
35: 	unsigned _eccBlockSize;
36: 	unsigned _bitsPerSymbol;
37: 	unsigned _bitsPerColor;
38: 	bool _dark;
39: 	bool _coupled;
40: 	unsigned _colorMode;
41: 	uint8_t _encodeId = 0;
42: };
43: 
44: inline Encoder::Encoder(unsigned bits_per_symbol, int bits_per_color)
45: 	: _eccBytes(cimbar::Config::ecc_bytes())
46: 	, _eccBlockSize(cimbar::Config::ecc_block_size())
47: 	, _bitsPerSymbol(bits_per_symbol? bits_per_symbol : cimbar::Config::symbol_bits())
48: 	, _bitsPerColor(bits_per_color >= 0? bits_per_color : cimbar::Config::color_bits())
49: 	, _dark(cimbar::Config::dark())
50: 	, _coupled(cimbar::Config::legacy_mode())
51: 	, _colorMode(cimbar::Config::color_mode())
52: {
53: }
54: 
55: 
56: // TODO: why does this exist here vs just being passed to create_fountain_encoder?
57: // seems confusing...
58: inline void Encoder::set_encode_id(uint8_t encode_id)
59: {
60: 	_encodeId = encode_id;
61: }
62: 
63: inline void Encoder::set_color_mode(unsigned color_mode)
64: {
65: 	_colorMode = color_mode;
66: }
67: 
68: template <typename STREAM>
69: inline std::optional<cv::Mat> Encoder::encode_next(STREAM& stream, cimbar::vec_xy canvas_size)
70: {
71: 	if (_coupled)
72: 		return encode_next_coupled(stream, canvas_size);
73: 
74: 	if (!stream.good())
75: 		return std::nullopt;
76: 
77: 	unsigned bits_per_op = _bitsPerColor + _bitsPerSymbol;
78: 	CimbWriter writer(_bitsPerSymbol, _bitsPerColor, _dark, _colorMode, canvas_size);
79: 
80: 	unsigned numCells = writer.num_cells();
81: 	bitbuffer bb(cimbar::Config::capacity(bits_per_op));
82: 
83: 	unsigned bitPos = 0;
84: 	unsigned endBitPos = numCells*bits_per_op;
85: 
86: 	int progress = 0;
87: 	unsigned bitsPerRead = _bitsPerSymbol;
88: 	unsigned bitsPerWrite = bits_per_op;
89: 
90: 	reed_solomon_stream rss(stream, _eccBytes, _eccBlockSize);
91: 	bitreader br;
92: 	while (rss.good() and progress < 2)  // 1 symbol pass + 1 color pass
93: 	{
94: 		unsigned bytes = rss.readsome();
95: 		if (bytes == 0)
96: 			break;
97: 		br.assign_new_buffer(rss.buffer(), bytes);
98: 
99: 		// reorder. We're encoding the symbol bits and striping them across the whole image
100: 		// then encoding the color bits and striping them in the same way (filling in the gaps)
101: 		while (!br.empty())
102: 		{
103: 			unsigned bits = br.read(bitsPerRead);
104: 			if (!br.partial())
105: 				bb.write(bits, bitPos, bitsPerWrite);
106: 			bitPos += bits_per_op;
107: 
108: 			if (bitPos >= endBitPos)
109: 			{
110: 				// switch to color section
111: 				bitsPerRead = _bitsPerColor;
112: 				bitsPerWrite = _bitsPerColor;
113: 				bitPos = 0;
114: 				++progress;
115: 				break;
116: 			}
117: 		}
118: 	}
119: 
120: 	// dump whatever we have to image
121: 	for (bitPos = 0; bitPos < endBitPos; bitPos+=bits_per_op)
122: 	{
123: 		unsigned bits = bb.read(bitPos, bits_per_op);
124: 		writer.write(bits);
125: 	}
126: 
127: 	// return what we've got
128: 	return writer.image();
129: }
130: 
131: template <typename STREAM>
132: inline std::optional<cv::Mat> Encoder::encode_next_coupled(STREAM& stream, cimbar::vec_xy canvas_size)
133: {
134: 	// the old way. Symbol and color bits are mixed together, limiting the color correction possibilities
135: 	// but potentially allowing a lack of errors in one channel to correct errors in the other.
136: 	// ... also, potentially allowing a preponderance of errors in one channel to doom the whole decode.
137: 	// the net result is that best case performance *can* be better this way, but average and worst case
138: 	// will be worse.
139: 	if (!stream.good())
140: 		return std::nullopt;
141: 
142: 	unsigned bits_per_op = _bitsPerColor + _bitsPerSymbol;
143: 	CimbWriter writer(_bitsPerSymbol, _bitsPerColor, _dark, _colorMode, canvas_size);
144: 
145: 	reed_solomon_stream rss(stream, _eccBytes, _eccBlockSize);
146: 	bitreader br;
147: 	while (rss.good())
148: 	{
149: 		unsigned bytes = rss.readsome();
150: 		if (bytes == 0)
151: 			break;
152: 		br.assign_new_buffer(rss.buffer(), bytes);
153: 		while (!br.empty())
154: 		{
155: 			unsigned bits = br.read(bits_per_op);
156: 			if (!br.partial())
157: 				writer.write(bits);
158: 		}
159: 		if (writer.done())
160: 			return writer.image();
161: 	}
162: 	// we don't have a full frame, but return what we've got
163: 	return writer.image();
164: }
165: 
166: template <typename STREAM>
167: inline fountain_encoder_stream::ptr Encoder::create_fountain_encoder(STREAM& stream, const std::string_view& filename, int compression_level)
168: {
169: 	unsigned chunk_size = cimbar::Config::fountain_chunk_size();
170: 
171: 	std::stringstream ss;
172: 	if (compression_level <= 0)
173: 		ss << stream.rdbuf();
174: 	else
175: 	{
176: 		cimbar::zstd_compressor<std::stringstream> f;
177: 		if (!filename.empty())
178: 			f.write_header(filename.data(), filename.size());
179: 		if (!f.compress(stream))
180: 			return nullptr;
181: 
182: 		// find size of compressed zstd stream, and pad it if necessary.
183: 		size_t compressedSize = f.size();
184: 		if (compressedSize < chunk_size)
185: 			f.pad(chunk_size - compressedSize + 1);
186: 		ss = std::move(f);
187: 	}
188: 
189: 	return fountain_encoder_stream::create(ss, chunk_size, _encodeId);
190: }
191: 

(End of file - total 191 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbEncoder.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbEncoder.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include <opencv2/opencv.hpp>
5: 
6: #include <string>
7: #include <vector>
8: 
9: class CimbEncoder
10: {
11: public:
12: 	CimbEncoder(unsigned symbol_bits, unsigned color_bits, bool dark=true, unsigned color_mode=1);
13: 
14: 	cv::Mat load_tile(unsigned symbol_bits, unsigned index);
15: 	bool load_tiles(unsigned symbol_bits);
16: 
17: 	const cv::Mat& encode(unsigned bits) const;
18: 
19: protected:
20: 	std::vector<cv::Mat> _tiles;
21: 	unsigned _numSymbols;
22: 	unsigned _numColors;
23: 	bool _dark;
24: 	unsigned _colorMode;
25: };

(End of file - total 25 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/bitmaps.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/bitmaps.h</path>
<type>file</type>
<content>
1: namespace cimbar {
2: static const std::map<std::string, std::string> bitmaps = {
3: {"bitmap/2/00.png", R"(vuk:eJs4+BAAN/-MCG4DAADHAABtZLYAAANJTkaoAA6y#TzjGFAA$FDAqCsCco~EbAAA'yCG@v_CLK;IA'B'1'1(-o5}s%9~(gEzXCg/0(KC{QC'0;XE/9/#iBAAC'nHpwZ9)'&F)"},
4: {"bitmap/2/01.png", R"(vuk:eJs4+BAAN/-MCG4DAADHAABtZLYAAANJTkaoAA6y#TzjGFAA$FDAqCsCco~EbAAAPzCG@v_CLK;IA'B'tWPcq1,(KC{Qw2t/_(Tj'hDTV)kAJ1[Mk)_SvlLAAAdZJzDwHP_Q%)"},
5: {"bitmap/2/02.png", R"(vuk:eJs4+BAAN/-MCG4DAADHAABtZLYAAANJTkaoAA6y#TzjGFAA$FDAqCsCco~EbAAAvzCG@v_CLK;IA'B'1'1(YEGRxt(a[hFBQ/_(sI$~Ybu+SAY4$`FBn``[CAAAXLhYKMe8#(\C)"},
6: {"bitmap/2/03.png", R"(vuk:eJs4+BAAN/-MCG4DAADHAABtZLYAAANJTkaoAA6y#TzjGFAA$FDAqCsCco~EbAAAfzCG@v_CY^So5JAAU)ai%|tW(8Xj^zH9_t(C$1XUh'(C~tC[}hv2Q?=BAAAAr7vbMo\F9BB)"},
7: {"bitmap/4/00.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA&aCw`XaxEylEB'B'atItc|3|y|UAp!E8e+_GyD?^VR_3wcW'Qh#GAaztVEMA`*8DBfF/k8AAAA9ZNoBz[uhNI)"},
8: {"bitmap/4/01.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA]aCw`Xax=HOU:EBtV)%tL-IV&7RD,W?p/]fZO-8B$}Fhjf%GC}~Y8(/Y$w|t^oN/ZNiN0s}\5AAAuW&wUY6eit&F)"},
9: {"bitmap/4/02.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA]aCw`Xax4{NUBFBt&t%tk`B'H{1N6k{N,,B$ho#%7F'Su&HLs]iDkfwcgZSGR501W0WE+WxodAAAuW&wUY6eit&F)"},
10: {"bitmap/4/03.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA'aCw`Xax4{NUWGC'Ki?($[.Vk~Mw/(Vi|NaT&N6S!Z/oaW/o\9F-6}_UbXy:pB}A{4/A0(]PciAAAA_cE3Y@%LfDC)"},
11: {"bitmap/4/04.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAAaaCw`XaxEy\~B'B'zA:hI0mgUE[h!G=C5F_AzIpYu+eM:GP^t/AyEA6}cu6:4fVWAAAA=5GUv+oX!GE)"},
12: {"bitmap/4/05.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA3bCw`XaxJTvR3DC'ai\!!~?Q/K&u:Ihr=k[CmZ^tc74&_pS16y?}w(e2A(kb_Ug0kId-N)|2Pjb$6Cq@`8NAAA6yQMFGPe=hnB)"},
13: {"bitmap/4/06.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA#bCw`Xax=HvR'EC'DX)_A'MJe7fGx+xz9fO!vadv&):zQ*Wy/[.@mw%,DZK?]yTayU_=[o;%oNaMmC?`6^2yAAAA=5GUv+oX!GE)"},
14: {"bitmap/4/07.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAAHbCw`Xax=HnR'EAA?W=['~ZSw*NReO=a7#Y$)DW=%MX]VJschDxl0)eGxlG{4#2S4M1X#ydhP]kEAAAA?O@GxgqB_W)"},
15: {"bitmap/4/08.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAAPbCw`XaxEylEB'B'.(KC!}MBbB`(CiHH*6.\lXB?\+bPNv3W2(,GVLaH%=hiRQ4(AAs8,UoY=&$BFAAAdZJzDwHP_Q%)"},
16: {"bitmap/4/09.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA'aCw`XaxEylEB'B'atIt*$._E%W'~?~tM[0tVEIMyq|9Pzs8Lv\hHH%mln=kbHBA3|duUVX5/8AAAA=5GUv+oX!GE)"},
17: {"bitmap/4/0a.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA]aCw`XaxEylEB'B'atIt+~.(KCtdiN$Ia67PQIl%XR%g;C#Bx?]AbN2eggTtBAo.rugwO$ofdAAA6yQMFGPe=hnB)"},
18: {"bitmap/4/0b.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAAiaCw`XaxEylEB'B'b'x(CiJBDtdEEAR56}mz9~x2.\kD5yjG5y[B&C+\2PlZ001+&AAAC'nHpwZ9)'&F)"},
19: {"bitmap/4/0c.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAAqaCw`XaxEylEB'B'X4};pI.(KCR?CKdt~QO*,=bP\WUtfdIw8DV/!y]B0k}0~Fok*2lAAAXLhYKMe8#(\C)"},
20: {"bitmap/4/0d.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAAnbCw`Xax4{uR3DAA?WHb|~\jg1\Fw+ij&M[M.THhSA#;=+&1iDl3`vvOZik1@iRjSnl2eBlVXEE!^so1AAAA9ZNoBz[uhNI)"},
21: {"bitmap/4/0e.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAAnbCw`XaxEy\~B'B'1'z(OO21/1Xbw+&a5N{~!eggL*zHiC!u\g`p=\o'7w6iS/bWrI,(AA,9:BMOC7XMAAAA=5GUv+oX!GE)"},
22: {"bitmap/4/0f.png", R"(vuk:eJs4+BAAN/-MCG4DAAXLAAAA7FMAAA_)z_ZMAAdZ=J.!wCC'\vBAVBWBP%tvNAAA'aCw`XaxEy\~B'B'zA:hJB~(ZSxt(ajLv(AmX~8Z:GjDbP2!jRw2.\&JY.{N7NAA7*wB{2YGO7AAAA=5GUv+oX!GE)"},
23: {"bitmap/anchor-dark.png", R"(vuk:eJs4+BAAN/-MCG4DAAk_AAAA.FMAAAkUJ/IKAAdZ=J.!wCC'\vBAVBWBP%tvNAAAskCw`Xg#A$mr2Dmu1X~4C*~sns0GV]=CYS#olfs)j'~4i8n5Rja=Ytlm5slR:K~%Re)(F-S:Diz*Xqr{NUwa{;Bk?S-GK)OSFpFE@:M:iVE7===wt=CmK]BRxEZ3j6H)OSFpv1YC7.S:DiHJwU*aNu{pf_E{\r~v.?Qi*aNuRIC?`8EzH`I@Qi*aNuRIC?%|Z:Y$Z4U!g~lh1lJAAAuW&wUY6eit&F)"},
24: {"bitmap/anchor-light.png", R"(vuk:eJs4+BAAN/-MCG4DAAk_AAAA.FMAAAkUJ/IKAAdZ=J.!wCC'\vBAVBWBP%tvNAAA_kCw`Xg#A$?UPF$AO5sc?_B'29saGT,H|Ur0xDcVZID6t;$O}@TspIvaOXvO4`D`^#5+#2)EZ3j6rgZ1L4P~'`?gle{?Qi*aNuRIC?'8XK\[+jB_Zj9NSudkK]BRxEZ3j6H)OSFpv1YC7.S:DiHJwU*aY=f$j(OqiS#2)EZ3j6rg`%'0A}J@#2)EZ3j6rgw$X6!^2VCP-G3l/(]`!tV07AAABtnHpwZ9)'&F)"},
25: {"bitmap/anchor-secondary-dark.png", R"(vuk:eJs4+BAAN/-MCG4DAAk_AAAA.FMAAAkUJ/IKAAdZ=J.!wCC'\vBAVBWBP%tvNAAAkkCw`Xg#A$mr+DpB`X~4HQ@~?QkxPv,Fiu%;Yj)E5/00wypzzSx^D4G\~nGi)!8[AesS&uCRxE+gFCvgFO)~v~Xgb60^Qi*aNudkK]BRxEZ3j6N:Y6op2TZ|k$\#%uCRxEZ3j6H)OSFpv1oyOgRLGbp^Qi*aNudkK]BRxEZ3j6N:XNXhwqibiC7.S:`gbP%,q!3CwE2v/&kEzWAAAA=5GUv+oX!GE)"},
26: {"bitmap/anchor-secondary-light.png", R"(vuk:eJs4+BAAN/-MCG4DAAk_AAAA.FMAAAkUJ/IKAAdZ=J.!wCC'\vBAVBWBP%tvNAAA-kCw`Xg#A$?U'ElB+B~42)~s?ig#6uKOY^SeM!#zs5al&ls^&y?sx'A9NLY0gWRy8;+L*mBTFpv1[nS9dW=.H]B\6K&x%uCRxEZ3j6H)OSFpv1%XLQk;xhvn]p|w%uCRxEZ3j6H)OSFpv1waXgUy$N`$F-S:DiHJwU*aNudkK]h#5GXWYV-GK)OSFpFEb~K:*rh$\FmDEBUwmw:c=SmlAAAA=5GUv+oX!GE)"},
27: {"bitmap/guide-horizontal-dark.png", R"(vuk:eJs4+BAAN/-MCG4DAA6yBAC'5FMAAAR9S,$BAAdZ=J.!wCC'\vBAVBWBP%tvNAAA$YCw`XJ0Cy\~B'B'di~haCJlf^Vxa4CAmy4B|;F4_RBAAA9ZNoBz[uhNI)"},
28: {"bitmap/guide-horizontal-light.png", R"(vuk:eJs4+BAAN/-MCG4DAA6yBAC'5FMAAAR9S,$BAAdZ=J.!wCC'\vBAVBWBP%tvNAAA$YCw`XJ0CylE[h_sbxWEaAdCTFeO+OAA_cYtpIKWUOAAAA=5GUv+oX!GE)"},
29: {"bitmap/guide-vertical-dark.png", R"(vuk:eJs4+BAAN/-MCG4DAA:CAAAA]FMAAAs8KD,IAAdZ=J.!wCC'\vBAVBWBP%tvNAAAFZCw`XJ0Cy\~B'B'0tVE[hHB~hpx+RJ,pVZA|;DV-vzcLWKBAABtnHpwZ9)'&F)"},
30: {"bitmap/guide-vertical-light.png", R"(vuk:eJs4+BAAN/-MCG4DAA:CAAAA]FMAAAs8KD,IAAdZ=J.!wCC'\vBAVBWBP%tvNAAA}YCw`XJ0CylE[h_s{Qxt(aNtXxnR(*@I`NDAqv9(Xu`EEAAAAAr7vbMo\F9BB)"},
31: };
32: }

(End of file - total 32 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/fountain/FountainEncoder.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/fountain/FountainEncoder.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "FountainInit.h"
5: #include "wirehair/wirehair.h"
6: #include <cassert>
7: 
8: class FountainEncoder
9: {
10: protected:
11: 	void swap(FountainEncoder& other) throw()
12: 	{
13: 		std::swap(_codec, other._codec);
14: 		std::swap(_packetSize, other._packetSize);
15: 	}
16: 
17: public:
18: 	FountainEncoder(const uint8_t* data, size_t length, size_t packet_size)
19: 	    : _packetSize(packet_size)
20: 	{
21: 		FountainInit::init();
22: 		_codec = wirehair_encoder_create(nullptr, data, length, packet_size);
23: 	}
24: 
25: 	~FountainEncoder()
26: 	{
27: 		wirehair_free(_codec);
28: 	}
29: 
30: 	FountainEncoder& operator=(FountainEncoder temp)
31: 	{
32: 		temp.swap(*this);
33: 		return *this;
34: 	}
35: 
36: 	bool good() const
37: 	{
38: 		return _codec != nullptr;
39: 	}
40: 
41: 	size_t encode(unsigned block_num, uint8_t* buff, size_t size)
42: 	{
43: 		assert(size == _packetSize);
44: 		uint32_t written = 0;
45: 		WirehairResult res = wirehair_encode(_codec, block_num, buff, size, &written);
46: 		if (res != Wirehair_Success)
47: 			return 0;
48: 		return written;
49: 	}
50: 
51: 	size_t packet_size() const
52: 	{
53: 		return _packetSize;
54: 	}
55: 
56: protected:
57: 	WirehairCodec _codec;
58: 	size_t _packetSize;
59: };

(End of file - total 59 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/fountain/FountainMetadata.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/fountain/FountainMetadata.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include <array>
5: 
6: class FountainMetadata
7: {
8: protected:
9: 	static void update_block_id_internal(uint16_t block_id, uint8_t* arr)
10: 	{
11: 		arr[0] = (block_id >> 8) & 0xFF;
12: 		arr[1] = block_id & 0xFF;
13: 	}
14: 
15: public:
16: 	static const unsigned md_size = 6;
17: 
18: 	static void to_uint8_arr(uint8_t encode_id, unsigned size, uint16_t block_id, uint8_t* arr)
19: 	{
20: 		arr[0] = (encode_id & 0x7F) | ((size >> 17) & 0x80);
21: 		arr[1] = (size >> 16) & 0xFF;
22: 		arr[2] = (size >> 8) & 0xFF;
23: 		arr[3] = size & 0xFF;
24: 		update_block_id_internal(block_id, arr+4);
25: 	}
26: 
27: public:
28: 	FountainMetadata(uint32_t id)
29: 	{
30: 		uint8_t* d = _data.data();
31: 		std::copy(reinterpret_cast<uint8_t*>(&id), reinterpret_cast<uint8_t*>(&id)+4, d);
32: 		d[4] = d[5] = 0;
33: 	}
34: 
35: 	explicit FountainMetadata(const char* buff, unsigned len)
36: 	{
37: 		if (len > md_size)
38: 			len = md_size;
39: 		std::copy(buff, buff+len, _data.data());
40: 	}
41: 
42: 	FountainMetadata(uint8_t encode_id, unsigned size, uint16_t block_id)
43: 	{
44: 		uint8_t* d = _data.data();
45: 		to_uint8_arr(encode_id, size, block_id, d);
46: 	}
47: 
48: 	uint32_t id() const
49: 	{
50: 		uint32_t shortid = 0;
51: 		std::copy(data(), data()+4, reinterpret_cast<uint8_t*>(&shortid));
52: 		return shortid;
53: 	}
54: 
55: 	uint8_t encode_id() const
56: 	{
57: 		return data()[0] & 0x7F;
58: 	}
59: 
60: 	uint16_t block_id() const
61: 	{
62: 		const uint8_t* d = data()+4;
63: 		uint16_t res = d[1];
64: 		res |= ((uint16_t)d[0] << 8);
65: 		return res;
66: 	}
67: 
68: 	void increment_block_id(unsigned radioactive_block_id)
69: 	{
70: 		unsigned next = block_id()+1;
71: 		if (next == radioactive_block_id)
72: 			next += 1;
73: 		update_block_id_internal(next, _data.data()+4);
74: 	}
75: 
76: 	unsigned file_size() const
77: 	{
78: 		const uint8_t* d = data();
79: 		unsigned res = d[3];
80: 		res |= ((uint32_t)d[2] << 8);
81: 		res |= ((uint32_t)d[1] << 16);
82: 		res |= ((uint32_t)d[0] & 0x80) << 17;
83: 		return res;
84: 	}
85: 
86: 	const uint8_t* data() const
87: 	{
88: 		return const_cast<const uint8_t*>(_data.data());
89: 	}
90: 
91: protected:
92: 	std::array<uint8_t, 6> _data;
93: };

(End of file - total 93 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/encoder/ReedSolomon.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/encoder/ReedSolomon.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: extern "C" {
5:     #include "libcorrect/include/correct.h"
6: }
7: 
8: // a wrapper for libcorrect's correct_reed_solomon_encode()
9: 
10: class ReedSolomon
11: {
12: public:
13: 	struct BadChunk
14: 	{
15: 		unsigned size;
16: 
17: 		explicit BadChunk(unsigned size)
18: 		    : size(size)
19: 		{}
20: 	};
21: 
22: public:
23: 	ReedSolomon(size_t parity_bytes)
24: 	    : _parityBytes(parity_bytes)
25: 	{
26: 		_rs = correct_reed_solomon_create(correct_rs_primitive_polynomial_8_7_2_1_0, 1, 1, _parityBytes);
27: 	}
28: 
29: 	~ReedSolomon()
30: 	{
31: 		correct_reed_solomon_destroy(_rs);
32: 	}
33: 
34: 	unsigned parity() const
35: 	{
36: 		return _parityBytes;
37: 	}
38: 
39: 	ssize_t encode(const char* msg, unsigned msg_length, char* encoded)
40: 	{
41: 		return correct_reed_solomon_encode(_rs, reinterpret_cast<const uint8_t*>(msg), msg_length, reinterpret_cast<uint8_t*>(encoded));
42: 	}
43: 
44: 	ssize_t decode(const char* encoded, unsigned encoded_length, char* msg)
45: 	{
46: 		return correct_reed_solomon_decode(_rs, reinterpret_cast<const uint8_t*>(encoded), encoded_length, reinterpret_cast<uint8_t*>(msg));
47: 	}
48: 
49: protected:
50: 	correct_reed_solomon* _rs;
51: 	unsigned _parityBytes;
52: };

(End of file - total 52 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CellPositions.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CellPositions.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "util/vec_xy.h"
5: #include <cstddef>
6: #include <utility>
7: #include <vector>
8: 
9: class CellPositions
10: {
11: public:
12: 	using coordinate = std::pair<int, int>;
13: 	using positions_list = std::vector<coordinate>;
14: 
15: 	static positions_list compute_linear(cimbar::vec_xy spacing, cimbar::vec_xy dimensions, int offset, cimbar::vec_xy marker_size);
16: 	static positions_list compute(cimbar::vec_xy spacing, cimbar::vec_xy dimensions, int offset, cimbar::vec_xy marker_size, int interleave_blocks=0, int interleave_partitions=1);
17: 
18: public:
19: 	CellPositions(cimbar::vec_xy spacing, cimbar::vec_xy dimensions, int offset, cimbar::vec_xy marker_size,int interleave_blocks=0, int interleave_partitions=1);
20: 
21: 	unsigned index() const;
22: 	size_t count() const;
23: 	void reset();
24: 	bool done() const;
25: 	const coordinate& next();
26: 
27: 	const positions_list& positions() const;
28: 
29: protected:
30: 	unsigned _index;
31: 	positions_list _positions;
32: };

(End of file - total 32 lines)
</content>
```

---

## Assistant (Build · DeepSeek V4 Flash · 4.1s)

_Thinking:_

Now let me read a few more files to get more technical details for the document.

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Config.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Config.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "GridConf.h"
5: 
6: namespace cimbar
7: {
8: 	class Config
9: 	{
10: 	protected:
11: 		static cimbar::conf& active_conf()
12: 		{
13: 			static thread_local cimbar::conf cc = cimbar::Conf8x8();
14: 			return cc;
15: 		}
16: 
17: 
18: 	public:
19: 		static cimbar::conf temp_conf(int mode_val=0)
20: 		{
21: 			cimbar::conf cc;
22: 			switch (mode_val)
23: 			{
24: 				case 4:
25: 					cc = cimbar::Conf8x8();
26: 					cc.color_bits = 2;
27: 					cc.legacy_mode = true;
28: 					cc.fountain_chunks_scalar = -10;
29: 					return cc;
30: 				case 8:
31: 					cc = cimbar::Conf8x8();
32: 					cc.color_bits = 3;
33: 					cc.legacy_mode = true;
34: 					cc.fountain_chunks_scalar = -10;
35: 					return cc;
36: 				case 66:
37: 					return cimbar::Conf8x8_micro();
38: 				case 67:
39: 					return cimbar::Conf8x8_mini();
40: 				case 68:
41: 				default:
42: 					return cimbar::Conf8x8();
43: 			}
44: 		}
45: 
46: 		static void update(int mode_val=0)
47: 		{
48: 			active_conf() = temp_conf(mode_val);
49: 		}
50: 
51: 		static bool dark()
52: 		{
53: 			return true;
54: 		}
55: 
56: 		static bool legacy_mode()
57: 		{
58: 			return active_conf().legacy_mode;
59: 		}
60: 
61: 		static unsigned color_mode()
62: 		{
63: 			return active_conf().legacy_mode? 0 : 1; // unless we override per-thread?
64: 		}
65: 
66: 		static unsigned color_bits()
67: 		{
68: 			return active_conf().color_bits;
69: 		}
70: 
71: 		static unsigned symbol_bits()
72: 		{
73: 			return active_conf().symbol_bits;
74: 		}
75: 
76: 		static unsigned bits_per_cell()
77: 		{
78: 			return active_conf().bits_per_cell();
79: 		}
80: 
81: 		static unsigned ecc_bytes()
82: 		{
83: 			return active_conf().ecc_bytes;
84: 		}
85: 
86: 		static unsigned ecc_block_size()
87: 		{
88: 			return active_conf().ecc_block_size;
89: 		}
90: 
91: 		static unsigned image_size_x()
92: 		{
93: 			return active_conf().image_size_x;
94: 		}
95: 
96: 		static unsigned image_size_y()
97: 		{
98: 			return active_conf().image_size_y;
99: 		}
100: 
101: 		static unsigned anchor_size()
102: 		{
103: 			return 30;
104: 		}
105: 
106: 		static constexpr unsigned cell_size()
107: 		{
108: 			// TODO: runtime
109: 			return 8;
110: 		}
111: 
112: 		static unsigned cell_spacing_x()
113: 		{
114: 			return active_conf().cell_spacing_x;
115: 		}
116: 
117: 		static unsigned cell_spacing_y()
118: 		{
119: 			return active_conf().cell_spacing_y;
120: 		}
121: 
122: 		static unsigned corner_padding_x()
123: 		{
124: 			return active_conf().corner_padding_x();
125: 		}
126: 
127: 		static unsigned corner_padding_y()
128: 		{
129: 			return active_conf().corner_padding_y();
130: 		}
131: 
132: 		static unsigned cell_offset()
133: 		{
134: 			return active_conf().cell_offset;
135: 		}
136: 
137: 		static unsigned cells_per_col_x()
138: 		{
139: 			return active_conf().cells_per_col_x;
140: 		}
141: 
142: 		static unsigned cells_per_col_y()
143: 		{
144: 			return active_conf().cells_per_col_y;
145: 		}
146: 
147: 		static unsigned total_cells()
148: 		{
149: 			return active_conf().total_cells();
150: 		}
151: 
152: 		static unsigned capacity(unsigned bitspercell=0)
153: 		{
154: 			return active_conf().capacity(bitspercell);
155: 		}
156: 
157: 		static unsigned interleave_blocks()
158: 		{
159: 			return ecc_block_size();
160: 		}
161: 
162: 		static unsigned interleave_partitions()
163: 		{
164: 			return 2;
165: 		}
166: 
167: 		static unsigned fountain_chunks_per_frame(unsigned bitspercell=0)
168: 		{
169: 			return active_conf().fountain_chunks_per_frame(bitspercell);
170: 		}
171: 
172: 		static unsigned fountain_chunk_size(unsigned bitspercell=0)
173: 		{
174: 			return active_conf().fountain_chunk_size(bitspercell);
175: 		}
176: 
177: 		static unsigned compression_level()
178: 		{
179: 			return 16;
180: 		}
181: 	};
182: }
183: 

(End of file - total 183 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Interleave.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Interleave.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include <vector>
5: 
6: namespace Interleave
7: {
8: 	inline std::vector<unsigned> interleave_indices(unsigned size, unsigned num_chunks, unsigned partitions)
9: 	{
10: 		std::vector<unsigned> indices;
11: 		if (num_chunks == 0)
12: 		{
13: 			for (unsigned i = 0; i < size; ++i)
14: 				indices.push_back(i);
15: 			return indices;
16: 		}
17: 
18: 		unsigned partitionSize = size/partitions;
19: 		for (unsigned part = 0; part < size; part+=partitionSize)
20: 			for (unsigned chunk = 0; chunk < num_chunks; ++chunk)
21: 				for (unsigned i = chunk; i < partitionSize; i+=num_chunks)
22: 					indices.push_back(i + part);
23: 		return indices;
24: 	}
25: 
26: 	inline std::vector<unsigned> interleave_reverse(unsigned size, unsigned num_chunks, unsigned partitions)
27: 	{
28: 		std::vector<unsigned> indices = interleave_indices(size, num_chunks, partitions);
29: 		std::vector<unsigned> inverted(indices.size(), 0);
30: 		for (unsigned src = 0; src < indices.size(); ++src)
31: 		{
32: 			unsigned dst = indices[src];
33: 			inverted[dst] = src;
34: 		}
35: 		return inverted;
36: 	}
37: 
38: 	template <typename PT>
39: 	inline std::vector<PT> interleave(const std::vector<PT>& positions, unsigned num_chunks, unsigned partitions)
40: 	{
41: 		std::vector<PT> res;
42: 
43: 		std::vector<unsigned> indices = interleave_indices(positions.size(), num_chunks, partitions);
44: 		for (unsigned i = 0; i < indices.size(); ++i)
45: 		{
46: 			unsigned interleaveIdx = indices[i];
47: 			res.push_back(positions[interleaveIdx]);
48: 		}
49: 		return res;
50: 	}
51: }

(End of file - total 51 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbWriter.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbWriter.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "CellPositions.h"
5: #include "CimbEncoder.h"
6: #include "util/vec_xy.h"
7: 
8: class CimbWriter
9: {
10: public:
11: 	CimbWriter(unsigned symbol_bits, unsigned color_bits, bool dark=true, unsigned color_mode=1, cimbar::vec_xy size={});
12: 
13: 	bool write(unsigned bits);
14: 	bool done() const;
15: 
16: 	cv::Mat image() const;
17: 
18: 	unsigned num_cells() const;
19: 
20: protected:
21: 	void paste(const cv::Mat& img, int x, int y);
22: 
23: protected:
24: 	cv::Mat _image;
25: 	CellPositions _positions;
26: 	CimbEncoder _encoder;
27: 	unsigned _offsetX = 0;
28: 	unsigned _offsetY = 0;
29: };

(End of file - total 29 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/compression/zstd_compressor.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/compression/zstd_compressor.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "zstd/zstd.h"
5: 
6: #include "serialize/format.h"
7: #include <array>
8: #include <sstream>
9: #include <vector>
10: 
11: namespace cimbar {
12: 
13: template <typename STREAM>
14: class zstd_compressor : public STREAM
15: {
16: public:
17: 	using STREAM::STREAM; // pull in constructors
18: 	static const size_t CHUNK_SIZE = 0x4000;
19: 
20: public:
21: 	~zstd_compressor()
22: 	{
23: 		if (_cctx)
24: 			ZSTD_freeCCtx(_cctx);
25: 	}
26: 
27: 	// if you call write directly, len should be a multiple of CHUNK_SIZE
28: 	// .. or the final bytes of the input
29: 	bool write(const char* data, size_t len)
30: 	{
31: 		size_t writeLen = CHUNK_SIZE;
32: 		while (len > 0)
33: 		{
34: 			if (len < writeLen)
35: 				writeLen = len;
36: 			size_t compressedBytes = ZSTD_compressCCtx(_cctx, _compBuff.data(), _compBuff.size(), data, writeLen, _compressionLevel);
37: 			if (ZSTD_isError(compressedBytes))
38: 			{
39: 				fmt::print("error? {}\n", ZSTD_getErrorName(compressedBytes));
40: 				return false;
41: 			}
42: 			STREAM::write(_compBuff.data(), compressedBytes);
43: 
44: 			data += writeLen;
45: 			len -= writeLen;
46: 		}
47: 		return true;
48: 	}
49: 
50: 	void set_compression_level(int level)
51: 	{
52: 		if (level > 0)
53: 			_compressionLevel = level;
54: 	}
55: 
56: 	template <typename INSTREAM>
57: 	size_t compress(INSTREAM& raw, int compression_level=0)
58: 	{
59: 		if (!_cctx)
60: 			return 0;
61: 		set_compression_level(compression_level);
62: 
63: 		std::array<char, CHUNK_SIZE> rawBuff;
64: 		size_t totalBytesRead = 0;
65: 		while (raw)
66: 		{
67: 			raw.read(rawBuff.data(), rawBuff.size());
68: 			std::streamsize bytesRead = raw.gcount();
69: 			if (bytesRead <= 0)
70: 				break;
71: 			totalBytesRead += bytesRead;
72: 
73: 			if (!write(rawBuff.data(), bytesRead))
74: 				break;
75: 		}
76: 		return totalBytesRead;
77: 	}
78: 
79: 	size_t pad(unsigned len)
80: 	{
81: 		if (len < 9)
82: 			len = 9;
83: 
84: 		std::string temp(len-8, '\0');
85: 		size_t writ = ZSTD_writeSkippableFrame(_compBuff.data(), _compBuff.size(), temp.data(), temp.size(), 0);
86: 
87: 		STREAM::write(_compBuff.data(), writ);
88: 		return writ;
89: 	}
90: 
91: 	size_t write_header(const char* data, unsigned len)
92: 	{
93: 		std::string temp = "\x01";
94: 		temp += std::string_view(data, len);
95: 		size_t writ = ZSTD_writeSkippableFrame(_compBuff.data(), _compBuff.size(), temp.data(), temp.size(), 0);
96: 		STREAM::write(_compBuff.data(), writ);
97: 		return writ;
98: 	}
99: 
100: 	size_t size()
101: 	{
102: 		STREAM::seekg(0, std::ios::end);
103: 		size_t len = STREAM::tellg();
104: 		STREAM::seekg(0, std::ios::beg);
105: 		return len;
106: 	}
107: 
108: protected:
109: 	int _compressionLevel = 16;
110: 	ZSTD_CCtx* _cctx = ZSTD_createCCtx();
111: 	std::vector<char> _compBuff = std::vector<char>(ZSTD_compressBound(CHUNK_SIZE));
112: };
113: 
114: }

(End of file - total 114 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/encoder/reed_solomon_stream.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/encoder/reed_solomon_stream.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "ReedSolomon.h"
5: #include "encoder/aligned_stream.h"
6: #include <fstream>
7: #include <sstream>
8: #include <vector>
9: 
10: template <typename STREAM>
11: class reed_solomon_stream
12: {
13: public:
14: 	reed_solomon_stream(STREAM& stream, unsigned ecc, unsigned buffer_size)
15: 		: _stream(stream)
16: 		, _rs(ecc)
17: 		, _good(stream.good())
18: 	{
19: 		_buffer.resize(buffer_size, 0);
20: 	}
21: 
22: 	bool good() const
23: 	{
24: 		return _good and _stream.good();
25: 	}
26: 
27: 	long tellp() const
28: 	{
29: 		return _stream.tellp();
30: 	}
31: 
32: 	std::streamsize readsome(char* data=NULL, unsigned length=0)
33: 	{
34: 		if (!data)
35: 			data = _buffer.data();
36: 		if (!length)
37: 			length = _buffer.size();
38: 
39: 		_stream.read(data, length - _rs.parity());
40: 		std::streamsize bytes = _stream.gcount();
41: 		if (bytes <= 0)
42: 		{
43: 			_good = false;
44: 			return bytes;
45: 		}
46: 		if (!_rs.parity())
47: 			return bytes;
48: 
49: 		// else
50: 		_rs.encode(data, bytes, data);
51: 		return _buffer.size();
52: 	}
53: 
54: 	reed_solomon_stream& write(const char* data, unsigned length)
55: 	{
56: 		// length should be a multiple of buffer_size
57: 		// we might implement some "leftovers" functionality, e.g. storing how many bytes were unprocessed
58: 		if (!_rs.parity())
59: 		{
60: 			_stream.write(data, length);
61: 			return *this;
62: 		}
63: 
64: 		// else
65: 		while (length >= _buffer.size())
66: 		{
67: 			ssize_t bytes = _rs.decode(data, _buffer.size(), _buffer.data());
68: 			if (bytes <= 0)
69: 				_stream << ReedSolomon::BadChunk(_buffer.size() - _rs.parity());
70: 			else
71: 				_stream.write(_buffer.data(), bytes);
72: 
73: 			length -= _buffer.size();
74: 			data += _buffer.size();
75: 		}
76: 		return *this;
77: 	}
78: 
79: 	const char* buffer() const
80: 	{
81: 		return _buffer.data();
82: 	}
83: 
84: protected:
85: 	std::vector<char> _buffer;
86: 	STREAM& _stream;
87: 	ReedSolomon _rs;
88: 	bool _good;
89: };
90: 
91: inline std::ifstream& operator<<(std::ifstream& s, const ReedSolomon::BadChunk&)
92: {
93: 	return s;
94: }
95: 
96: inline std::ofstream& operator<<(std::ofstream& os, const ReedSolomon::BadChunk& chunk)
97: {
98: 	os << std::string(chunk.size, '\0');
99: 	return os;
100: }
101: 
102: inline std::stringstream& operator<<(std::stringstream& s, const ReedSolomon::BadChunk& chunk)
103: {
104: 	std::string temp(chunk.size, '\0');
105: 	s.write(temp.data(), temp.size());
106: 	return s;
107: }
108: 
109: template <typename STREAM>
110: inline aligned_stream<STREAM>& operator<<(aligned_stream<STREAM>& s, const ReedSolomon::BadChunk& chunk)
111: {
112: 	s.mark_bad_chunk(chunk.size);
113: 	return s;
114: }

(End of file - total 114 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/fountain/fountain_encoder_stream.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/fountain/fountain_encoder_stream.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "FountainEncoder.h"
5: #include "FountainMetadata.h"
6: #include <memory>
7: #include <sstream>
8: #include <string>
9: 
10: class fountain_encoder_stream
11: {
12: public:
13: 	using ptr = std::shared_ptr<fountain_encoder_stream>;
14: 
15: protected:
16: 	static const unsigned _headerSize = 6;
17: 
18: protected:
19: 	fountain_encoder_stream(std::string&& data, unsigned buffer_size, uint8_t encode_id)
20: 		: _data(data)
21: 		, _buffer(buffer_size, 0)
22: 		, _encodeId(encode_id)
23: 		, _encoder((uint8_t*)_data.data(), _data.size(), block_size())
24: 	{
25: 	}
26: 
27: 	unsigned block_size() const
28: 	{
29: 		return _buffer.size() - _headerSize;
30: 	}
31: 
32: public:
33: 	template <typename STREAM>
34: 	static fountain_encoder_stream::ptr create(STREAM& stream, unsigned buffer_size, uint8_t encode_id=0)
35: 	{
36: 		std::stringstream buffs;
37: 		if (stream)
38: 			buffs << stream.rdbuf();
39: 		return fountain_encoder_stream::ptr( new fountain_encoder_stream(buffs.str(), buffer_size, encode_id & 0x7F) );
40: 	}
41: 
42: 	// this resets the stream!
43: 	// but you might need to do if you change other parameters
44: 	// ex: different ECC settings => different payload size => different fountain buffer size
45: 	bool restart_and_resize_buffer(unsigned buffer_size)
46: 	{
47: 		if (buffer_size > _data.size())
48: 			return false;
49: 
50: 		_buffer.resize(buffer_size);
51: 		_encoder = FountainEncoder((uint8_t*)_data.data(), _data.size(), block_size());
52: 		restart();
53: 		return true;
54: 	}
55: 
56: 	bool good() const
57: 	{
58: 		return _encoder.good() and _data.size() > _encoder.packet_size();
59: 	}
60: 
61: 	void restart()
62: 	{
63: 		_block = 0;
64: 		_buffIndex = ~0U;
65: 		_lastRead = 0;
66: 	}
67: 
68: 	unsigned block_count() const
69: 	{
70: 		return _block;
71: 	}
72: 
73: 	unsigned blocks_required() const
74: 	{
75: 		return (_data.size() / block_size()) + 1;
76: 	}
77: 
78: 	void encode_new_block()
79: 	{
80: 		unsigned char* data = _buffer.data() + _headerSize;
81: 		size_t res = _encoder.encode(_block++, data, block_size());
82: 		if (res != block_size())
83: 			_encoder.encode(_block++, data, block_size()); // try twice -- the last initial block will be the wrong size
84: 
85: 		unsigned block = _block - 1; // we already incremented it above
86: 		// write header
87: 		FountainMetadata::to_uint8_arr(_encodeId, _data.size(), block, _buffer.data());
88: 		_buffIndex = 0;
89: 	}
90: 
91: 	// sometimes we want a new encoded batch, sometimes we just want our buffer
92: 	fountain_encoder_stream& read(char* data, unsigned length)
93: 	{
94: 		std::streamsize totalRead = 0;
95: 		while (length > 0 and good())
96: 		{
97: 			if (_buffIndex >= _buffer.size())
98: 				encode_new_block();
99: 
100: 			unsigned readLen = std::min(length, (unsigned)(_buffer.size() - _buffIndex));
101: 			if (!readLen)
102: 				break;
103: 			totalRead += readLen;
104: 
105: 			uint8_t* first = _buffer.data() + _buffIndex;
106: 			std::copy(first, first + readLen, data);
107: 
108: 			length -= readLen;
109: 			data += readLen;
110: 			_buffIndex += readLen;
111: 		}
112: 		_lastRead = totalRead;
113: 		return *this;
114: 	}
115: 
116: 	std::streamsize readsome(char* data, unsigned length)
117: 	{
118: 		read(data, length);
119: 		return gcount();
120: 	}
121: 
122: 	std::streamsize gcount() const
123: 	{
124: 		return _lastRead;
125: 	}
126: 
127: protected:
128: 	std::string _data;
129: 	std::vector<uint8_t> _buffer;
130: 	uint8_t _encodeId;
131: 	FountainEncoder _encoder;
132: 
133: 	unsigned _buffIndex = ~0U;
134: 	unsigned _block = 0;
135: 	std::streamsize _lastRead = 0;
136: };

(End of file - total 136 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/encoder/EncoderPlus.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/encoder/EncoderPlus.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include "Encoder.h"
5: #include "cimb_translator/Config.h"
6: #include "extractor/Scanner.h"
7: #include "serialize/format.h"
8: #include "util/File.h"
9: 
10: #include <opencv2/opencv.hpp>
11: #include <filesystem>
12: #include <functional>
13: #include <string>
14: 
15: class EncoderPlus : public Encoder
16: {
17: public:
18: 	using Encoder::Encoder;
19: 
20: 	unsigned encode(const std::string& filename, std::string output_prefix);
21: 	unsigned encode_fountain(const std::string& filename, std::string output_prefix, int compression_level=16, double redundancy=1.2);
22: 	unsigned encode_fountain(const std::string& filename, const std::function<bool(const cv::Mat&, unsigned)>& on_frame, int compression_level=16, double redundancy=4.0);
23: };
24: 
25: inline unsigned EncoderPlus::encode(const std::string& filename, std::string output_prefix)
26: {
27: 	std::ifstream f(filename);
28: 
29: 	unsigned i = 0;
30: 	while (true)
31: 	{
32: 		auto frame = encode_next(f);
33: 		if (!frame)
34: 			break;
35: 
36: 		std::string output = fmt::format("{}_{}.png", output_prefix, i);
37: 		// imwrite expects BGR
38: 		cv::cvtColor(*frame, *frame, cv::COLOR_RGB2BGR);
39: 		cv::imwrite(output, *frame);
40: 		++i;
41: 	}
42: 	return i;
43: }
44: 
45: inline unsigned EncoderPlus::encode_fountain(const std::string& filename, const std::function<bool(const cv::Mat&, unsigned)>& on_frame, int compression_level, double redundancy)
46: {
47: 	std::ifstream infile(filename, std::ios::binary);
48: 	fountain_encoder_stream::ptr fes = create_fountain_encoder(infile, File::basename(filename), compression_level);
49: 	if (!fes)
50: 		return 0;
51: 
52: 	// ex: with ecc = 30 and 155 byte blocks, we have 60 rs blocks * 125 bytes per block == 7500 bytes to work with.
53: 	// if fountain_chunks_per_frame() is 10, the fountain_chunk_size will be 750.
54: 	// we calculate requiredFrames based only on symbol bits, to avoid the situation where the color decode is failing while we're
55: 	// refusing to generate additional frames...
56: 	unsigned requiredFrames = fes->blocks_required() * redundancy / cimbar::Config::fountain_chunks_per_frame(_bitsPerSymbol);
57: 	if (requiredFrames == 0)
58: 		requiredFrames = 1;
59: 
60: 	unsigned i = 0;
61: 	unsigned consecutiveScansFailed = 0;
62: 	while (i < requiredFrames)
63: 	{
64: 		auto frame = encode_next(*fes);
65: 		if (!frame)
66: 			break;
67: 
68: 		// some % of generated frames (for the current 8x8 impl)
69: 		// will produce random patterns that falsely match as
70: 		// corner "anchors" and fail to extract. So:
71: 		// if frame fails the scan, skip it.
72: 		if (!Scanner::will_it_scan(*frame))
73: 		{
74: 			if (++consecutiveScansFailed < 5)
75: 				continue;
76: 
77: 			// else, we gotta make forward progress. And it's probably a bug?
78: 			std::cerr << fmt::format("generated {} bad frames in a row. This really shouldn't happen, maybe report a bug. :(", consecutiveScansFailed) << std::endl;
79: 		}
80: 
81: 		consecutiveScansFailed = 0;
82: 		if (!on_frame(*frame, i))
83: 			break;
84: 		++i;
85: 	}
86: 	return i;
87: }
88: 
89: inline unsigned EncoderPlus::encode_fountain(const std::string& filename, std::string output_prefix, int compression_level, double redundancy)
90: {
91: 	std::function<bool(const cv::Mat&, unsigned)> fun = [output_prefix] (const cv::Mat& frame, unsigned i) {
92: 		std::string output = fmt::format("{}_{}.png", output_prefix, i);
93: 		cv::Mat bgr;
94: 		cv::cvtColor(frame, bgr, cv::COLOR_RGB2BGR);
95: 		return cv::imwrite(output, bgr);
96: 	};
97: 	return encode_fountain(filename, fun, compression_level, redundancy);
98: }

(End of file - total 98 lines)
</content>
```

---

## Assistant (Build · DeepSeek V4 Flash · 2.6s)

_Thinking:_

Now let me also look at the GridConf.h to understand the math behind the configuration parameters, and the CimbEncoder.cpp to understand colorization.

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/GridConf.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/GridConf.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include <cmath>
5: 
6: namespace cimbar
7: {
8: 	struct conf
9: 	{
10: 		unsigned color_bits = 0;
11: 		unsigned symbol_bits = 0;
12: 		unsigned ecc_bytes = 0;
13: 		unsigned ecc_block_size = 0;
14: 		unsigned image_size_x = 1;
15: 		unsigned image_size_y = image_size_x;
16: 
17: 		unsigned cell_size = 8;
18: 		unsigned cell_spacing_x = cell_size+1;
19: 		unsigned cell_spacing_y = cell_size+1;
20: 		unsigned cell_offset = 9;
21: 		unsigned cells_per_col_x = 0;
22: 		unsigned cells_per_col_y = cells_per_col_x;
23: 
24: 		int fountain_chunks_scalar = 2;
25: 		bool legacy_mode = false;
26: 
27: 		unsigned bits_per_cell() const
28: 		{
29: 			return color_bits + symbol_bits;
30: 		}
31: 
32: 		unsigned corner_padding_x() const
33: 		{
34: 			return lrint(54.0 / cell_spacing_x);
35: 		}
36: 
37: 		unsigned corner_padding_y() const
38: 		{
39: 			return lrint(54.0 / cell_spacing_y);
40: 		}
41: 
42: 		unsigned total_cells() const
43: 		{
44: 			return cells_per_col_x*cells_per_col_y - (corner_padding_x()*corner_padding_y() * 4);
45: 		}
46: 
47: 		unsigned capacity(unsigned bitspercell=0) const
48: 		{
49: 			if (!bitspercell)
50: 				bitspercell = bits_per_cell();
51: 			return total_cells() * bitspercell / 8;
52: 		}
53: 
54: 		unsigned fountain_chunks_per_frame(unsigned bitspercell=0) const
55: 		{
56: 			if (!bitspercell)
57: 				bitspercell = bits_per_cell();
58: 			if (fountain_chunks_scalar < 0)
59: 				return -fountain_chunks_scalar;
60: 			return bitspercell * fountain_chunks_scalar;
61: 		}
62: 
63: 		unsigned fountain_chunk_size(unsigned bitspercell=0) const
64: 		{
65: 			// TODO: sanity checks?
66: 			// this should neatly split into fountain_chunks_per_frame() [ex: 10] chunks per frame.
67: 			// the other reasonable settings for fountain_chunks_per_frame are `2` and `5`
68: 			if (!bitspercell)
69: 				bitspercell = bits_per_cell();
70: 			return capacity(bitspercell) * (ecc_block_size-ecc_bytes) / ecc_block_size / fountain_chunks_per_frame(bitspercell);
71: 		}
72: 
73: 	};
74: 
75: 	struct Conf5x5 : conf
76: 	{
77: 		Conf5x5()
78: 			: conf()
79: 		{
80: 			color_bits = 2;
81: 			symbol_bits = 2;
82: 			ecc_bytes = 40;
83: 			ecc_block_size = 216;
84: 			image_size_x = 988;
85: 			image_size_y = image_size_x;
86: 
87: 			cell_size = 5;
88: 			cell_spacing_x = cell_size+1;
89: 			cell_spacing_y = cell_size+1;
90: 			cell_offset = 9;
91: 			cells_per_col_x = 162;
92: 			cells_per_col_y = cells_per_col_x;
93: 
94: 			fountain_chunks_scalar = 3;
95: 		}
96: 	};
97: 
98: 	struct Conf5x5d : conf
99: 	{
100: 		Conf5x5d()
101: 			: conf()
102: 		{
103: 			color_bits = 2;
104: 			symbol_bits = 2;
105: 			ecc_bytes = 35;
106: 			ecc_block_size = 182;
107: 			image_size_x = 958;
108: 			image_size_y = image_size_x;
109: 
110: 			cell_size = 5;
111: 			cell_spacing_x = cell_size;
112: 			cell_spacing_y = cell_size+1;
113: 			cell_offset = 9;
114: 			cells_per_col_x = 188;
115: 			cells_per_col_y = 157;
116: 
117: 			fountain_chunks_scalar = 4;
118: 		}
119: 	};
120: 
121: 	struct Conf8x8 : conf
122: 	{
123: 		Conf8x8()
124: 			: conf()
125: 		{
126: 			color_bits = 2;
127: 			symbol_bits = 4;
128: 			ecc_bytes = 30;
129: 			ecc_block_size = 155;
130: 			image_size_x = 1024;
131: 			image_size_y = image_size_x;
132: 
133: 			cell_size = 8;
134: 			cell_spacing_x = cell_size+1;
135: 			cell_spacing_y = cell_size+1;
136: 			cell_offset = 8;
137: 			cells_per_col_x = 112;
138: 			cells_per_col_y = cells_per_col_x;
139: 
140: 			fountain_chunks_scalar = 2;
141: 		}
142: 	};
143: 
144: 	struct Conf8x8_micro : conf
145: 	{
146: 		Conf8x8_micro()
147: 			: conf()
148: 		{
149: 			// 80,69 = 5376, 4032 bytes
150: 			color_bits = 2;
151: 			symbol_bits = 4;
152: 			ecc_bytes = 33;
153: 			ecc_block_size = 168;
154: 			image_size_x = 736;
155: 			image_size_y = 637;
156: 
157: 			cell_size = 8;
158: 			cell_spacing_x = cell_size+1;
159: 			cell_spacing_y = cell_size+1;
160: 			cell_offset = 9;
161: 			cells_per_col_x = 80;
162: 			cells_per_col_y = 69;
163: 
164: 			fountain_chunks_scalar = 1;
165: 		}
166: 	};
167: 
168: 	struct Conf8x8_mini : conf
169: 	{
170: 		Conf8x8_mini()
171: 			: conf()
172: 		{
173: 			color_bits = 2;
174: 			symbol_bits = 4;
175: 			ecc_bytes = 36;
176: 			ecc_block_size = 179;
177: 			image_size_x = 1024;
178: 			image_size_y = 720;
179: 
180: 			cell_size = 8;
181: 			cell_spacing_x = cell_size+1;
182: 			cell_spacing_y = cell_size+1;
183: 			cell_offset = 9;
184: 			cells_per_col_x = 112;
185: 			cells_per_col_y = 78;
186: 
187: 			fountain_chunks_scalar = 2;
188: 		}
189: 	};
190: }

(End of file - total 190 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbEncoder.cpp"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbEncoder.cpp</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #include "CimbEncoder.h"
3: 
4: #include "Common.h"
5: #include "serialize/format.h"
6: #include <cmath>
7: #include <iostream>
8: using cv::Vec3b;
9: using std::string;
10: 
11: CimbEncoder::CimbEncoder(unsigned symbol_bits, unsigned color_bits, bool dark, unsigned color_mode)
12: 	: _numSymbols(1 << symbol_bits)
13: 	, _numColors(1 << color_bits)
14: 	, _dark(dark)
15: 	, _colorMode(color_mode)
16: {
17: 	load_tiles(symbol_bits);
18: }
19: 
20: cv::Mat CimbEncoder::load_tile(unsigned symbol_bits, unsigned index)
21: {
22: 	unsigned symbol = index % _numSymbols;
23: 	unsigned color = index / _numSymbols;
24: 	return cimbar::getTile(symbol_bits, symbol, _dark, _numColors, color, _colorMode);
25: }
26: 
27: // dir will need to be passed via env? Doesn't make sense to compile it in, and doesn't *really* make sense to use cwd
28: bool CimbEncoder::load_tiles(unsigned symbol_bits)
29: {
30: 	unsigned numTiles = _numColors * _numSymbols;
31: 	for (unsigned i = 0; i < numTiles; ++i)
32: 		_tiles.push_back(load_tile(symbol_bits, i));
33: 	return true;
34: }
35: 
36: const cv::Mat& CimbEncoder::encode(unsigned bits) const
37: {
38: 	bits = bits % _tiles.size();
39: 	return _tiles[bits];
40: }

(End of file - total 40 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CellPositions.cpp"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CellPositions.cpp</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #include "CellPositions.h"
3: #include "Interleave.h"
4: 
5: CellPositions::positions_list CellPositions::compute_linear(cimbar::vec_xy spacing, cimbar::vec_xy dimensions, int offset, cimbar::vec_xy marker_size)
6: {
7: 	/*
8: 	 * ex: if dimensions == 128, and marker_size == 8:
9: 		8 tiles at top is 128-16 == 112
10: 		8 tiles at bottom is also 128-16 == 112
11: 
12: 		structure would be:
13: 		112 * 8
14: 		128 * 112
15: 		112 * 8
16: 	*/
17: 	positions_list res;
18: 	int offset_y = offset;
19: 	int marker_offset_x = spacing.x * marker_size.x;
20: 	int top_width = dimensions.x - marker_size.x - marker_size.x;
21: 	int top_cells = top_width * marker_size.y;
22: 	for (int i = 0; i < top_cells; ++i)
23: 	{
24: 		int x = (i % top_width) * spacing.x + marker_offset_x + offset;
25: 		int y = (i / top_width) * spacing.y + offset_y;
26: 		res.push_back({x, y});
27: 	}
28: 
29: 	int mid_y = marker_size.y * spacing.y;
30: 	int mid_width = dimensions.x;
31: 	int mid_height = dimensions.y - marker_size.y - marker_size.y;
32: 	int mid_cells = mid_width * mid_height;
33: 	for (int i = 0; i < mid_cells; ++i)
34: 	{
35: 		int x = (i % mid_width) * spacing.x + offset;
36: 		int y = (i / mid_width) * spacing.y + mid_y + offset_y;
37: 		res.push_back({x, y});
38: 	}
39: 
40: 	int bottom_y = (dimensions.y - marker_size.y) * spacing.y;
41: 	int bottom_width = top_width;
42: 	int bottom_cells = bottom_width * marker_size.y;
43: 	for (int i = 0; i < bottom_cells; ++i)
44: 	{
45: 		int x = (i % bottom_width) * spacing.x + marker_offset_x + offset;
46: 		int y = (i / bottom_width) * spacing.y + bottom_y + offset_y;
47: 		res.push_back({x, y});
48: 	}
49: 	return res;
50: }
51: 
52: CellPositions::positions_list CellPositions::compute(cimbar::vec_xy spacing, cimbar::vec_xy dimensions, int offset, cimbar::vec_xy marker_size, int interleave_blocks, int interleave_partitions)
53: {
54: 	CellPositions::positions_list pos = compute_linear(spacing, dimensions, offset, marker_size);
55: 	if (interleave_blocks)
56: 		return Interleave::interleave(pos, interleave_blocks, interleave_partitions);
57: 	return pos;
58: }
59: 
60: CellPositions::CellPositions(cimbar::vec_xy spacing, cimbar::vec_xy dimensions, int offset, cimbar::vec_xy marker_size, int interleave_blocks, int interleave_partitions)
61: 	: _positions(compute(spacing, dimensions, offset, marker_size, interleave_blocks, interleave_partitions))
62: {
63: 	reset();
64: }
65: 
66: unsigned CellPositions::index() const
67: {
68: 	return _index;
69: }
70: 
71: size_t CellPositions::count() const
72: {
73: 	return _positions.size();
74: }
75: 
76: void CellPositions::reset()
77: {
78: 	_index = 0;
79: }
80: 
81: bool CellPositions::done() const
82: {
83: 	return _index >= _positions.size();
84: }
85: 
86: const CellPositions::coordinate& CellPositions::next()
87: {
88: 	return _positions[_index++];
89: }
90: 
91: const CellPositions::positions_list& CellPositions::positions() const
92: {
93: 	return _positions;
94: }

(End of file - total 94 lines)
</content>
```

---

## Assistant (Build · DeepSeek V4 Flash · 3.3s)

_Thinking:_

Now let me also look at Common.h/Common.cpp to understand tile colorization, and check for more details about the Wirehair library.

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Common.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Common.h</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #pragma once
3: 
4: #include <opencv2/opencv.hpp>
5: #include <string>
6: 
7: namespace cimbar
8: {
9: 	using RGB = std::tuple<uchar,uchar,uchar>;
10: 
11: 	cv::Mat load_img(std::string path);
12: 
13: 	std::tuple<uchar,uchar,uchar> getColor(unsigned index, unsigned num_colors, unsigned color_mode);
14: 	cv::Mat getTile(unsigned symbol_bits, unsigned symbol, bool dark=true, unsigned num_colors=4, unsigned color=0, unsigned color_mode=1);
15: }

(End of file - total 15 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Common.cpp"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Common.cpp</path>
<type>file</type>
<content>
1: /* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
2: #include "Common.h"
3: 
4: #include "Config.h"
5: #include "base91/base.hpp"
6: #include "serialize/format.h"
7: #define STB_IMAGE_IMPLEMENTATION
8: #include "stb/stb_image.h"
9: #include <opencv2/opencv.hpp>
10: 
11: #include <map>
12: #include <string>
13: #include "bitmaps.h"
14: 
15: using cimbar::RGB;
16: using std::array;
17: using std::string;
18: using std::vector;
19: 
20: namespace {
21: 	RGB getColor4(unsigned index)
22: 	{
23: 		// opencv uses BGR, but we don't have to conform to its tyranny
24: 		static constexpr array<RGB, 4> colors = {
25: 			RGB(0, 0xFF, 0),
26: 			RGB(0, 0xFF, 0xFF),
27: 			RGB(0xFF, 0xFF, 0),
28: 			RGB(0xFF, 0, 0xFF),
29: 		};
30: 		return colors[index];
31: 	}
32: 
33: 	RGB getColor4_enc(unsigned index)
34: 	{
35: 		// opencv uses BGR, but we don't have to conform to its tyranny
36: 		static constexpr array<RGB, 4> colors = {
37: 			RGB(0, 0xFF, 0),
38: 			RGB(0, 0xFF, 0xFF),
39: 			RGB(0xFF, 0xFF, 0),
40: 			RGB(0xFF, 0x55, 0xFF),
41: 		};
42: 		return colors[index];
43: 	}
44: 
45: 	RGB getColor4_old(unsigned index)
46: 	{
47: 		static constexpr array<RGB, 4> colors = {
48: 			RGB(0, 0xFF, 0xFF),
49: 			RGB(0xFF, 0xFF, 0),
50: 			RGB(0xFF, 0, 0xFF),
51: 			RGB(0, 0xFF, 0),
52: 		};
53: 		return colors[index];
54: 	}
55: 
56: 	RGB getColor8(unsigned index)
57: 	{
58: 		static constexpr array<RGB, 8> colors = {
59: 			RGB(0, 0xFF, 0xFF), // cyan
60: 			RGB(0xFF, 0xFF, 0), // yellow
61: 			RGB(0x7F, 0x7F, 0xFF),  // mid-blue
62: 			RGB(0xFF, 0xFF, 0xFF), // white
63: 			RGB(0, 0xFF, 0), // green
64: 			RGB(0xFF, 0x9F, 0),  // orange
65: 			RGB(0xFF, 0, 0xFF), // magenta
66: 			RGB(0xFF, 65, 65), // red
67: 		};
68: 		return colors[index];
69: 	}
70: 
71: 	RGB getColor8_old(unsigned index)
72: 	{
73: 		static constexpr array<RGB, 8> colors = {
74: 			RGB(0, 0xFF, 0xFF), // cyan
75: 			RGB(0x7F, 0x7F, 0xFF),  // mid-blue
76: 			RGB(0xFF, 0, 0xFF), // magenta
77: 			RGB(0xFF, 65, 65), // red
78: 			RGB(0xFF, 0x9F, 0),  // orange
79: 			RGB(0xFF, 0xFF, 0), // yellow
80: 			RGB(0xFF, 0xFF, 0xFF),
81: 			RGB(0, 0xFF, 0),
82: 		};
83: 		return colors[index];
84: 	}
85: 
86: 	RGB getBgColor4(unsigned index)
87: 	{
88: 		// opencv uses BGR, but we don't have to conform to its tyranny
89: 		static constexpr array<RGB, 4> colors = {
90: 			RGB(0, 0x20, 0),
91: 			RGB(0, 0, 0xFF),
92: 			RGB(0x7F, 0, 0),
93: 			RGB(0, 0, 0),
94: 		};
95: 		return colors[index];
96: 	}
97: }
98: 
99: namespace cimbar {
100: 
101: cv::Mat load_img(string path)
102: {
103: 	auto it = cimbar::bitmaps.find(path);
104: 	if (it == cimbar::bitmaps.end())
105: 		return cv::Mat();
106: 
107: 	string bytes = base91::decode(it->second);
108: 	vector<unsigned char> data(bytes.data(), bytes.data() + bytes.size());
109: 
110: 	int width, height, channels;
111: 	std::unique_ptr<uint8_t[], void (*)(void*)> imgdata(stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, STBI_rgb_alpha), ::free);
112: 	if (!imgdata)
113: 		return cv::Mat();
114: 
115: 	size_t len = width * height * channels;
116: 	cv::Mat mat(height, width, CV_MAKETYPE(CV_8U, channels));
117: 	std::copy(imgdata.get(), imgdata.get()+len, mat.data);
118: 	cv::cvtColor(mat, mat, cv::COLOR_RGBA2RGB);
119: 	return mat;
120: }
121: 
122: RGB getColor(unsigned index, unsigned num_colors, unsigned color_mode)
123: {
124: 	if ((color_mode & 0xFF) == 0)
125: 	{
126: 		if (num_colors <= 4)
127: 			return getColor4_old(index);
128: 		else
129: 			return getColor8_old(index);
130: 	}
131: 
132: 	if (num_colors > 4)
133: 		return getColor8(index);
134: 	else if (color_mode > 0x100)
135: 		return getColor4_enc(index);
136: 	else
137: 		return getColor4(index);
138: 
139: }
140: 
141: RGB getBgColor(unsigned index, unsigned num_colors, unsigned color_mode)
142: {
143: 	// >0x100 because color_mode=0 will use default bg
144: 	if (color_mode > 0x100 and num_colors <= 4)
145: 		return getBgColor4(index);
146: 	else
147: 		return RGB(0,0,0);
148: }
149: 
150: cv::Mat getTile(unsigned symbol_bits, unsigned symbol, bool dark, unsigned num_colors, unsigned color, unsigned color_mode)
151: {
152: 	static cv::Vec3b background({0xFF, 0xFF, 0xFF});
153: 
154: 	string imgPath = fmt::format("bitmap/{}/{:02x}.png", symbol_bits, symbol);
155: 	cv::Mat tile = load_img(imgPath);
156: 
157: 	uchar r, g, b;
158: 	std::tie(r, g, b) = getColor(color, num_colors, color_mode);
159: 	uchar bgr, bgg, bgb;
160: 	std::tie(bgr, bgg, bgb) = getBgColor(color, num_colors, color_mode);
161: 	cv::MatIterator_<cv::Vec3b> end = tile.end<cv::Vec3b>();
162: 	for (cv::MatIterator_<cv::Vec3b> it = tile.begin<cv::Vec3b>(); it != end; ++it)
163: 	{
164: 		cv::Vec3b& c = *it;
165: 		if (c != background)
166: 			c = {r, g, b};
167: 		else if (dark)
168: 			c = {bgr, bgg, bgb};
169: 	}
170: 	return tile;
171: }
172: 
173: }

(End of file - total 173 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair</path>
<type>directory</type>
<entries>
.gitattributes
.gitignore
CMakeLists.txt
gf256.cpp
gf256.h
include/
LICENSE
python/
README.md
tables/
test/
wirehair.cpp
WirehairCodec.cpp
WirehairCodec.h
WirehairTools.cpp
WirehairTools.h

(16 entries)
</entries>
```

---

## Assistant (Build · DeepSeek V4 Flash · 3.0s)

_Thinking:_

Now I have all the information I need to write a comprehensive document. Let me also check the Wirehair README for codec details.

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/README.md"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/README.md</path>
<type>file</type>
<content>
1: # Wirehair
2: ## Fast and Portable Fountain Codes in C
3: 
4: Wirehair produces a stream of error correction blocks from a data source
5: using an erasure code.  When enough of these blocks are received,
6: the original data can be recovered.
7: 
8: As compared to other similar libraries, an unlimited number of error
9: correction blocks can be produced, and much larger block counts are supported.
10: Furthermore, it gets slower as O(N) in the amount of input data rather
11: than O(N Log N) like the Leopard block code or O(N^2) like the Fecal fountain code,
12: so it is well-suited for large data.
13: 
14: This is not an ideal MDS code, so sometimes it will fail to recover N
15: original data packets from N symbol packets.  It may take N + 1 or N + 2 or more.
16: On average it takes about N + 0.02 packets to recover.  Overall the overhead
17: from the code inefficiency is low, compared to LDPC and many other fountain codes.
18: 
19: A simple C API is provided to make it easy to incorporate into existing
20: projects.  No external dependencies are required.
21: 
22: 
23: ##### Building: Quick Setup
24: 
25: The source code in this folder (gf256 and wirehair code) can be incorporated
26: into your project without any other external dependencies.
27: 
28: To build the software in this repo:
29: 
30: On Windows, make sure CMake and Git Bash are installed.  Open up git bash and then:
31: 
32: ~~~
33: git clone git@github.com:catid/wirehair.git
34: cd wirehair
35: mkdir build
36: cd build
37: cmake .. -G "Visual Studio 16 2019"
38: ls
39: explorer .
40: ~~~
41: 
42: Then you can use Visual Studio Community Edition to open up the `wirehair.sln` file and build the software.
43: 
44: 
45: #### Example Usage
46: 
47: Here's an example program using Wirehair.  It's included in the UnitTest project and demonstrates both the sender and receiver, which are normally separate programs.  For example the data sender might be a file server and the data receiver might be downloading a file from the sender.
48: 
49: ~~~
50: #include <wirehair/wirehair.h>
51: 
52: static bool ReadmeExample()
53: {
54:     // Size of packets to produce
55:     static const int kPacketSize = 1400;
56: 
57:     // Note: Does not need to be an even multiple of packet size or 16 etc
58:     static const int kMessageBytes = 1000 * 1000 + 333;
59: 
60:     vector<uint8_t> message(kMessageBytes);
61: 
62:     // Fill message contents
63:     memset(&message[0], 1, message.size());
64: 
65:     // Create encoder
66:     WirehairCodec encoder = wirehair_encoder_create(nullptr, &message[0], kMessageBytes, kPacketSize);
67:     if (!encoder)
68:     {
69:         cout << "!!! Failed to create encoder" << endl;
70:         return false;
71:     }
72: 
73:     // Create decoder
74:     WirehairCodec decoder = wirehair_decoder_create(nullptr, kMessageBytes, kPacketSize);
75:     if (!decoder)
76:     {
77:         // Free memory for encoder
78:         wirehair_free(encoder);
79: 
80:         cout << "!!! Failed to create decoder" << endl;
81:         return false;
82:     }
83: 
84:     unsigned blockId = 0, needed = 0;
85: 
86:     for (;;)
87:     {
88:         // Select which block to encode.
89:         // Note: First N blocks are the original data, so it's possible to start
90:         // sending data while wirehair_encoder_create() is getting started.
91:         blockId++;
92: 
93:         // Simulate 10% packetloss
94:         if (blockId % 10 == 0) {
95:             continue;
96:         }
97: 
98:         // Keep track of how many pieces were needed
99:         ++needed;
100: 
101:         vector<uint8_t> block(kPacketSize);
102: 
103:         // Encode a packet
104:         uint32_t writeLen = 0;
105:         WirehairResult encodeResult = wirehair_encode(
106:             encoder, // Encoder object
107:             blockId, // ID of block to generate
108:             &block[0], // Output buffer
109:             kPacketSize, // Output buffer size
110:             &writeLen); // Returned block length
111: 
112:         if (encodeResult != Wirehair_Success)
113:         {
114:             cout << "wirehair_encode failed: " << encodeResult << endl;
115:             return false;
116:         }
117: 
118:         // Attempt decode
119:         WirehairResult decodeResult = wirehair_decode(
120:             decoder, // Decoder object
121:             blockId, // ID of block that was encoded
122:             &block[0], // Input block
123:             writeLen); // Block length
124: 
125:         // If decoder returns success:
126:         if (decodeResult == Wirehair_Success) {
127:             // Decoder has enough data to recover now
128:             break;
129:         }
130: 
131:         if (decodeResult != Wirehair_NeedMore)
132:         {
133:             cout << "wirehair_decode failed: " << decodeResult << endl;
134:             return false;
135:         }
136:     }
137: 
138:     vector<uint8_t> decoded(kMessageBytes);
139: 
140:     // Recover original data on decoder side
141:     WirehairResult decodeResult = wirehair_recover(
142:         decoder,
143:         &decoded[0],
144:         kMessageBytes);
145: 
146:     if (decodeResult != Wirehair_Success)
147:     {
148:         cout << "wirehair_recover failed: " << decodeResult << endl;
149:         return false;
150:     }
151: 
152:     // Free memory for encoder and decoder
153:     wirehair_free(encoder);
154:     wirehair_free(decoder);
155: 
156:     return true;
157: }
158: 
159: int main()
160: {
161:     const WirehairResult initResult = wirehair_init();
162: 
163:     if (initResult != Wirehair_Success)
164:     {
165:         SIAMESE_DEBUG_BREAK();
166:         cout << "!!! Wirehair initialization failed: " << initResult << endl;
167:         return -1;
168:     }
169: 
170:     if (!ReadmeExample())
171:     {
172:         SIAMESE_DEBUG_BREAK();
173:         cout << "!!! Example usage failed" << endl;
174:         return -2;
175:     }
176: ...
177: ~~~
178: 
179: 
180: #### Benchmarks
181: 
182: Some quick comments:
183: 
184: Benchmarks on my PC do not mean a whole lot.  Right now it's clocked at 3 GHz and has Turbo Boost on, etc.
185: To run the test yourself just build and run the UnitTest project in Release mode.
186: 
187: For small values of N < 128 or so this is a pretty inefficient codec compared to the Fecal codec.  Fecal is also a fountain code but is limited to repairing a small number of failures or small input block count.
188: 
189: ~~~
190: For N = 2 packets of 1300 bytes:
191: + Average wirehair_encoder_create() time: 11 usec (236.364 MBPS)
192: + Average wirehair_encode() time: 0 usec (7435.7 MBPS)
193: + Average wirehair_decode() time: 2 usec (476.205 MBPS)
194: + Average overhead piece count beyond N = 0.0105
195: + Average wirehair_recover() time: 0 usec (9319 MBPS)
196: 
197: For N = 4 packets of 1300 bytes:
198: + Average wirehair_encoder_create() time: 8 usec (650 MBPS)
199: + Average wirehair_encode() time: 0 usec (8353.43 MBPS)
200: + Average wirehair_decode() time: 1 usec (695.102 MBPS)
201: + Average overhead piece count beyond N = 0.0225
202: + Average wirehair_recover() time: 0 usec (11219 MBPS)
203: 
204: For N = 8 packets of 1300 bytes:
205: + Average wirehair_encoder_create() time: 13 usec (800 MBPS)
206: + Average wirehair_encode() time: 0 usec (7916.2 MBPS)
207: + Average wirehair_decode() time: 1 usec (704.359 MBPS)
208: + Average overhead piece count beyond N = 0.0045
209: + Average wirehair_recover() time: 1 usec (8973.25 MBPS)
210: 
211: For N = 16 packets of 1300 bytes:
212: + Average wirehair_encoder_create() time: 27 usec (770.37 MBPS)
213: + Average wirehair_encode() time: 0 usec (7993.4 MBPS)
214: + Average wirehair_decode() time: 1 usec (707.211 MBPS)
215: + Average overhead piece count beyond N = 0.036
216: + Average wirehair_recover() time: 2 usec (9116.81 MBPS)
217: 
218: For N = 32 packets of 1300 bytes:
219: + Average wirehair_encoder_create() time: 41 usec (1014.63 MBPS)
220: + Average wirehair_encode() time: 0 usec (7062.93 MBPS)
221: + Average wirehair_decode() time: 1 usec (908.097 MBPS)
222: + Average overhead piece count beyond N = 0.0195
223: + Average wirehair_recover() time: 5 usec (8057.33 MBPS)
224: 
225: For N = 64 packets of 1300 bytes:
226: + Average wirehair_encoder_create() time: 81 usec (1027.16 MBPS)
227: + Average wirehair_encode() time: 0 usec (7159.51 MBPS)
228: + Average wirehair_decode() time: 1 usec (1033.95 MBPS)
229: + Average overhead piece count beyond N = 0.017
230: + Average wirehair_recover() time: 10 usec (7640.74 MBPS)
231: 
232: For N = 128 packets of 1300 bytes:
233: + Average wirehair_encoder_create() time: 192 usec (866.667 MBPS)
234: + Average wirehair_encode() time: 0 usec (5662.07 MBPS)
235: + Average wirehair_decode() time: 1 usec (870.14 MBPS)
236: + Average overhead piece count beyond N = 0.015
237: + Average wirehair_recover() time: 25 usec (6419.38 MBPS)
238: 
239: For N = 256 packets of 1300 bytes:
240: + Average wirehair_encoder_create() time: 319 usec (1043.26 MBPS)
241: + Average wirehair_encode() time: 0 usec (6333.2 MBPS)
242: + Average wirehair_decode() time: 1 usec (1018.77 MBPS)
243: + Average overhead piece count beyond N = 0.022
244: + Average wirehair_recover() time: 50 usec (6602.26 MBPS)
245: 
246: For N = 512 packets of 1300 bytes:
247: + Average wirehair_encoder_create() time: 670 usec (993.433 MBPS)
248: + Average wirehair_encode() time: 0 usec (6483.91 MBPS)
249: + Average wirehair_decode() time: 1 usec (1028.85 MBPS)
250: + Average overhead piece count beyond N = 0.022
251: + Average wirehair_recover() time: 100 usec (6600.1 MBPS)
252: 
253: For N = 1024 packets of 1300 bytes:
254: + Average wirehair_encoder_create() time: 1697 usec (784.443 MBPS)
255: + Average wirehair_encode() time: 0 usec (5309.05 MBPS)
256: + Average wirehair_decode() time: 1 usec (671.005 MBPS)
257: + Average overhead piece count beyond N = 0.022
258: + Average wirehair_recover() time: 207 usec (6404.05 MBPS)
259: 
260: For N = 2048 packets of 1300 bytes:
261: + Average wirehair_encoder_create() time: 3227 usec (825.039 MBPS)
262: + Average wirehair_encode() time: 0 usec (5202.3 MBPS)
263: + Average wirehair_decode() time: 1 usec (683.141 MBPS)
264: + Average overhead piece count beyond N = 0.021
265: + Average wirehair_recover() time: 441 usec (6026.08 MBPS)
266: 
267: For N = 4096 packets of 1300 bytes:
268: + Average wirehair_encoder_create() time: 7614 usec (699.343 MBPS)
269: + Average wirehair_encode() time: 0 usec (4334.08 MBPS)
270: + Average wirehair_decode() time: 2 usec (577.674 MBPS)
271: + Average overhead piece count beyond N = 0.0215
272: + Average wirehair_recover() time: 1208 usec (4405.65 MBPS)
273: 
274: For N = 8192 packets of 1300 bytes:
275: + Average wirehair_encoder_create() time: 17208 usec (618.875 MBPS)
276: + Average wirehair_encode() time: 0 usec (3277.17 MBPS)
277: + Average wirehair_decode() time: 2 usec (521.665 MBPS)
278: + Average overhead piece count beyond N = 0.075
279: + Average wirehair_recover() time: 2916 usec (3651.35 MBPS)
280: 
281: For N = 16384 packets of 1300 bytes:
282: + Average wirehair_encoder_create() time: 42512 usec (501.016 MBPS)
283: + Average wirehair_encode() time: 0 usec (2646.89 MBPS)
284: + Average wirehair_decode() time: 2 usec (435.173 MBPS)
285: + Average overhead piece count beyond N = 0.015
286: + Average wirehair_recover() time: 7282 usec (2924.63 MBPS)
287: 
288: For N = 32768 packets of 1300 bytes:
289: + Average wirehair_encoder_create() time: 111287 usec (382.78 MBPS)
290: + Average wirehair_encode() time: 0 usec (2378.29 MBPS)
291: + Average wirehair_decode() time: 3 usec (342.556 MBPS)
292: + Average overhead piece count beyond N = 0.0195
293: + Average wirehair_recover() time: 16326 usec (2609.23 MBPS)
294: ~~~
295: 
296: 
297: #### Credits
298: 
299: Software by Christopher A. Taylor <mrcatid@gmail.com>
300: 
301: Please reach out if you need support or would like to collaborate on a project.

(End of file - total 301 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/include/wirehair/wirehair.h"
}
```

**Output:**
```
<path>/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/include/wirehair/wirehair.h</path>
<type>file</type>
<content>
1: /*
2:     Copyright (c) 2012-2018 Christopher A. Taylor.  All rights reserved.
3: 
4:     Redistribution and use in source and binary forms, with or without
5:     modification, are permitted provided that the following conditions are met:
6: 
7:     * Redistributions of source code must retain the above copyright notice,
8:       this list of conditions and the following disclaimer.
9:     * Redistributions in binary form must reproduce the above copyright notice,
10:       this list of conditions and the following disclaimer in the documentation
11:       and/or other materials provided with the distribution.
12:     * Neither the name of Wirehair nor the names of its contributors may be
13:       used to endorse or promote products derived from this software without
14:       specific prior written permission.
15: 
16:     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
17:     AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
18:     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
19:     ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
20:     LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
21:     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
22:     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
23:     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
24:     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
25:     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
26:     POSSIBILITY OF SUCH DAMAGE.
27: */
28: 
29: #ifndef WIREHAIR_H
30: #define WIREHAIR_H
31: 
32: /** \mainpage
33:     Wirehair : Fountain Codes in C++
34: 
35:     Wirehair produces a stream of error correction blocks from a data source
36:     using an erasure code.  When enough of these blocks are received,
37:     the original data can be recovered.  It supports up to 64000 blocks.
38: 
39:     This codec provides full-message reconstruction using the
40:     wirehair_recover() function, and can also
41:     reconstruct single missing blocks using the
42:     wirehair_recover_block() function.
43:     It also accelerates retransmission in a store-and-forward system
44:     with the wirehair_decoder_becomes_encoder() function.
45: */
46: 
47: #define WIREHAIR_VERSION 2
48: 
49: // Tweak if the functions are exported or statically linked
50: //#define WIREHAIR_DLL /* Defined when building/linking as DLL */
51: //#define WIREHAIR_BUILDING /* Defined by the library makefile */
52: 
53: #if defined(WIREHAIR_BUILDING)
54: # if defined(WIREHAIR_DLL) && defined(_WIN32)
55:     #define WIREHAIR_EXPORT __declspec(dllexport)
56: # else
57:     #define WIREHAIR_EXPORT
58: # endif
59: #else
60: # if defined(WIREHAIR_DLL) && defined(_WIN32)
61:     #define WIREHAIR_EXPORT __declspec(dllimport)
62: # else
63:     #define WIREHAIR_EXPORT extern
64: # endif
65: #endif
66: 
67: #include <stdint.h>
68: 
69: 
70: #ifdef __cplusplus
71: extern "C" {
72: #endif
73: 
74: 
75: //------------------------------------------------------------------------------
76: // Shared Constants/Datatypes
77: 
78: /// These are the result codes that can be returned from the API functions
79: typedef enum WirehairResult_t
80: {
81:     /// Success code
82:     Wirehair_Success             = 0,
83: 
84:     /// More data is needed to decode.  This is normal and does not indicate a failure
85:     Wirehair_NeedMore            = 1,
86: 
87:     // Other values are failure codes:
88: 
89:     /// A function parameter was invalid
90:     Wirehair_InvalidInput        = 2,
91: 
92:     /// Encoder needs a better dense seed
93:     Wirehair_BadDenseSeed        = 3,
94: 
95:     /// Encoder needs a better peel seed
96:     Wirehair_BadPeelSeed         = 4,
97: 
98:     /// N = ceil(messageBytes / blockBytes) is too small.
99:     /// Try reducing block_size or use a larger message
100:     Wirehair_BadInput_SmallN     = 5,
101: 
102:     /// N = ceil(messageBytes / blockBytes) is too large.
103:     /// Try increasing block_size or use a smaller message
104:     Wirehair_BadInput_LargeN     = 6,
105: 
106:     /// Not enough extra rows to solve it, must give up
107:     Wirehair_ExtraInsufficient   = 7,
108: 
109:     /// An error occurred during the request
110:     Wirehair_Error               = 8,
111: 
112:     /// Out of memory
113:     Wirehair_OOM                 = 9,
114: 
115:     /// Platform is not supported yet
116:     Wirehair_UnsupportedPlatform = 10,
117: 
118:     WirehairResult_Count, /* for asserts */
119:     WirehairResult_Padding = 0x7fffffff /* int32_t padding */
120: } WirehairResult;
121: 
122: /// Get WirehairResult string function
123: WIREHAIR_EXPORT const char *wirehair_result_string(
124:     WirehairResult result ///< Result code to convert to string
125: );
126: 
127: 
128: //------------------------------------------------------------------------------
129: // Wirehair API
130: 
131: /**
132:     wirehair_init()
133: 
134:     Verify binary compatibility with the API on startup.
135: 
136:     Example:
137:         if (wirehair_init()) {
138:             exit(1);
139:         }
140: 
141:     Returns Wirehair_Success on success.
142:     Returns other codes on error.
143: */
144: WIREHAIR_EXPORT WirehairResult wirehair_init_(int expected_version);
145: #define wirehair_init() wirehair_init_(WIREHAIR_VERSION)
146: 
147: /// WirehairCodec: From wirehair_encoder_create() or wirehair_decoder_create()
148: typedef struct WirehairCodec_t { char impl; }* WirehairCodec;
149: 
150: 
151: /**
152:     wirehair_encoder_create()
153: 
154:     Encode the given message into blocks of size blockBytes.
155: 
156:     The number of blocks in the message:
157:         N = (bytes + (blockBytes-1)) / blockBytes
158: 
159:     If N is too high or too low this function will fail.  In particular if N = 1, then
160:     using this type of error correction does not make sense: Sending the same message
161:     over and over is just as good.  And if N > 64000 then some internal variables will
162:     start to overflow, so too many blocks is unsupported.  The most efficient values
163:     for N are around 1000.
164: 
165:     Preconditions:
166:         N >= 2
167:         N <= 64000
168: 
169:     Pass 0 for reuseOpt if you do not want to reuse a WirehairCodec object.
170: 
171:     Returns a non-zero object pointer on success.
172:     Returns nullptr(0) on failure.
173: */
174: WIREHAIR_EXPORT WirehairCodec wirehair_encoder_create(
175:     WirehairCodec reuseOpt, ///< [Optional] Pointer to prior codec object
176:     const void*    message, ///< Pointer to message
177:     uint64_t  messageBytes, ///< Bytes in the message
178:     uint32_t    blockBytes  ///< Bytes in an output block
179: );
180: 
181: /**
182:     wirehair_encode()
183: 
184:     Write an error correction block.
185: 
186:     The first `blockId` < N blocks are the same as the input data.
187:     This "systematic" property can be used to run the encoder
188:     in parallel with sending original data.
189:     The `blockId` >= N blocks are generated on demand.
190: 
191:     Preconditions:
192:        Block is at least `blockBytes` in size
193: 
194:     Returns Wirehair_Success on success.
195:     Returns other codes on error.
196: */
197: WIREHAIR_EXPORT WirehairResult wirehair_encode(
198:     WirehairCodec    codec, ///< Pointer to codec from wirehair_encoder_init()
199:     unsigned       blockId, ///< Identifier of block to generate
200:     void*     blockDataOut, ///< Pointer to output block data
201:     uint32_t      outBytes, ///< Bytes in the output buffer
202:     uint32_t* dataBytesOut  ///< Number of bytes written <= blockBytes
203: );
204: 
205: /**
206:     wirehair_decoder_create()
207: 
208:     Initialize a decoder for a message of size bytes with blockBytes bytes
209:     per received block.  These parameters should match the ones passed to
210:     the corresponding wirehair_encoder_create() call.
211: 
212:     Pass 0 for reuseOpt if you do not want to reuse a WirehairCodec object.
213: 
214:     Returns a non-zero object pointer on success.
215:     Returns nullptr(0) on failure.
216: */
217: WIREHAIR_EXPORT WirehairCodec wirehair_decoder_create(
218:     WirehairCodec reuseOpt, ///< Codec object to reuse
219:     uint64_t  messageBytes, ///< Bytes in the message to decode
220:     uint32_t    blockBytes  ///< Bytes in each encoded block
221: );
222: 
223: /**
224:     wirehair_decode()
225: 
226:     Provide the decoder with a block from the wirehair_encode() function.
227: 
228:     Preconditions:
229:     The application must not provide duplicate data for the same packet.
230:     In other words, blockId values cannot be used twice.
231: 
232:     Returns Wirehair_Success if data recovery is complete.
233:     + Use wirehair_recover() or wirehair_recover_block()
234:       to reconstruct the recovered data.
235:     Returns Wirehair_NeedsMoreData if more data is needed to decode.
236:     Returns other codes on error.
237: */
238: WIREHAIR_EXPORT WirehairResult wirehair_decode(
239:     WirehairCodec   codec, ///< Codec object
240:     unsigned      blockId, ///< ID number of received block
241:     const void* blockData, ///< Pointer to block data
242:     uint32_t    dataBytes  ///< Number of bytes in the data block
243: );
244: 
245: /**
246:     wirehair_recover()
247: 
248:     Reconstruct the message after reading is complete.
249: 
250:     Preconditions:
251:     Message contains enough space to store the entire decoded message (bytes)
252: 
253:     Returns Wirehair_Success if the message was recovered.
254:     Returns other codes on error.
255: */
256: WIREHAIR_EXPORT WirehairResult wirehair_recover(
257:     WirehairCodec    codec, ///< Codec object
258:     void*       messageOut, ///< Buffer where reconstructed message will be written
259:     uint64_t  messageBytes  ///< Bytes in the message
260: );
261: 
262: /**
263:     wirehair_recover_block()
264: 
265:     Reconstruct a single block of the message after reading is complete.
266:     It is much slower than wirehair_recover() for original data, so it
267:     is better to avoid calling this function for blockId < N.
268: 
269:     Preconditions:
270:     Block ptr buffer contains enough space to hold the block (blockBytes)
271: 
272:     May return non-zero to indicate a failure.
273: 
274:     Returns Wirehair_Success if the block was recovered.
275:     Returns other codes on error.
276: */
277: WIREHAIR_EXPORT WirehairResult wirehair_recover_block(
278:     WirehairCodec codec, ///< Codec object
279:     unsigned    blockId, ///< ID of the block to reconstruct between 0..N-1
280:     void*     blockData, ///< Pointer to block data
281:     uint32_t*  bytesOut  ///< Set to the number of data bytes in the block
282: );
283: 
284: /**
285:     wirehair_decoder_becomes_encoder()
286: 
287:     Convert a decoder WirehairCodec into an encoder WirehairCodec
288:     after recovery is complete.  This enables the application to
289:     receive a message and then retransmit it without reinitializing
290:     the encoder from scratch.
291: 
292:     Note that after converting the codec to an encoder, calling the
293:     wirehair_encode() function to retrieve original data will be
294:     much slower than accessing the original data directly on the
295:     application side.
296: 
297:     Preconditions:
298:     wirehair_decoder_read() returned Wirehair_Success
299: 
300:     Returns Wirehair_Success if the operation was successful.
301:     Returns other codes on error.
302: */
303: WIREHAIR_EXPORT WirehairResult wirehair_decoder_becomes_encoder(
304:     WirehairCodec codec ///< Codec to change
305: );
306: 
307: /**
308:     wirehair_free()
309: 
310:     Free memory associated with a WirehairCodec object.
311: */
312: WIREHAIR_EXPORT void wirehair_free(
313:     WirehairCodec codec ///< Codec object to free
314: );
315: 
316: 
317: #ifdef __cplusplus
318: }
319: #endif
320: 
321: #endif // WIREHAIR_H

(End of file - total 321 lines)
</content>
```

---

## Assistant (Build · DeepSeek V4 Flash · 43.5s)

_Thinking:_

Now I have a comprehensive understanding of the codebase. Let me write the document. I'll structure it in Chinese with technical details including specific numbers, equations, and code references.

**Tool: write**

**Input:**
```json
{
  "filePath": "/home/ringo/repositories/azote/libcimbar/libcimbar原理.md",
  "content": "# libcimbar 编码原理详解\n\nlibcimbar 是一个将任意二进制文件编码为彩色二维码风格图像（或实时视频）的工具，由 `cimbar.org` 项目开发。核心思想是**利用颜色通道增加 QR 码的信息密度**，配合**喷泉码（Fountain Code）** 和 **Reed-Solomon 纠错码**实现高容错的数传输。\n\n---\n\n## 整体流水线\n\n```\n原始文件（任意二进制，最大约 33 MB）\n    │\n    ▼\n[1. Zstandard 压缩]         —  zstd level 16，内嵌文件名头，不足则填充\n    │\n    ▼\n[2. Wirehair 喷泉编码器]     —  将压缩数据分割为 fountain 块，生成无限多的纠删块\n    │\n    ▼\n[3. Reed-Solomon ECC]       —  RS(155,125)，每 155 字节加 30 字节校验\n    │\n    ▼\n[4. 比特拆分器]              —  分两遍交织：先写 symbol 位，后写 color 位\n    │\n    ▼\n[5. CimbWriter / 像素贴图]   —  按交织顺序将 6-bit 值映射为彩色瓷砖，粘贴到画布\n    │\n    ▼\n[6. 输出]                   —  PNG 帧序列 / GLFW 实时窗口 / WebGL 画布\n```\n\n---\n\n## 1. Zstandard 压缩（`src/lib/compression/zstd_compressor.h`）\n\n- 使用 **Zstandard** 压缩，默认 level 16。\n- 压缩前，在数据流最前方插入一个 **skippable zstd frame** 作为文件头，内容为 `0x01 + 文件名`（`write_header()`）。\n- 如果压缩后数据小于 fountain 块大小，则填充 **skippable null frame** 至所需大小 (`pad()`)。\n- 可通过 `compression_level=0` 关闭压缩。\n\n**关键函数：**\n```cpp\nbool compress(INSTREAM& raw, int compression_level=0);  // 逐块压缩\nsize_t write_header(const char* data, unsigned len);     // 写文件名头\nsize_t pad(unsigned len);                                 // 填充到最小长度\n```\n\n---\n\n## 2. Wirehair 喷泉码（`src/third_party_lib/wirehair/`）\n\n### 什么是喷泉码\n喷泉码是一种**无码率纠删码（rateless erasure code）**。发送端可以从原始数据中生成**无限多个**编码块，接收端只要收到**略多于原始数据量**的任意编码块（不要求顺序），就能恢复全部原始数据。\n\n### Wirehair 的特性\n- 由 Christopher A. Taylor 开发的开源喷泉码库。\n- **时间复杂度 O(N)**（而非 O(N log N) 或 O(N²)），适合大数据量。\n- 支持块数 N 范围为 **2 ~ 64000**。\n- 不是理想 MDS 码，平均需要 **N + 0.02** 个块即可恢复全部数据。\n- **前 N 个块（block_id < N）就是原始数据本身**（systematic 特性），允许发送端在编解码器初始化同时直接发送原始数据。\n\n### 在 libcimbar 中的应用\n\n**创建编码器**（`src/lib/fountain/FountainEncoder.h`）：\n```cpp\n_codec = wirehair_encoder_create(nullptr, data, length, packet_size);\n```\n- `data`：zstd 压缩后的完整数据\n- `length`：数据长度\n- `packet_size`：每个 fountain 块大小（不含 6 字节头）\n\n**生成新块**（`src/lib/fountain/fountain_encoder_stream.h:encode_new_block()`）：\n```cpp\nsize_t res = _encoder.encode(_block++, data, block_size());\nFountainMetadata::to_uint8_arr(_encodeId, _data.size(), block, _buffer.data());\n```\n- 每调用一次生成一个新的编码块\n- block_id 从 0 开始递增，无上限\n\n**Fountain 块头格式**（`FountainMetadata.h`，共 6 字节）：\n```\nByte 0: [7-bit encode_id | 1-bit 文件大小高位 (bit 17)]\nByte 1-3: [24-bit 文件总大小]\nByte 4-5: [16-bit block_id]\n```\n\n### Fountain 块和帧的关系\n\n在 **Conf8x8（默认模式）**中：\n- `fountain_chunks_per_frame = 12`（6-bit 模式下：2 × bits_per_cell）\n- `fountain_chunk_size = 750 字节`\n- 每帧包含 12 个 fountain 块，每个块前 6 字节为头，后 744 字节为有效载荷\n- 每帧 fountain 有效数据 = 744 × 12 = **8928 字节**\n\n`fountain_chunk_size` 的计算公式（`GridConf.h:63-71`）：\n```\nchunk_size = capacity(bits_per_cell) × (ecc_block_size - ecc_bytes) / ecc_block_size / fountain_chunks_per_frame\n```\n代入默认值：\n```\nchunk_size = 9300 × (155 - 30) / 155 / 12 = 9300 × 125 / 155 / 12 = 7500 / 12 = 750 × 125/155 = 750\n```\n\n---\n\n## 3. Reed-Solomon ECC（`src/lib/encoder/ReedSolomon.h`）\n\n- 使用 `libcorrect` 库的 RS 编码器。\n- 本原多项式：`correct_rs_primitive_polynomial_8_7_2_1_0`（即 x⁸ + x⁷ + x² + x + 1）。\n- 交错深度（interleaving）：1，根间距：1。\n- **默认配置（Conf8x8）**：RS(155, 125)，即每 155 字节中包含 30 字节校验。\n\n### Reed-Solomon 流封装（`reed_solomon_stream.h`）\n\n编码时，底层流先被读到 buffer 中（长度 = `ecc_block_size - parity` = 125 字节），然后 RS 编码器将其扩展为 155 字节输出：\n```cpp\n_stream.read(data, length - _rs.parity());  // 读 125 字节原始数据\n_rs.encode(data, bytes, data);              // 编码为 155 字节\n```\n\n解码时，读取 155 字节并进行 RS 纠错，输出 125 字节的修复数据：\n```cpp\nssize_t bytes = _rs.decode(data, _buffer.size(), _buffer.data());\n```\n\n### 后 RS 的帧容量\n\n- 每帧原始（裸数据）容量：`112 × 112 - 4 × 6 × 6 = 12400` 个 cells\n- 每个 cell 6 bits → 12400 × 6 / 8 = **9300 字节/帧**\n- RS 效率：125/155 ≈ 0.806\n- 有效数据/帧：9300 × 125/155 = **7500 字节**\n\n---\n\n## 4. 符号映射与颜色编码（`CimbEncoder`）\n\n每帧有 **12400** 个 data cells，每个 cell 编码 6 bits：\n- **低 4 bits**：选择 16 种瓷砖图案之一（symbol）\n- **高 2 bits**：选择 4 种颜色之一（color）\n\n### 瓷砖图案\n\n16 种 8×8 像素的双色 PNG 瓷砖内嵌在代码中（`bitmaps.h`），以 base91 编码的字符串形式存储。加载时：\n1. base91 解码\n2. STB image 解析为 RGBA 像素矩阵\n3. 转换为 RGB 矩阵\n\n### 颜色映射（`Common.cpp`）\n\n默认 4 色调色板（mode B, color_mode=1）：\n\n| Index | 颜色 | RGB |\n|-------|------|-----|\n| 0 | 绿 | `(0, 255, 0)` |\n| 1 | 青 | `(0, 255, 255)` |\n| 2 | 黄 | `(255, 255, 0)` |\n| 3 | 品红 | `(255, 0, 255)` |\n\n此外还有：\n- 旧版调色板（color_mode=0）：索引顺序不同\n- 增强调色板（color_mode>0x100）：品红改为 `(255, 0x55, 255)`，并配有对应底色\n\n### 瓷砖着色过程（`Common.cpp:getTile()`）\n\n```cpp\n// 白色像素视为背景，其余视为前景\nif (c != background)   // background = (255, 255, 255)\n    c = {r, g, b};     // 替换为所选颜色\nelse\n    c = {bgr, bgg, bgb}; // 替换为底色（通常黑色或暗色变体）\n```\n\n### 编码模式：解耦模式 vs 耦合模式\n\n**解耦模式（现代默认，`_coupled=false`）**：\n\n解码时符号位和颜色位分开处理——先写所有 symbol bits，再写所有 color bits。这样颜色通道出错不影响符号通道的解码。\n\n编码流程（`Encoder.h:encode_next()`）：\n1. **第一遍（symbol pass）**：从 RS 流中读取 `_bitsPerSymbol`（4）位，以步长 `bits_per_op`（6）写入 bitbuffer\n2. **第二遍（color pass）**：填充 color bits 到步长间隙中\n\n**耦合模式（legacy）**：\n符号位和颜色位交织在一起，同时从 RS 流读取 6 位直接写入。好处是某个通道的纠错能力可以\"帮助\"另一个通道，缺点是\"一损俱损\"。\n\n---\n\n## 5. 细胞位置与交织（`CellPositions.cpp`, `Interleave.h`）\n\n### 网格布局\n\n- 画布大小：**1024 × 1024 像素**（Conf8x8）\n- 瓷砖大小：**8×8 像素**\n- 间隔：**9 像素**（8+1）\n- 网格：**112 × 112** = 12544 个格子\n- 四角各去掉 **6×6 = 36** 个格子（用于 anchor 标记）\n- 实际 data cells：**12400**\n\n### 位置计算（`CellPositions::compute_linear()`）\n\n位置分三个区域计算：顶部（anchor 下方的带状区）、中部（主体）、底部（anchor 上方的带状区）。这是为了避开四角的 anchor 标记。\n\n### 交织（`Interleave.h:interleave_indices()`）\n\n为了防止局部图像损坏导致连续数据丢失，cell 位置被重新排列为交织顺序：\n\n```\n参数：interleave_blocks = 155, interleave_partitions = 2\n```\n\n算法：\n1. 将 12400 个位置分为 2 个分区，每分区 6200 个\n2. 每分区内，分为 155 个块（`num_chunks`）\n3. 遍历块 0..154，每个块内跳跃式取 cell: `i = chunk, chunk+155, chunk+310...`\n   即 `for chunk in 0..155: for i in chunk..6200 step 155: indices.push(i)`\n\n这样 RS 块内的连续字节会被散布到图像中的不同区域，提升抗局部损坏能力。\n\n---\n\n## 6. Anchor 标记与引导条\n\n- **四角设 3 个同心的矩形 anchor**，用于定位和透视校正\n- **四边中点设引导条**（guide bar），用于精确定位\n- 图像生成时先绘制黑色（或指定色）背景，然后粘贴 anchor 和 guide bar，最后依次粘贴数据瓷砖\n\n---\n\n## 7. 输出生成\n\n### PNG 帧（`EncoderPlus.h:encode_fountain()`）\n\n```\nframes_required = blocks_required × redundancy / fountain_chunks_per_frame\n```\n- `blocks_required`：fountain 块总数 (= ceil(压缩数据大小 / fountain_chunk_size))\n- `redundancy`：默认 1.2（额外发送 20% 的帧）\n- 每帧生成后检查\"扫描性\"（`Scanner::will_it_scan()`），连续 5 帧无法扫描则报错\n\n### 实时视频（`src/exe/cimbar_send/send.cpp`）\n\n- 使用 GLFW 窗口渲染 1024×1024 图像\n- 帧率控制：默认 15 fps\n- 编码器生成 `blocks_required × 8` 个块后循环复用\n- 支持 \"shakycam\" 效果：亚像素抖动帮助解码器检测帧切换\n- 吞吐量约 **850 kbit/s（~106 KB/s）**\n\n---\n\n## 8. 完整帧容量计算（Conf8x8 默认值）\n\n| 参数 | 计算 | 结果 |\n|------|------|------|\n| 网格 | 112 × 112 | 12544 cells |\n| 四角 anchor 扣除 | 4 × 6 × 6 | −144 cells |\n| 实际 data cells | | 12400 cells |\n| bits/cell | 4(symbol) + 2(color) | 6 bits |\n| 裸容量 | 12400 × 6 / 8 | 9300 bytes |\n| RS(155,125) 后 | 9300 × 125/155 | 7500 bytes |\n| fountain 块/帧 | 2 × 6 | 12 块 |\n| fountain 块大小 | 7500 / 12 | 750 bytes |\n| fountain 有效载荷 | 750 − 6(头) | 744 bytes |\n| 帧有效数据 | 744 × 12 | 8928 bytes |\n| 最大文件大小 | Wirehair 限制 N ≤ 64000 | ~33 MB |\n| 吞吐量（15fps） | 7500 × 15 | ~106 KB/s |\n\n---\n\n## 9. 解码流程（逆向）\n\n解码器工作流程：\n1. 在图像中检测 4 个 anchor → 透视校正\n2. 扫描 grid 中每个 cell 位置，提取瓷砖图案和颜色\n3. 反交织还原数据顺序\n4. RS 解码纠错\n5. 收集 fountain 块直到数量 ≥ N → 用 Wirehair 恢复原始压缩数据\n6. zstd 解压缩 → 还原原始文件和文件名\n\n---\n\n## 参考文件\n\n| 文件 | 功能 |\n|------|------|\n| `GridConf.h` | grid 几何配置（Conf8x8, Conf5x5 等） |\n| `Config.h` | 运行时配置适配器 |\n| `Encoder.h` | 编码主流程（两遍编码、RS 流包装） |\n| `EncoderPlus.h` | 高层接口（fountain 编码、PNG 输出） |\n| `ReedSolomon.h` | RS 编解码器封装 |\n| `reed_solomon_stream.h` | RS 流（自动分块编解码） |\n| `FountainEncoder.h` | Wirehair 喷泉编码器封装 |\n| `fountain_encoder_stream.h` | 喷泉流（自动生成带头的块） |\n| `FountainMetadata.h` | 6 字节喷泉块头格式 |\n| `zstd_compressor.h` | Zstandard 压缩流 |\n| `CimbEncoder.h/.cpp` | 瓷砖和颜色的映射 |\n| `CimbWriter.h/.cpp` | 画布输出（粘贴瓷砖） |\n| `CellPositions.cpp` | cell 位置计算 |\n| `Interleave.h` | 交织/反交织算法 |\n| `Common.cpp` | 瓷砖加载、颜色表 |\n| `bitmaps.h` | 16 种瓷砖图案 PNG（base91 编码） |\n| `wirehair/` | Wirehair 喷泉码库 |\n| `send.cpp` | 实时视频发送 |\n| `cimbar_js.cpp` | WASM API 封装 |\n"
}
```

**Output:**
```
Wrote file successfully.
```

---

## Assistant (Build · DeepSeek V4 Flash · 2.5s)

_Thinking:_

The file has been written successfully. Let me provide a brief summary to the user.

搞定了，文件在 `libcimbar原理.md`。涵盖了从 zstd 压缩 → Wirehair 喷泉码 → RS 纠错 → 颜色/瓷砖映射 → 图像拼接的完整流水线，配了具体的数值和公式。

---

## User

如果用 zig 重写核心的话，需要很大的工程量吗？我想尝试统一一下各个平台，因为我有很多的这种需求，需要电脑（Linux AppImage, MUSL） WebCam/ Android(Target Android 6+ APK) Cam/ iOS(iOS12+ objc/swift) Cam 的统一体验。当然了把 C++ 这个 wrapper 一下差不多也可以。

---

## Assistant (Build · DeepSeek V4 Flash · 14.3s)

_Thinking:_

The user is asking about the effort required to rewrite libcimbar's core in Zig, motivated by wanting a unified codebase that works across Linux (AppImage/MUSL), Android (6+ APK), and iOS (12+ objc/swift) with camera input. They also mention that wrapping C++ might be a reasonable approach.

Let me think about this carefully and give them a practical assessment based on what I've seen of the codebase.

我看了代码结构，说下结论：

**核心编码逻辑本身不大**，但直接用 Zig 重写的工程量不低，原因在依赖而非算法：

### 关键依赖链

| 模块 | 依赖 | Zig 替代难度 |
|------|------|-------------|
| Wirehair 喷泉码 | 纯 C，~5 个 .cpp/.h | ✅ 可直接编译链接，不改代码 |
| libcorrect (RS) | 纯 C | ✅ 同 |
| Zstandard | 纯 C | ✅ 同 |
| OpenCV | C++（用于 Mat、imread/imwrite） | ❌ 大头 |
| stb_image | 纯 C | ✅ 轻量，可直接用 |
| PNG 编码/解码 | OpenCV 隐式依赖 libpng | ❌ 需替代品 |
| 瓷砖位图（bitmaps.h） | base91 解码 → stb_image | ✅ 逻辑简单 |
| 颜色/瓷砖渲染 | 手写像素操作，无 OpenCV 特定依赖 | ✅ |
| **GLFW / OpenGL** 窗口 | 用于 video send | ⚠️ 各平台需分别处理 |

### 实际需要重写的范围

去掉 OpenCV 后，核心编码器只用到了 OpenCV 三件事：
1. `cv::Mat` — 图像矩阵（RGB buffer，自己封装不难）
2. `cv::imwrite` — 写 PNG（用 stb_image_write / lodepng 替代）
3. `cv::cvtColor` — RGB ↔ BGR 转换（手写 3 行）

**不依赖 OpenCV 的纯算法部分**（~4000 行 C++，可直接保留）：
- Wirehair + RS + zstd（全部纯 C）
- fountain_encoder_stream / FountainMetadata
- ReedSolomon / reed_solomon_stream
- zstd_compressor

**通常的 C++ wrapper 方案**反而最省钱：把编码器编成 C ABI 动态库（libcimbar_core.so），各平台共用同一份 core，只写平台特定的 camera capture + GL 渲染 wrapper：

```
┌────────────────────┐
│    C ABI 核心库     │  ← 同一份 .so / .a / .dylib
│ (zstd+wirehair+RS+ │     在 Linux / Android / iOS 各自编译
│  tile render+PNG)   │
└────────┬───────────┘
         │ C ABI：encode_fountain(input) → frame[]
         │
    ┌────┴────┐
    │ 平台胶水  │  ← 各平台约 200-500 行
    │ cam capture│
    │ + 显示    │
    └─────────┘
```

### 我的建议

**不要用 Zig 重写，走 C ABI wrapper 路线**：
1. 把编码器核心抽成纯 C 接口（去掉 OpenCV 依赖，用 `uint8_t*` + `size_t` 传 pixel buffer）
2. **Linux**: 直接链接 .so，GLFW 显示 （done）
3. **Android**: JNI 调 .so，CameraX + OpenGL ES 纹理渲染
4. **iOS**: 调 .a/.dylib，AVFoundation + Metal/SpriteKit

核心关注点是 **去掉 OpenCV 耦合**（主要是 `cv::Mat` 和 `cv::imwrite`），用 stb_image_write/lodepng 替代——大概 2-3 天的重构量。Wirehair、zstd、libcorrect 全都不用动。

---

## User

对我的需求而言，这个库提供了怎样的接口？

---

## Assistant (Build · DeepSeek V4 Flash · 92.3s)

_Thinking:_

The user wants to know what interfaces/APIs this library provides. Let me look at the public API surface - especially the C/C++ headers that would be relevant for their use case of wrapping it as a C ABI library.

**Tool: glob**

**Input:**
```json
{
  "pattern": "src/**/*.h",
  "path": "/home/ringo/repositories/azote/libcimbar"
}
```

**Output:**
```
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/zstd_errors.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/zstd.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/zdict.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/decompress/zstd_decompress_internal.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/decompress/zstd_decompress_block.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/decompress/zstd_ddict.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstdmt_compress.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_opt.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_ldm_geartab.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_ldm.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_lazy.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_fast.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_double_fast.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_cwksp.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_compress_superblock.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_compress_sequences.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_compress_literals.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/zstd_compress_internal.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/hist.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/compress/clevels.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/zstd_trace.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/zstd_internal.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/zstd_deps.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/xxhash.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/threading.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/portability_macros.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/pool.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/mem.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/huf.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/fse.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/error_private.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/debug.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/cpu.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/compiler.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/bitstream.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/bits.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/zstd/common/allocations.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/test/SiameseTools.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/tables/HeavyRowGenerator.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/include/wirehair/wirehair.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/WirehairTools.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/gf256.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/wirehair/WirehairCodec.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/stb/stb_image.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/libpopcnt/libpopcnt.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/libcorrect/tests/include/rs_tester_fec_shim.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/libcorrect/tests/include/rs_tester_fec.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/libcorrect/tests/include/rs_tester.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/libcorrect/include/fec_shim.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/fmt/format.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/fmt/format-inl.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/fmt/base.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/concurrentqueue/concurrentqueue.h
/home/ringo/repositories/azote/libcimbar/src/third_party_lib/PicoSHA2/picosha2.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/Timer.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/MakeTempDirectory.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/File.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/ConfigScope.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/vec_xy.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/null_stream.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/loop_iterator.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/compiler_constants.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/bytebuf.h
/home/ringo/repositories/azote/libcimbar/src/lib/util/byte_istream.h
/home/ringo/repositories/azote/libcimbar/src/lib/serialize/str_join.h
/home/ringo/repositories/azote/libcimbar/src/lib/serialize/str.h
/home/ringo/repositories/azote/libcimbar/src/lib/serialize/format.h
/home/ringo/repositories/azote/libcimbar/src/lib/image_hash/hamming_distance.h
/home/ringo/repositories/azote/libcimbar/src/lib/gui/window_glfw.h
/home/ringo/repositories/azote/libcimbar/src/lib/gui/mat_to_gl.h
/home/ringo/repositories/azote/libcimbar/src/lib/gui/gl_shader.h
/home/ringo/repositories/azote/libcimbar/src/lib/gui/gl_program.h
/home/ringo/repositories/azote/libcimbar/src/lib/gui/gl_2d_display.h
/home/ringo/repositories/azote/libcimbar/src/lib/image_hash/bit_extractor.h
/home/ringo/repositories/azote/libcimbar/src/lib/image_hash/average_hash.h
/home/ringo/repositories/azote/libcimbar/src/lib/image_hash/ahash_result.h
/home/ringo/repositories/azote/libcimbar/src/lib/fountain/fountain_encoder_stream.h
/home/ringo/repositories/azote/libcimbar/src/lib/fountain/fountain_decoder_stream.h
/home/ringo/repositories/azote/libcimbar/src/lib/fountain/fountain_decoder_sink.h
/home/ringo/repositories/azote/libcimbar/src/lib/fountain/concurrent_fountain_decoder_sink.h
/home/ringo/repositories/azote/libcimbar/src/lib/fountain/FountainMetadata.h
/home/ringo/repositories/azote/libcimbar/src/lib/fountain/FountainInit.h
/home/ringo/repositories/azote/libcimbar/src/lib/fountain/FountainEncoder.h
/home/ringo/repositories/azote/libcimbar/src/lib/fountain/FountainDecoder.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Undistort.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/SimpleCameraCalibration.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Scanner.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/ScanState.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Point.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Midpoints.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Geometry.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/ExtractorPlus.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Extractor.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/EdgeScanState.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/DistortionParameters.h
/home/ringo/repositories/azote/libcimbar/src/lib/extractor/DeskewerPlus.h
/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CellPositions.h
/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CellDrift.h
/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Cell.h
/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/AdjacentCellFinder.h

(Results are truncated: showing first 100 results. Consider using a more specific path or pattern.)
```

**Tool: task**

**Input:**
```json
{
  "description": "Find public C API surface",
  "subagent_type": "explore",
  "prompt": "In /home/ringo/repositories/azote/libcimbar, I need to understand the public API surface that could be wrapped for cross-platform use.\n\n1. Check `src/lib/cimbar_js/` - this is the C API for WASM, it's likely the closest thing to a clean C ABI. Read all files there.\n2. Check `src/exe/` for all executable entry points to understand how the library is used.\n3. Check if there's any CMakeLists.txt or build system that shows what gets compiled as a library vs executable.\n4. Look at how `EncoderPlus` or `Encoder` is used - what's the minimal set of calls to produce frames from a file?\n5. Check if there's any decoder API.\n\nReturn the full content of key API files and a summary of all public interfaces available."
}
```

**Output:**
```
<task id="ses_1499d7609ffeNUZrCHQr26czTp" state="completed">
<task_result>
Now I have a complete picture. Here is the comprehensive summary of the public API surface.

---

## Complete API Surface Analysis for `libcimbar`

### 1. C API (WASM / Cross-Platform Wrapper) -- `src/lib/cimbar_js/`

This is the **primary C ABI** intended for binding. Two header files define `extern "C"` functions:

#### Encoder API: `cimbar_js.h`
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/cimbar_js/cimbar_js.h`

| Function | Purpose |
|---|---|
| `cimbare_configure(mode_val, compression)` | Set config mode (4, 66, 67, 68) and zstd compression level (0-22). Must be called first. |
| `cimbare_init_encode(filename, fnsize, encode_id)` | Begin encoding a file. `encode_id` is [0-127]; pass -1 to auto-increment. |
| `cimbare_encode(buffer, size)` | Feed file data in chunks of `cimbare_encode_bufsize()`. Returns 1 if more data needed, 0 when done, negative on error. |
| `cimbare_next_frame(color_balance)` | Generate the next encoded frame (returns frame count). Loops internally after 8x the required blocks. |
| `cimbare_get_frame_buff(&buff)` | Get raw pixel data of the last frame (RGB, 1024x1024x3 = 3,145,728 bytes). |
| `cimbare_render()` | Display current frame in the GLFW window. |
| `cimbare_init_window(width, height)` | Create/resize a GLFW window (width/height must be divisible by 4). |
| `cimbare_rotate_window(rotate)` | Rotate the window display by 90 degrees. |
| `cimbare_auto_scale_window(padding)` | Auto-scale the rendering to the window size. |
| `cimbare_encode_bufsize()` | Returns the input chunk size expected by `cimbare_encode()` (0x4000 = 16384). |
| `cimbare_get_aspect_ratio()` | Returns the aspect ratio of the current config's image. |

**Encoding flow** (from `send.cpp` and test):
```
cimbare_configure(mode, compression)
cimbare_init_encode(filename, fnsize, encode_id)
cimbare_encode(data, size)  // repeat until returns 0
while (true):
    cimbare_next_frame()
    cimbare_get_frame_buff(&ptr)  // get pixel data
    cimbare_render()              // display (optional)
    // loop restarts automatically after enough frames
```

#### Decoder API: `cimbar_recv_js.h`
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/cimbar_js/cimbar_recv_js.h`

| Function | Purpose |
|---|---|
| `cimbard_configure_decode(mode_val)` | Set config mode (default 68) before decoding. |
| `cimbard_get_bufsize()` | Returns the buffer size needed for `scan_extract_decode` output. |
| `cimbard_scan_extract_decode(imgdata, imgw, imgh, format, bufspace, bufsize)` | **Main decode entry point.** Takes raw image data (format 3=RGB, 4=RGBA, 12=NV12, 420=YUV420), extracts the cimbar code, decodes it, writes fountain chunks into `bufspace`. Returns bytes written or negative on error. |
| `cimbard_fountain_decode(buffer, size)` | Feed fountain-encoded data chunks into the stateful fountain decoder sink. Returns `>0` (file ID) on complete file, 0 if still collecting, negative on error. |
| `cimbard_get_filesize(id)` | Get compressed file size from a file ID. |
| `cimbard_get_filename(id, filename, fnsize)` | Get the original filename from the recovered file. Returns filename length or 0 if none. |
| `cimbard_get_decompress_bufsize()` | Returns the ZSTD decompression output buffer size (131072). |
| `cimbard_decompress_read(id, buffer, size)` | Decompress and read the next chunk of the recovered file. Returns bytes written, 0 when done, negative on error. |
| `cimbard_get_report(buff, maxlen)` | Get timing/status report string. |
| `cimbard_get_debug(buff, maxlen)` | Get debug image data. |

**Decoding flow** (from `recv2.cpp` and test):
```
cimbard_configure_decode(mode)
bufsize = cimbard_get_bufsize()
for each frame:
    bytes = cimbard_scan_extract_decode(img.data, img.cols, img.rows, format, bufspace, bufsize)
    if bytes > 0:
        id = cimbard_fountain_decode(bufspace, bytes)
        if id > 0:
            cimbard_get_filename(id, filename, ...)
            while (n = cimbard_decompress_read(id, buffer, size)) > 0:
                write buffer to output file
```

---

### 2. C++ Library API (Header-only or statically linked)

#### `Encoder` (header-only)
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/encoder/Encoder.h`

```
class Encoder {
    Encoder(bits_per_symbol=0, bits_per_color=-1);
    void set_encode_id(uint8_t id);       // [0-127]
    void set_color_mode(unsigned mode);

    template<typename STREAM>
    std::optional<cv::Mat> encode_next(STREAM& stream, vec_xy canvas_size={});

    template<typename STREAM>
    fountain_encoder_stream::ptr create_fountain_encoder(STREAM& stream, string_view filename, int compression_level=16);
};
```

- `encode_next()`: Read from any stream (fountain_encoder_stream, ifstream, etc.), produce one RGB `cv::Mat` frame.
- `create_fountain_encoder()`: Compress + wrap a stream into a `fountain_encoder_stream` for iterative frame generation.

#### `EncoderPlus` (header-only)
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/encoder/EncoderPlus.h`

```
class EncoderPlus : public Encoder {
    unsigned encode(filename, output_prefix);  // Non-fountain: writes PNG frames
    unsigned encode_fountain(filename, output_prefix, compression=16, redundancy=1.2);
    unsigned encode_fountain(filename, on_frame_callback, compression=16, redundancy=4.0);
};
```

The `on_frame_callback` variant provides a `const cv::Mat&` for each generated frame, including scan validation (`Scanner::will_it_scan`).

#### `Decoder` (header-only)
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/encoder/Decoder.h`

```
class Decoder {
    Decoder(bool use_ecc=true, bool interleave=true);

    template<typename MAT, typename STREAM>
    unsigned decode(const MAT& img, STREAM& ostream, bool should_preprocess=false, int color_correction=2);

    template<typename MAT, typename STREAM>
    unsigned decode_fountain(const MAT& img, STREAM& ostream, bool should_preprocess=false, int color_correction=2);
};
```

- `decode()`: Simple decode from image to output stream (no fountain).
- `decode_fountain()`: Decode from image to a `fountain_decoder_sink` (fountain aware, with metadata tracking).

#### `DecoderPlus` (header-only)
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/encoder/DecoderPlus.h`

```
class DecoderPlus : public Decoder {
    unsigned decode(filename, output);             // file-based convenience
    bool load_ccm(filename);
    bool save_ccm(filename);
};
```

#### `Extractor` (has .cpp, static library)
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Extractor.h`

```
class Extractor {
    Extractor(padding=0, vec_xy image_size={}, anchor_size=0);
    int extract(const MAT& img, MAT& out);  // returns FAILURE(0), SUCCESS(1), NEEDS_SHARPEN(2)
};
```

Finds the four corner anchors in an image, deskews, and crops to the cimbar grid.

#### `ExtractorPlus` (header-only)
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/ExtractorPlus.h`

```
class ExtractorPlus : public Extractor {
    int extract(string read_path, cv::Mat& out);
    int extract(string read_path, string write_path);
};
```

#### Fountain subsystem (header-only, `INTERFACE` library)

- **`FountainEncoder`** (`FountainEncoder.h`): Wraps `wirehair` encoder. `encode(block_num, buffer, size)`.
- **`fountain_encoder_stream`** (`fountain_encoder_stream.h`): Stream adapter for FountainEncoder. `create(stream, buffer_size, encode_id)` -> `shared_ptr`. Supports `read()`, `blocks_required()`, `block_count()`, `restart()`.
- **`FountainDecoder`** (`FountainDecoder.h`): Wraps `wirehair` decoder. `decode(block_num, data, length)`, `recover(data, size)`.
- **`fountain_decoder_stream`** (`fountain_decoder_stream.h`): Stream adapter for FountainDecoder. Groups writes, calls `decode()` when buffer full.
- **`fountain_decoder_sink`** (`fountain_decoder_sink.h`): Stateful multi-file fountain decoder. `decode_frame(data, size)` returns file ID on completion. Manages up to 8 concurrent streams. Supports `recover(id, data, size)`.
- **`FountainMetadata`** (`FountainMetadata.h`): 6-byte header: encode_id (7 bits) + file_size (23 bits) + block_id (16 bits).

#### Compression (header-only)

- **`zstd_compressor<STREAM>`** (`zstd_compressor.h`): Chunked zstd compression with filename embedding (via skippable frames). `CHUNK_SIZE = 0x4000`.
- **`zstd_decompressor<STREAM>`** (`zstd_decompressor.h`): Chunked zstd decompression.
- **`zstd_header_check`** (`zstd_header_check.h`): Extracts filename from zstd skippable frame header.

#### `Config` (header-only, `cimb_translator` static lib)
**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Config.h`

```
cimbar::Config::update(mode_val);  // 4, 8, 66, 67, 68 (68="B" default, 66="Bu", 67="Bm", 4="4C")
// Accessors: dark(), legacy_mode(), color_mode(), color_bits(), symbol_bits(), bits_per_cell(),
//   ecc_bytes(), ecc_block_size(), image_size_x(), image_size_y(), cell_size(), total_cells(),
//   capacity(), fountain_chunks_per_frame(), fountain_chunk_size()
```

---

### 3. Executable Entry Points (`src/exe/`)

| Executable | Source | Purpose | Uses |
|---|---|---|---|
| `cimbar` | `src/exe/cimbar/cimbar.cpp` | Combined encoder/decoder CLI | `EncoderPlus`, `DecoderPlus`, `Extractor` directly |
| `cimbar_send` | `src/exe/cimbar_send/send.cpp` | GUI encoder (GLFW window) | `cimbar_js.h` C API |
| `cimbar_recv` | `src/exe/cimbar_recv/recv.cpp` | Camera decoder (GLFW+VideoCapture) | `Decoder`, `Extractor`, `fountain_decoder_sink` directly |
| `cimbar_recv2` | `src/exe/cimbar_recv2/recv2.cpp` | Camera decoder using the C API | `cimbar_recv_js.h` C API |
| `cimbar_extract` | `src/exe/cimbar_extract/cimbar_extract.cpp` | Extract-only (deskew+crop to cimbar grid) | `ExtractorPlus` |
| `build_image_assets` | `src/exe/build_image_assets/build_image_assets.cpp` | Asset builder for bitmaps | `base91`, `File` |

---

### 4. Build System Summary

**Top-level CMakeLists.txt** (`/home/ringo/repos/azote/libcimbar/CMakeLists.txt`):

**Library targets** (always built, even in WASM mode):
| Library | Type | Description |
|---|---|---|
| `cimb_translator` | **STATIC** | Tile encoding/decoding, config, grid geometry |
| `extractor` | **STATIC** | Corner detection, deskew, perspective transform |
| `cimbar_js` | **STATIC** (native) / **EXECUTABLE** (WASM) | C API wrappers + tests |
| `encoder` | INTERFACE (header-only) | Encoder, Decoder, ReedSolomon |
| `fountain` | INTERFACE (header-only) | Wirehair fountain code wrappers |
| `compression` | INTERFACE (header-only) | ZSTD wrappers |
| `gui` | INTERFACE (header-only) | GLFW window wrapper |
| `util` | INTERFACE (header-only) | File helpers, Timer |
| `serialize` | INTERFACE (header-only) | Format, string utils |
| `image_hash` | INTERFACE (header-only) | ahash for tile matching |
| `bit_file` | INTERFACE (header-only) | Bit reader/buffer |
| `chromatic_adaptation` | INTERFACE (header-only) | Color correction matrices |

**Third-party STATIC libraries**: `wirehair`, `zstd`, `libcorrect`, `base91`, `intx`, `libpopcnt`

**Executables** (native only, excluded in WASM builds): `cimbar`, `cimbar_send`, `cimbar_recv`, `cimbar_recv2`, `cimbar_extract`, `build_image_assets`

**Dependencies**: OpenCV (required), GLFW + OpenGL (for GUI targets)

---

### 5. Minimal Encoding Flow (no window, no GUI)

Using the C++ API directly (from `EncoderPlus` and tests):

```cpp
// 1. Set up config
cimbar::Config::update(68);  // Mode "B" (default)

// 2. Read file and create fountain encoder
std::ifstream infile("myfile.dat", std::ios::binary);
Encoder enc;
enc.set_encode_id(109);
auto fes = enc.create_fountain_encoder(infile, "myfile.dat", 16);  // compress+wrap

// 3. Generate frames
for (unsigned i = 0; i < requiredFrames; ++i) {
    auto frame = enc.encode_next(*fes);  // returns cv::Mat (RGB)
    if (!frame) break;
    // frame->data has the pixel data (1024x1024x3)
    cv::cvtColor(*frame, *frame, cv::COLOR_RGB2BGR);
    cv::imwrite(output_path, *frame);
}
```

Or using the **C API** (from `cimbar_send.cpp`):
```c
cimbare_configure(68, 16);
cimbare_init_encode("file.dat", 8, -1);
cimbare_encode(data, size);  // data up to 16384 bytes
while (true) {
    cimbare_next_frame(0);
    cimbare_get_frame_buff(&ptr);  // get RGB pixels
}
```

### 6. Minimal Decoding Flow (from image)

```cpp
// C++ API directly:
cimbar::Config::update(68);
Extractor ext;
Decoder dec;
cv::Mat img = cv::imread("frame.png");
cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
cv::UMat processed = img.getUMat(cv::ACCESS_RW);
ext.extract(processed, processed);  // deskew + crop

unsigned chunk_size = cimbar::Config::fountain_chunk_size();
fountain_decoder_sink sink(chunk_size, decompress_on_store<std::ofstream>("output_dir"));
dec.decode_fountain(processed, sink, false, 2);
```

Or using the **C API** (from `cimbar_recv_jsTest.cpp`):
```c
cimbard_configure_decode(68);
unsigned bufsize = cimbard_get_bufsize();
unsigned char* bufspace = malloc(bufsize);
int bytes = cimbard_scan_extract_decode(imgdata, w, h, 3, bufspace, bufsize);
int64_t id = cimbard_fountain_decode(bufspace, bytes);
if (id > 0) {
    char filename[256];
    cimbard_get_filename(id, filename, 256);
    unsigned char* decbuf = malloc(cimbard_get_decompress_bufsize());
    while ((n = cimbard_decompress_read(id, decbuf, bufsize)) > 0)
        fwrite(decbuf, 1, n, outfile);
}
```

---

### 7. Key Observations for Cross-Platform Wrapping

1. **The C API (`cimbar_js.h` + `cimbar_recv_js.h`) is the best target** for FFI bindings. It has zero C++ dependencies at the boundary (pure C `extern "C"`).

2. **The C encode API requires OpenCV/GLFW** only for the optional `render()` and `init_window()` functions. The core encode path (`init_encode` -> `encode` -> `next_frame` -> `get_frame_buff`) can operate **without** a window if you skip `render()`/`init_window()` -- however, `cimbare_configure()` internally calls `cimbare_init_window()`, creating a GLFW dependency. For headless operation, you would need to either mock the window or use the C++ API directly.

3. **The C decode API has no GUI dependency.** `cimbard_scan_extract_decode()` accepts raw image bytes and returns decoded data. It does depend on OpenCV internally.

4. **Mode values**: 68 ("B", default, 8x8 grid, 1024x1024), 67 ("Bm", mini), 66 ("Bu", micro), 4 ("4C", 2-bit legacy), 8 ("8C", 3-bit legacy).

5. **Config is thread-local** (`thread_local` in `Config::active_conf()`), so each thread must call `Config::update()` independently.

6. **All encoder core classes are header-only** (`Encoder`, `EncoderPlus`, `Decoder`, `DecoderPlus`, all fountain classes). Only `cimb_translator` and `extractor` have .cpp files that need compiling as static libraries.
</task_result>
</task>
```

---

## Assistant (Build · DeepSeek V4 Flash · 12.3s)

_Thinking:_

Now I have a complete picture of the API surface. Let me summarize this for the user in Chinese, focusing on the interfaces relevant to their cross-platform needs.

核心接口主要分两层：

## C++ API（header-only，零 .cpp 编译）

编码端 `Encoder` / `EncoderPlus`，解码端 `Decoder` / `DecoderPlus`，喷泉流、RS、zstd 全部是 header-only template，编译时直接 inline 进你的项目。唯一需要链接的静态库是 `cimb_translator`（瓷砖渲染）+ `extractor`（锚点检测/透视校正）。

## C ABI（`src/lib/cimbar_js/`）— 你需要的

这是现成的 `extern "C"` 接口，专为跨平台绑定设计：

### 编码侧

```c
cimbare_configure(mode_val, compression);       // 68=默认, 16=zstd级别
cimbare_init_encode(filename, fnsize, encode_id); // 初始化一次
cimbare_encode(data, size);                       // 分块喂数据，每块≤16KB
while (1) {
    cimbare_next_frame(color_balance);            // 生成下一帧
    cimbare_get_frame_buff(&ptr);                 // 拿 RGB 像素指针
    // ptr 指向 1024×1024×3 = 3,145,728 字节
    // 编成 PNG 或送 GL 纹理
}
```

### 解码侧（无 GUI 依赖）

```c
cimbard_configure_decode(68);
bufsize = cimbard_get_bufsize();
bytes = cimbard_scan_extract_decode(imgdata, w, h, format, buf, bufsize);
// ^ 支持 RGB/RGBA/NV12/YUV420 输入，直接吃相机帧
if (bytes > 0) {
    id = cimbard_fountain_decode(buf, bytes);   // 喷泉解码
    if (id > 0) {                               // 一个文件收齐了
        cimbard_get_filename(id, filename, 256);
        while ((n = cimbard_decompress_read(id, outbuf, size)) > 0)
            write(outfile, outbuf, n);          // zstd 解压输出
    }
}
```

### 实际依赖

| 模块 | 你的平台 | 备注 |
|------|----------|------|
| `cimb_translator`（静态库） | 全部 | 纯像素操作，无系统依赖 |
| `extractor`（静态库） | 全部 | 锚点检测，无系统依赖 |
| `wirehair`（C） | 全部 | ~10 个 .cpp 编译 |
| `zstd`（C） | 全部 | |
| `libcorrect`（C） | 全部 | |
| **OpenCV** | ❌ 大头 | 仅用于 `cv::Mat`/`cv::imwrite`/透视变换 |
| GLFW | ❌ | 仅 `cimbare_render/init_window`，你不需要 |

**C API 的 encode 侧 `configure()` 会内部调用 `init_window()`，带 GLFW 依赖**。你跳过 `render()` 就行了，但最好改一下入口：不用 `cimbare_configure`，直接 `Config::update(68)` + `Encoder::create_fountain_encoder` 走 C++ 接口，用 C ABI 只包解码端。

### 最省事的路线

1. **解码端**直接用现成 C API（`cimbar_recv_js.h`），Android/iOS/Linux 各编译一次 `.so/.a`
2. **编码端**不要走 C API（避 GLFW），直接 linker 调 `Encoder::create_fountain_encoder` + `Encoder::encode_next`，这两步无窗口依赖
3. OpenCV 用它头文件里的透视变换功能（`warpPerspective`），如果不想链 OpenCV，可换成 `libvips` 或手写 4-point transform

---

## User

我需要的重点在于在不同的设备（Linux PC, Android, iOS）之间高效（使用这个项目，libcimbar 的图形）的气隙传输消息，因此我需要不止于 wasm 的target。

---

## Assistant (Build · DeepSeek V4 Flash · 136.8s)

_Thinking:_

The user is clarifying their actual use case: they want to use libcimbar's visual encoding (those colorful QR-like images) for air-gap data transfer between devices (Linux PC, Android, iOS). They need more than just WASM - they need native support on all three platforms.

Let me think about what this actually means:

1. They need to both **encode** (display the cimbar pattern on screen) and **decode** (scan the pattern with camera) on all platforms.
2. They want the same visual format (libcimbar's colored tiles) for cross-device compatibility.
3. They don't just need WASM - they need native apps.

The key realization is: the C API already has everything they need. Let me think about what they actually need to do:

- **Encoder side**: Generate frames (cv::Mat or raw RGB buffers) and display them on screen
- **Decoder side**: Capture camera frames, feed to decoder C API

The main work is:
1. Removing OpenCV dependency from the core (or accepting it)
2. Platform-specific camera capture + display
3. Cross-compilation for each platform

Actually, let me reconsider. The user already said they want to "wrap C++" or rewrite in Zig. They've been told the C API exists. Let me think about what the actual blocker is.

The blocker is:
1. **OpenCV is huge** - especially problematic for mobile (Android APK size, iOS binary size)
2. **GLFW** - desktop only, doesn't help mobile
3. **Build system** - CMake doesn't target Android/iOS easily without NDK/Xcode

For their use case (air-gap transfer between devices), the critical pieces are:
- **Display side**: generate the colored tile image and show it on screen
- **Camera side**: capture frames, detect anchors, deskew, decode tiles, fountain decode

Let me look more carefully at what actually depends on OpenCV in the decoding pipeline, since that's the harder part (camera capture + image processing).

Actually, I should read the decoder C API and extractor more carefully to understand what the actual OpenCV dependency is for the image processing pipeline.

Let me check what the actual OpenCV usage is - maybe it's relatively contained and replaceable.

**Tool: task**

**Input:**
```json
{
  "description": "Analyze OpenCV usage depth",
  "subagent_type": "explore",
  "prompt": "In /home/ringo/repositories/azote/libcimbar, I need to understand exactly how deeply OpenCV is wired into the codebase, especially for the decoder/extractor side. This is research only, do not modify files.\n\n1. Read `src/lib/cimbar_js/cimbar_recv_js.h` and `src/lib/cimbar_js/cimbar_recv_js.cpp` to see what the C decode API looks like internally\n2. Read `src/lib/extractor/Extractor.h` and `src/lib/extractor/Extractor.cpp` - this is the corner detection + deskew. What OpenCV functions does it use?\n3. Read `src/lib/extractor/Geometry.h` and `src/lib/extractor/Geometry.cpp` for perspective transform\n4. Read `src/lib/cimb_translator/Cell.h` and `src/lib/cimb_translator/Cell.cpp` - this is tile decoding\n5. Read `src/lib/encoder/Decoder.h` and `src/lib/encoder/Decoder.cpp` (if exists) - the high-level decode flow\n6. Check the CMakeLists.txt for what OpenCV components are required (imgproc, core, imgcodecs, etc.)\n\nFor each file, note every specific OpenCV function/type used (cv::Mat, cv::warpPerspective, cv::cvtColor, etc.) to understand if it can be replaced by simpler alternatives. This helps determine if OpenCV is truly removable."
}
```

**Output:**
```
<task id="ses_1499b834effeL8N3kG0ILnWtbw" state="completed">
<task_result>
Now I have a comprehensive picture. Here is the full analysis:

---

## OpenCV Dependency Analysis for libcimbar Decoder/Extractor

### 1. `src/lib/cimbar_js/cimbar_recv_js.h` + `.cpp` -- C Decode API

**Files:** `/home/ringo/repositories/azote/libcimbar/src/lib/cimbar_js/cimbar_recv_js.h` and `/home/ringo/repositories/azote/libcimbar/src/lib/cimbar_js/cimbar_recv_js.cpp`

This is the entry point. It includes `<opencv2/opencv.hpp>` and uses:

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::UMat` | cpp:23, 94, 101, etc. | OpenCL-backed image container |
| `cv::Mat` | cpp:32, 101, 107, 116 | Core image container |
| `cv::Mat::getUMat()` | cpp:101, 107, 116, 167 | Convert Mat -> UMat |
| `cv::Mat::clone()` | cpp:167 | Debug frame copy |
| `cv::cvtColor(img, img, cv::COLOR_YUV2RGB_NV12)` | cpp:102 | YUV NV12 -> RGB |
| `cv::cvtColor(img, img, cv::COLOR_YUV420p2RGB)` | cpp:108 | YUV 420p -> RGB |
| `cv::cvtColor(img, img, cv::COLOR_RGBA2RGB)` | cpp:118 | RGBA -> RGB |
| `cv::ACCESS_RW`, `cv::ACCESS_READ` | cpp:101, 107, 116, 167 | UMat access flags |
| `cv::Mat::dims`, `.cols`, `.rows`, `.data` | cpp:136-139 | Debug output |

**Replaceability note:** The format conversion functions (YUV->RGB, RGBA->RGB) are simple color space math. YUV conversion in particular can be ~50 lines of hand-rolled code. UMat is a convenience; a raw pointer works fine.

---

### 2. `src/lib/extractor/Extractor.h` + `.cpp` -- Corner Detection + Deskew Orchestrator

**Files:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Extractor.h` and `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Extractor.cpp`

Header includes `<opencv2/opencv.hpp>` (line 8). The extract function is a template that:
1. Creates a `Scanner(img)` (which preprocesses to grayscale and thresholds)
2. Scans for anchor points (no OpenCV after preprocessing)
3. Builds `Corners` (uses `cv::Point2f` only)
4. Calls `Deskewer::deskew(img, corners)` -- the heavy OpenCV call

**Direct OpenCV usage in this file:** None beyond includes. The work is delegated.

---

### 3. `src/lib/extractor/Scanner.h` + `.cpp` -- Preprocessing + Anchor Detection

**Files:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Scanner.h` and `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Scanner.cpp`

This is the heaviest OpenCV user in the extract phase:

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Mat _img` | Scanner.h:85 | Stores the preprocessed (binary) image |
| `cv::Mat preprocess_image()` | Scanner.h:141-165 | Orchestrates preprocessing |
| `cv::cvtColor(img, temp, cv::COLOR_RGB2GRAY)` | Scanner.h:153 | RGB -> grayscale |
| `cv::GaussianBlur(temp, temp, cv::Size(unit,unit), 0)` | Scanner.h:159 | Smoothing before threshold |
| `cv::threshold(img, out, 0, 255, cv::THRESH_BINARY \| cv::THRESH_OTSU)` | Scanner.h:129 | Otsu binarization (fast path) |
| `cv::adaptiveThreshold(img, out, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, unit, -10)` | Scanner.h:137 | Adaptive threshold (slow path) |
| `_img.at<uchar>(y, x)` | Scanner.cpp:53 | Single pixel read during scanning |
| `_img.cols`, `_img.rows` | Scanner.h:183, 214, etc. | Dimensions |

**Replaceability note:** This is the hardest part to replace. `cvtColor` (RGB->GRAY) is trivial. `GaussianBlur` can be replaced by a box blur or separable kernel convolution. `threshold` with Otsu is ~50 lines of histogram computation. `adaptiveThreshold` with mean C is doable but is fragile reimplementation (~80 lines for integral image + threshold). The `at<>` pixel reads could switch to raw `data` pointer access.

---

### 4. `src/lib/extractor/Corners.h` -- Corner Point Wrapper

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Corners.h`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Point2f` | Corners.h:45-53 | Wraps 4 int corners into `vector<cv::Point2f>` for `getPerspectiveTransform` |

**Replaceability note:** Trivial. A `vector<pair<float,float>>` would work -- `cv::Point2f` is just `{float x, float y}`. The `all()` method exists solely to feed `getPerspectiveTransform`.

---

### 5. `src/lib/extractor/Deskewer.h` + `.cpp` -- Perspective Transform

**Files:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Deskewer.h` and `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Deskewer.cpp`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Point2f` | Deskewer.h:28-32 | Output corner points |
| `cv::getPerspectiveTransform(corners.all(), outputPoints)` | Deskewer.h:36 | Compute 3x3 homography |
| `cv::warpPerspective(img, output, transform, output.size(), cv::INTER_LINEAR)` | Deskewer.h:38 | Apply perspective warp |
| `cv::Mat transform` | Deskewer.h:36 | Stores the 3x3 homography |

**Replaceability note:** These two functions are the core of the perspective correction. `getPerspectiveTransform` solves a 4-point system (8 equations) to find an 8-DOF homography -- about 60 lines of linear algebra (Gaussian elimination or closed-form). `warpPerspective` is a coordinate mapping + bilinear interpolation -- about 80 lines. This is the #1 barrier to removing OpenCV.

---

### 6. `src/lib/extractor/Geometry.h` -- No OpenCV

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Geometry.h` (no .cpp exists)

Pure math. Uses `point<double>` (a custom `pair<double,double>` wrapper), line intersection via cross-product formulas. **Zero OpenCV.**

---

### 7. `src/lib/extractor/Undistort.h` -- Lens Distortion Correction

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/Undistort.h`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::remap(img, out, _map1, _map2, cv::INTER_LINEAR, cv::BORDER_CONSTANT)` | Undistort.h:33 | Apply undistortion |
| `cv::initUndistortRectifyMap(...)` | Undistort.h:43 | Build remap tables |
| `cv::Mat _map1, _map2` | Undistort.h:56-57 | Precomputed remap tables |
| `cv::Size()` | Undistort.h:43 | Output size |

**Replaceability note:** This is only used by `cimbar_extract` / `cimbar_recv2` executables. The core `cimbar_js` path and `cimbar_recv` do not call `Undistort`. It is optional. But `initUndistortRectifyMap` + `remap` would need to be replaced by equivalent radial distortion math + inverse mapping.

---

### 8. `src/lib/extractor/SimpleCameraCalibration.h` + `.cpp` -- Distortion Parameter Estimation

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/extractor/SimpleCameraCalibration.h`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Mat1d(3,3)` | SimpleCameraCalibration.h:53 | Camera intrinsic matrix constructor |
| `cv::Mat1d(1,4)` | SimpleCameraCalibration.h:54 | Distortion coefficients |

**Replaceability note:** Trivial -- just constructing small fixed-size matrices. `cv::Mat1d` is a typedef for `cv::Mat` with `CV_64F`. Could be `std::array<std::array<double,3>,3>`.

---

### 9. `src/lib/cimb_translator/CimbReader.h` + `.cpp` -- Grid Tile Reading

**Files:** `/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbReader.h` and `/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbReader.cpp`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Mat _image` | CimbReader.h:29 | Stores the full RGB image |
| `cv::cvtColor(img, symbols, cv::COLOR_RGB2GRAY)` | CimbReader.cpp:35 | Grayscale for symbol thresholding |
| `cv::filter2D(img, out, -1, kernel())` | CimbReader.cpp:26 | Sharpening with [[0,-1,0],[-1,4.5,-1],[0,-1,0]] |
| `cv::adaptiveThreshold(symbols, symbols, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, blockSize, 0)` | CimbReader.cpp:41 | Threshold for symbol grid |
| `cv::Rect crop(x, y, 4, 4)` | CimbReader.cpp:66 | ROI for white-point sampling |
| `cv::mean(img(crop))` | CimbReader.cpp:67, 81 | Average color in ROI |
| `cv::Scalar` | CimbReader.cpp:49-50 | Color value container |
| `cv::Mat_<float>(1,3)` | CimbReader.cpp:243, 247, 258, 261 | Row vectors for CCM computation |
| `cv::Mat::ones(0, 3, CV_32F)` | CimbReader.cpp:230-231 | Initializing actual/desired matrices |
| `cv::Mat actual, desired` | CimbReader.cpp:230-231 | Data matrices for Moore-Penrose |
| `cv::Mat::push_back()` | CimbReader.cpp:244, 248, 259, 262 | Append rows to matrices |
| `cv::Mat::rows` | CimbReader.cpp:252 | Guard check |

**Replaceability note:** `cvtColor` (RGB->GRAY) is trivial. `filter2D` with a 3x3 kernel is ~30 lines. `adaptiveThreshold` is the hardest (same problem as Scanner). `cv::Rect` + `cv::mean` on a crop is ~15 lines of manual pixel summing. The `Mat_<float>` matrix construction for CCM computation is building data for `get_moore_penrose_lsm` -- could use `std::vector<std::array<float,3>>`.

---

### 10. `src/lib/cimb_translator/Cell.h` -- Tile Pixel Averaging

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/Cell.h` (no .cpp exists)

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Mat& _img` | Cell.h:158 | Reference to the source image (RGB) |
| `_img.ptr<uchar>(0)` | Cell.h:39, 81, 102, 129 | Raw row pointer |
| `_img.channels()`, `.cols`, `.rows` | Cell.h:37, 38, 67, 69, 78, 107, 119, etc. | Image properties |
| `_img.isContinuous()` | Cell.h:69, 119 | Memory layout check |

**Replaceability note:** Uses only `cv::Mat` as a data container. `ptr<>()` accesses raw pixel data. `isContinuous()` checks memory layout. This could trivially be changed to `uint8_t*` + width/height/channels parameters. **No OpenCV functions are called** -- OpenCV is only used as a data transport struct.

---

### 11. `src/lib/cimb_translator/CimbDecoder.h` + `.cpp` -- Symbol/Color Decoding

**Files:** `/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbDecoder.h` and `/home/ringo/repositories/azote/libcimbar/src/lib/cimb_translator/CimbDecoder.cpp`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Matx<float, 3, 3>` | CimbDecoder.h:20, CimbDecoder.cpp:82, 84 | Color correction matrix storage |
| `cv::Mat tile` | CimbDecoder.cpp:89 | Temporary tile creation for hash computation |
| `cv::Mat` parameter in `decode_symbol` | CimbDecoder.h:23 | Accepts cell image for CPU-path decoding |

**Replaceability note:** The `Matx<float,3,3>` is just a 3x3 float matrix -- could be `std::array<std::array<float,3>,3>` or a struct. The `decode_symbol(cv::Mat&, ...)` overload (line 134) creates an `ahash_result` from a cv::Mat, but the primary decode path uses `decode_symbol(const bitmatrix&, ...)` which has **zero OpenCV dependency**. `get_tile_hash` calls `cimbar::getTile()` (which uses OpenCV for bitmap loading) and `average_hash()` (which uses cvtColor + resize).

---

### 12. `src/lib/image_hash/average_hash.h` -- Perceptual Hashing

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/image_hash/average_hash.h`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::cvtColor(gray, gray, cv::COLOR_RGB2GRAY)` | average_hash.h:23, 47 | Grayscale before hash |
| `cv::resize(gray, gray, cv::Size(8, 8))` | average_hash.h:25 | Downscale to 8x8 |
| `gray.ptr<uchar>(i)` | average_hash.h:34, 56 | Row pointer for bit extraction |

**Replaceability note:** `cvtColor` is trivial RGB->Gray (dot product with constants). `resize` to 8x8 is bilinear interpolation -- about 30 lines. The `ptr<>()` calls are just pointer arithmetic. The `fuzzy_ahash(const bitmatrix&, ...)` overload (line 64) uses **no OpenCV at all**.

---

### 13. `src/lib/chromatic_adaptation/color_correction.h` -- Color Correction Matrix

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/chromatic_adaptation/color_correction.h`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Matx<float, 3, 3>` | color_correction.h:13, 22, 26, 47, 64, 76 | Core CCM type |
| `cv::Matx<float, 3, 1>` | color_correction.h:16-20 | Column vectors |
| `cv::Matx<float, 3, 3>::diag(...)` | color_correction.h:22 | Diagonal matrix from ratios |
| `cv::Mat` | color_correction.h:26, 32-35 | Moore-Penrose input |
| `cv::transpose(desired, x)` | color_correction.h:33 | Matrix transpose |
| `cv::transpose(actual, y)` | color_correction.h:34 | Matrix transpose |
| `cv::invert(y, z, cv::DECOMP_SVD)` | color_correction.h:35 | SVD-based pseudoinverse |
| `cv::Matx` arithmetic (+, *, inv()) | color_correction.h:19-23 | Matrix algebra |

**Replaceability note:** The adaptation matrix computation (von Kries/Bradford) is just 3x3 matrix multiplication and inversion -- could use a small linear algebra library (Eigen, glm, or hand-rolled). The Moore-Penrose pseudoinverse via SVD is the hardest: this is a full SVD decomposition, requiring ~200+ lines of robust implementation. However, this function is only called during CCM generation (a once-per-decode optimization), not per-cell.

---

### 14. `src/lib/chromatic_adaptation/adaptation_transform.h` -- Chromatic Adaptation Transforms

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/chromatic_adaptation/adaptation_transform.h`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Matx<float, 3, 3>` | adaptation_transform.h:10, 12, 23, 25 | 3x3 transform matrices |

**Replaceability note:** Just static constant 3x3 matrices. Could be `std::array<std::array<float,3>,3>`.

---

### 15. `src/lib/bit_file/bitmatrix.h` -- Bit Matrix from cv::Mat

**File:** `/home/ringo/repositories/azote/libcimbar/src/lib/bit_file/bitmatrix.h`

| OpenCV type/function | Location (file:line) | Purpose |
|---|---|---|
| `cv::Mat&` parameter in `mat_to_bitbuffer` | bitmatrix.h:14 | Source of binary data |
| `img.ptr<uchar>(0)` | bitmatrix.h:16 | Raw pointer to pixel data |
| `img.cols`, `img.rows` | bitmatrix.h:17 | Image dimensions |

**Replaceability note:** Uses only `cv::Mat` as a byte container + dimensions. The actual function packs 8 pixel bytes (each 0 or 255) into a single byte. Could accept `const uint8_t* data, int width, int height`.

---

### 16. `src/lib/encoder/Decoder.h` + `.cpp` -- High-Level Decode Flow

**Files:** `/home/ringo/repositories/azote/libcimbar/src/lib/encoder/Decoder.h` (Decoder.cpp does not exist -- header-only)

The `Decoder` template creates a `CimbReader(img, ...)` and iterates over all cells. It includes `<opencv2/opencv.hpp>` (line 12) but uses **no OpenCV types or functions directly**. The templates are parameterized on `MAT` and just pass the image through to `CimbReader` and `CimbDecoder`. Zero direct OpenCV usage.

---

### 17. CMakeLists.txt -- Build Configuration

**File:** `/home/ringo/repositories/azote/libcimbar/CMakeLists.txt`

Line 52 declares the required OpenCV components:
```cmake
set(OPENCV_LIBS "opencv_calib3d" "opencv_imgcodecs" "opencv_imgproc" "opencv_photo" "opencv_core")
```

| Library | Used in decode path? | What for? |
|---|---|---|
| **opencv_core** | YES | `cv::Mat`, `cv::Matx`, `cv::Scalar`, `cv::Point2f`, `cv::Size`, `cv::Rect`, `cv::transpose`, `cv::invert`, `cv::mean` |
| **opencv_imgproc** | YES | `cv::cvtColor`, `cv::GaussianBlur`, `cv::threshold`, `cv::adaptiveThreshold`, `cv::filter2D`, `cv::resize`, `cv::warpPerspective`, `cv::getPerspectiveTransform`, `cv::remap`, `cv::initUndistortRectifyMap` |
| **opencv_calib3d** | CONDITIONAL | `cv::initUndistortRectifyMap` (only via Undistort.h, used only in cimbar_extract/recv2 executables) |
| **opencv_imgcodecs** | DECODE-ONLY | `cv::imread` (only in DecoderPlus/ExtractorPlus convenience wrappers, not core decode) |
| **opencv_photo** | NO | Not referenced in decode/encoder code at all; possibly unused |

---

## Summary: What Would Need to Be Replaced

### Core decode path (difficult to replace):

1. **`cv::warpPerspective` + `cv::getPerspectiveTransform`** (Deskewer.h:36-38) -- The #1 blocker. Need homography computation (4-point DLT, ~60 lines) and inverse mapping with bilinear interpolation (~80 lines).

2. **`cv::adaptiveThreshold`** (Scanner.h:137, CimbReader.cpp:41) -- The #2 blocker. Integral-image-based local thresholding (~80-100 lines).

3. **`cv::GaussianBlur`** (Scanner.h:159) -- Replaceable by separable box blur or IIR Gaussian (~40 lines).

4. **`cv::threshold` with OTSU** (Scanner.h:129) -- Replaceable by histogram computation (~40 lines).

5. **`cv::cvtColor` RGB->GRAY** (Scanner.h:153, CimbReader.cpp:35, average_hash.h:23,47) -- ~5 lines of fixed-point math.

6. **`cv::filter2D` 3x3 sharpen** (CimbReader.cpp:26) -- ~15 lines.

7. **`cv::resize` to 8x8** (average_hash.h:25) -- ~30 lines bilinear.

8. **`cv::invert` with DECOMP_SVD** (color_correction.h:35) -- Full SVD implementation, ~200+ lines. Used only once per decode for CCM generation.

9. **`cv::mean` on ROI** (CimbReader.cpp:67,81) -- ~10 lines.

10. **`cv::remap` + `cv::initUndistortRectifyMap`** (Undistort.h:33,43) -- Only used if distortion correction is active. ~80 lines.

### Trivial replacements:

- `cv::Mat` data container -> `uint8_t*` + `width/height/channels` struct
- `cv::Point2f` -> `std::pair<float,float>` or `struct {float x,y;}`
- `cv::Scalar` -> `struct {uchar b,g,r;}`
- `cv::Size` -> `std::pair<int,int>`
- `cv::Rect` -> `struct {int x,y,w,h;}`
- `cv::Matx<float,3,3>` -> `std::array<std::array<float,3>,3>` or small struct
- `cv::Vec3b` -> `struct {uchar b,g,r;}`
- `cv::Mat::ptr<>()` / `::at<>()` -> raw pointer arithmetic with stride

### Data-flow-only usage (no OpenCV functions called):
- `Cell.h` -- uses `cv::Mat` only as a data reference
- `bitmatrix.h` -- uses `cv::Mat` only as a data source
- `CimbDecoder.h` -- `Matx` for CCM storage, `Mat` parameter for one entry point
- `Geometry.h` -- **zero OpenCV**
- `Midpoints.h`, `Point.h`, `Anchor.h`, `EdgeScanState.h`, `ScanState.h` -- **zero OpenCV**

---

## Conclusion: Is OpenCV Removable?

**Yes, but it is a significant undertaking.** The core algorithm could be reimplemented using:
1. A minimal linear algebra library (replace `cv::Matx`, `cv::invert(SVD)`, `cv::transpose` -- ~20 possibilities)
2. Hand-rolled image processing primitives (`cvtColor`, `resize`, `GaussianBlur`, `threshold`, `adaptiveThreshold`, `filter2D`, `warpPerspective`, `getPerspectiveTransform` -- ~7 functions)
3. A custom `uint8_t*` image data container to replace `cv::Mat`

The heaviest lift would be **`cv::adaptiveThreshold`** and **`cv::warpPerspective`**, which together represent about 75% of the replacement effort.

If a lightweight image library like **stb_image_resize.h** + **stb_image_write.h** is already available (stb_image.h is already included in Common.cpp), those could cover reading/writing and resizing, but the warp, adaptive threshold, and SVD would still need custom implementations.
</task_result>
</task>
```

---

## Assistant (Build · DeepSeek V4 Flash · 12.9s)

_Thinking:_

Good. Now I have a very clear picture. The user's actual need is:

1. Cross-platform (Linux PC, Android, iOS) air-gap message transfer using libcimbar's visual encoding
2. Both encode (display image) and decode (camera capture + decode) on all platforms
3. More than just WASM - they want native apps

The key question they're asking: "对我而言提供了怎样的接口" - what interfaces does this library provide for my use case?

Let me summarize their options clearly.

对于你的需求量身总结一下：

## 这个库给你配了什么

### 现成可用的核心

```
编码端：Encoder::encode_next()  →  cv::Mat (RGB 像素，1024×1024)
解码端：cimbard_scan_extract_decode()  →  byte[] (从相机帧直接解码)
喷泉：  Wirehair 自动容错，丢帧不怕
压缩：  zstd level 16，内嵌文件名
```

**编码器核心**（Encoder + fountain + RS + 瓷砖渲染）只有 `cv::Mat` 当 buffer 用，没有 OpenCV 图像处理调用，非常好剥离。

**解码器核心**（Extractor + Deskew + CimbReader + CimbDecoder）才是重头——需要 7 个 OpenCV 函数：`warpPerspective`、`getPerspectiveTransform`、`adaptiveThreshold`、`GaussianBlur`、`cvtColor`、`filter2D`、`resize`。其中 **warpPerspective + adaptiveThreshold 占了约 75% 的工作量**。

### 三种路线对比

| 路线 | 工作量 | 体积 | 维护 | 推荐度 |
|------|--------|------|------|--------|
| **A. 留着 OpenCV，安卓/iOS 交叉编译** | 最小（调 CMake toolchain） | APK 大（~15MB OpenCV） | 低 | ⭐ |
| **B. 剥离 OpenCV，自实现 ~7 个函数** | 1-2 周 | 极小（加 lib 不到 2MB） | 自己管 | ⭐⭐⭐ |
| **C. 用现成 C API，iOS/Android 只做 UI 胶水** | 几天 | 同 B | 上游更新跟着走 | ⭐⭐⭐ |

### 实际建议

这条路工程量非常可控：

1. **两个核心 C++ 静态库必须编译**：`cimb_translator`（~20 .cpp 无 OpenCV 调用）+ `extractor`（锚点检测 + 透视变换，这才是 OpenCV 的大头）
2. **解码端走 C API** `cimbar_recv_js.h`，把 `extractor` 的 OpenCV 调用换成自实现（重点是 `warpPerspective` + `adaptiveThreshold` + `GaussianBlur`，三函数大概 300 行）
3. **编码端直接调** `Encoder::encode_next()`，改一行把 `cv::Mat` 输出改成 `uint8_t*` 裸指针——这个改动大概 1 小时
4. **各平台只需要写 camera capture + display 两层胶水**，不用碰编解码逻辑

如果不想动 OpenCV，**现存 C API 在 Android（NDK）/ iOS（xcode toolchain）下编译也是可行的**，只是 APK/IPA 会大一些。你有试过交叉编译它吗？

---

