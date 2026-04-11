/**
 * proxy.js — WebSocket → TCP bridge
 * Chạy: node proxy.js <ESP32_IP> [port=3333]
 * Ví dụ: node proxy.js 192.168.1.241 3333
 */
const net = require('net');
const { WebSocketServer } = require('ws');

const [,, ESP_IP, ESP_PORT_STR] = process.argv;
if (!ESP_IP) {
  console.error('Usage: node proxy.js <ESP32_IP> [port=3333]');
  process.exit(1);
}

const ESP_PORT = parseInt(ESP_PORT_STR) || 3333;
const WS_PORT  = ESP_PORT + 1;

const wss = new WebSocketServer({ port: WS_PORT });
console.log(`\n╔══════════════════════════════════════╗`);
console.log(`║  Water Curtain WS↔TCP Proxy          ║`);
console.log(`║  Browser → ws://localhost:${WS_PORT}      ║`);
console.log(`║  ESP32  → ${ESP_IP}:${ESP_PORT}        ║`);
console.log(`╚══════════════════════════════════════╝\n`);

wss.on('connection', (ws, req) => {
  console.log(`[WS] Browser connected`);

  const tcp = net.createConnection({ port: ESP_PORT, host: ESP_IP });

  tcp.on('connect', () => {
    console.log(`[TCP] Connected to ESP32 ${ESP_IP}:${ESP_PORT}`);
  });

  ws.on('message', (data, isBinary) => {
    const buf = isBinary ? data : Buffer.from(data);
    tcp.write(buf);
  });

  tcp.on('data', (data) => {
    if (ws.readyState === 1) ws.send(data, { binary: true });
  });

  const cleanup = () => { try { tcp.destroy(); } catch(_){} };
  ws.on('close', () => { console.log('[WS] Disconnected'); cleanup(); });
  ws.on('error', (e) => { console.error('[WS]', e.message); cleanup(); });
  tcp.on('error', (e) => { console.error('[TCP]', e.message); try { ws.close(); } catch(_){} });
  tcp.on('close', () => { try { ws.close(); } catch(_){} });

  const ping = setInterval(() => {
    if (ws.readyState === 1) ws.ping();
    else clearInterval(ping);
  }, 3000);
  ws.on('pong', () => {});
});

process.on('SIGINT', () => { wss.close(() => process.exit(0)); });
