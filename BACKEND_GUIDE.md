# Python Backend Server — Installation & Usage

## 📦 Installation

### 1. Install Flask

```bash
# Activate venv first
& .\.venv\Scripts\Activate.ps1

# Install requirements
pip install -r requirements_backend.txt

# Or install directly
pip install flask==2.3.2 werkzeug==2.3.6
```

### 2. Run Backend Server

```bash
python backend.py
```

**Output:**

```
╔═══════════════════════════════════════════════╗
║  Water Curtain Backend — Flask Server         ║
╚═══════════════════════════════════════════════╝

📡 Backend running on http://localhost:5000
📂 Serving: C:\Users\SGM\Documents\PlatformIO\Projects\fall
🌐 Open: http://localhost:5000
```

### 3. Open in Browser

```
http://localhost:5000
```

---

## 🌐 API Endpoints

### Health Check

```
GET /api/health

Response:
{
  "status": "ok",
  "service": "Water Curtain Backend",
  "version": "1.0"
}
```

### Scan Network

```
POST /api/scan

Request Body (optional):
{
  "prefix": "192.168.1",
  "workers": 20
}

Response:
{
  "success": true,
  "devices": [
    {
      "ip": "192.168.1.241",
      "info": "OK:ESP32:192.168.1.241 WS:3333 TD:192.168.1.100 Ver:1.0",
      "port": 3333
    },
    {
      "ip": "192.168.1.159",
      "info": "OK:ESP32:192.168.1.159 WS:3333 TD:0.0.0.0 Ver:1.0",
      "port": 3333
    }
  ],
  "count": 2,
  "elapsed_ms": 3245
}
```

### Get Device Info

```
GET /api/device/<ESP32_IP>/info

Example:
GET /api/device/192.168.1.241/info

Response:
{
  "success": true,
  "ip": "192.168.1.241",
  "info": "OK:ESP32:192.168.1.241 WS:3333 TD:192.168.1.100 Ver:1.0"
}
```

### Set Device Target IP

```
POST /api/device/<ESP32_IP>/set-ip

Request Body:
{
  "ip": "192.168.1.100"
}

Response:
{
  "success": true,
  "result": "OK:192.168.1.100"
}
```

### Set Device Target Port

```
POST /api/device/<ESP32_IP>/set-port

Request Body:
{
  "port": 3333
}

Response:
{
  "success": true,
  "result": "OK:3333"
}
```

### Reset Device

```
POST /api/device/<ESP32_IP>/reset

Response:
{
  "success": true,
  "result": "OK:RESET"
}
```

### Get Files

```
GET /api/files

Response:
{
  "success": true,
  "files": [
    {
      "name": "animation1.bin",
      "size": 12345,
      "path": "animation1.bin"
    }
  ],
  "count": 1
}
```

### Upload File

```
POST /api/files/upload

Form Data:
- file: <binary file>

Response:
{
  "success": true,
  "filename": "animation1.bin",
  "size": 12345,
  "path": "uploads/animation1.bin"
}
```

---

## 🔌 Using Backend from Web UI

### Option 1: Update Scanning Function

Update `scanESPNetwork()` in index.html to use backend API:

```javascript
async function scanESPNetworkAPI() {
  const results = document.getElementById("scan-results");
  results.style.display = "block";
  results.innerHTML = "Scanning via backend...";

  try {
    const response = await fetch(`/api/scan`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        prefix: "192.168.1",
        workers: 20,
      }),
    });

    const data = await response.json();

    if (data.success) {
      scannedDevices = data.devices;
      log(
        `Found ${data.devices.length} device(s) in ${data.elapsed_ms}ms`,
        "log-ok",
      );

      // Update dropdown
      const select = document.getElementById("scan-devices");
      select.innerHTML = '<option value="">— Select device —</option>';
      scannedDevices.forEach((d, idx) => {
        const opt = document.createElement("option");
        opt.value = idx;
        opt.textContent = `${d.ip}:${d.port}`;
        select.appendChild(opt);
      });

      results.innerHTML = `<div style="color: var(--success)">✅ Found ${data.devices.length} device(s)</div>`;
    } else {
      throw new Error(data.error);
    }
  } catch (e) {
    log(`Scan error: ${e.message}`, "log-err");
    results.innerHTML = `<div style="color: var(--danger)">❌ Scan failed: ${e.message}</div>`;
  }
}
```

