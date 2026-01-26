// ESP Sensor Hub Admin Panel v2 - Modern Sidebar Layout
// Initialize Socket.IO connection
const socket = io();

// Register mqtt_message handler immediately
socket.on('mqtt_message', (data) => {
    addMessage(data);
    updateDashboardStats();
    
    // Check if this message confirms a command success
    if (data.type === 'events' && data.payload) {
        const message = typeof data.payload === 'string' ? data.payload : JSON.stringify(data.payload);
        checkCommandSuccess(data.device, message);
    }
    
    // Update device modal if open and message is for current device
    if (currentDevice && data.device === currentDevice) {
        renderDeviceLog();
        // Update message count
        const deviceMessages = messages.filter(m => m.device === currentDevice);
        document.getElementById('device-modal-msg-count').textContent = deviceMessages.length;
    }
});

// WORKAROUND: Since MQTT→Socket.IO emits don't work from background thread,
// request fresh state every N seconds to get new messages
let pollingInterval = parseInt(localStorage.getItem('pollingInterval')) || 10;
let pollingTimer = null;

function startPolling() {
    if (pollingTimer) {
        clearInterval(pollingTimer);
    }
    pollingTimer = setInterval(() => {
        socket.emit('request_state');
    }, pollingInterval * 1000);
    console.log(`Polling started: ${pollingInterval}s interval`);
}

function updatePollingInterval() {
    const input = document.getElementById('polling-interval');
    const newInterval = parseInt(input.value);
    
    if (newInterval < 1 || newInterval > 60) {
        showToast('Polling interval must be between 1-60 seconds', 'error', 3000);
        return;
    }
    
    pollingInterval = newInterval;
    localStorage.setItem('pollingInterval', pollingInterval);
    document.getElementById('current-polling').textContent = `${pollingInterval}s`;
    
    startPolling();
    showToast(`Polling interval updated to ${pollingInterval} seconds`, 'success', 3000);
}

// Start polling on page load
startPolling();

// Global State
let devices = {};
let messages = [];
let currentSleepDevice = null;
let currentDevice = null;  // Track device with open modal
let currentSection = 'dashboard';
let messageStats = { lastMinute: 0, startTime: Date.now() };
let hiddenDevices = new Set();  // Devices hidden by user
let activeToasts = new Map();  // Track active toasts by key for dismissal
let useUTC = localStorage.getItem('useUTC') === 'true' || false;  // Timezone preference

// Load hidden devices from localStorage
function loadHiddenDevices() {
    const stored = localStorage.getItem('hiddenDevices');
    if (stored) {
        hiddenDevices = new Set(JSON.parse(stored));
    }
}

// Save hidden devices to localStorage
function saveHiddenDevices() {
    localStorage.setItem('hiddenDevices', JSON.stringify(Array.from(hiddenDevices)));
}

// Load hidden devices on startup
loadHiddenDevices();

// ============================================
// INITIALIZATION
// ============================================

document.addEventListener('DOMContentLoaded', () => {
    initializeSidebar();
    initializeFilters();
    initializeMQTTForm();
    loadDeviceInventory();
    
    // Load saved polling interval into settings input
    const pollingInput = document.getElementById('polling-interval');
    if (pollingInput) {
        pollingInput.value = pollingInterval;
        document.getElementById('current-polling').textContent = `${pollingInterval}s`;
    }
});

// ============================================
// SIDEBAR NAVIGATION
// ============================================

function initializeSidebar() {
    const navItems = document.querySelectorAll('.nav-item');
    const sidebarToggle = document.getElementById('sidebar-toggle');
    const sidebar = document.querySelector('.sidebar');
    
    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const section = item.dataset.section;
            switchSection(section);
            updateActiveNav(item);
        });
    });
    
    if (sidebarToggle) {
        sidebarToggle.addEventListener('click', () => {
            sidebar.classList.toggle('collapsed');
        });
    }
}

function updateActiveNav(activeItem) {
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
    });
    activeItem.classList.add('active');
}

function switchSection(sectionName) {
    // Hide all sections
    document.querySelectorAll('.content-section').forEach(section => {
        section.classList.remove('active');
    });
    
    // Show selected section
    const section = document.getElementById(`${sectionName}-section`);
    if (section) {
        section.classList.add('active');
    }
    
    // Update title
    const titles = {
        dashboard: 'Dashboard',
        devices: 'Devices',
        messages: 'MQTT Log',
        settings: 'Settings'
    };
    document.getElementById('section-title').textContent = titles[sectionName] || 'Dashboard';
    
    currentSection = sectionName;
    
    // Render content based on section
    if (sectionName === 'devices') {
        renderDevicesTable();
    } else if (sectionName === 'settings') {
        populateDeviceVisibilityList(devices);
    }
}

// ============================================
// TOAST NOTIFICATION SYSTEM
// ============================================

