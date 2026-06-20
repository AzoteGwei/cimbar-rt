var Recv = function () {

  var _counter = 0;
  var _recentDecode = -1;
  var _recentExtract = -1;
  var _renderTime = 0;
  var _captureNextFrame = 0;

  var _watchmanEnabled = 0;
  var _watchmanLastSeen = 1;

  var _video = 0;
  var _workers = [];
  var _nextWorker = 0;
  var _workerReady;
  var _framesInFlight = 0;
  var _supportedFormats = ["NV12", "I420"];

  var _mode = 0;

  function _toggleFullscreen() {
    if (document.fullscreenElement) {
      return document.exitFullscreen();
    }
    else {
      return document.documentElement.requestFullscreen();
    }
  }

  function isIOS() {
    const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent) && !window.MSStream;
    const isAppleDevice = navigator.userAgent.includes('Macintosh');
    const isTouchScreen = navigator.maxTouchPoints >= 1;
    return isIOS || (isAppleDevice && isTouchScreen);
  }

  function _getModeAspectRatio(mode) {
    switch (mode) {
      case 66: return 1.1516;
      case 67: return 1.413;
      default: return 1.0;
    }
  }

  function _updateCrosshairPositions() {
    if (!_video || !_video.videoWidth || !_video.videoHeight)
      return;

    var modeAspect = _getModeAspectRatio(_mode);

    var windowW = window.innerWidth;
    var windowH = window.innerHeight;
    var camAspect = _video.videoWidth / _video.videoHeight;
    var windowAspect = windowW / windowH;

    var vidW = windowW;
    var vidH = windowH;
    if (camAspect > windowAspect)
      vidH = vidW / camAspect;
    else
      vidW = vidH * camAspect;

    var offsetY;
    var offsetX;
    if (windowH > windowW) {
      offsetY = (windowH - (vidW * modeAspect)) / 2;
      offsetX = (windowW - vidW) / 2;
    }
    else {
      offsetY = (windowH - vidH) / 2;
      offsetX = (windowW - (vidH * modeAspect)) / 2;
    }

    var xh1 = document.getElementById("crosshair1");
    var xh2 = document.getElementById("crosshair2");
    xh1.style.top = offsetY + "px";
    xh1.style.right = offsetX + "px";
    xh2.style.bottom = offsetY + "px";
    xh2.style.left = offsetX + "px";
  }

  return {
    init: function (video, num_workers) {
      Recv.init_ww(num_workers);
      Recv.init_video(video);
    },

    set_error: function (msg) {
      Recv.set_HTML('errorbox', msg);
      return false;
    },

    ww_ready: new Promise(resolve => {
      _workerReady = resolve;
    }),

    frames_in_flight_incr: function () {
      _framesInFlight += 1;
      var el = document.getElementById('framesInFlight');
      if (el) el.innerHTML = _framesInFlight;
    },

    frames_in_flight_decr: function () {
      _framesInFlight -= 1;
      var el = document.getElementById('framesInFlight');
      if (el) el.innerHTML = _framesInFlight;
    },

    init_ww: function (num_workers) {
      _workers = [];
      for (let i = 0; i < num_workers; i++) {
        _workers.push(new Worker('recv-worker.js'));

        _workers[i].onmessage = (event) => {
          Recv.on_decode(i, event.data);
        };

        _workers[i].onerror = (error) => {
          console.error('Worker' + i + ' error:', error);
        };
      }
    },

    init_video: function (video) {
      _video = video;
      window.addEventListener('resize', _updateCrosshairPositions);

      var constraints = {
        audio: false,
        video: {
          width: { min: 720, ideal: 1920 },
          height: { min: 720, ideal: 1080 },
          aspectRatio: matchMedia('all and (orientation:landscape)').matches ? 16 / 9 : 9 / 16,
          facingMode: 'environment',
          exposureMode: 'continuous',
          focusMode: 'continuous',
          frameRate: { ideal: 15 },
        }
      };

      if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
        return Recv.set_error('mediaDevices not supported? :(');
      }

      navigator.mediaDevices.getUserMedia(constraints)
        .then(localMediaStream => {
          if ('srcObject' in video) {
            video.srcObject = localMediaStream;
          } else {
            video.src = URL.createObjectURL(localMediaStream);
          }
          video.play();
          video.requestVideoFrameCallback(Recv.on_frame);
        })
        .catch(err => {
          console.error(`OH NO!!!!`, err);
          Recv.set_error("Failed to initialize camera. " + err);
          Recv.set_HTML("crosshair1", "Failed to initialize camera. " + err);
        });
    },

    watch_for_camera_pause: function () {
      if (_watchmanEnabled) {
        return;
      }
      _watchmanEnabled = true;

      if (!isIOS()) {
        return;
      }

      setInterval(Recv.restart_paused_camera, 1000);
    },

    restart_paused_camera: function () {
      if (!_video) {
        return;
      }

      if (_counter > _watchmanLastSeen) {
        _watchmanLastSeen = _counter;
        return;
      }

      Recv.init_video(_video);
    },

    on_decode: function (wid, data) {
      Recv.frames_in_flight_decr();

      if (data.ready) {
        if (_workerReady)
          _workerReady();
        return;
      }

      if (data.nodata) {
        _recentExtract = _counter;
        return;
      }
      if (data.failed_extract) {
        return;
      }

      if (data.error) {
        Recv.set_HTML("t" + wid, "error: " + data.res);
        return;
      }

      if (data.progress !== undefined) {
        _recentDecode = _counter;
        if (data.mode) Recv.setMode(data.mode);
        Recv.render_progress(data.progress);
        return;
      }

      if (data.complete) {
        _recentDecode = _counter;
        if (data.mode) Recv.setMode(data.mode);
        Recv.on_file_complete(data.buff, data.filename, data.fileSize);
        return;
      }

      if (data.res) {
        Recv.set_HTML("t" + wid, "msg is " + data.res);
      }
    },

    on_file_complete: function (buff, filename, fileSize) {
      console.log("file complete: " + filename + " (" + fileSize + " bytes)");
      Recv.set_HTML("tdec", "complete: " + filename + " (" + fileSize + " bytes)");

      // Trigger download via Zstd
      var blob = new Blob([buff], { type: 'application/octet-stream' });
      Zstd.download_blob(filename || (fileSize + ".bin"), blob);
    },

    on_frame: function (now, metadata) {
      _counter += 1;
      if (_workers.length == 0)
        return;
      if (_nextWorker >= _workers.length)
        _nextWorker = 0;

      Recv.update_visual_state();
      Recv.watch_for_camera_pause();

      const modeVals = [66, 68, 67, 4];

      var vf = undefined;
      if (_framesInFlight > 20) {
        console.log("stalling, worker queues are full");
      }
      else {
        Recv.frames_in_flight_incr();
        try {
          vf = new VideoFrame(_video, { timestamp: now });
          const width = vf.displayWidth;
          const height = vf.displayHeight;

          let vfparams = {};
          if (!_supportedFormats.includes(vf.format)) {
            vfparams.format = "RGBA";
          }
          const size = vf.allocationSize(vfparams);
          const buff = new Uint8Array(size);
          vf.copyTo(buff, vfparams);

          let format = vfparams.format || vf.format;
          if (format == "RGBA" && size != width * height * 4) {
            format = vf.format;
          }
          if (_captureNextFrame == 1) {
            _captureNextFrame = 0;
            Recv.download_bytes(buff, width + "x" + height + "x" + _counter + "." + format);
          }

          let mode = _mode || modeVals[_counter % modeVals.length];
          _workers[_nextWorker].postMessage({ type: 'proc', pixels: buff, format: format, width: width, height: height, mode: mode }, [buff.buffer]);
        } catch (e) {
          console.log(e);
        }
        _nextWorker += 1;
      }
      if (vf)
        vf.close();

      _video.requestVideoFrameCallback(Recv.on_frame);
    },

    captureFrame: function () {
      _captureNextFrame = 1;
      alert("about to capture!");
    },

    download_bytes: function (buff, name) {
      var blob = new Blob([buff], { type: 'application/octet-stream' });
      Zstd.download_blob(name, blob);
    },

    update_visual_state: function () {
      _updateCrosshairPositions();

      var xh1 = document.getElementById("crosshair1");
      var xh2 = document.getElementById("crosshair2");
      if (_recentDecode > 0 && _recentDecode + 30 > _counter) {
        xh1.classList.add("active_xhairs");
        xh1.classList.remove("scanning_xhairs");
        xh2.classList.add("active_xhairs");
        xh2.classList.remove("scanning_xhairs");
      }
      else if (_recentExtract > 0 && _recentExtract + 30 > _counter) {
        xh1.classList.add("scanning_xhairs");
        xh1.classList.remove("active_xhairs");
        xh2.classList.add("scanning_xhairs");
        xh2.classList.remove("active_xhairs");
      }
      else {
        xh1.classList.remove("active_xhairs");
        xh1.classList.remove("scanning_xhairs");
        xh2.classList.remove("active_xhairs");
        xh2.classList.remove("scanning_xhairs");
      }
    },

    render_progress: function (progress) {
      console.log("progress: " + progress + "%");
      Recv.set_HTML("tdec", "progress " + progress + "%");
      const progress_container = document.getElementById('progress_bars');
      const query = '#progress_bars > div[class="progress"]';
      const prev = document.querySelectorAll(query);

      // Ensure exactly one progress bar exists
      if (!prev || prev.length === 0) {
        var bar = document.createElement('div');
        bar.classList.add("progress");
        progress_container.appendChild(bar);
      } else if (prev.length > 1) {
        for (var i = 1; i < prev.length; i++) {
          prev[i].remove();
        }
      }

      bar = document.querySelector(query);
      if (bar) {
        bar.style.width = progress + "%";
      }
    },

    toggleFullscreen: function () {
      _toggleFullscreen();
    },

    showDebug: function () {
      document.getElementById("debug-button").focus();
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
    },

    setMode: function (modeVal) {
      const modeToString = {
        4: "4C",
        8: "8C",
        66: "Bu",
        67: "Bm",
        68: "B"
      };
      let modeStringToVal = {
        "Auto": 0
      };
      for (const val in modeToString) {
        modeStringToVal[modeToString[val]] = val;
      }

      if (modeVal in modeStringToVal) {
        modeVal = modeStringToVal[modeVal];
      }

      _mode = modeVal;

      if (_mode > 0) {
        var nav = document.getElementById("mode-val");
        nav.innerHTML = modeToString[_mode];
      }

      var nav = document.getElementById("nav-container");
      if (_mode == 0) {
        nav.classList.add("mode-auto");
        nav.classList.remove("mode-b");
      } else {
        nav.classList.add("mode-b");
        nav.classList.remove("mode-auto");
      }
    },

    set_HTML: function (id, msg, only_if_unset) {
      const elem = document.getElementById(id);
      if (!elem) return;
      if (only_if_unset && elem.innerHTML) {
        return;
      }
      elem.innerHTML = msg;
    },

    set_title: function (msg) {
      document.title = "Cimbar: " + msg;
    },

    set_video: function (video) {
      _video = video;
    }
  };
}();
