# Plan: 移除 GLFW 依赖，C-API 成为一等公民

## 架构变更

```
变更前:
  libcimbar ─┬── core (编解码)
             ├── api/cimbar_js.cpp ──> window_glfw ──> GLFW + OpenGL ES
             ├── api/cimbar_recv_js.cpp ──> 纯 decode API
             └── api/cimbar_api.cc ──> 纯 C-API

变更后:
  libcimbar ─┬── core (编解码)
             └── api/cimbar_api.cc ──> include/libcimbar/cimbar.h [唯一公共 API]

  WASM 导出: C-API 编码器/解码器函数（无窗口管理）
  JS 编码端: 调用编码器 C-API，获取 symbol/color 矩阵，WebGL 渲染
  JS 解码端: 调用解码器 API，camera → 像素 → 扫描解码
```

---

## 第一部分：C++ 侧删除与修改

### 删除文件（14 个）

**GLFW/OpenGL 显示层（5 个）：**
- `src/support/display/window_glfw.h`
- `src/support/display/gl_2d_display.h`
- `src/support/display/gl_shader.h`
- `src/support/display/gl_program.h`
- `src/support/display/mat_to_gl.h`

**旧 JS API（4 个）：**
- `src/api/cimbar_js.h`
- `src/api/cimbar_js.cpp`
- `src/api/cimbar_recv_js.h`
- `src/api/cimbar_recv_js.cpp`

**依赖窗口的工具（3 个）：**
- `tools/cimbar-send/send.cpp`
- `tools/cimbar-recv/recv.cpp`
- `tools/cimbar-recv2/recv2.cpp`

**依赖旧 JS API 的测试（2 个）：**
- `test/api/cimbar_jsTest.cpp`
- `test/api/cimbar_recv_jsTest.cpp`

### 修改构建系统（5 个文件）

**`meson.build`（第 124-125 行）：**
删除 `glfw_dep = dependency('glfw3', required: false)` 和 `gl_dep = dependency('gl', required: false)`

**`src/api/meson.build`：**
- 从 `api_sources` 移除 `cimbar_js.cpp`、`cimbar_js.h`、`cimbar_recv_js.cpp`、`cimbar_recv_js.h`
- 从 `api_lib` 的 `dependencies` 移除 `glfw_dep`、`gl_dep`
- 重写 WASM 部分：
  - 源文件改为仅 `cimbar_api.cc`
  - 移除 `-s USE_GLFW=3`
  - 移除 `-s USE_WEBGL2=1`、`-s MIN_WEBGL_VERSION=2`、`-s MAX_WEBGL_VERSION=2`（编码器不再需要 WebGL）
  - 重写 `wasm_exported_fns` 为 C-API 导出函数（见下方）
  - 移除 `--bind`（不再需要 Emscripten bindings）

**`src/support/meson.build`（第 17-20 行）：**
删除 `support_display_dep`

**`tools/meson.build`：**
- 删除 `cimbar_send`、`cimbar_recv`、`cimbar_recv2` 三个 executable 目标
- 删除 `recv_deps` 中的 `glfw_dep`、`gl_dep`

**`test/meson.build`（第 115-138 行）：**
从 `test_api_src` 移除 `cimbar_jsTest.cpp` 和 `cimbar_recv_jsTest.cpp`

### C++ 源码修改

**`src/api/cimbar_api.cc`：无需修改。** 已经是纯 C-API，零 GLFW 依赖。

---

## 第二部分：WASM 导出函数

### 编码器（新 C-API 导出）

| WASM 导出 | C-API 函数 | 用途 |
|-----------|-----------|------|
| `_cimbar_encoder_create` | `cimbar_encoder_create()` | 创建编码器实例 |
| `_cimbar_encoder_destroy` | `cimbar_encoder_destroy()` | 销毁编码器实例 |
| `_cimbar_encoder_set_config` | `cimbar_encoder_set_config()` | 设置 PRESET/COMPRESSION |
| `_cimbar_encoder_get_config` | `cimbar_encoder_get_config()` | 查询 IMAGE_SIZE_X/Y |
| `_cimbar_encoder_set_input` | `cimbar_encoder_set_input()` | 一次性传入文件数据 |
| `_cimbar_encoder_encode_next_cells` | `cimbar_encoder_encode_next_cells()` | **获取 symbol/color 矩阵**（非 RGBA） |
| `_cimbar_encoder_reset` | `cimbar_encoder_reset()` | 重置编码器 |
| `_cimbar_encoder_get_stats` | `cimbar_encoder_get_stats()` | 统计信息 |
| `_malloc` / `_free` | Emscripten 内置 | 内存管理 |

> **关键变更**：使用 `encode_next_cells` 而非 `encode_next`。返回 `cimbar_cell_t` 数组（每个 cell 2 字节：symbol + color），而非 RGBA 像素。JS 端用 WebGL 实例化渲染这些 tile，实现硬件加速。

### 解码器（保留现有 `cimbard_*` 接口）

| WASM 导出 | 用途 |
|-----------|------|
| `_cimbard_configure_decode` | 配置解码模式 |
| `_cimbard_get_bufsize` | 缓冲区大小 |
| `_cimbard_scan_extract_decode` | 扫描+提取+解码 |
| `_cimbard_fountain_decode` | Fountain 解码 |
| `_cimbard_get_filesize` | 文件大小 |
| `_cimbard_get_filename` | 文件名 |
| `_cimbard_get_decompress_bufsize` | 解压缓冲区大小 |
| `_cimbard_decompress_read` | 解压读取 |
| `_cimbard_get_report` | 报告/进度 |
| `_malloc` / `_free` | 内存管理 |

