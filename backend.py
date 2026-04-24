#!/usr/bin/env python3
"""
Water Curtain Controller — Python Flask Backend
Cung cấp REST API cho Web UI, quản lý ESP32 devices, scan network, etc.

Chạy: python backend.py
Mở: http://localhost:5000
"""

import os
import sys
import socket
import threading
import json
import time
import logging
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

import flask
from flask import Flask, jsonify, request, send_file, send_from_directory
import werkzeug

# MQTT (optional — chỉ chạy khi có paho-mqtt và biến môi trường MQTT_BROKER)
try:
    import paho.mqtt.client as mqtt_lib
    MQTT_AVAILABLE = True
except ImportError:
    MQTT_AVAILABLE = False

# ⚙️ Configuration
PORT_BACKEND = 5000
UDP_PORT     = 8888
TIMEOUT_UDP  = 1.0
SCAN_WORKERS = 20

# MQTT config (đọc từ environment — set trong docker-compose hoặc .env)
MQTT_BROKER   = os.environ.get('MQTT_BROKER', '')       # rỗng = tắt MQTT
MQTT_PORT     = int(os.environ.get('MQTT_PORT', 1883))
MQTT_USER     = os.environ.get('MQTT_USER', '')
MQTT_PASSWORD = os.environ.get('MQTT_PASSWORD', '')

# MQTT Topics — Waterfall
TOPIC_CMD    = 'waterfall/cmd/#'     # subscribe — nhận lệnh từ web client
TOPIC_STATUS = 'waterfall/status'    # publish   — trạng thái ESP32
TOPIC_VALVE  = 'waterfall/cmd/valve' # publish   — điều khiển van
TOPIC_STREAM = 'waterfall/cmd/stream'# publish   — gửi animation frame

# MQTT Topics — Paint Controller
TOPIC_PAINT_STATUS = 'paint/status'  # subscribe — trạng thái paint device
TOPIC_PAINT_CMD    = 'paint/cmd'     # publish   — điều khiển paint device

# Setup logging - simple approach (Windows-safe)
# Use basicConfig's default stream (auto-handles encoding)
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] %(levelname)s: %(message)s',
    handlers=[
        logging.FileHandler('backend.log', encoding='utf-8')
    ]
)
logger = logging.getLogger(__name__)
# Also add console handler (simpler)
console_handler = logging.StreamHandler()
console_handler.setFormatter(logging.Formatter('[%(asctime)s] %(levelname)s: %(message)s'))
logger.addHandler(console_handler)
# Suppress only UDP error logging (too noisy during scan)
logging.getLogger('udp').setLevel(logging.CRITICAL)

# Flask app
app = Flask(__name__, static_folder='./', static_url_path='')

# Store device cache (IP → info)
device_cache = {}
cache_timeout = 60  # seconds
cache_last_update = 0

# ============================================================
# MQTT Relay (Cloud ↔ ESP32)
# ============================================================

