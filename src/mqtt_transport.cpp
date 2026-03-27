#include "mqtt_transport.h"

#ifdef DEVICE_ROLE_HUB

#include "IceHubLog.h"
extern IceHubLog iceLog;

MqttTransport::MqttTransport() 
    : _mqttClient(_wifiClient), _port(1883), _callback(nullptr), _context(nullptr), 
      _connectCallback(nullptr), _connectContext(nullptr), _lastReconnectAttempt(0) {}

void MqttTransport::begin() {
    loadConfig();
    _mqttClient.setBufferSize(2048); // Increase buffer for large HA discovery payloads
    if (_server.length() > 0) {
        _mqttClient.setServer(_server.c_str(), _port);
        _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            iceLog.printf("MQTT Rx: [%s] %.*s\n", topic, (int)length, (char*)payload);
            if (this->_callback) {
                this->_callback(topic, payload, length, this->_context);
            }
        });
    }
}

void MqttTransport::loadConfig() {
    _prefs.begin("hub_config", true);
    _server = _prefs.getString("mqtt_server", "");
    _user = _prefs.getString("mqtt_user", "");
    _pass = _prefs.getString("mqtt_pass", "");
    _prefs.end();
}

void MqttTransport::loop() {
    if (_server.length() == 0) return;
    
    if (WiFi.status() != WL_CONNECTED) return;

    if (!_mqttClient.connected()) {
        reconnect();
    } else {
        _mqttClient.loop();
    }
}

void MqttTransport::reconnect() {
    unsigned long now = millis();
    if (now - _lastReconnectAttempt > 5000) {
        _lastReconnectAttempt = now;
        iceLog.print("Attempting MQTT connection...");
        
        bool connected = false;
        String clientId = "IceHub-" + String((uint32_t)ESP.getEfuseMac(), HEX);
        
        if (_user.length() > 0) {
            connected = _mqttClient.connect(clientId.c_str(), _user.c_str(), _pass.c_str());
        } else {
            connected = _mqttClient.connect(clientId.c_str());
        }

        if (connected) {
            iceLog.println("connected");
            if (_connectCallback) {
                _connectCallback(_connectContext);
            }
        } else {
            iceLog.printf("failed, rc=%d try again in 5 seconds\n", _mqttClient.state());
        }
    }
}

bool MqttTransport::publish(const char* topic, const char* payload, bool retained) {
    if (!_mqttClient.connected()) return false;
    return _mqttClient.publish(topic, payload, retained);
}

void MqttTransport::subscribe(const char* topic) {
    if (_mqttClient.connected()) {
        _mqttClient.subscribe(topic);
    }
}

void MqttTransport::onMessage(MessageCallback callback, void* context) {
    _callback = callback;
    _context = context;
}

void MqttTransport::onConnect(ConnectCallback callback, void* context) {
    _connectCallback = callback;
    _connectContext = context;
}

bool MqttTransport::isConnected() {
    return _mqttClient.connected();
}

#endif