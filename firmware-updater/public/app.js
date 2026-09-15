import { ESPLoader, Transport } from './esptool-js/bundle.js';

// ---------- DOM ----------
const fwInfo = document.getElementById('fw-info');
const eraseAll = document.getElementById('erase-all');
const wifiSsid = document.getElementById('wifi-ssid');
const wifiPass = document.getElementById('wifi-pass');
const wifiOverride = document.getElementById('wifi-override');
const wifiStatusEl = document.getElementById('wifi-status');
const btnConnect = document.getElementById('btn-connect');
const btnFlash = document.getElementById('btn-flash');
const btnProvision = document.getElementById('btn-provision');
const btnClearWifi = document.getElementById('btn-clear-wifi');
const statusEl = document.getElementById('status');
const progressWrap = document.getElementById('progress-wrap');
const progressBar = document.getElementById('progress-bar');
const progressText = document.getElementById('progress-text');
const browserWarn = document.getElementById('browser-warn');
const logEl = document.getElementById('log');
const modal = document.getElementById('modal');
const modalTitle = document.getElementById('modal-title');
const modalMsg = document.getElementById('modal-msg');
const modalOk = document.getElementById('modal-ok');

// ---------- constants ----------
const CHIP_PATTERN = { esp32c3: /esp32-c3/i };
const SERIAL_BAUD = 115200;
const PING_TIMEOUT_MS = 15000;
const CONNECT_TIMEOUT_MS = 45000;
const PROV_REOPEN_DELAY_MS = 500;

// ---------- state ----------
let manifest = null;
let esploader = null;
let transport = null;
let port = null;
let connected = false;
let flashing = false;
let provisionBusy = false;
let pendingProvision = false;   // 端口失效后等待用户重新选择串口继续配网

// 串口配网 I/O（raw serial，与 esptool 通道分离）
let provReader = null;
let provWriter = null;
let serialLineBuf = '';
let provEvents = [];
let provWaiters = [];
let lastStatus = null;
let pumping = false;

// ---------- helpers ----------
function log(msg) {
  const t = new Date().toLocaleTimeString('zh-CN', { hour12: false });
  logEl.textContent += `[${t}] ${msg}\n`;
  logEl.scrollTop = logEl.scrollHeight;
}
function setStatus(msg, cls) {
  statusEl.textContent = msg;
  statusEl.className = 'status' + (cls ? ' ' + cls : '');
}
function setProgress(pct, text) {
  progressWrap.classList.remove('hidden');
  progressBar.style.width = Math.min(100, Math.max(0, pct)) + '%';
  progressText.textContent = text || (pct.toFixed(1) + '%');
}
function showModal(title, msg) {
  modalTitle.textContent = title;
  modalMsg.textContent = msg;
  modal.classList.remove('hidden');
}
modalOk.addEventListener('click', () => modal.classList.add('hidden'));

async function sha256Hex(buf) {
  const h = await crypto.subtle.digest('SHA-256', buf);
  return [...new Uint8Array(h)].map(b => b.toString(16).padStart(2, '0')).join('');
}

// ---------- manifest ----------
async function loadManifest() {
  const path = '/firmware/manifest.json';
  try {
    const res = await fetch(path, { cache: 'no-store' });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    manifest = await res.json();
  } catch (e) {
    manifest = null;
    fwInfo.innerHTML = '<span style="color:#d63031">未找到固件包（' + path + '）。' +
      '请维护者先运行 build_package.cmd 生成固件包后，再重新打开本工具。</span>';
    return;
  }
  const credBadge = manifest.credentials === 'empty'
    ? '<span class="cred-badge">本固件不含 WiFi 凭据</span>' : '';
  let rows = '';
  for (const f of manifest.files) {
    rows += `<tr><td class="mono">${f.name}</td><td class="mono">0x${f.offset.toString(16)}</td>` +
            `<td>${(f.size / 1024).toFixed(1)} KB</td></tr>`;
  }
  fwInfo.innerHTML =
    `<div class="fw-ver">版本 ${manifest.version}${credBadge}</div>` +
    `<table class="files"><tr><th>文件</th><th>偏移</th><th>大小</th></tr>${rows}</table>`;
  log(`固件包已加载：v${manifest.version}（${manifest.flash.mode} / ${manifest.flash.freq} / ${manifest.flash.size}）`);
}