### Option 2: Use Backend API Directly via Fetch

```javascript
// Example: Get device info
async function getDeviceInfoAPI(ip) {
  try {
    const response = await fetch(`/api/device/${ip}/info`);
    const data = await response.json();

    if (data.success) {
      console.log(`Device: ${data.info}`);
    } else {
      console.error(`Error: ${data.error}`);
    }
  } catch (e) {
    console.error(`Fetch error: ${e.message}`);
  }
}
```

---

## 🐍 Python CLI Usage

### Scan from Python

```python
import subprocess

# Call backend via Python
result = subprocess.run(['python', 'backend.py'],
                       capture_output=True,
                       text=True)
```

### Direct Using requests

```python
import requests

# Scan devices
response = requests.post('http://localhost:5000/api/scan',
                        json={'prefix': '192.168.1', 'workers': 20})
devices = response.json()

for dev in devices['devices']:
    print(f"Found: {dev['ip']} - {dev['info']}")
```

---

## 🚀 Running Both Web UI & Backend

### Option A: Separate Terminals

```bash
# Terminal 1: Backend server
python backend.py          # Runs on http://localhost:5000

# Terminal 2: Keep Python scan tool available
python set_config.py       # For CLI operations
```

### Option B: Frontend only + Backend API

```bash
# Run backend
python backend.py

# Then open in browser
http://localhost:5000      # Serves index.html + API
```

### Option C: PowerShell Task (Advanced)

```powershell
# Run backend in background
Start-Process python -ArgumentList 'backend.py' -NoNewWindow
```

---

## 🔄 Backend vs Python CLI vs Web UI

| Feature         | Backend API   | Python Tool        | Web UI (Direct)  |
| --------------- | ------------- | ------------------ | ---------------- |
| **Scan Speed**  | 3-5 sec       | 3 sec              | 10-15 sec        |
| **Protocol**    | UDP → HTTP    | UDP                | WebSocket        |
| **Reliability** | Very reliable | Very reliable      | Sometimes issues |
| **GUI**         | Yes (web)     | Optional (tkinter) | Yes (web)        |
| **Automation**  | Easy (REST)   | Easy (CLI)         | Difficult        |
| **Best for**    | Production    | Scripting          | Interactive      |

---

## 📊 Performance Notes

- **Scan 254 IPs**: ~3-5 seconds with 20 workers
- **Single device query**: ~50-100ms (UDP)
- **HTTP overhead**: ~10-20ms per request
- **Concurrent limit**: Up to 254 devices simultaneously

---

## 🐛 Troubleshooting

### Backend won't start

```
ERROR: Address already in use
→ Change PORT_BACKEND in backend.py (default: 5000)
```

### Scan via API returns timeout

```
→ Increase UDP_PORT timeout or check WiFi signal
```

### Web UI can't reach backend API

```
→ Make sure backend.py is running first
→ Check http://localhost:5000/api/health
→ Check browser console (F12) for CORS errors
```

### Files don't upload

```
→ Create "uploads/" directory: mkdir uploads
```

---

## 📝 Next Steps

1. **Install Flask**: `pip install -r requirements_backend.txt`
2. **Run backend**: `python backend.py`
3. **Test API**: Visit `http://localhost:5000/api/health`
4. **Update Web UI**: Use API endpoints in scanESPNetworkAPI()
5. **Enjoy** reliable scanning! ✅
