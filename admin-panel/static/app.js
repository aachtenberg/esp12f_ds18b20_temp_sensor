// ESP Sensor Hub Admin Panel - Client-side JavaScript

// Initialize Socket.IO connection
const socket = io();

// State
let devices = {};
let messages = [];
let currentSleepDevice = null;

// Socket.IO Event Handlers
socket.on('connect', () => {
    console.log('Connected to server');
    updateMQTTStatus(true);
});

socket.on('disconnect', () => {
    console.log('Disconnected from server');
    updateMQTTStatus(false);
});

socket.on('mqtt_status', (data) => {
    updateMQTTStatus(data.connected);
});

socket.on('mqtt_message', (data) => {
    addMessage(data);
    updateDeviceFromMessage(data);
});

socket.on('device_update', (data) => {
    updateDevice(data.device, data.state);
});

socket.on('initial_state', (data) => {
    console.log('Received initial state:', data);
    // Update devices
    for (const [deviceName, state] of Object.entries(data.devices)) {
        updateDevice(deviceName, state);
    }
    // Load messages
    data.messages.forEach(msg => addMessage(msg, false));
});

socket.on('command_response', (data) => {
    if (data.success) {
        showNotification(`Command '${data.command}' sent to ${data.device}`, 'success');
    } else {
        showNotification(`Failed to send command to ${data.device}`, 'error');
    }
});

// MQTT Status
function updateMQTTStatus(connected) {
    const indicator = document.getElementById('mqtt-status-indicator');
    const text = document.getElementById('mqtt-status-text');
    
    if (connected) {
        indicator.classList.remove('offline');
        indicator.classList.add('online');
        text.textContent = 'MQTT: Connected';
    } else {
        indicator.classList.remove('online');
        indicator.classList.add('offline');
        text.textContent = 'MQTT: Disconnected';
    }
}

// Device Management
function updateDevice(deviceName, state) {
    devices[deviceName] = state;
    
    const deviceCard = document.querySelector(`.device-card[data-device="${deviceName}"]`);
    if (!deviceCard) return;
    
    const statusEl = deviceCard.querySelector('.device-status');
    const tempEl = deviceCard.querySelector('.device-temp');
    const lastSeenEl = deviceCard.querySelector('.device-last-seen');
    
    // Determine online/offline based on last_seen timestamp
    let isOnline = false;
    let lastSeenText = 'Never';
    
    if (state.last_seen) {
        const lastSeen = new Date(state.last_seen);
        const now = new Date();
        const diffMs = now - lastSeen;
        const diffSec = Math.floor(diffMs / 1000);
        
        // Consider online if seen in last 5 minutes (300 seconds)
        isOnline = diffSec < 300;
        
        // Format time ago
        if (diffSec < 60) {
            lastSeenText = `${diffSec}s ago`;
        } else if (diffSec < 3600) {
            lastSeenText = `${Math.floor(diffSec / 60)}m ago`;
        } else if (diffSec < 86400) {
            lastSeenText = `${Math.floor(diffSec / 3600)}h ago`;
        } else {
            lastSeenText = `${Math.floor(diffSec / 86400)}d ago`;
        }
    }
    
    // Update status indicator
    if (isOnline) {
        statusEl.textContent = 'Online';
        statusEl.classList.remove('offline');
        statusEl.classList.add('online');
    } else {
        statusEl.textContent = 'Offline';
        statusEl.classList.remove('online');
        statusEl.classList.add('offline');
    }
    
    // Update last seen
    lastSeenEl.textContent = `Last seen: ${lastSeenText}`;
    
    // Update temperature if available
    if (state.temperature && state.temperature.payload) {
        const temp = state.temperature.payload.current_temp_c || 
                     state.temperature.payload.temperature_c;
        if (temp !== undefined) {
            tempEl.textContent = `${temp.toFixed(1)}°C`;
        }
    }
}

function updateDeviceFromMessage(data) {
    const deviceCard = document.querySelector(`.device-card[data-device="${data.device}"]`);
    if (!deviceCard) return;
    
    if (data.type === 'temperature' && data.payload) {
        const tempEl = deviceCard.querySelector('.device-temp');
        const temp = data.payload.current_temp_c || data.payload.temperature_c;
        if (temp !== undefined) {
            tempEl.textContent = `${temp.toFixed(1)}°C`;
        }
    }
}