function showToast(message, type = 'info', duration = 4000, key = null) {
    const container = document.getElementById('toast-container');
    
    // If key provided and toast exists, remove old one first
    if (key && activeToasts.has(key)) {
        dismissToast(key);
    }
    
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    const icons = {
        success: '✓',
        error: '✗',
        warning: '⚠',
        info: 'ℹ'
    };
    
    toast.innerHTML = `
        <span class="toast-icon">${icons[type]}</span>
        <span class="toast-message">${message}</span>
    `;
    
    container.appendChild(toast);
    
    // Track toast by key if provided
    if (key) {
        activeToasts.set(key, toast);
    }
    
    if (duration > 0) {
        setTimeout(() => {
            toast.style.animation = 'slideOut 0.3s ease';
            setTimeout(() => {
                toast.remove();
                if (key) activeToasts.delete(key);
            }, 300);
        }, duration);
    }
    
    return toast;
}

function dismissToast(key) {
    if (activeToasts.has(key)) {
        const toast = activeToasts.get(key);
        toast.style.animation = 'slideOut 0.3s ease';
        setTimeout(() => {
            toast.remove();
            activeToasts.delete(key);
        }, 300);
    }
}

// ============================================
// MQTT STATUS
// ============================================

function updateMQTTStatus(connected) {
    // Only update dashboard MQTT health indicator
    const health = document.getElementById('mqtt-health');
    if (health) {
        health.textContent = connected ? '🟢 Connected' : '🔴 Disconnected';
        health.style.color = connected ? '#10b981' : '#ef4444';
    }
}

// Socket.IO Event Handlers
socket.on('connect', () => {
    console.log('[Socket.IO] Connected');
});

socket.on('disconnect', () => {
    console.log('[Socket.IO] ✗ Disconnected from server');
    updateMQTTStatus(false);
});

socket.on('mqtt_status', (data) => {
    console.log('[Socket.IO] mqtt_status event:', data);
    updateMQTTStatus(data.connected);
    if (!data.connected) {
        showToast(`MQTT Disconnected from ${data.broker || 'broker'}`, 'error', 5000);
    } else {
        showToast(`MQTT Connected to ${data.broker || 'broker'}`, 'success', 3000);
    }
});

socket.on('device_update', (data) => {
    devices[data.device] = data.state;
    if (currentSection === 'devices') {
        renderDevicesTable();
    }
    updateDashboardStats();
    
    // Update device modal if open and update is for current device
    if (currentDevice && data.device === currentDevice) {
        const state = data.state;
        const isOnline = isDeviceOnline(state);
        const statusBadge = document.getElementById('device-modal-status');
        statusBadge.textContent = isOnline ? 'Online' : 'Offline';
        statusBadge.className = `status-badge ${isOnline ? 'status-online' : 'status-offline'}`;
        
        document.getElementById('device-modal-last-seen').textContent = formatLastSeen(state?.last_seen);
    }
});

socket.on('initial_state', (data) => {
    // Update devices
    for (const [deviceName, state] of Object.entries(data.devices)) {
        devices[deviceName] = state;
    }
    
    // Replace messages array with fresh data from server
    messages = data.messages;
    
    // Check new messages for command success confirmations
    data.messages.forEach(msg => {
        if (msg.type === 'events' && msg.payload) {
            const message = typeof msg.payload === 'string' ? msg.payload : JSON.stringify(msg.payload);
            checkCommandSuccess(msg.device, message);
        }
    });
    
    // Always refresh the MQTT log display if container exists (handles polling updates)
    const container = document.getElementById('messages-container');
    if (container && container.parentElement && container.parentElement.offsetParent !== null) {
        // Store current scroll position to check if user is at bottom
        const wasAtBottom = container.scrollHeight - container.scrollTop <= container.clientHeight + 50;
        
        // Re-render visible messages based on current filters
        container.innerHTML = '';
        data.messages.forEach(msg => {
            const typeFilter = document.getElementById('message-filter')?.value || 'all';
            const deviceFilter = document.getElementById('device-message-filter')?.value || 'all';
            const topicFilter = document.getElementById('topic-filter')?.value?.toLowerCase() || '';
            
            if (typeFilter !== 'all' && msg.type !== typeFilter) return;
            if (deviceFilter !== 'all' && msg.device !== deviceFilter) return;
            if (topicFilter && !msg.topic.toLowerCase().includes(topicFilter)) return;
            
            const messageEl = document.createElement('div');
            messageEl.className = `message-item ${msg.type}`;
            
            const timestamp = new Date(msg.timestamp).toLocaleTimeString();
            const payloadStr = typeof msg.payload === 'object'
                ? JSON.stringify(msg.payload, null, 2)
                : msg.payload;
            
            messageEl.innerHTML = `
                <div class="message-header">
                    <span class="message-topic">${msg.topic}</span>
                    <span class="message-time">${timestamp}</span>
                </div>
                <div class="message-payload">${escapeHtml(payloadStr)}</div>
            `;
            
            container.appendChild(messageEl);
            
            // Limit DOM size
            while (container.children.length > 100) {
                container.removeChild(container.firstChild);
            }
        });
        
        // Auto-scroll to show latest messages if enabled AND user was already at bottom
        const autoScrollCheckbox = document.getElementById('auto-scroll');
        if (autoScrollCheckbox?.checked && wasAtBottom) {
            requestAnimationFrame(() => {
                container.scrollTop = container.scrollHeight;
            });
        }
    }
    
    // Update MQTT connection status from initial state
    if (data.mqtt_connected !== undefined) {
        updateMQTTStatus(data.mqtt_connected);
    }
    
    // Populate filters and visibility
    populateDeviceFilters(data.devices);
    populateDeviceTopicFilter(data.devices);
    populateDeviceVisibilityList(data.devices);
    
    // Update dashboard
    updateDashboardStats();
    initializeFilters();
    
    // If device drawer is open, re-render to show new messages
    if (currentDevice) {
        renderDeviceLog();
    }
});

