// Control functions for Waterfall ESP32
// Uses WebSocket directly (like index.html) for device control

let ws = null;
let wsConnected = false;

function getSelectedDevice() {
    const configAllDevices = JSON.parse(localStorage.getItem('configAllDevices') || '[]');
    const configAllActive = localStorage.getItem('configAllActive') === 'true';
    
    if (configAllActive && configAllDevices.length > 0) {
        console.log('Config All mode active:', configAllDevices.length, 'devices');
        return configAllDevices.map(d => ({
            ip: d.ip,
            port: d.port
        }));
    }
    
    const ip = localStorage.getItem('selected_esp_ip');
    const port = localStorage.getItem('selected_esp_port');
    
    if (!ip || !port) {
        alert('⚠️ Chưa chọn thiết bị!\n\nVui lòng vào tab HOME và click "Access" trên thiết bị muốn điều khiển.');
        return null;
    }
    
    return { ip, port: parseInt(port) };
}

// WebSocket connection (like index.html)
function connectWS(ip, port) {
    return new Promise((resolve, reject) => {
        if (ws && wsConnected) {
            resolve(ws);
            return;
        }
        
        try {
            ws = new WebSocket(`ws://${ip}:${port}`);
            ws.binaryType = 'arraybuffer';
            
            ws.onopen = () => {
                wsConnected = true;
                console.log('WS connected');
                resolve(ws);
            };
            
            ws.onerror = (e) => {
                console.error('WS error:', e);
                reject(e);
            };
            
            ws.onclose = () => {
                wsConnected = false;
                ws = null;
            };
            
            // Timeout after 3 seconds
            setTimeout(() => {
                if (!wsConnected) {
                    reject(new Error('Connection timeout'));
                }
            }, 3000);
            
        } catch (e) {
            reject(e);
        }
    });
}

function sendCommand(command) {
    const device = getSelectedDevice();
    if (!device) return Promise.reject('No device selected');
    
    const ip = device.ip;
    const port = device.port || 3333;
    
    console.log(`Sending via WebSocket to ${ip}:${port}:`, command);
    
    return new Promise((resolve, reject) => {
        // For text commands, we need to send via UDP (only GET_INFO supported)
        // For frame data, use WebSocket (like index.html)
        
        if (command === 'GET_INFO' || command.startsWith('SET_')) {
            // Use UDP via backend
            fetch('/api/send-command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ip: ip, port: port, command: command })
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) resolve(true);
                else reject(new Error(data.error || 'Failed'));
            })
            .catch(e => reject(e));
        } else {
            // For other commands, show info
            console.log('Command not supported via this API. Use Control tab (index.html) for full control.');
            resolve(false);
        }
    });
}

// Get device info via UDP
function getDeviceInfo() {
    const device = getSelectedDevice();
    if (!device) return Promise.reject('No device selected');
    
    return fetch('/api/send-command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ 
            ip: device.ip, 
            port: 8888, // Config port
            command: 'GET_INFO' 
        })
    })
    .then(r => r.json())
    .then(data => {
        return data;
    });
}

// Valve Control Functions via WebSocket (use Control tab for full control)
function allValvesOn() {
    alert('Sử dụng tab Control (index.html) để điều khiển van - tab này chỉ giám sát');
    return Promise.resolve(false);
}

function allValvesOff() {
    alert('Sử dụng tab Control (index.html) để điều khiển van - tab này chỉ giám sát');
    return Promise.resolve(false);
}

function testValve(num) {
    alert('Sử dụng tab Control (index.html) để test van - tab này chỉ giám sát');
    return Promise.resolve(false);
}

function testBoardSequence() {
    alert('Sử dụng tab Control (index.html) để test - tab này chỉ giám sát');
    return Promise.resolve(false);
}

function resetDevice() {
    // Reset via UDP
    const device = getSelectedDevice();
    if (!device) return Promise.reject('No device selected');
    
    return fetch('/api/send-command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ip: device.ip, port: 8888, command: 'RESET' })
    })
    .then(r => r.json());
}

function startStream() {
    alert('Sử dụng tab Control (index.html) để gửi animation');
    return Promise.resolve(false);
}

function stopStream() {
    return allValvesOff();
}

// Configuration functions
function setBoardCount(count) {
    alert('Cấu hình board trong tab Control (index.html)');
    const display = document.getElementById('board-count-display');
    if (display) display.textContent = count;
    localStorage.setItem('numBoards', count);
    return Promise.resolve(true);
}

function setRowInterval(ms) {
    alert('Cài đặt timing trong tab Control (index.html)');
    return Promise.resolve(true);
}

function setDropHeight(cm) {
    alert('Cài đặt height trong tab Control (index.html)');
    return Promise.resolve(true);
}

// Display selected device info
function updateDeviceInfo() {
    const device = getSelectedDevice();
    const infoElements = document.querySelectorAll('.device-info-display');
    
    infoElements.forEach(el => {
        if (device) {
            el.textContent = `🎯 Đang điều khiển: ${device.ip}:${device.port}`;
            el.style.color = '#27ae60';
        } else {
            el.textContent = '⚠️ Chưa chọn thiết bị';
            el.style.color = '#e74c3c';
        }
    });
}

// Custom command sender
function sendCustomCommand() {
    const command = document.getElementById('custom-command').value;
    if (!command.trim()) {
        alert('Vui lòng nhập command!');
        return;
    }
    
    sendCommand(command).then(success => {
        if (success) {
            document.getElementById('command-status').textContent = `✅ Đã gửi: ${command}`;
            document.getElementById('command-status').style.color = '#27ae60';
        } else {
            document.getElementById('command-status').textContent = '❌ Lỗi gửi command';
            document.getElementById('command-status').style.color = '#e74c3c';
        }
    });
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', updateDeviceInfo);