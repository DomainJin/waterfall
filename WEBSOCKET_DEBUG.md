# WebSocket Scan Issue — Debug Guide

## Problem

- **Python scan** ✅ Works (finds devices using UDP port 8888)
- **Web UI scan** ❌ Doesn't work (WebSocket port 3333)

---

## Step 1: Run Diagnostic Test

1. Open `index.html` in browser
2. Go to **"SCAN ESP"** section
3. Make sure "IP Address" field has device IP: `192.168.1.241`
4. Click **"🔧 Test"** button
5. Check the log output:

### Expected Results

**If WebSocket works:**

```
✅ WebSocket port 3333 OK
✅ UDP Config port 8888 responding
✅ Both ports responding! Everything OK
```

**If WebSocket doesn't work but UDP does:**

```
❌ WebSocket error/refused
✅ UDP Config port 8888 responding
⚠️ Config OK but WebSocket issue (try Python scan)
```

---

## Step 2: Check Browser Console

Press `F12` → Console tab → Look for WebSocket errors:

```javascript
// Good:
WebSocket connection established

// Bad:
ERROR: WebSocket connection refused
ERR: Net::ERR_ADDRESS_UNREACHABLE
```

---

## Fixes to Try

### Fix 1: Reload Page (Clear Cache)

```
Ctrl+Shift+Delete → Clear browsing data
Then refresh index.html
```

### Fix 2: Check ESP32 Serial Output

```
Serial Monitor should show:
[SETUP] Starting WebSocket server on port 3333...
[WS] WebSocket server on port 3333
```

If you don't see these, WebSocket server didn't start.

### Fix 3: Try Python Scan Instead

```bash
python set_config.py --scan 192.168.1
```

This is faster and more reliable anyway (uses UDP port 8888).

### Fix 4: Manual Device Connection

Instead of scan:

1. Type IP manually: `192.168.1.241`
2. Type Port: `3333`
3. Click **"Connect"**

If manual connect works → Scan code is the issue  
If manual connect fails → WebSocket server issue

---

## Most Likely Causes

| Cause                         | Symptom                   | Fix                    |
| ----------------------------- | ------------------------- | ---------------------- |
| WebSocket server crashed      | Test shows ❌ WebSocket   | Restart ESP32          |
| Port 3333 blocked by firewall | Test shows ❌ WebSocket   | Check firewall rules   |
| Browser CORS issue            | Console shows CORS error  | Refresh cache + reload |
| Wrong IP address              | Can't connect to anything | Verify IP is correct   |
| WiFi disconnected             | All tests fail            | Reconnect WiFi         |

---

## Recommended: Use Python Tool

Since Python scan works but Web scan doesn't, use Python tool for consistency:

```bash
# Scan network
python set_config.py --scan 192.168.1

# Get device info
python set_config.py --esp 192.168.1.241 --info

# Load animation in Web UI - manual process:
# 1. Open index.html
# 2. Type IP: 192.168.1.241, Port: 3333
# 3. Click "Connect"
# 4. Load animation file + Send
```

---

## What I Fixed This Session

✅ Added diagnostic test button ("🔧 Test")  
✅ Increased WebSocket timeout: 3000ms → 5000ms  
✅ Added retry mechanism (retry once on timeout/error)  
✅ Added elapsed time logging  
✅ Better error messages

Next reload of index.html will have these improvements.

---

## Quick Checklist

- [ ] Reload index.html (clear cache with Ctrl+Shift+Delete)
- [ ] Run "🔧 Test" button to diagnose ports
- [ ] Check ESP32 serial output for "[WS] WebSocket server"
- [ ] Try manual connection (IP + port) to test WebSocket
- [ ] Use Python tool for scanning (if Web UI still fails)