socket.on('command_response', (data) => {
    // Dismiss the loading toast
    const toastKey = `cmd_${data.device}_${data.command}`;
    dismissToast(toastKey);
    
    // Show result toast
    if (data.success) {
        showToast(`✓ Command '${data.command}' sent to ${data.device}`, 'success', 4000);
    } else {
        showToast(`✗ Failed to send '${data.command}' to ${data.device}`, 'error', 5000);
    }
});

// ============================================
// DEVICE INVENTORY & TABLE
// ============================================

function loadDeviceInventory() {
    // Trigger API call to load initial devices
    fetch('/api/devices')
        .then(r => r.json())
        .then(devicesList => {
            devicesList.forEach(dev => {
                if (!devices[dev.mqtt_name]) {
                    devices[dev.mqtt_name] = {};
                }
            });
            populateDeviceFilters(devices);
            populateDeviceTopicFilter(devices);
            populateDeviceVisibilityList(devices);
        })
        .catch(err => console.error('Error loading devices:', err));
}

function renderDevicesTable() {
    const tbody = document.getElementById('devices-tbody');
    if (!tbody) return;
    
    tbody.innerHTML = '';
    
    // Apply filters
    const nameFilter = document.getElementById('device-filter')?.value?.toLowerCase() || '';
    const statusFilter = document.getElementById('status-filter')?.value || 'all';
    const topicFilter = document.getElementById('device-topic-filter')?.value || 'all';
    
    const filteredDevices = Object.entries(devices).filter(([name, state]) => {
        // Hidden device filter
        if (hiddenDevices.has(name)) return false;
        
        // Name filter
        if (nameFilter && !name.toLowerCase().includes(nameFilter)) return false;
        
        // Status filter
        if (statusFilter !== 'all') {
            const isOnline = isDeviceOnline(state);
            if (statusFilter === 'online' && !isOnline) return false;
            if (statusFilter === 'offline' && isOnline) return false;
        }
        
        // Topic filter
        if (topicFilter !== 'all' && state) {
            const topics = Object.keys(state).filter(k => k !== 'last_seen');
            if (!topics.includes(topicFilter)) return false;
        }
        
        return true;
    });
    
    // Sort online devices first
    filteredDevices.sort(([, a], [, b]) => {
        const aOnline = isDeviceOnline(a);
        const bOnline = isDeviceOnline(b);
        return bOnline - aOnline;
    });
    
    // Render rows
    filteredDevices.forEach(([name, state]) => {
        const row = createDeviceRow(name, state);
        tbody.appendChild(row);
    });
    
    // Update dashboard counts
    updateDeviceCounts();
}

function createDeviceRow(deviceName, state) {
    const row = document.createElement('tr');
    const isOnline = isDeviceOnline(state);
    const type = getDeviceType(state);
    const ip = getDeviceIP(state);
    const rssi = getDeviceRSSI(state);
    const deepSleep = getDeepSleepStatus(state);
    
    row.style.cursor = 'pointer';
    row.onclick = () => {
        openDeviceDetails(deviceName);
    };
    
    row.innerHTML = `
        <td><strong>${deviceName}</strong></td>
        <td>
            <div class="device-row-status">
                <span class="status-dot ${isOnline ? 'online' : 'offline'}"></span>
                ${isOnline ? 'Online' : 'Offline'}
            </div>
        </td>
        <td><small>${type}</small></td>
        <td><code>${ip || '--'}</code></td>
        <td>${rssi ? rssi + ' dBm' : '--'}</td>
        <td>${deepSleep}</td>
    `;
    
    return row;
}

