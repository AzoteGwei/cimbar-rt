var Main = function () {

  var _interval = 66;
  var _colorBalance = false;

  var _pause = 0;
  var _showStats = false;
  var _counter = 0;
  var _renderTime = 0;

  var _lastFrame = 0;
  var _wakeLock = undefined;

  var _encoder = 0;
  var _imageW = 0;
  var _imageH = 0;

  var CIMBAR_CFG_PRESET = 9;
  var CIMBAR_CFG_IMAGE_SIZE_X = 7;
  var CIMBAR_CFG_IMAGE_SIZE_Y = 8;

  function toggleFullscreen() {
    if (document.fullscreenElement) {
      return document.exitFullscreen();
    }
    else {
      return document.documentElement.requestFullscreen();
    }
  }

  function copyToWasmHeap(abuff) {
    const dataPtr = Module._malloc(abuff.length);
    const wasmData = new Uint8Array(Module.HEAPU8.buffer, dataPtr, abuff.length);
    wasmData.set(abuff);
    return wasmData;
  }

  function importFile(file) {
    Main.encode_init(file.name);

    let reader = new FileReader();
    reader.onload = function (event) {
      const uint8View = new Uint8Array(event.target.result);
      const dataPtr = Module._malloc(uint8View.length);
      const wasmData = new Uint8Array(Module.HEAPU8.buffer, dataPtr, uint8View.length);
      wasmData.set(uint8View);

      const wasmFn = copyToWasmHeap(new TextEncoder("utf-8").encode(file.name));
      var res = Module._cimbar_encoder_set_input(_encoder, dataPtr, uint8View.length, wasmFn.byteOffset);
      Module._free(wasmFn.byteOffset);
      Module._free(dataPtr);
      if (res < 0) {
        console.error("cimbar_encoder_set_input failed: " + res);
        return;
      }

      var wPtr = Module._malloc(8);
      Module._cimbar_encoder_get_config(_encoder, CIMBAR_CFG_IMAGE_SIZE_X, wPtr);
      Module._cimbar_encoder_get_config(_encoder, CIMBAR_CFG_IMAGE_SIZE_Y, wPtr + 4);
      _imageW = Module.HEAPU32[wPtr >> 2];
      _imageH = Module.HEAPU32[(wPtr + 4) >> 2];
      Module._free(wPtr);

      var canvas = document.getElementById('canvas');
      canvas.width = _imageW;
      canvas.height = _imageH;

      Main.setActive();
      console.log("encoding " + file.name + " (" + uint8View.length + " bytes, image " + _imageW + "x" + _imageH + ")");
    };
    reader.readAsArrayBuffer(file);
  }

  return {
    init: function (canvas) {
      Main.setMode('B');
    },

    resize: function () {
      var canvas = document.getElementById('canvas');
      var width = window.innerWidth - 10;
      var height = window.innerHeight - 10;
      Main.alignInvisibleClick(canvas);
      Main.checkNavButtonOverlap();
    },

    toggleFullscreen: function () {
      toggleFullscreen().then(Main.resize);
      Main.togglePause(true);
    },

    togglePause: function (pause) {
      if (pause === undefined) {
        pause = !Main.isPaused();
      }
      _pause = pause ? 15 : 0;
    },

    isPaused: function () {
      return _pause > 0;
    },

    scaleCanvas: function (canvas, width, height) {
      canvas.style.width = width + "px";
      canvas.style.height = height + "px";
    },

    alignInvisibleClick: function (canvas) {
      canvas = canvas || document.getElementById('canvas');
      var cpos = canvas.getBoundingClientRect();
      var invisible_click = document.getElementById("invisible_click");
      invisible_click.style.width = canvas.style.width;
      invisible_click.style.height = canvas.style.height;
      invisible_click.style.top = cpos.top + "px";
      invisible_click.style.left = cpos.left + "px";
      invisible_click.style.zoom = canvas.style.zoom;
    },

    encode_init: function (filename) {
      if (_encoder) {
        Module._cimbar_encoder_destroy(_encoder);
      }
      _encoder = Module._cimbar_encoder_create(0);

      Main.setTitle(filename);
      Main.setHTML("current-file", filename);
    },

    prevent_sleep: async function () {
      if (_wakeLock) {
        return;
      }
      const requestWakeLock = async () => {
        try {
          _wakeLock = await navigator.wakeLock.request('screen');
          console.log('got wake lock!');
          _wakeLock.addEventListener('release', () => {
            _wakeLock = undefined;
          });
        } catch (err) { }
      };
      requestWakeLock();
    },

    dragDrop: function (event) {
      const files = event.dataTransfer.files;
      if (files && files.length === 1) {
        importFile(files[0]);
      }
    },

    checkNavButtonOverlap: function () {
      var nav = document.getElementById("nav-button");
      var navBounds = nav.getBoundingClientRect();
      var canvas = document.getElementById('canvas').getBoundingClientRect();
      if (navBounds.right > canvas.left && navBounds.bottom > canvas.top) {
        nav.classList.add("hide");
      }
      else {
        nav.classList.remove("hide");
      }
    },

    clickNav: function () {
      document.getElementById("nav-button").focus();
    },

    blurNav: function (pause) {
      if (pause === undefined) {
        pause = true;
      }
      document.getElementById("nav-button").blur();
      document.getElementById("nav-content").blur();
      document.getElementById("nav-find-file-link").blur();
      Main.togglePause(pause);
    },

    clickFileInput: function () {
      document.getElementById("file_input").click();
    },

    fileInput: function (ev) {
      var file = document.getElementById('file_input').files[0];
      if (file) {
        importFile(file);
      }
      Main.blurNav(false);
    },

    nextFrame: function (timestamp) {
      requestAnimationFrame(Main.nextFrame);
      let elapsed = timestamp - _lastFrame;
      if (!timestamp || elapsed < _interval) {
        return;
      }
      _lastFrame = timestamp;

      _counter += 1;
      if (_pause > 0) {
        _pause -= 1;
      }

      if (_encoder == 0 || _imageW == 0 || Main.isPaused()) {
        return;
      }

      var maxCells = _imageW * _imageH;
      var cellsPtr = Module._malloc(maxCells * 2 + 4);
      var numCellsPtr = cellsPtr + maxCells * 2;
      Module.HEAPU32[numCellsPtr >> 2] = 0;

      var result = Module._cimbar_encoder_encode_next_cells(_encoder, cellsPtr, maxCells, numCellsPtr);
      var numCells = Module.HEAPU32[numCellsPtr >> 2];

      if (result > 0 && numCells > 0) {
        var rgbaSize = _imageW * _imageH * 4;
        var rgbaPtr = Module._malloc(rgbaSize);
        var outWPtr = Module._malloc(8);

        Module._cimbar_render(cellsPtr, numCells, rgbaPtr, rgbaSize, outWPtr, outWPtr + 4);

        var canvas = document.getElementById('canvas');
        var ctx = canvas.getContext('2d');
        var imageData = ctx.createImageData(_imageW, _imageH);
        var pixelView = new Uint8Array(Module.HEAPU8.buffer, rgbaPtr, rgbaSize);
        imageData.data.set(pixelView);
        ctx.putImageData(imageData, 0, 0);

        Module._free(rgbaPtr);
        Module._free(outWPtr);

        if (_showStats) {
          _renderTime += elapsed;
          Main.setHTML("status", elapsed + " : " + result + " : " + Math.ceil(_renderTime / _counter));
        }
      }
      else if (result == 0) {
        console.log("encode complete, " + _counter + " frames");
        _encoder = 0;
        _imageW = 0;
      }

      Module._free(cellsPtr);

      if (!Main.isPaused() && _counter % 16 == 0) {
        setTimeout(Main.prevent_sleep, 0);
      }
    },

    setActive: function (active) {
      var invisi = document.getElementById("invisible_click");
      invisi.classList.remove("active");
      invisi.classList.add("active");
    },

    setMode: function (mode_str) {
      let modeVal = 68;
      if (mode_str == "4C") {
        modeVal = 4;
      }
      else if (mode_str == "Bu") {
        modeVal = 66;
      }
      else if (mode_str == "Bm") {
        modeVal = 67;
      }

      if (_encoder) {
        Module._cimbar_encoder_set_config(_encoder, CIMBAR_CFG_PRESET, modeVal);
      }

      var nav = document.getElementById("nav-container");
      if (modeVal == 4) {
        nav.classList.remove("mode-b");
        nav.classList.add("mode-4c");
        nav.classList.remove("mode-b");
        nav.classList.remove("mode-bm");
        nav.classList.remove("mode-bu");
      } else if (modeVal == 66) {
        nav.classList.add("mode-bu");
        nav.classList.remove("mode-b");
        nav.classList.remove("mode-bm");
        nav.classList.remove("mode-4c");
      } else if (modeVal == 67) {
        nav.classList.add("mode-bm");
        nav.classList.remove("mode-b");
        nav.classList.remove("mode-bu");
        nav.classList.remove("mode-4c");
      } else if (modeVal == 68) {
        nav.classList.add("mode-b");
        nav.classList.remove("mode-bm");
        nav.classList.remove("mode-bu");
        nav.classList.remove("mode-4c");
      } else {
        nav.classList.remove("mode-b");
        nav.classList.remove("mode-bm");
        nav.classList.remove("mode-bu");
        nav.classList.remove("mode-4c");
      }
    },

    setFPS: function (val) {
      if (!val) {
        return;
      }
      _interval = Math.floor(1000 / val);
    },

    setHTML: function (id, msg) {
      document.getElementById(id).innerHTML = msg;
    },

    setTitle: function (msg) {
      document.title = "Cimbar: " + msg;
    }
  };
}();

