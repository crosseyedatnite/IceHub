#include "ui_handler.h"
#ifdef DEVICE_ROLE_HUB

#include "system_config.h"
#include "IceHubLog.h"
extern IceHubLog iceLog;

UiHandler::UiHandler(NodeRegistry& registry) : _registry(registry), _server(nullptr) {}

void UiHandler::begin(WebServer* server) {
    _server = server;
    
    _server->on("/", HTTP_GET, [this]() { handleRoot(); });
    _server->on("/logs", HTTP_GET, [this]() { handleLogsPage(); });
    
    _server->on("/reset_id", HTTP_POST, [this]() { 
        iceLog.println("API [POST]: Node ID counter reset");
        _registry.resetNextId();
        _server->sendHeader("Location", "/");
        _server->send(303);
    });
}

void UiHandler::handleRoot() {
    SystemConfig config;
    String hostname = config.getHostname();
    String tz = config.getTimezone();
    String mqttServer = config.getMqttServer();
    String mqttUser = config.getMqttUser();

    String html = R"rawliteral(
<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body{font-family:sans-serif; background:#1e1e1e; color:#d4d4d4; padding:10px;}
.node{border:1px solid #444; padding:10px; margin:10px 0; border-radius:5px; background:#2a2a2a;}
button, input, select{padding:8px; margin:2px; background:#333; color:#fff; border:1px solid #555; border-radius:3px;}
button:hover{background:#444; cursor:pointer;}
a{color:#4fc3f7; text-decoration:none;}
.status-on{color:#4caf50;} .status-off{color:#9e9e9e;}
.sys-bar{background:#111; padding:10px; border-radius:5px; margin-bottom:10px; font-size:0.9em;}
details {background: #2a2a2a; padding: 10px; border-radius: 5px; margin-top: 20px;}
summary {cursor: pointer; font-weight: bold;}
</style>
<script>
async function api(url, method="GET", body=null) {
    let opts = { method };
    if (body) {
        if (body instanceof URLSearchParams) opts.body = body;
        else { opts.headers = {'Content-Type': 'application/json'}; opts.body = JSON.stringify(body); }
    }
    try {
        let r = await fetch(url, opts);
        if(!r.ok) return null;
        const ct = r.headers.get("content-type");
        return ct && ct.includes("application/json") ? await r.json() : await r.text();
    } catch(e) { return null; }
}
async function setEffect(id, effect) { await api(`/api/nodes/${id}/light`, 'POST', {effect}); }
async function setColor(id, hex) {
    let r = parseInt(hex.slice(1,3), 16), g = parseInt(hex.slice(3,5), 16), b = parseInt(hex.slice(5,7), 16);
    await api(`/api/nodes/${id}/light`, 'POST', {color: {r,g,b}});
}
async function rename(id, oldName) {
    let n = prompt("New name:", oldName || "");
    if(n !== null && n !== oldName) { 
        await api(`/api/nodes/${id}`, 'PATCH', {name: n}); render(); 
    }
}
async function action(url, method='GET') { if(confirm('Are you sure?')) { await api(url, method); render(); } }

async function saveConfig() {
    let payload = {
        hostname: document.getElementById('cfg-host').value,
        tz: document.getElementById('cfg-tz').value,
        mqtt_server: document.getElementById('cfg-mqtt-server').value,
        mqtt_user: document.getElementById('cfg-mqtt-user').value
    };
    if(confirm('Save settings and reboot?')) {
        await api('/api/config', 'PATCH', payload);
        setTimeout(() => location.reload(), 3000);
    }
}

async function changePassword() {
    let p = prompt("New MQTT Password:");
    if (p !== null) {
        await api('/api/config/mqtt_password', 'PUT', {password: p});
        setTimeout(() => location.reload(), 3000);
    }
}

async function render() {
    let [sys, nodes] = await Promise.all([api('/api/system'), api('/api/nodes')]);
    if (!sys || !nodes) return;
    
    let dom = `<div class="sys-bar"><b>Hub Uptime:</b> ${Math.floor(sys.uptime/60)}m ${sys.uptime%60}s | <b>Heap:</b> ${(sys.free_heap/1024).toFixed(1)} KB | <b>Time:</b> ${sys.time} <br><br><a href='/logs' style='font-weight:bold;'>&#128196; View Logs</a></div>`;

    let hasNodes = false;
    for(let n of nodes) {
        if (n.id === 0) continue;
        hasNodes = true;
        let statusClass = n.isOnline ? 'status-on' : 'status-off';
        let statusText = n.isOnline ? 'Online' : 'Offline';
        
        dom += `<div class='node'><h3 style="margin-top:0;">${n.name || 'Node ' + n.id} (ID: ${n.id}) <span style="float:right; font-size:0.8em;" class="${statusClass}">&#9679; ${statusText}</span></h3>`;
        
        let caps = await api(`/api/nodes/${n.id}/capabilities`);
        if(caps && caps.leds !== undefined) {
            dom += `<div style="margin: 10px 0; padding: 5px; background: #222; border-radius: 3px;">`;
            if(caps.leds > 0) {
                dom += `Color: <input type="color" onchange="setColor(${n.id}, this.value)"> <br><br>`;
                if(caps.modes) caps.modes.forEach(m => {
                    dom += `<button onclick="setEffect(${n.id}, '${m}')">${m}</button> `;
                });
            }
            dom += `</div>`;
        }

        let sens = await api(`/api/nodes/${n.id}/sensors`);
        if(sens && sens.valid && !sens.stale) {
            dom += `<div style="color:#a5d6ff; margin: 10px 0; padding: 5px; background: #112a46; border-radius: 3px;">&#127777; Temp: <b>${sens.temperature.toFixed(1)}&deg;C</b> | &#128167; Hum: <b>${sens.humidity.toFixed(1)}%</b></div>`;
        }

        dom += `<div style="margin-top:10px;"><button onclick="rename(${n.id}, '${n.name}')">Rename</button> <button onclick="action('/api/nodes/${n.id}/reboot', 'POST')">Reboot</button> <button onclick="action('/api/nodes/${n.id}', 'DELETE')" style="background:#8b0000;">Unregister</button></div></div>`;
    }
    if (!hasNodes) dom += `<p>No nodes detected yet.</p>`;
    document.getElementById('app').innerHTML = dom;
}
window.onload = () => { render(); setInterval(render, 5000); };
</script>
</head><body>
<h1>IceHub</h1>
<div id="app">Loading...</div>
<details><summary>Configuration & Maintenance</summary>
<div style="margin-bottom:10px;">
)rawliteral";

    html += "Hostname: <input type='text' id='cfg-host' value='" + hostname + "'><br>";
    html += "Timezone (POSIX): <input type='text' id='cfg-tz' value='" + tz + "' placeholder='EST5EDT,M3.2.0,M11.1.0'><br>";
    html += "MQTT Server: <input type='text' id='cfg-mqtt-server' value='" + mqttServer + "'><br>";
    html += "MQTT User: <input type='text' id='cfg-mqtt-user' value='" + mqttUser + "'><br>";
    html += "<button onclick='saveConfig()' style='margin-top:10px;'>Save Config & Reboot</button></div><hr>";
    
    html += "<button onclick='changePassword()'>Change MQTT Password</button><hr>";

    html += "<form action='/reset_id' method='POST' style='margin-top:10px;'>";
    html += "<input type='submit' value='Reset Node ID Counter' style='background:#8b0000;' onclick=\"return confirm('Reset ID counter to 1?')\">";
    html += "</form></details></body></html>";
    
    _server->send(200, "text/html", html);
}

void UiHandler::handleLogsPage() {
    String html = R"(
<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body { font-family: sans-serif; background: #1e1e1e; color: #d4d4d4; padding: 10px; }
h2 { color: #fff; margin-top: 0; }
a { color: #4fc3f7; text-decoration: none; display: inline-block; margin-bottom: 10px; }
#log-container { background: #0b0b0b; padding: 10px; border-radius: 5px; height: 75vh; overflow-y: auto; font-family: monospace; white-space: pre-wrap; margin-top: 10px; }
.log-line { margin: 0; padding: 3px 0; border-bottom: 1px solid #222; }
select { padding: 8px; margin-bottom: 10px; background: #333; color: #fff; border: 1px solid #555; border-radius: 3px; }
</style>
<script>
let currentNodeId = 0;

async function loadNodes() {
    try {
        let res = await fetch('/api/nodes');
        if (res.ok) {
            let nodes = await res.json();
            let sel = document.getElementById('node-select');
            sel.innerHTML = '';
            nodes.forEach(n => {
                let opt = document.createElement('option');
                opt.value = n.id;
                opt.text = n.id === 0 ? "Hub Logs" : `Node ${n.id} (${n.name})`;
                sel.appendChild(opt);
            });
        }
    } catch(e) { console.error("Failed to load nodes", e); }
}

async function fetchLogs() {
    try {
        let res = await fetch(`/api/nodes/${currentNodeId}/logs`);
        if (res.ok) {
            let logs = await res.json();
            let html = '';
            for (let i = 0; i < logs.length; i++) {
                html += '<div class="log-line">' + logs[i] + '</div>';
            }
            
            let container = document.getElementById('log-container');
            // Check if user is scrolled near the bottom, or if it's the initial load
            let isAtBottom = (container.innerHTML === 'Loading...') || 
                             (container.scrollHeight - container.scrollTop <= container.clientHeight + 10);
            
            container.innerHTML = html;
            
            if (isAtBottom) {
                container.scrollTop = container.scrollHeight;
            }
        }
    } catch(e) {
        console.error("Failed to fetch logs", e);
    }
    setTimeout(fetchLogs, 2000);
}

function changeNode() {
    currentNodeId = document.getElementById('node-select').value;
    document.getElementById('log-container').innerHTML = 'Loading...';
}

window.onload = async () => {
    await loadNodes();
    fetchLogs();
};
</script>
</head>
<body>
<a href='/'>&larr; Back to Dashboard</a>
<h2>System Logs</h2>
<select id='node-select' onchange='changeNode()'>
    <option value='0'>Hub Logs</option>
</select>
<div id='log-container'>Loading...</div>
</body></html>
)";
    _server->send(200, "text/html", html);
}
#endif