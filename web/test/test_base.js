QUnit.module("base");
QUnit.config.reorder = false;
QUnit.config.testTimeout = 10000;

let _zstdCalls = [];

Zstd.download_blob = function (name, blob) {
  console.log("in fake download blob for " + name);
  _zstdCalls.push({ download_blob: [name, blob.size] });
};

function wait_for(assert, block) {
  var done = assert.async();
  return new Promise(resolve => {
    const check = () => {
      var res;
      try {
        res = block();
      } catch (ex) {
        assert.ok(false, ex);
      }
      if (res) {
        done();
        resolve(res);
      } else {
        requestAnimationFrame(check);
      }
    };
    check();
  });
}

async function load_image(num) {
  const img = document.getElementById('example_frame' + num);
  const imageBitmap = await window.createImageBitmap(img);
  imageBitmap.requestVideoFrameCallback = function () { };

  try {
    Recv.init_video(imageBitmap);
  } catch (ex) { }
}

QUnit.testStart(async function (details) {
  await WAIT_UNTIL_READY;
  await Recv.ww_ready;
});

QUnit.testDone(function (details) {
  _zstdCalls = [];
});

QUnit.test("stable decode", async function (assert) {
  await load_image(0);
  Recv.on_frame(0, '');

  const progress_container = document.getElementById('progress_bars');
  const query = '#progress_bars > div[class="progress"]';

  const pro0 = await wait_for(assert, () => {
    return document.querySelector(query);
  });
  var w0 = parseFloat(pro0.style.width);
  assert.ok(w0 > 0 && w0 < 100, "first frame progress: " + w0 + "%");

  // test mode autodetect
  const navcont = await wait_for(assert, () => {
    return document.querySelector('#nav-container');
  });
  assert.equal("mode-b", navcont.classList.toString());

  const modebutton = await wait_for(assert, () => {
    return document.querySelector('#mode-val');
  });
  assert.equal("B", modebutton.textContent);

  // next frame
  await load_image(1);
  Recv.on_frame(0, '');

  const pro1 = await wait_for(assert, () => {
    var bar = document.querySelector(query);
    if (bar && parseFloat(bar.style.width) > w0) return bar;
    return null;
  });
  var w1 = parseFloat(pro1.style.width);
  assert.ok(w1 > w0, "progress increased: " + w0 + "% -> " + w1 + "%");

  await load_image(2);
  Recv.on_frame(0, '');

  const pro2 = await wait_for(assert, () => {
    var bar = document.querySelector(query);
    if (bar && parseFloat(bar.style.width) > w1) return bar;
    return null;
  });
  var w2 = parseFloat(pro2.style.width);
  assert.ok(w2 > w1, "progress increased: " + w1 + "% -> " + w2 + "%");
  assert.deepEqual(_zstdCalls, []);

  // last one
  await load_image(3);
  Recv.on_frame(0, '');

  const pro3 = await wait_for(assert, () => {
    var bar = document.querySelector(query);
    if (bar && parseFloat(bar.style.width) >= 100) return bar;
    return null;
  });
  assert.equal(pro3.style.width, "100%");

  const numCalls = await wait_for(assert, () => {
    return _zstdCalls.length > 0;
  });
  assert.ok(_zstdCalls.length > 0, "Zstd.download_blob was called");
  assert.ok(_zstdCalls[0].download_blob[1] > 0, "file size > 0");
});