window.addEventListener('keydown', function (e) {
  e = e || event;
  if (e.target instanceof HTMLBodyElement) {
    if (e.key == 'Enter' || e.keyCode == 13 ||
      e.key == 'Tab' || e.keyCode == 9 ||
      e.key == 'Space' || e.keyCode == 32
    ) {
      Main.clickNav();
      e.preventDefault();
    }
    else if (e.key == 'Backspace' || e.keyCode == 8) {
      Main.togglePause(true);
      e.preventDefault();
    }
  }
  else {
    if (e.key == 'Escape' || e.keyCode == 27 ||
      e.key == 'Backspace' || e.keyCode == 8 ||
      e.key == 'End' || e.keyCode == 35 ||
      e.key == 'Home' || e.keyCode == 36
    ) {
      Main.blurNav();
    }
    else if (e.key == 'Tab' || e.keyCode == 9 ||
      e.key == 'ArrowDown' || e.keyCode == 40
    ) {
      var nav = document.getElementById('nav-button');
      var links = document.getElementById('nav-content').getElementsByTagName('a');
      if (nav.classList.contains('attention')) {
        nav.classList.remove('attention');
        links[0].classList.add('attention');
        return;
      }
      for (var i = 0; i < links.length; i++) {
        if (links[i].classList.contains('attention')) {
          var next = i + 1 == links.length ? nav : links[i + 1];
          links[i].classList.remove('attention');
          next.classList.add('attention');
          break;
        }
      }
    }
    else if (e.key == 'ArrowUp' || e.keyCode == 38) {
      var nav = document.getElementById('nav-button');
      var links = document.getElementById('nav-content').getElementsByTagName('a');
      if (nav.classList.contains('attention')) {
        nav.classList.remove('attention');
        links[links.length - 1].classList.add('attention');
        return;
      }

      for (var i = 0; i < links.length; i++) {
        if (links[i].classList.contains('attention')) {
          var next = i == 0 ? nav : links[i - 1];
          links[i].classList.remove('attention');
          next.classList.add('attention');
          break;
        }
      }
    }
    else if (e.key == 'Enter' || e.keyCode == 13 ||
      e.key == ' ' || e.keyCode == 32
    ) {
      var nav = document.getElementById('nav-button');
      if (nav.classList.contains('attention')) {
        Main.blurNav();
        return;
      }
      var links = document.getElementById('nav-content').getElementsByTagName('a');
      for (var i = 0; i < links.length; i++) {
        if (links[i].classList.contains('attention')) {
          links[i].click();
        }
      }
    }
  }
}, true);

window.addEventListener("touchstart", function (e) {
  e = e || event;
  Main.togglePause(true);
}, false);

window.addEventListener("touchend", function (e) {
  e = e || event;
  Main.togglePause(false);
}, false);

window.addEventListener("touchcancel", function (e) {
  e = e || event;
  Main.togglePause(false);
}, false);

window.addEventListener("dragover", function (e) {
  e = e || event;
  e.preventDefault();

  document.body.style["opacity"] = 0.5;
}, false);

window.addEventListener("dragleave", function (e) {
  e = e || event;
  e.preventDefault();

  document.body.style["opacity"] = 1.0;
}, false);

window.addEventListener("drop", function (e) {
  e = e || event;
  e.preventDefault();
  e.stopPropagation();
  Main.dragDrop(e);
  document.body.style["opacity"] = 1.0;
}, false);

window.addEventListener('resize', () => {
  Main.resize();
});