// ---------- esptool 连接 / 烧录 ----------
btnConnect.addEventListener('click', async () => {
  if (pendingProvision) {
    // 端口失效（C3 重新枚举）：用户手势重新申请串口后继续配网
    try {
      const p = await navigator.serial.requestPort();
      port = p;
      pendingProvision = false;
      btnConnect.textContent = '连接设备';
      setStatus('串口已重选，继续配网…', 'ok');
      await runProvision();
    } catch (e) {
      log('重新选择串口被取消');
    }
    return;
  }
  if (connected) return;
  if (!('serial' in navigator)) { browserWarn.classList.remove('hidden'); return; }
  try {
    btnConnect.disabled = true;
    setStatus('请选择设备串口…');
    port = await navigator.serial.requestPort();
    log('串口已选择，正在连接…');
    transport = new Transport(port, true);
    esploader = new ESPLoader({
      transport,
      baudrate: 921600,
      terminal: {
        clean() {},
        write(data) { if (data) logEl.textContent += data; logEl.scrollTop = logEl.scrollHeight; },
        writeLine(data) { log(String(data).trimEnd()); },
      },
      debugLogging: false,
    });
    setStatus('正在握手（设备会自动复位进入下载模式）…');
    const chip = await esploader.main();
    log('检测到芯片: ' + chip);
    const pattern = (manifest && CHIP_PATTERN[manifest.chip]) || CHIP_PATTERN.esp32c3;
    if (!pattern.test(chip)) {
      throw new Error('芯片不是 ' + (manifest ? manifest.chip : 'ESP32-C3') + '（检测到 ' + chip + '），已取消');
    }
    connected = true;
    setStatus('已连接 ' + chip + '，可以开始升级', 'ok');
    btnFlash.disabled = false;
    btnConnect.textContent = '已连接';
  } catch (e) {
    log('连接失败: ' + (e && e.message ? e.message : e));
    setStatus('连接失败，请重试（保持设备接好 USB）', 'err');
    try { if (transport) await transport.disconnect(); } catch (_) {}
    transport = null; esploader = null;
    btnConnect.disabled = false;
  }
});