function getDeviceTemperature(state) {
    if (!state || !state.temperature) return null;
    
    const payload = state.temperature.payload;
    if (!payload) return null;
    
    // Try multiple temperature field names
    let temp = payload.current_temp_c || 
               payload.temperature_c || 
               payload.temp_c ||
               payload.current_temp ||
               payload.temperature;
    
    if (temp !== undefined && temp !== null) {
        return `${parseFloat(temp).toFixed(1)}°C`;
    }
    
    return null;
}

function getDeviceType(state) {
    // Determine device type based on available topics
    if (!state) return 'Unknown';
    
    const topics = Object.keys(state).filter(k => k !== 'last_seen');
    
    if (topics.includes('temperature')) return 'Temp Sensor';
    if (topics.includes('camera')) return 'Camera';
    if (topics.includes('solar')) return 'Solar Monitor';
    
    // Fallback to chip ID if available
    if (state.status?.payload?.chip_id) {
        const chipId = state.status.payload.chip_id;
        if (chipId.startsWith('E09806')) return 'ESP8266';
        if (chipId.startsWith('80F3DA')) return 'ESP32';
        if (chipId.startsWith('ECE334')) return 'ESP32';
        if (chipId.startsWith('2805A5')) return 'ESP32';
    }
    
    return 'Generic';
}

function getDeviceIP(state) {
    if (state.status && state.status.payload && state.status.payload.ip) {
        return state.status.payload.ip;
    }
    return null;
}

function getDeviceRSSI(state) {
    if (state.status && state.status.payload && state.status.payload.wifi_rssi) {
        return state.status.payload.wifi_rssi;
    }
    return null;
}

function getDeepSleepStatus(state) {
    if (!state.status?.payload) return '--';
    
    const payload = state.status.payload;
    if (payload.deep_sleep_enabled === true) {
        const seconds = payload.deep_sleep_seconds || 0;
        return `<span style="color: var(--warning-color);">⏸️ ${seconds}s</span>`;
    }
    
    return '<span style="color: var(--text-secondary);">Disabled</span>';
}

function getDeviceTopics(state) {
    const topics = Object.keys(state).filter(k => k !== 'last_seen');
    return topics.length > 0 ? topics.join(', ') : 'none';
}

function isDeviceOnline(state) {
    if (!state || !state.last_seen) return false;
    const lastSeen = new Date(state.last_seen);
    const now = new Date();
    const diffSec = Math.floor((now - lastSeen) / 1000);
    return diffSec < 300; // 5 minutes
}

function updateDeviceCounts() {
    const online = Object.values(devices).filter(d => isDeviceOnline(d)).length;
    const total = Object.keys(devices).length;
    
    const onlineEl = document.getElementById('active-devices-count');
    const totalEl = document.getElementById('total-devices-count');
    
    if (onlineEl) onlineEl.textContent = online;
    if (totalEl) totalEl.textContent = total;
}

// ============================================
// FILTERS
// ============================================

function initializeFilters() {
    const deviceFilter = document.getElementById('device-filter');
    const statusFilter = document.getElementById('status-filter');
    const deviceTopicFilter = document.getElementById('device-topic-filter');
    const messageFilter = document.getElementById('message-filter');
    const deviceMessageFilter = document.getElementById('device-message-filter');
    const topicFilter = document.getElementById('topic-filter');
    
    // Device filters
    if (deviceFilter) {
        deviceFilter.addEventListener('input', () => {
            if (currentSection === 'devices') renderDevicesTable();
        });
    }
    if (statusFilter) {
        statusFilter.addEventListener('change', () => {
            if (currentSection === 'devices') renderDevicesTable();
        });
    }
    if (deviceTopicFilter) {
        deviceTopicFilter.addEventListener('change', () => {
            if (currentSection === 'devices') renderDevicesTable();
        });
    }
    
    // Message filters
    if (messageFilter) {
        messageFilter.addEventListener('change', reFilterMessages);
    }
    if (deviceMessageFilter) {
        deviceMessageFilter.addEventListener('change', reFilterMessages);
    }
    if (topicFilter) {
        topicFilter.addEventListener('input', reFilterMessages);
    }
}

function populateDeviceFilters(deviceStates) {
    // Populate device message filter
    const deviceFilter = document.getElementById('device-message-filter');
    if (!deviceFilter) return;
    
    while (deviceFilter.children.length > 1) {
        deviceFilter.removeChild(deviceFilter.lastChild);
    }
    
    const deviceNames = Object.keys(deviceStates).sort();
    deviceNames.forEach(name => {
        const option = document.createElement('option');
        option.value = name;
        option.textContent = name;
        deviceFilter.appendChild(option);
    });
}