> 解码器保留 `cimbard_*` 接口，因为 `recv-worker.js` 的提取+解码流水线与之匹配良好。未来可统一到 `cimbar_decoder_*` C-API。

---

## 第三部分：JavaScript 胶水重写

### `web/main.js`（编码器端）— 重写

**旧流程（GLFW 窗口 + RGBA）：**
```
cimbare_configure(mode)
→ cimbare_init_window() [GLFW]
→ cimbare_init_encode()
→ [循环] cimbare_encode(chunk)
→ cimbare_render() [WebGL + GLFW]
→ cimbare_next_frame()
```

**新流程（C-API + symbol/color 矩阵 + WebGL 渲染）：**
```
cimbar_encoder_create()
→ cimbar_encoder_set_config(PRESET, mode)
→ cimbar_encoder_set_input(fileData, fileLen, filename)
→ cimbar_encoder_get_config(IMAGE_SIZE_X/Y) → 获取尺寸
→ [循环] cimbar_encoder_encode_next_cells(cellsBuf, maxCells, &numCells)
→ JS 端 WebGL 实例化渲染 cells（tile-based GPU 加速）
→ cimbar_encoder_reset()
```

**具体改动：**

1. **文件读取** — 一次性读取整个文件（`FileReader.readAsArrayBuffer`），调用 `cimbar_encoder_set_input` 传入完整数据。分片读取的 fountain code bug 后续修复。

2. **帧生成** — 调用 `cimbar_encoder_encode_next_cells` 获取 cell 矩阵（`cimbar_cell_t[]`），每个 cell 包含 `(symbol, color)`。

3. **渲染** — JS 端实现 WebGL 渲染器：
   - 预加载 tile 纹理图集（symbol 图片）
   - 用 cell 的 color 值做 tint
   - 通过 instanced drawing 批量渲染所有 tiles
   - 替代旧的 `cimbare_render()`（GLFW + OpenGL ES）

4. **Canvas** — 保持 WebGL 上下文（用于 tile 渲染），但不再依赖 GLFW/OpenGL ES 的 C++ 端渲染

5. **配置** — `setMode()` 调用 `cimbar_encoder_set_config(CIMBAR_CFG_PRESET, modeVal)` + `cimbar_encoder_get_config(CIMBAR_CFG_IMAGE_SIZE_X/Y)`

6. **移除的调用**：`cimbare_render`、`cimbare_rotate_window`、`cimbare_auto_scale_window`、`cimbare_init_window`、`cimbare_get_frame_buff`

### `web/recv-worker.js`（解码器端）— 无变更

### `web/recv.js`（解码器端）— 无变更

### `web/zstd.js` — 无变更

### `web/index.html` — 小改动

- 移除 WebGL 检查逻辑（`check_GL_enabled`）— 编码器端仍用 WebGL，但由 JS 管理
- Canvas 保持 WebGL 上下文（用于 tile 渲染）

---

## 第四部分：CI、打包、文档

| 文件 | 变更 |
|------|------|
| `.github/workflows/ci.yml:48,93` | 移除 `libglfw3-dev` |
| `scripts/package-portable-linux.sh:21` | 移除 `libglfw3-dev` |
| `examples/build-demo.sh:30,42` | 从 `PKGS` 移除 `glfw3` 和 `gl` |
| `README.md` | 从依赖列表移除 GLFW |
| `README.zh-cn.md` | 同步更新 |

---

## 不变的部分

- `cimbar`（CLI 无头编解码）
- `cimbar_extract`
- `include/libcimbar/cimbar.h`（公共 C-API 头文件）
- `include/libcimbar/cimbar_export.h`
- 所有非 API 测试套件（support、imgproc、core、pipeline）
- WASM 构建流程（仍通过 Emscripten）
- `web/recv.html`、`web/recv-sw.js`、`web/sw.js`、`web/pwa.json` 等静态资源

---

## 实施顺序

1. **Phase 1**：删除 C++ 文件 + 修改构建系统（meson.build）
2. **Phase 2**：验证 native 构建通过（`meson setup build && ninja -C build`）
3. **Phase 3**：重写 `web/main.js`（编码器端 JS 胶水）
4. **Phase 4**：更新 CI/打包/文档
5. **Phase 5**：验证测试通过（`meson test -C build`）

---

## 风险与注意事项

1. **Tile 纹理图集** — `encode_next_cells` 返回 symbol index + color index，JS 端需要对应的 tile 图集资源。这些 tile 图片可能需要从现有的 symbol 图片生成，或者在 WASM 构建时打包。

2. **WebGL 渲染器复杂度** — 替代 GLFW 的 JS 端 WebGL 渲染器需要一定工作量。可以先用简单的 2D Canvas 实现（`putImageData`），后续再优化为 WebGL instanced rendering。

3. **WASM 文件大小** — 移除 GLFW/OpenGL ES 依赖后，WASM 二进制应该会变小。

4. **测试覆盖** — 删除了两个 API 测试文件后，`test_api` 套件只剩 `cimbar_api_test.cpp`。需要确认它仍能正常链接和运行。