btnFlash.addEventListener('click', async () => {
  if (!connected || flashing) return;
  if (!manifest) {
    setStatus('未找到固件包（manifest.json），请先运行 build_package.cmd', 'err');
    return;
  }
  flashing = true;
  btnFlash.disabled = true;
  btnConnect.disabled = true;
  btnProvision.disabled = true;
  btnClearWifi.disabled = true;
  try {
    setStatus('正在校验固件完整性…');
    const fileArray = [];
    for (let i = 0; i < manifest.files.length; i++) {
      const f = manifest.files[i];
      setProgress(i / manifest.files.length * 5, `读取 ${f.name}…`);
      const res = await fetch('/firmware/' + f.name, { cache: 'no-store' });
      if (!res.ok) throw new Error('读取 ' + f.name + ' 失败 (HTTP ' + res.status + ')');
      const buf = await res.arrayBuffer();
      const hex = await sha256Hex(buf);
      if (hex !== f.sha256.toLowerCase()) {
        throw new Error(`${f.name} SHA256 校验失败（包已损坏），已拒绝烧录`);
      }
      log(`${f.name} 校验通过 (${(buf.byteLength / 1024).toFixed(1)} KB)`);
      fileArray.push({ data: new Uint8Array(buf), address: f.offset });
    }

    setStatus('正在升级，请勿断电、勿拔线…');
    log(`开始烧录（${manifest.flash.mode} / ${manifest.flash.freq} / ${manifest.flash.size}` +
        (eraseAll.checked ? '，全擦除' : '，保留配置') + '）…');
    await esploader.writeFlash({
      fileArray,
      flashMode: manifest.flash.mode,
      flashFreq: manifest.flash.freq,
      flashSize: manifest.flash.size,
      eraseAll: eraseAll.checked,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const base = fileIndex / fileArray.length * 90;
        const span = 1 / fileArray.length * 90;
        setProgress(base + written / total * span);
      },
    });
    log('写入完成，正在重启设备…');
    setProgress(100, '重启设备…');
    try { await esploader.after('hard_reset'); } catch (e) { log('复位提示: ' + e); }
    try { await transport.disconnect(); } catch (_) {}
    transport = null; esploader = null;
    connected = false;

    if (wifiSsid.value.trim().length === 0) {
      setProgress(100, '升级完成');
      setStatus('✅ 升级完成！设备已重启（未填写 WiFi，可用「仅配网」配置）', 'ok');
      showModal('✅ 升级完成', '固件已写入。未填写 WiFi 凭据，稍后可用「仅配网」配置。');
      log('========== 升级成功（未配网） ==========');
    } else {
      setStatus('固件写入完成，正在配网…', 'ok');
      log('========== 升级成功，开始配网 ==========');
      await sleep(PROV_REOPEN_DELAY_MS);
      await runProvision(true);
    }
  } catch (e) {
    log('升级失败: ' + (e && e.message ? e.message : e));
    setStatus('❌ 升级失败：' + (e && e.message ? e.message : e) + '（可重试；若变砖用 flash_cli.bat 恢复）', 'err');
  } finally {
    flashing = false;
    btnConnect.disabled = false;
    if (manifest) btnFlash.disabled = false;
    btnProvision.disabled = false;
    btnClearWifi.disabled = false;
  }
});

// ---------- 串口配网（raw serial） ----------
function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

function pushProvEvent(obj) {
  provEvents.push(obj);
  if (obj.evt === 'status') lastStatus = obj;
  provWaiters = provWaiters.filter(w => {
    if (w.types.includes(obj.evt)) { w.resolve(obj); return false; }
    return true;
  });
}

function waitForEvent(types, timeoutMs) {
  return new Promise(resolve => {
    const idx = provEvents.findIndex(e => types.includes(e.evt));
    if (idx >= 0) { const e = provEvents.splice(idx, 1)[0]; resolve(e); return; }
    const w = { types, resolve };
    provWaiters.push(w);
    setTimeout(() => {
      const i = provWaiters.indexOf(w);
      if (i >= 0) provWaiters.splice(i, 1);
      resolve(null);
    }, timeoutMs);
  });
}

async function pumpSerial() {
  if (pumping) return;
  pumping = true;
  const decoder = new TextDecoder();
  try {
    while (provReader) {
      const { value, done } = await provReader.read();
      if (done) break;
      const s = decoder.decode(value, { stream: true });
      for (const ch of s) {
        if (ch === '\n') {
          const line = serialLineBuf.trim();
          serialLineBuf = '';
          if (line.startsWith('@')) {
            try {
              const obj = JSON.parse(line.substring(1));
              if (obj && obj.evt) pushProvEvent(obj);
            } catch (_) { /* 忽略非协议行 */ }
          }
        } else {
          serialLineBuf += ch;
        }
      }
    }
  } catch (_) {}
  pumping = false;
}

async function sendCommand(cmd, expected, timeoutMs, payload) {
  const line = '@' + JSON.stringify(Object.assign({ cmd }, payload || {})) + '\n';
  log('→ ' + line.trim());
  await provWriter.write(new TextEncoder().encode(line));
  const resp = await waitForEvent(expected, timeoutMs);
  if (resp && resp.evt === 'error') {
    throw new Error('设备返回错误: ' + (resp.msg || 'unknown'));
  }
  return resp;
}