function populateDeviceTopicFilter(deviceStates) {
    // Collect unique topics
    const topics = new Set();
    Object.values(deviceStates).forEach(state => {
        if (state) {
            Object.keys(state).forEach(key => {
                if (key !== 'last_seen') {
                    topics.add(key);
                }
            });
        }
    });
    
    const topicFilter = document.getElementById('device-topic-filter');
    if (!topicFilter) return;
    
    while (topicFilter.children.length > 1) {
        topicFilter.removeChild(topicFilter.lastChild);
    }
    
    Array.from(topics).sort().forEach(topic => {
        const option = document.createElement('option');
        option.value = topic;
        option.textContent = topic.charAt(0).toUpperCase() + topic.slice(1);
        topicFilter.appendChild(option);
    });
}

function populateDeviceVisibilityList(deviceStates) {
    const container = document.getElementById('device-visibility-list');
    if (!container) return;
    
    container.innerHTML = '';
    
    const deviceNames = Object.keys(deviceStates).sort();
    if (deviceNames.length === 0) {
        container.innerHTML = '<p style="color: var(--text-tertiary); padding: 10px;">No devices found</p>';
        return;
    }
    
    deviceNames.forEach(name => {
        const state = deviceStates[name];
        const isOnline = isDeviceOnline(state);
        const isHidden = hiddenDevices.has(name);
        
        const item = document.createElement('div');
        item.className = 'device-visibility-item';
        item.innerHTML = `
            <input type="checkbox" id="visibility-${name}" ${!isHidden ? 'checked' : ''}>
            <span class="device-status-badge ${isOnline ? 'online' : 'offline'}"></span>
            <label for="visibility-${name}">${name}</label>
        `;
        
        const checkbox = item.querySelector('input');
        checkbox.addEventListener('change', () => {
            toggleDeviceVisibility(name, !checkbox.checked);
        });
        
        container.appendChild(item);
    });
}

function toggleDeviceVisibility(deviceName, hide) {
    if (hide) {
        hiddenDevices.add(deviceName);
    } else {
        hiddenDevices.delete(deviceName);
    }
    saveHiddenDevices();
    
    // Re-render devices table if visible
    if (currentSection === 'devices') {
        renderDevicesTable();
    }
}

// ============================================
// MESSAGES
// ============================================

function addMessage(data, shouldAutoScroll = true) {
    messages.push(data);
    if (messages.length > 100) {
        messages.shift();
    }
    
    // Only render if filtering would show it
    const typeFilter = document.getElementById('message-filter')?.value || 'all';
    const deviceFilter = document.getElementById('device-message-filter')?.value || 'all';
    const topicFilter = document.getElementById('topic-filter')?.value?.toLowerCase() || '';
    
    if (typeFilter !== 'all' && data.type !== typeFilter) return;
    if (deviceFilter !== 'all' && data.device !== deviceFilter) return;
    if (topicFilter && !data.topic.toLowerCase().includes(topicFilter)) return;
    
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
    
    // Auto-scroll
    const autoScrollCheckbox = document.getElementById('auto-scroll');
    if (shouldAutoScroll && autoScrollCheckbox?.checked) {
        container.scrollTop = container.scrollHeight;
    }
    
    // Limit DOM size
    while (container.children.length > 100) {
        container.removeChild(container.firstChild);
    }
}

function clearMessages() {
    document.getElementById('messages-container').innerHTML = '';
    messages = [];
    showToast('Messages cleared', 'info', 2000);
}

function reFilterMessages() {
    const container = document.getElementById('messages-container');
    if (!container) return;
    
    container.innerHTML = '';
    
    messages.forEach(msg => {
        const typeFilter = document.getElementById('message-filter')?.value || 'all';
        const deviceFilter = document.getElementById('device-message-filter')?.value || 'all';
        const topicFilter = document.getElementById('topic-filter')?.value?.toLowerCase() || '';
        
        if (typeFilter !== 'all' && msg.type !== typeFilter) return;
        if (deviceFilter !== 'all' && msg.device !== deviceFilter) return;
        if (topicFilter && !msg.topic.toLowerCase().includes(topicFilter)) return;
        
        const messageEl = document.createElement('div');
        messageEl.className = `message-item ${msg.type}`;
        
        const timestamp = new Date(msg.timestamp).toLocaleTimeString();
        const payloadStr = typeof msg.payload === 'object'
            ? JSON.stringify(msg.payload, null, 2)
            : msg.payload;
        
        messageEl.innerHTML = `
            <div class="message-header">
                <span class="message-topic">${msg.topic}</span>
                <span class="message-time">${timestamp}</span>
            </div>
            <div class="message-payload">${escapeHtml(payloadStr)}</div>
        `;
        
        container.appendChild(messageEl);
    });
}

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

