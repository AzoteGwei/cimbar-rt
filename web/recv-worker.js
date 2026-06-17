let _wasmInitialized = false;
let _decoder = 0;

var Module = {
  preRun: [],
  onRuntimeInitialized: function load_done_callback() {
    console.info("The Module is loaded and is accessible here", Module);
    _wasmInitialized = true;
    self.postMessage({ type: 'startWasm', ready: "ready!" });
  }
};

var RecvWorker = function () {

  function format_to_enum(fmt) {
    if (fmt == "RGB") return 1;
    if (fmt == "RGBA") return 2;
    if (fmt == "BGR") return 3;
    if (fmt == "BGRA") return 4;
    if (fmt == "GRAY") return 5;
    return 2; // default RGBA
  }

  return {
    on_frame: function (data) {
      const pixels = data.pixels;
      const format = data.format;
      const width = data.width;
      const height = data.height;
      const mode = data.mode;

      if (!_decoder) {
        _decoder = Module._cimbar_decoder_create(0);
        if (!_decoder) {
          self.postMessage({ error: true, res: "failed to create decoder" });
          return;
        }
      }

      if (mode) {
        Module._cimbar_decoder_set_config(_decoder, 9, mode); // CIMBAR_CFG_PRESET
      }

      var fmt = format_to_enum(format);

      // Copy pixels to WASM heap
      const imgPtr = Module._malloc(pixels.length);
      Module.HEAPU8.set(pixels, imgPtr);

      // Feed frame to decoder
      var res = Module._cimbar_decoder_fountain_feed(_decoder, imgPtr, pixels.length, width, height, fmt);
      Module._free(imgPtr);

      if (res < 0) {
        var errBuf = Module._malloc(256);
        var errLen = Module._cimbar_decoder_dump(_decoder, errBuf, 256);
        var errMsg = res + " ";
        if (errLen > 0) errMsg += Module.UTF8ToString(errBuf, errLen);
        Module._free(errBuf);
        self.postMessage({ error: true, res: errMsg });
        return;
      }

      // Check progress
      var progress = Module._cimbar_decoder_fountain_get_progress(_decoder);
      var isComplete = Module._cimbar_decoder_fountain_is_complete(_decoder);

      if (isComplete) {
        // Read filename
        var filename = "";
        var nameBuf = Module._malloc(256);
        var nameLen = Module._cimbar_decoder_fountain_get_filename(_decoder, nameBuf, 256);
        if (nameLen > 0) {
          filename = Module.UTF8ToString(nameBuf, nameLen);
        }
        Module._free(nameBuf);

        // Read file data
        var fileSize = Module._cimbar_decoder_fountain_get_filesize(_decoder);
        var dataBuf = Module._malloc(fileSize);
        var outLenPtr = Module._malloc(4);
        Module.HEAPU32[outLenPtr >> 2] = fileSize;

        var readRes = Module._cimbar_decoder_fountain_read(_decoder, dataBuf, outLenPtr);
        var bytesRead = Module.HEAPU32[outLenPtr >> 2];
        Module._free(outLenPtr);

        if (readRes > 0 && bytesRead > 0) {
          var data = new Uint8Array(Module.HEAPU8.buffer, dataBuf, bytesRead).slice();
          self.postMessage({ complete: true, buff: data, filename: filename, fileSize: fileSize, mode: mode }, [data.buffer]);
        } else {
          self.postMessage({ error: true, res: "fountain_read failed: " + readRes });
        }

        Module._free(dataBuf);
      } else {
        self.postMessage({ progress: progress, mode: mode });
      }
    },

    get_error: function () {
      if (!_decoder) return "";
      var buf = Module._malloc(256);
      var len = Module._cimbar_decoder_dump(_decoder, buf, 256);
      var msg = "";
      if (len > 0) {
        msg = Module.UTF8ToString(buf, len);
      }
      Module._free(buf);
      return msg;
    }
  };
}();

importScripts('cimbar_js.js');

self.onmessage = async (event) => {
  if (!_wasmInitialized) {
    console.log('we got no wasm :(');
    self.postMessage({ type: 'startWasm', error: "no wasm" });
    return;
  }

  try {
    RecvWorker.on_frame(event.data);
  } catch (ex) {
    console.log("unexpected error: " + ex);
    self.postMessage({ error: true, res: ex });
  }
};