class MQTTRelay:
    """Kết nối Flask với MQTT broker để relay lệnh đến ESP32."""

    def __init__(self):
        self.client   = None
        self.connected = False
        self._last_status = {}   # cache trạng thái từ ESP32
        self._esp_devices = {}   # {name: {online, ip, last_seen}}
        self._paint_status = {}  # cache trạng thái paint device

    def start(self):
        if not MQTT_AVAILABLE or not MQTT_BROKER:
            logger.info("[MQTT] Bỏ qua — MQTT_BROKER chưa cấu hình")
            return

        self.client = mqtt_lib.Client(client_id="waterfall-backend", clean_session=True)
        self.client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
        self.client.on_connect    = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message    = self._on_message

        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            self.client.loop_start()   # non-blocking background thread
            logger.info(f"[MQTT] Đang kết nối tới {MQTT_BROKER}:{MQTT_PORT}")
        except Exception as e:
            logger.error(f"[MQTT] Không kết nối được: {e}")

    def _on_connect(self, client, _userdata, _flags, rc):
        if rc == 0:
            self.connected = True
            client.subscribe(TOPIC_CMD, qos=1)
            client.subscribe(TOPIC_STATUS, qos=1)
            client.subscribe(TOPIC_PAINT_STATUS, qos=1)
            logger.info(f"[MQTT] Đã kết nối, subscribe: waterfall/# + paint/status")
        else:
            logger.error(f"[MQTT] Kết nối thất bại, rc={rc}")

    def _on_disconnect(self, _client, _userdata, rc):
        self.connected = False
        logger.warning(f"[MQTT] Mất kết nối (rc={rc}), tự reconnect...")

    def _on_message(self, _client, _userdata, msg):
        """Nhận message từ MQTT broker."""
        try:
            payload = msg.payload.decode('utf-8')
            topic   = msg.topic
            logger.info(f"[MQTT] Nhận: {topic} → {payload[:80]}")
            data = json.loads(payload)

            # Paint device gửi trạng thái
            if topic == TOPIC_PAINT_STATUS:
                self._paint_status = {
                    'online':    data.get('online', False),
                    'btn':       data.get('btn', 0),
                    'color':     data.get('color', 0),
                    'level':     data.get('level', 0),
                    'hex':       data.get('hex', '#000000'),
                    'name':      data.get('name', 'paint'),
                    'ip':        data.get('ip', ''),
                    'last_seen': time.time()
                }
                return

            # Waterfall ESP32 gửi trạng thái online/offline
            if topic == TOPIC_STATUS:
                name = data.get('name', 'esp32')
                self._esp_devices[name] = {
                    'name': name,
                    'ip':   data.get('ip', ''),
                    'online': data.get('online', False),
                    'last_seen': time.time()
                }
                return

            # Lệnh điều khiển từ web → relay đến ESP32 qua UDP
            esp_ip = data.get('ip') or self._get_default_esp_ip()
            if not esp_ip:
                return
            cmd = data.get('cmd', '')
            if cmd:
                send_udp_cmd(esp_ip, cmd, suppress_errors=False)
        except Exception as e:
            logger.error(f"[MQTT] Lỗi xử lý message: {e}")

    def publish(self, topic, payload, qos=1):
        if self.client and self.connected:
            self.client.publish(topic, payload, qos=qos)

    def _get_default_esp_ip(self):
        """Lấy IP ESP32 đầu tiên từ cache."""
        if device_cache:
            return next(iter(device_cache))
        return None

    @property
    def status(self):
        return {'connected': self.connected, 'broker': MQTT_BROKER}

    @property
    def paint_status(self):
        return self._paint_status


mqtt_relay = MQTTRelay()
mqtt_relay.start()   # Chạy khi gunicorn import module (không chỉ khi gọi main())

# ============================================================
# UDP Communication Functions
# ============================================================