async function provisionOpenPort() {
  await provisionClosePort();
  try {
    await port.open({ baudRate: SERIAL_BAUD });
  } catch (e) {
    // C3 原生 USB 重新枚举：先尝试 getPorts() 重取
    log('原串口打开失败，尝试重新获取端口…');
    const ports = await navigator.serial.getPorts();
    if (ports.length === 0) {
      pendingProvision = true;
      btnConnect.textContent = '重新连接并配网';
      setStatus('设备已重新枚举，请点击「重新连接并配网」选择串口', 'err');
      throw new Error('未找到设备串口，请点击「重新连接并配网」');
    }
    port = ports[ports.length - 1];
    await port.open({ baudRate: SERIAL_BAUD });
    log('已通过 getPorts() 重新获取串口');
  }
  provReader = port.readable.getReader();
  provWriter = port.writable.getWriter();
  provEvents = [];
  lastStatus = null;
  pumpSerial();
}

async function provisionClosePort() {
  if (provReader) { try { await provReader.cancel(); } catch (_) {} provReader = null; }
  if (provWriter) { try { provWriter.releaseLock(); } catch (_) {} provWriter = null; }
  if (port) { try { await port.close(); } catch (_) {} }
  provEvents = [];
}

function renderWifiStatus(st) {
  if (!st) { wifiStatusEl.textContent = '固件内置凭据：—｜NVS 凭据：—｜覆盖：—｜WiFi 状态：—'; return; }
  const stateLabel = { provision: '配网等待', connecting: '连接中', connected: '已连接', failed: '失败', disconnected: '未连接' };
  let ip = '';
  if (st.state === 'connected' && st.ip) ip = '（' + st.ip + '）';
  wifiStatusEl.textContent =
    `固件内置凭据：${st.compile ? '有' : '无'}｜NVS 凭据：${st.nvs ? '有' : '无'}｜覆盖：${st.override ? '开' : '关'}` +
    `｜WiFi 状态：${(stateLabel[st.state] || st.state)}${ip}`;
}

async function runProvision(fromFlash) {
  if (provisionBusy) return;
  provisionBusy = true;
  btnProvision.disabled = true;
  btnClearWifi.disabled = true;
  btnFlash.disabled = true;
  try {
    await provisionOpenPort();

    // 1. 读取当前状态（eraseAll=false 且 NVS 已有凭据时提示可跳过）
    const st0 = await sendCommand('status', ['status'], PING_TIMEOUT_MS);
    renderWifiStatus(st0);
    if (st0) {
      if (!eraseAll.checked && st0.nvs) {
        log('提示：未勾选擦除且 NVS 已有 WiFi 凭据，可跳过配网（设备将使用 NVS 凭据）');
      }
      if (st0.compile && !wifiOverride.checked) {
        log('提示：固件含内置凭据且未勾选「覆盖」，本次配网仅临时生效，重启回内置');
      }
    }

    // 2. ping 握手
    const pong = await sendCommand('ping', ['pong'], PING_TIMEOUT_MS);
    if (!pong) throw new Error('设备无响应（pong 超时）');
    if (pong.proto !== 1) throw new Error('协议版本不兼容（proto=' + pong.proto + '）');
    log('设备就绪（fw=' + (pong.fw || '?') + '）');

    // 3. 下发凭据（可留空 → 仅握手，不写 NVS）
    let sentCreds = false;
    if (wifiSsid.value.trim().length > 0) {
      const saved = await sendCommand('wifi', ['wifi_saved'], PING_TIMEOUT_MS, {
        ssid: wifiSsid.value.trim(),
        pass: wifiPass.value,
        override: wifiOverride.checked,
      });
      if (!saved) throw new Error('设备未确认保存凭据（wifi_saved 超时）');
      sentCreds = true;
      log('凭据已保存（override=' + (wifiOverride.checked ? 'on' : 'off') + '）');
    } else {
      log('未填写 WiFi，仅查询状态');
    }

    // 4. 等待连接结果
    if (sentCreds || st0 === null || st0.state !== 'connected') {
      const evt = await waitForEvent(['wifi_connected', 'wifi_failed', 'error'], CONNECT_TIMEOUT_MS);
      if (evt && evt.evt === 'wifi_connected') {
        log('WiFi 已连接: ' + evt.ip);
        setStatus('✅ 配网成功（IP ' + evt.ip + '）', 'ok');
        showModal(fromFlash ? '✅ 固件已写入并配网成功' : '✅ 配网成功',
                  '设备已连接 WiFi（IP ' + evt.ip + '），可关闭窗口拔线。');
      } else if (evt && evt.evt === 'wifi_failed') {
        log('WiFi 连接失败（status=' + evt.status + '）');
        setStatus('⚠️ 凭据已保存但未连上（status=' + evt.status + '），可重试或检查 WiFi', 'err');
      } else if (evt && evt.evt === 'error') {
        throw new Error('设备返回错误: ' + (evt.msg || 'unknown'));
      } else {
        setStatus('⚠️ 凭据已保存但连接超时，可重试「仅配网」', 'err');
      }
    }

    // 5. 刷新状态显示
    const st1 = await sendCommand('status', ['status'], PING_TIMEOUT_MS);
    renderWifiStatus(st1 || lastStatus);
  } catch (e) {
    log('配网失败: ' + (e && e.message ? e.message : e));
    setStatus('❌ ' + (e && e.message ? e.message : e), 'err');
  } finally {
    provisionBusy = false;
    try { await provisionClosePort(); } catch (_) {}
    btnProvision.disabled = false;
    btnClearWifi.disabled = false;
    if (manifest && connected) btnFlash.disabled = false;
  }
}

