function itemChosen(item, className) {
  if (className === 'main-menu') {
    for (const node of document.getElementsByClassName('menu-item')) {
      node.classList.remove('chosen');
    }
  } else {
    for (const node of document.getElementsByClassName('menu-item')) {
      if (node.parentNode.className === className && node.id !== item.id) {
        node.classList.remove('chosen');
      }
    }
  }

  item.classList.add('chosen');
}

function hideSubMenu() {
  for (const node of document.getElementsByClassName('sub-menu')) {
    node.style.display = 'none';
  }
}

function hideMenuContent() {
  for (const content of document.getElementsByClassName('menu-content')) {
    content.style.display = 'none';
  }
}

function clickMainMenuVideo(item) {
  itemChosen(item, 'main-menu');
  hideMenuContent();
  hideSubMenu();
  document.getElementById('sub-menu-video').style.display = "block";
}

function clickMainMenuConfig(item) {
  itemChosen(item, 'main-menu');
  hideMenuContent();
  hideSubMenu();
  document.getElementById('sub-menu-config').style.display = 'block';
}

function clickMainMenuInfo(item) {
  itemChosen(item, 'main-menu');
  hideMenuContent();
  hideSubMenu();
  document.getElementById('sub-menu-info').style.display = 'block';
}

function clickSubMenuParam(item) {
  itemChosen(item, 'sub-menu');
  hideMenuContent();
  document.getElementById('menu-content-param').style.display = 'block';
}

function clickSubMenuSys(item) {
  itemChosen(item, 'sub-menu');
  hideMenuContent();
  document.getElementById('menu-content-sys').style.display = 'block';
}

function clickSubMenuNet(item) {
  itemChosen(item, 'sub-menu');
  hideMenuContent();
  document.getElementById('menu-content-net').style.display = 'block';
}

function clickSubMenuModbus(item) {
  itemChosen(item, 'sub-menu');
  hideMenuContent();
  document.getElementById('menu-content-modbus').style.display = 'block';
}

function clickSubMenuInfo(item) {
  itemChosen(item, 'sub-menu');
  hideMenuContent();
  document.getElementById('menu-content-info').style.display = 'block';
}


var request = new XMLHttpRequest();

function sysConfig() {
  if (request.status == 4) {
    
  }
}

function startWebSocket(ip, port) {
  var ws = new WebSocket('ws://192.168.245.128:9000');
  ws.binaryType = 'arraybuffer';

  var canvas = document.getElementById('canvas');

  ws.addEventListener('message', function(event) {
    // Width & height must be same as resolution of YUV frame
    // 宽高必须跟视频的分辨率保持一致
    var [width, height] = [640, 480];
    let uintArray = new Uint8Array(event.data)
    var renderContext = new Yuv2rgbPlayer(canvas);
    renderContext.renderFrame(uintArray, width, height, '422-uyvy');
  });
}