def send_udp_cmd(esp_ip, cmd, timeout=TIMEOUT_UDP, suppress_errors=True):
    """Send UDP command to ESP32 and get reply"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    try:
        sock.sendto(cmd.encode(), (esp_ip, UDP_PORT))
        reply, addr = sock.recvfrom(512)
        return reply.decode().strip()
    except socket.timeout:
        return None
    except Exception as e:
        # Suppress noisy errors during scanning (most IPs don't have devices)
        # Only log real errors if suppress_errors=False
        if not suppress_errors:
            logger.warning(f"UDP error to {esp_ip}: {e}")
        return None
    finally:
        sock.close()


def is_esp32_available(ip_str):
    """Check if ESP32 is available at IP"""
    try:
        # Stage 1: Quick check
        r = send_udp_cmd(ip_str, "GET_INFO", timeout=0.5)
        if r and not r.startswith("ERROR"):
            return True
        
        # Stage 2: Retry with longer timeout
        r = send_udp_cmd(ip_str, "GET_INFO", timeout=TIMEOUT_UDP)
        return r is not None and not r.startswith("ERROR")
    except:
        return False


def scan_network_parallel(network_prefix="192.168.1", max_workers=SCAN_WORKERS):
    """Parallel scan for ESP32 devices"""
    found = []
    
    def check_ip(ip):
        if is_esp32_available(ip):
            # Get device info
            info = send_udp_cmd(ip, "GET_INFO", timeout=TIMEOUT_UDP)
            return {
                "ip": ip,
                "info": info,
                "port": 3333
            }
        return None
    
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {
            executor.submit(check_ip, f"{network_prefix}.{i}"): i 
            for i in range(1, 255)
        }
        
        for future in as_completed(futures):
            result = future.result()
            if result:
                found.append(result)
    
    return sorted(found, key=lambda x: x["ip"])


# ============================================================
# HTTP Endpoints
# ============================================================

@app.route('/')
def serve_index():
    """Serve home page"""
    return send_file('home.html', mimetype='text/html')


@app.route('/api/health')
def health():
    """Health check endpoint"""
    return jsonify({
        "status": "ok",
        "service": "Water Curtain Backend",
        "version": "1.0"
    })


@app.route('/api/scan', methods=['POST'])
def api_scan():
    """Scan network for ESP32 devices"""
    try:
        data = request.get_json() or {}
        prefix = data.get('prefix', '192.168.1')
        workers = data.get('workers', SCAN_WORKERS)
        
        logger.info(f"[SCAN] Scanning network {prefix}.* with {workers} workers...")
        start = time.time()
        
        devices = scan_network_parallel(prefix, max_workers=workers)
        elapsed = time.time() - start
        
        logger.info(f"[OK] Found {len(devices)} device(s) in {elapsed:.2f}s")
        
        return jsonify({
            "success": True,
            "devices": devices,
            "count": len(devices),
            "elapsed_ms": int(elapsed * 1000)
        })
    
    except Exception as e:
        logger.error(f"Scan error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/storage/<esp_ip>', methods=['GET'])
def api_storage_info(esp_ip):
    """Get SD card storage info from ESP32"""
    try:
        logger.info(f"Getting storage info from {esp_ip}...")
        
        # Try GET_STORAGE command first
        result = send_udp_cmd(esp_ip, "GET_STORAGE", timeout=TIMEOUT_UDP, suppress_errors=False)
        
        if result and result.startswith("OK:"):
            # Parse response: "OK: used=123456, total=7540000"
            storage_info = {
                "success": True,
                "device": esp_ip,
                "used_bytes": 0,
                "total_bytes": 0,
                "used_mb": 0,
                "total_mb": 0,
                "used_pct": 0
            }
            
            parts = result.split(':')[1].strip().split(',')
            for part in parts:
                part = part.strip()
                if '=' in part:
                    key, val = part.split('=', 1)
                    key = key.strip().lower()
                    try:
                        val = int(val.strip())
                        if 'used' in key:
                            storage_info["used_bytes"] = val
                            storage_info["used_mb"] = round(val / 1024 / 1024, 2)
                        elif 'total' in key:
                            storage_info["total_bytes"] = val
                            storage_info["total_mb"] = round(val / 1024 / 1024, 2)
                    except ValueError:
                        pass
            
            # Calculate percentage
            if storage_info["total_bytes"] > 0:
                storage_info["used_pct"] = round(
                    (storage_info["used_bytes"] / storage_info["total_bytes"]) * 100, 1
                )
            
            logger.info(f"[OK] Storage: {storage_info['used_mb']} MB / {storage_info['total_mb']} MB")
            return jsonify(storage_info)
        else:
            logger.warning(f"Device didn't respond to GET_STORAGE")
            return jsonify({
                "success": False,
                "device": esp_ip,
                "error": "Device not responding",
                "used_bytes": 0,
                "total_bytes": 0,
                "used_mb": 0,
                "total_mb": 0,
                "used_pct": 0
            }), 400
    
    except Exception as e:
        logger.error(f"Storage error: {e}")
        return jsonify({
            "success": False,
            "error": str(e),
            "used_bytes": 0,
            "total_bytes": 0,
            "used_mb": 0,
            "total_mb": 0,
            "used_pct": 0
        }), 500


@app.route('/api/device/<esp_ip>/info', methods=['GET'])
def api_device_info(esp_ip):
    """Get device info including storage"""
    try:
        logger.info(f"Getting info from {esp_ip}...")
        result = send_udp_cmd(esp_ip, "GET_INFO", timeout=TIMEOUT_UDP)
        
        if result and result.startswith("OK:"):
            # Parse storage info from response
            # Expected format: "OK: board_count=10, frame_size=1024, sd_used=123456, sd_total=7540000"
            info_dict = {
                "success": True,
                "ip": esp_ip,
                "info": result,
                "board_count": 10,
                "frame_size": 1024,
                "file_size": 0,  # in bytes
                "total_size": 4000000000,  # 4GB in bytes (default)
            }
            
            # Try to parse parameters from response
            parts = result.split(',')
            for part in parts:
                part = part.strip()
                if '=' in part:
                    key, val = part.split('=', 1)
                    key = key.strip().lower()
                    try:
                        val = int(val.strip())
                        if 'board' in key:
                            info_dict["board_count"] = val
                        elif 'frame' in key or 'size' in key:
                            info_dict["frame_size"] = val
                        elif 'sd_used' in key or 'used' in key:
                            info_dict["file_size"] = val
                        elif 'sd_total' in key or 'total' in key:
                            info_dict["total_size"] = val
                    except ValueError:
                        pass
            
            # If storage info not provided, try to estimate from SD card
            # This is a fallback - ESP32 should provide actual values
            if info_dict["file_size"] == 0 and info_dict["total_size"] == 4000000000:
                # Try GET_STORAGE command if available
                storage_result = send_udp_cmd(esp_ip, "GET_STORAGE", timeout=TIMEOUT_UDP, suppress_errors=True)
                if storage_result and storage_result.startswith("OK:"):
                    try:
                        storage_parts = storage_result.split(':')[1].strip().split(',')
                        for part in storage_parts:
                            if 'used' in part.lower():
                                info_dict["file_size"] = int(''.join(filter(str.isdigit, part)))
                            elif 'total' in part.lower():
                                info_dict["total_size"] = int(''.join(filter(str.isdigit, part)))
                    except:
                        pass
            
            return jsonify(info_dict)
        else:
            return jsonify({
                "success": False,
                "error": f"Device response: {result or 'timeout'}"
            }), 400
    
    except Exception as e:
        logger.error(f"Info error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/device/<esp_ip>/set-ip', methods=['POST'])
def api_device_set_ip(esp_ip):
    """Set device's target IP"""
    try:
        data = request.get_json() or {}
        new_ip = data.get('ip', '')
        
        if not new_ip:
            return jsonify({
                "success": False,
                "error": "Missing 'ip' parameter"
            }), 400
        
        logger.info(f"Setting IP on {esp_ip} → {new_ip}...")
        result = send_udp_cmd(esp_ip, f"SET_IP:{new_ip}", timeout=TIMEOUT_UDP)
        
        if result and result.startswith("OK:"):
            logger.info(f"[OK] IP changed: {result}")
            return jsonify({
                "success": True,
                "result": result
            })
        else:
            return jsonify({
                "success": False,
                "error": f"Device response: {result or 'timeout'}"
            }), 400
    
    except Exception as e:
        logger.error(f"Set IP error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/device/<esp_ip>/set-port', methods=['POST'])
def api_device_set_port(esp_ip):
    """Set device's target port"""
    try:
        data = request.get_json() or {}
        port = data.get('port', 0)
        
        if not port or port < 1000 or port > 65535:
            return jsonify({
                "success": False,
                "error": "Invalid port (must be 1000-65535)"
            }), 400
        
        logger.info(f"Setting port on {esp_ip} → {port}...")
        result = send_udp_cmd(esp_ip, f"SET_PORT:{port}", timeout=TIMEOUT_UDP)
        
        if result and result.startswith("OK:"):
            logger.info(f"[OK] Port changed: {result}")
            return jsonify({
                "success": True,
                "result": result
            })
        else:
            return jsonify({
                "success": False,
                "error": f"Device response: {result or 'timeout'}"
            }), 400
    
    except Exception as e:
        logger.error(f"Set port error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/device/<esp_ip>/reset', methods=['POST'])
def api_device_reset(esp_ip):
    """Reset device"""
    try:
        logger.info(f"Resetting {esp_ip}...")
        result = send_udp_cmd(esp_ip, "RESET", timeout=2.0)
        
        if result and result.startswith("OK:"):
            logger.info(f"[OK] Device reset: {result}")
            return jsonify({
                "success": True,
                "result": result
            })
        else:
            return jsonify({
                "success": False,
                "error": f"Device response: {result or 'timeout'}"
            }), 400
    
    except Exception as e:
        logger.error(f"Reset error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/device/<esp_ip>/files', methods=['GET'])
def api_device_files(esp_ip):
    """List animation files from ESP32 SD card"""
    try:
        logger.info(f"Listing files from {esp_ip}...")
        
        # Send LIST_FILES command to ESP32
        result = send_udp_cmd(esp_ip, "LIST_FILES", timeout=TIMEOUT_UDP, suppress_errors=False)
        
        files = []
        if result and result.startswith("OK:"):
            # Parse file list response
            # Expected format: "OK: file1.bin,1024;file2.bin,2048;..."
            file_data = result.split(':', 1)[1].strip()
            if file_data:
                for entry in file_data.split(';'):
                    entry = entry.strip()
                    if ',' in entry:
                        name, size = entry.rsplit(',', 1)
                        try:
                            files.append({
                                "name": name.strip(),
                                "size": int(size.strip()),
                                "size_kb": round(int(size.strip()) / 1024, 2)
                            })
                        except ValueError:
                            files.append({
                                "name": name.strip(),
                                "size": 0,
                                "size_kb": 0
                            })
            
            logger.info(f"[OK] Found {len(files)} files on {esp_ip}")
            return jsonify({
                "success": True,
                "device": esp_ip,
                "files": files,
                "count": len(files)
            })
        else:
            logger.warning(f"Device didn't respond to LIST_FILES: {result}")
            return jsonify({
                "success": False,
                "files": [],
                "message": "Device not responding to file list request"
            })
    
    except Exception as e:
        logger.error(f"Device files error: {e}")
        return jsonify({
            "success": False,
            "error": str(e),
            "files": []
        }), 500


@app.route('/api/files', methods=['GET'])
def api_files():
    """Get list of animation files from SD card"""
    try:
        sd_path = Path('SD_MOUNT') if Path('SD_MOUNT').exists() else Path('./animations')
        
        if not sd_path.exists():
            return jsonify({
                "success": True,
                "files": [],
                "message": "No SD mount found"
            })
        
        files = []
        for f in sd_path.glob('*.bin'):
            files.append({
                "name": f.name,
                "size": f.stat().st_size,
                "path": f.relative_to(sd_path)
            })
        
        logger.info(f"Found {len(files)} animation files")
        return jsonify({
            "success": True,
            "files": files,
            "count": len(files)
        })
    
    except Exception as e:
        logger.error(f"Files error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/files/upload', methods=['POST'])
def api_upload_file():
    """Upload animation file"""
    try:
        if 'file' not in request.files:
            return jsonify({
                "success": False,
                "error": "No file provided"
            }), 400
        
        file = request.files['file']
        
        if file.filename == '':
            return jsonify({
                "success": False,
                "error": "Empty filename"
            }), 400
        
        # Save file
        upload_dir = Path('./uploads')
        upload_dir.mkdir(exist_ok=True)
        
        filepath = upload_dir / werkzeug.utils.secure_filename(file.filename)
        file.save(filepath)
        
        logger.info(f"[OK] Uploaded: {filepath}")
        
        return jsonify({
            "success": True,
            "filename": file.filename,
            "size": filepath.stat().st_size,
            "path": str(filepath.relative_to('.'))
        })
    
    except Exception as e:
        logger.error(f"Upload error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/stream/<esp_ip>', methods=['POST'])
def api_stream(esp_ip):
    """Send animation frames to device (proxy WebSocket frames via HTTP)"""
    try:
        # Get binary data from POST body
        frames_data = request.get_data()
        
        if not frames_data:
            return jsonify({
                "success": False,
                "error": "No frame data"
            }), 400
        
        # Note: This would need WebSocket connection to esp_ip:3333
        # For now, just log and acknowledge
        logger.info(f"📤 Stream to {esp_ip}: {len(frames_data)} bytes")
        
        return jsonify({
            "success": True,
            "device": esp_ip,
            "bytes": len(frames_data),
            "message": "Frame data received (note: requires WebSocket to ESP32)"
        })
    
    except Exception as e:
        logger.error(f"Stream error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


# ============================================================
# Static Files & Error Handlers
# ============================================================

@app.route('/<path:filename>')
def serve_static(filename):
    """Serve static files (CSS, JS, images, etc.)"""
    try:
        return send_from_directory('./', filename)
    except:
        return "File not found", 404


@app.errorhandler(404)
def not_found(error):
    """404 handler - serve home page for unknown routes"""
    if request.path.startswith('/api/'):
        return jsonify({"error": "API endpoint not found"}), 404
    return send_file('home.html'), 200


# ============================================================
# Device Control & Monitoring APIs
# ============================================================

# Track connected devices (for heartbeat/listing)
connected_devices = {}
device_last_heartbeat = {}

@app.route('/api/scan')
def api_scan_get():
    """Scan network for ESP32 devices (GET version)"""
    try:
        prefix = request.args.get('prefix', '192.168.1')
        
        logger.info(f"[SCAN] Scanning network {prefix}.*...")
        start = time.time()
        
        devices = scan_network_parallel(prefix, max_workers=SCAN_WORKERS)
        elapsed = time.time() - start
        
        # Format for web UI
        device_list = []
        for d in devices:
            device_list.append({
                "ip": d["ip"],
                "port": d.get("port", 3333),
                "name": "Waterfall_" + d["ip"].split('.')[-1],
                "status": "Online",
                "is_online": True,
                "last_seen": str(int(elapsed*1000)) + "ms ago",
                "info": d.get("info", "")
            })
        
        logger.info(f"[OK] Found {len(device_list)} device(s)")
        
        return jsonify({
            "success": True,
            "devices": device_list
        })
    
    except Exception as e:
        logger.error(f"Scan error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/esp-devices')
def api_esp_devices():
    """Get list of ESP32 devices (simulated heartbeat)"""
    try:
        # Re-scan network to get current devices
        devices = scan_network_parallel("192.168.1", max_workers=10)
        
        device_list = []
        for i, d in enumerate(devices):
            device_list.append({
                "device_key": "Waterfall_" + d["ip"],
                "name": "Waterfall_" + d["ip"].split('.')[-1],
                "ip": d["ip"],
                "port": d.get("port", 3333),
                "layer_index": i + 1,
                "is_online": True,
                "status": "Online",
                "last_heartbeat": "Just now",
                "heartbeat_count": 1,
                "uptime": "N/A",
                "ping_ms": 0,
                "ping_status": "OK",
                "ping_color": "#27ae60",
                "ping_icon": "OK"
            })
        
        return jsonify(device_list)
    
    except Exception as e:
        logger.error(f"ESP devices error: {e}")
        return jsonify([]), 500


@app.route('/api/send-command', methods=['POST'])
def api_send_command():
    """Send command to ESP32 device"""
    try:
        data = request.get_json()
        esp_ip = data.get('ip')
        esp_port = data.get('port', 3333)
        command = data.get('command', '')
        
        if not esp_ip or not command:
            return jsonify({
                "success": False,
                "error": "Missing ip or command"
            }), 400
        
        logger.info(f"[CMD] Sending to {esp_ip}: {command}")
        
        # Send UDP command
        result = send_udp_cmd(esp_ip, command, timeout=2.0, suppress_errors=False)
        
        if result and not result.startswith("ERROR"):
            return jsonify({
                "success": True,
                "response": result
            })
        else:
            return jsonify({
                "success": False,
                "error": result or "No response"
            })
    
    except Exception as e:
        logger.error(f"Send command error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/device-status')
def api_device_status():
    """Get device status for monitoring"""
    try:
        device_ip = request.args.get('ip', '')
        
        if not device_ip:
            return jsonify({
                "connected": False,
                "error": "No IP provided"
            })
        
        # Try to get info from device
        info = send_udp_cmd(device_ip, "GET_INFO", timeout=1.0)
        
        connected = info is not None and not info.startswith("ERROR")
        
        return jsonify({
            "connected": connected,
            "ip": device_ip,
            "frames_sent": 0,
            "active_valves": 0,
            "uptime": "N/A",
            "heap": 0
        })
    
    except Exception as e:
        logger.error(f"Device status error: {e}")
        return jsonify({
            "connected": False,
            "error": str(e)
        }), 500


@app.route('/api/test-all-valves', methods=['POST'])
def api_test_all_valves():
    """Test all valves sequentially"""
    try:
        data = request.get_json() or {}
        esp_ip = data.get('ip')
        
        if not esp_ip:
            # Try to scan for any device
            devices = scan_network_parallel("192.168.1", max_workers=10)
            if devices:
                esp_ip = devices[0]["ip"]
        
        if not esp_ip:
            return jsonify({"success": False, "error": "No device found"}), 400
        
        # Start test sequence
        result = send_udp_cmd(esp_ip, "TEST_SEQ", timeout=5.0)
        
        return jsonify({
            "success": True,
            "device": esp_ip,
            "response": result
        })
    
    except Exception as e:
        logger.error(f"Test all valves error: {e}")
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


# ============================================================
# Error Handlers
# ============================================================

@app.errorhandler(500)
def server_error(error):
    """500 handler"""
    logger.error(f"Server error: {error}")
    return jsonify({"error": "Internal server error"}), 500


# ============================================================
# MQTT API Endpoints
# ============================================================

@app.route('/api/devices', methods=['GET'])
def api_devices():
    """Danh sách ESP32 devices đã từng kết nối qua MQTT."""
    devices = []
    for name, info in mqtt_relay._esp_devices.items():
        online = info.get('online', False)
        last_seen = info.get('last_seen', 0)
        devices.append({
            'name':      name,
            'ip':        info.get('ip', ''),
            'online':    online,
            'last_seen': int(last_seen)
        })
    return jsonify({'devices': devices})


@app.route('/api/cmd', methods=['POST'])
def api_cmd():
    """Gửi lệnh điều khiển tới ESP32 qua MQTT (cloud path).
    Body: {"cmd":"ALL_OFF"|"ALL_ON"|"SET", "bits":"FF00...", "target":"waterfall-abc123"}
    """
    data   = request.get_json() or {}
    cmd    = data.get('cmd', '')
    bits   = data.get('bits', '')    # hex string, only for SET
    target = data.get('target', '')  # device name, empty = broadcast

    if not cmd:
        return jsonify({"error": "Thiếu trường 'cmd'"}), 400

    if not mqtt_relay.connected:
        return jsonify({"error": "MQTT chưa kết nối", "connected": False}), 503

    payload_dict = {"cmd": cmd}
    if cmd == 'SET' and bits:
        payload_dict["bits"] = bits
    if target:
        payload_dict["target"] = target

    mqtt_relay.publish(TOPIC_VALVE, json.dumps(payload_dict), qos=1)
    logger.info(f"[CMD] cmd={cmd} target={target or 'broadcast'}")
    return jsonify({"success": True, "method": "mqtt", "cmd": cmd, "target": target or "broadcast"})


@app.route('/api/mqtt/status', methods=['GET'])
def api_mqtt_status():
    """Trạng thái kết nối MQTT broker"""
    return jsonify(mqtt_relay.status)


@app.route('/api/paint/status', methods=['GET'])
def api_paint_status():
    """Trạng thái hiện tại của Paint Controller (từ MQTT retained message)."""
    status = mqtt_relay.paint_status
    if not status:
        return jsonify({'online': False, 'message': 'Chưa nhận được dữ liệu từ paint device'})
    # Đánh dấu offline nếu quá 30s không có tin nhắn
    last_seen = status.get('last_seen', 0)
    if last_seen and (time.time() - last_seen) > 30:
        status = dict(status)
        status['online'] = False
    return jsonify(status)


@app.route('/api/paint/cmd', methods=['POST'])
def api_paint_cmd():
    """Gửi lệnh điều khiển đến Paint device qua MQTT.
    Body: {"color": 0-19} hoặc {"level": 0-6}
    """
    data = request.get_json() or {}
    if not data:
        return jsonify({'error': 'Thiếu body JSON'}), 400

    if not mqtt_relay.connected:
        return jsonify({'error': 'MQTT chưa kết nối', 'connected': False}), 503

    payload = json.dumps(data)
    mqtt_relay.publish(TOPIC_PAINT_CMD, payload, qos=1)
    logger.info(f"[PAINT CMD] {payload}")
    return jsonify({'success': True, 'sent': data})


@app.route('/api/mqtt/publish', methods=['POST'])
def api_mqtt_publish():
    """Gửi lệnh đến ESP32 qua MQTT (dùng khi truy cập từ internet)"""
    data = request.get_json() or {}
    topic   = data.get('topic', TOPIC_VALVE)
    payload = data.get('payload')
    esp_ip  = data.get('ip', '')

    if not payload:
        return jsonify({"error": "Thiếu payload"}), 400

    if not mqtt_relay.connected:
        # Fallback: gửi trực tiếp qua UDP nếu cùng mạng LAN
        if esp_ip:
            result = send_udp_cmd(esp_ip, payload)
            return jsonify({"success": bool(result), "method": "udp", "result": result})
        return jsonify({"error": "MQTT chưa kết nối và không có IP ESP32"}), 503

    msg = json.dumps({"ip": esp_ip, "cmd": payload}) if esp_ip else payload
    mqtt_relay.publish(topic, msg)
    return jsonify({"success": True, "method": "mqtt", "topic": topic})


# ============================================================
# Main
# ============================================================

def get_local_ip():
    """Get the machine's LAN IP address"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "localhost"


def main():
    local_ip = get_local_ip()
    logger.info("╔═══════════════════════════════════════════════╗")
    logger.info("║  Water Curtain Backend — Flask Server         ║")
    logger.info("╚═══════════════════════════════════════════════╝")
    logger.info(f"[*] Serving: {os.getcwd()}")
    logger.info(f"[*] Local:   http://localhost:{PORT_BACKEND}")
    logger.info(f"[*] LAN:     http://{local_ip}:{PORT_BACKEND}  ← dùng địa chỉ này cho điện thoại/máy khác\n")

    # Khởi động MQTT relay (nếu có cấu hình)
    mqtt_relay.start()
    
    # Run Flask
    app.run(
        host='0.0.0.0',
        port=PORT_BACKEND,
        debug=False,
        use_reloader=False
    )


if __name__ == '__main__':
    main()