// 「仅配网」：不刷固件，直接用串口协议配网
btnProvision.addEventListener('click', async () => {
  if (provisionBusy) return;
  if (!('serial' in navigator)) { browserWarn.classList.remove('hidden'); return; }
  try {
    if (!port) {
      setStatus('请选择设备串口…');
      port = await navigator.serial.requestPort();
    }
    await runProvision(false);
  } catch (e) {
    log('仅配网失败: ' + (e && e.message ? e.message : e));
  }
});

// 「清除配网信息」：恢复固件内置凭据
btnClearWifi.addEventListener('click', async () => {
  if (provisionBusy) return;
  if (!('serial' in navigator)) { browserWarn.classList.remove('hidden'); return; }
  try {
    if (!port) {
      setStatus('请选择设备串口…');
      port = await navigator.serial.requestPort();
    }
    provisionBusy = true;
    btnProvision.disabled = true;
    btnClearWifi.disabled = true;
    await provisionOpenPort();
    const saved = await sendCommand('wifi_clear', ['wifi_saved'], PING_TIMEOUT_MS);
    if (!saved) throw new Error('设备未确认清除（wifi_saved 超时）');
    log('配网信息已清除（回落到固件内置凭据）');
    const st = await sendCommand('status', ['status'], PING_TIMEOUT_MS);
    renderWifiStatus(st || lastStatus);
    setStatus('✅ 已清除配网信息', 'ok');
  } catch (e) {
    log('清除失败: ' + (e && e.message ? e.message : e));
    setStatus('❌ ' + (e && e.message ? e.message : e), 'err');
  } finally {
    provisionBusy = false;
    try { await provisionClosePort(); } catch (_) {}
    btnProvision.disabled = false;
    btnClearWifi.disabled = false;
  }
});

// ---------- init ----------
if (!('serial' in navigator)) {
  browserWarn.classList.remove('hidden');
  btnConnect.disabled = true;
  btnProvision.disabled = true;
  btnClearWifi.disabled = true;
  setStatus('当前浏览器不支持 Web Serial', 'err');
}
log('升级工具已就绪。');
loadManifest();