// Message Management
function addMessage(data, shouldAutoScroll = true) {
    messages.push(data);
    
    // Apply filter
    const filter = document.getElementById('message-filter')?.value || 'all';
    if (filter !== 'all' && data.type !== filter) {
        return;
    }
    
    const container = document.getElementById('messages-container');
    if (!container) return;
    
    const messageEl = document.createElement('div');
    messageEl.className = `message-item ${data.type}`;
    
    const timestamp = new Date(data.timestamp).toLocaleTimeString();
    const payloadStr = typeof data.payload === 'object' 
        ? JSON.stringify(data.payload, null, 2) 
        : data.payload;
    
    messageEl.innerHTML = `
        <div class="message-header">
            <span class="message-topic">${data.topic}</span>
            <span class="message-time">${timestamp}</span>
        </div>
        <div class="message-payload">${escapeHtml(payloadStr)}</div>
    `;
    
    container.appendChild(messageEl);
    
    // Auto-scroll if enabled and requested
    const autoScrollCheckbox = document.getElementById('auto-scroll');
    if (shouldAutoScroll && autoScrollCheckbox && autoScrollCheckbox.checked) {
        container.scrollTop = container.scrollHeight;
    }
    
    // Limit messages in DOM
    while (container.children.length > 100) {
        container.removeChild(container.firstChild);
    }
}

function clearMessages() {
    document.getElementById('messages-container').innerHTML = '';
    messages = [];
}

// Command Functions
function sendCommand(device, command) {
    console.log(`Sending command '${command}' to ${device}`);
    socket.emit('send_command', { device, command });
    
    // Add to message log
    addMessage({
        topic: `esp-sensor-hub/${device}/command`,
        payload: command,
        timestamp: new Date().toISOString(),
        device: device,
        type: 'command'
    });
}

function showDeepSleepModal(device) {
    currentSleepDevice = device;
    document.getElementById('sleep-device-name').textContent = device;
    document.getElementById('sleep-modal').style.display = 'block';
}

function closeSleepModal() {
    document.getElementById('sleep-modal').style.display = 'none';
    currentSleepDevice = null;
}

function sendDeepSleepCommand() {
    const seconds = document.getElementById('sleep-seconds').value;
    if (currentSleepDevice && seconds !== null) {
        sendCommand(currentSleepDevice, `deepsleep ${seconds}`);
        closeSleepModal();
    }
}

// Filters
document.getElementById('device-filter')?.addEventListener('input', (e) => {
    const filter = e.target.value.toLowerCase();
    document.querySelectorAll('.device-card').forEach(card => {
        const deviceName = card.dataset.device.toLowerCase();
        card.style.display = deviceName.includes(filter) ? 'block' : 'none';
    });
});

document.getElementById('status-filter')?.addEventListener('change', (e) => {
    const filter = e.target.value;
    document.querySelectorAll('.device-card').forEach(card => {
        const status = card.querySelector('.device-status').classList.contains('online');
        if (filter === 'all') {
            card.style.display = 'block';
        } else if (filter === 'online' && status) {
            card.style.display = 'block';
        } else if (filter === 'offline' && !status) {
            card.style.display = 'block';
        } else {
            card.style.display = 'none';
        }
    });
});

document.getElementById('message-filter')?.addEventListener('change', () => {
    const container = document.getElementById('messages-container');
    container.innerHTML = '';
    messages.forEach(msg => addMessage(msg, false));
});

// Utility Functions
function escapeHtml(text) {
    const map = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#039;'
    };
    return text.replace(/[&<>"']/g, m => map[m]);
}

function showNotification(message, type = 'info') {
    // Simple notification - can be enhanced with a proper notification library
    console.log(`[${type.toUpperCase()}] ${message}`);
    // TODO: Add visual notification
}

// Close modal on outside click
window.onclick = function(event) {
    const modal = document.getElementById('sleep-modal');
    if (event.target === modal) {
        closeSleepModal();
    }
}

// Periodic device status check (mark offline if no updates)
setInterval(() => {
    document.querySelectorAll('.device-card').forEach(card => {
        const deviceName = card.dataset.device;
        const state = devices[deviceName];
        if (state && state.last_seen) {
            updateDevice(deviceName, state);
        }
    });
}, 10000); // Check every 10 seconds