// Message Management
function addMessage(data, shouldAutoScroll = true) {
    messages.push(data);
    if (messages.length > 100) {
        messages.shift();
    }
    
    // Apply filters
    const typeFilter = document.getElementById('message-filter')?.value || 'all';
    const deviceFilter = document.getElementById('device-message-filter')?.value || 'all';
    const topicFilter = document.getElementById('topic-filter')?.value?.toLowerCase() || '';
    
    if (typeFilter !== 'all' && data.type !== typeFilter) return;
    if (deviceFilter !== 'all' && data.device !== deviceFilter) return;
    if (topicFilter && !data.topic.toLowerCase().includes(topicFilter)) return;
    
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
    
    // Auto-scroll
    const autoScrollCheckbox = document.getElementById('auto-scroll');
    if (shouldAutoScroll && autoScrollCheckbox?.checked) {
        container.scrollTop = container.scrollHeight;
    }
    
    // Limit DOM size
    while (container.children.length > 100) {
        container.removeChild(container.firstChild);
    }
}

function clearMessages() {
    document.getElementById('messages-container').innerHTML = '';
    messages = [];
    showToast('Messages cleared', 'info', 2000);
}

function reFilterMessages() {
    const container = document.getElementById('messages-container');
    if (!container) return;
    
    container.innerHTML = '';
    
    messages.forEach(msg => {
        const typeFilter = document.getElementById('message-filter')?.value || 'all';
        const deviceFilter = document.getElementById('device-message-filter')?.value || 'all';
        const topicFilter = document.getElementById('topic-filter')?.value?.toLowerCase() || '';
        
        if (typeFilter !== 'all' && msg.type !== typeFilter) return;
        if (deviceFilter !== 'all' && msg.device !== deviceFilter) return;
        if (topicFilter && !msg.topic.toLowerCase().includes(topicFilter)) return;
        
        const messageEl = document.createElement('div');
        messageEl.className = `message-item ${msg.type}`;
        
        const timestamp = new Date(msg.timestamp).toLocaleTimeString();
        const payloadStr = typeof msg.payload === 'object'
            ? JSON.stringify(msg.payload, null, 2)
            : msg.payload;
        
        messageEl.innerHTML = `
            <div class="message-header">
                <span class="message-topic">${msg.topic}</span>
                <span class="message-time">${timestamp}</span>
            </div>
            <div class="message-payload">${escapeHtml(payloadStr)}</div>
        `;
        
        container.appendChild(messageEl);
    });
}

// ============================================
// DASHBOARD STATS
// ============================================

function updateDashboardStats() {
    updateDeviceCounts();
    updateMessageRate();
    updateRecentActivity();
}

function updateMessageRate() {
    const rateEl = document.getElementById('messages-rate');
    if (!rateEl) return;
    
    const oneMinuteAgo = Date.now() - (60 * 1000);
    const recentMessages = messages.filter(msg => {
        const msgTime = new Date(msg.timestamp).getTime();
        return msgTime > oneMinuteAgo;
    });
    
    rateEl.textContent = recentMessages.length;
}

function updateRecentActivity() {
    const container = document.getElementById('recent-messages-list');
    if (!container) return;
    
    container.innerHTML = '';
    const recent = messages.slice(-5).reverse();
    
    recent.forEach(msg => {
        const time = new Date(msg.timestamp).toLocaleTimeString();
        const el = document.createElement('div');
        el.style.cssText = 'padding: 8px; border-bottom: 1px solid #374151; font-size: 0.85rem;';
        el.innerHTML = `
            <div><span style="color: #06b6d4;">${msg.topic}</span> - <span style="color: #9ca3af;">${time}</span></div>
        `;
        container.appendChild(el);
    });
}

// ============================================
// MQTT CONFIG FORM
// ============================================

function initializeMQTTForm() {
    const form = document.getElementById('mqtt-config-form');
    if (!form) return;
    
    form.addEventListener('submit', (e) => {
        e.preventDefault();
        submitMQTTConfig();
    });
    
    // Load current config
    fetch('/api/config')
        .then(r => r.json())
        .then(config => {
            if (config) {
                document.getElementById('mqtt-broker').value = config.broker || '';
                document.getElementById('mqtt-port').value = config.port || 1883;
                document.getElementById('mqtt-username').value = config.username || '';
                // Don't populate password for security
            }
        })
        .catch(err => console.error('Error loading MQTT config:', err));
}

function submitMQTTConfig() {
    const broker = document.getElementById('mqtt-broker').value;
    const port = parseInt(document.getElementById('mqtt-port').value);
    const username = document.getElementById('mqtt-username').value;
    const password = document.getElementById('mqtt-password').value;
    
    if (!broker || !port) {
        showToast('Broker address and port are required', 'error');
        return;
    }
    
    showToast('Testing connection...', 'info', 0);
    
    fetch('/api/config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            broker,
            port,
            username: username || null,
            password: password || null
        })
    })
    .then(async r => {
        const text = await r.text();
        try {
            return JSON.parse(text);
        } catch (e) {
            throw new Error('Server returned invalid response');
        }
    })
    .then(data => {
        if (data.success) {
            showToast('✓ MQTT connection successful and saved!', 'success', 3000);
            // Clear password field for security
            document.getElementById('mqtt-password').value = '';
        } else {
            showToast(`✗ Connection failed: ${data.error}`, 'error', 5000);
        }
    })
    .catch(err => {
        showToast(`✗ Error: ${err.message}`, 'error', 5000);
    });
}

