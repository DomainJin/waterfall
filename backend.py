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

# ⚙️ Configuration
PORT_BACKEND = 5000
UDP_PORT = 8888
TIMEOUT_UDP = 1.0
SCAN_WORKERS = 20

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
    """Serve index.html"""
    return send_file('index.html', mimetype='text/html')


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
    """404 handler - try to serve index.html for SPA routing"""
    if request.path.startswith('/api/'):
        return jsonify({"error": "API endpoint not found"}), 404
    return send_file('index.html'), 200


@app.errorhandler(500)
def server_error(error):
    """500 handler"""
    logger.error(f"Server error: {error}")
    return jsonify({"error": "Internal server error"}), 500


# ============================================================
# Main
# ============================================================

def main():
    logger.info("╔═══════════════════════════════════════════════╗")
    logger.info("║  Water Curtain Backend — Flask Server         ║")
    logger.info("╚═══════════════════════════════════════════════╝")
    logger.info(f"\n[*] Backend running on http://localhost:{PORT_BACKEND}")
    logger.info(f"[*] Serving: {os.getcwd()}")
    logger.info(f"[*] Open: http://localhost:{PORT_BACKEND}\n")
    
    # Run Flask
    app.run(
        host='0.0.0.0',
        port=PORT_BACKEND,
        debug=False,
        use_reloader=False
    )


if __name__ == '__main__':
    main()