function resetMQTTForm() {
    document.getElementById('mqtt-config-form').reset();
    showToast('Form reset', 'info', 2000);
}

// ============================================
// COMMANDS & MODALS
// ============================================

// Active command retries
const activeRetries = new Map();

function sendCommand(device, command, maxRetries = 15, retryInterval = 2000) {
    console.log(`Sending command '${command}' to ${device}`);
    const retryKey = `${device}_${command}`;
    const toastKey = `cmd_${retryKey}`;
    
    // Cancel any existing retry for this device/command
    if (activeRetries.has(retryKey)) {
        clearInterval(activeRetries.get(retryKey).interval);
        activeRetries.delete(retryKey);
    }
    
    let attempt = 0;
    let successPattern = getSuccessPattern(command);
    
    const sendAttempt = () => {
        attempt++;
        console.log(`[Retry ${attempt}/${maxRetries}] Sending: ${command} to ${device}`);
        
        if (attempt > 1) {
            showToast(`⏳ Attempt ${attempt}/${maxRetries}: Sending '${command}' to ${device}...`, 'info', 2000, toastKey);
        } else {
            showToast(`⏳ Sending '${command}' to ${device}...`, 'info', 2000, toastKey);
        }
        
        socket.emit('send_command', { device, command });
        
        if (attempt >= maxRetries) {
            if (activeRetries.has(retryKey)) {
                clearInterval(activeRetries.get(retryKey).interval);
                activeRetries.delete(retryKey);
            }
            showToast(`❌ No response after ${maxRetries} attempts. Device may be sleeping.`, 'error', 4000, toastKey);
        }
    };
    
    // Store retry info
    activeRetries.set(retryKey, {
        device,
        command,
        successPattern,
        toastKey,
        interval: setInterval(sendAttempt, retryInterval)
    });
    
    // Send first attempt immediately
    sendAttempt();
}

function getSuccessPattern(command) {
    // Return pattern to match in events topic for confirmation
    if (command.startsWith('deepsleep 0')) return 'deep_sleep_config.*disabled';
    if (command.startsWith('deepsleep')) return 'deep_sleep_config';
    if (command.startsWith('interval')) return 'sensor_interval_config';
    if (command === 'restart') return 'restarting';
    if (command === 'status') return 'status_request';
    return null;
}

function checkCommandSuccess(device, eventMessage) {
    // Check if any active retries match this success
    for (const [retryKey, retryInfo] of activeRetries.entries()) {
        if (retryInfo.device === device && retryInfo.successPattern) {
            const pattern = new RegExp(retryInfo.successPattern, 'i');
            if (pattern.test(eventMessage)) {
                console.log(`✓ Command confirmed: ${retryInfo.command} for ${device}`);
                clearInterval(retryInfo.interval);
                activeRetries.delete(retryKey);
                showToast(`✅ Success! ${retryInfo.command} confirmed for ${device}`, 'success', 3000, retryInfo.toastKey);
                return true;
            }
        }
    }
    return false;
}

function showDeepSleepModal(deviceName) {
    currentSleepDevice = deviceName;
    document.getElementById('sleep-device-name').textContent = deviceName;
    const modal = document.getElementById('sleep-modal');
    modal.classList.add('show');
}

function closeSleepModal() {
    const modal = document.getElementById('sleep-modal');
    modal.classList.remove('show');
    currentSleepDevice = null;
}

function sendDeepSleepCommand() {
    const seconds = document.getElementById('sleep-seconds').value;
    if (currentSleepDevice && seconds !== null) {
        sendCommand(currentSleepDevice, `deepsleep ${seconds}`);
        showToast(`Deep sleep set to ${seconds}s for ${currentSleepDevice}`, 'success');
        closeSleepModal();
    }
}

// ============================================
// DEVICE DETAILS MODAL
// ============================================

function openDeviceDetails(deviceName) {
    currentDevice = deviceName;
    const state = devices[deviceName] || {};
    const modal = document.getElementById('device-details-modal');
    
    // Update modal title
    document.getElementById('device-modal-title').textContent = deviceName;
    
    // Update device info
    const isOnline = isDeviceOnline(state);
    const statusBadge = document.getElementById('device-modal-status');
    statusBadge.textContent = isOnline ? 'Online' : 'Offline';
    statusBadge.className = `status-badge ${isOnline ? 'status-online' : 'status-offline'}`;
    
    document.getElementById('device-modal-last-seen').textContent = formatLastSeen(state?.last_seen);
    
    // Count messages for this device
    const deviceMessages = messages.filter(m => m.device === deviceName);
    document.getElementById('device-modal-msg-count').textContent = deviceMessages.length;
    
    // Render device log
    renderDeviceLog();
    
    // Show modal with animation
    modal.style.display = 'flex';
    // Force reflow to ensure display change is applied
    modal.offsetHeight;
    // Now add show class to trigger animation
    requestAnimationFrame(() => {
        modal.classList.add('show');
    });
}

function closeDeviceDetails() {
    const modal = document.getElementById('device-details-modal');
    modal.classList.remove('show');
    // Wait for animation to complete before hiding
    setTimeout(() => {
        if (!modal.classList.contains('show')) {
            modal.style.display = 'none';
        }
    }, 300); // Match transition duration
    currentDevice = null;
}

function renderDeviceLog() {
    if (!currentDevice) return;
    
    const logContainer = document.getElementById('device-modal-log');
    if (!logContainer) return;
    
    const deviceMessages = messages.filter(m => m.device === currentDevice);
    
    if (deviceMessages.length === 0) {
        logContainer.innerHTML = '<div style="color: var(--text-secondary); text-align: center; padding: 2rem;">No messages yet</div>';
        return;
    }
    
    // Show last 50 messages, oldest first (newest at bottom like a chat)
    const recentMessages = deviceMessages.slice(-50);
    
    logContainer.innerHTML = recentMessages.map(msg => {
        const date = new Date(msg.timestamp);
        const time = date.toLocaleTimeString('en-US', { hour12: true });
        const topic = msg.topic.split('/').pop(); // Last part of topic
        const payload = typeof msg.payload === 'object' ? JSON.stringify(msg.payload, null, 2) : msg.payload;
        
        return `
            <div class="device-log-entry">
                <span class="log-time">${time}</span>
                <span class="log-topic">${topic}</span>
                <div class="log-payload">${escapeHtml(payload)}</div>
            </div>
        `;
    }).join('');
    
    // Auto-scroll to bottom to show newest messages (like a chat)
    requestAnimationFrame(() => {
        logContainer.scrollTop = logContainer.scrollHeight;
    });
}

function clearDeviceLog() {
    if (!currentDevice) return;
    
    // Remove messages for this device
    messages = messages.filter(m => m.device !== currentDevice);
    renderDeviceLog();
    showToast(`Cleared log for ${currentDevice}`, 'info');
}

function sendCommandFromModal(command) {
    if (!currentDevice) return;
    sendCommand(currentDevice, command);
}

function configureDeepSleep() {
    if (!currentDevice) return;
    
    const seconds = document.getElementById('device-sleep-seconds').value;
    if (seconds === null || seconds === '') {
        showToast('Please enter a valid number of seconds', 'error');
        return;
    }
    
    const secs = parseInt(seconds);
    if (secs < 0 || secs > 3600) {
        showToast('Deep sleep must be between 0-3600 seconds', 'error');
        return;
    }
    
    sendCommand(currentDevice, `deepsleep ${secs}`);
    if (secs === 0) {
        showToast(`Deep sleep disabled for ${currentDevice}`, 'success');
    } else {
        showToast(`Deep sleep set to ${secs}s for ${currentDevice}`, 'success');
    }
}

function configureSensorInterval() {
    if (!currentDevice) return;
    
    const seconds = document.getElementById('device-interval-seconds').value;
    if (seconds === null || seconds === '') {
        showToast('Please enter a valid number of seconds', 'error');
        return;
    }
    
    const secs = parseInt(seconds);
    if (secs < 5 || secs > 3600) {
        showToast('Sensor interval must be between 5-3600 seconds', 'error');
        return;
    }
    
    sendCommand(currentDevice, `interval ${secs}`);
    showToast(`Sensor interval set to ${secs}s for ${currentDevice}`, 'success');
}

function showDeepSleepFromModal() {
    if (!currentDevice) return;
    showDeepSleepModal(currentDevice);
}

function formatLastSeen(timestamp) {
    if (!timestamp) return 'Never';
    
    // Convert ISO string to timestamp if needed
    const ts = typeof timestamp === 'string' ? new Date(timestamp).getTime() : timestamp;
    
    const now = Date.now();
    const diff = now - ts;
    
    if (diff < 60000) return 'Just now';
    if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
    if (diff < 86400000) return `${Math.floor(diff / 3600000)}h ago`;
    return `${Math.floor(diff / 86400000)}d ago`;
}

// ============================================
// UTILITY FUNCTIONS
// ============================================

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

// Close modal on outside click
window.onclick = function(event) {
    const sleepModal = document.getElementById('sleep-modal');
    const deviceModal = document.getElementById('device-details-modal');
    
    if (event.target === sleepModal) {
        closeSleepModal();
    }
    if (event.target === deviceModal) {
        closeDeviceDetails();
    }
}
