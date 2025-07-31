#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

const char* ssid = "Hidden"; // Replace with your router's SSID
const char* password = "abdullah1122"; // Replace with your router's password

LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, adjust if needed
bool lcdInitialized = false;

const int voltagePin = 34;
const int currentPin = 35;
const int flowSensorPin = 26;       
const int ldrPin = 33;
const int buzzerPin = 25;
const int pumpPin = 13;
const int testBuzzerPin = 27;
const int greenLedPin = 12;
const int analogTdsPin = 32;

const int TDS_RX_PIN = 16;
const int TDS_TX_PIN = 17;

volatile int flowPulseCount = 0;
float flowRate = 0.0;
float liters = 0.0;
unsigned long lastFlowTime = 0;
float FLOW_CALIBRATION_FACTOR = 7.5;

float ACS712_offset = 2.5;
float Vref = 3.3;
const float VOLTAGE_DIVIDER_RATIO = 5.0;
float tdsThreshold = 1000.0;
float flowThreshold = 0.5;

bool wifiConnected = false;
unsigned long lastAlertToggle = 0;
bool alertState = false;
bool useAnalogTds = false;
bool calibrateTdsOnStartup = true;

unsigned long lastSensorRead = 0;
unsigned long lastSerialOutput = 0;
unsigned long lastLdrRead = 0;
unsigned long lastDataLog = 0;
unsigned long lastTdsRequest = 0;
unsigned long lastNotification = 0;
const int SENSOR_READ_INTERVAL = 1000; // 1 second
const int LDR_READ_INTERVAL = 50;
const int SERIAL_OUTPUT_INTERVAL = 1000;
const int DATA_LOG_INTERVAL = 60000; // 1 minute
const int TDS_REQUEST_INTERVAL = 1200; // Slightly longer to avoid sensor overload
const int TDS_RESPONSE_DELAY = 200; // Increased for stability
const int TDS_TIMEOUT = 2000; // 2 seconds
const int WIFI_CONNECTION_TIMEOUT = 10000; // 10 seconds
const int MAX_LOG_ENTRIES = 100;
const int NOTIFICATION_DEBOUNCE = 5000; // 5 seconds

bool debugMode = true;

struct SensorData {
  float voltage;
  float current;
  float tdsValue;
  float flowRate;
  int ldrValue;
  bool pumpState;
  unsigned long timestamp;
  String tdsStatus;
  float temperature;
  float smoothedTds; // For smoothing
  float smoothedTemp; // For smoothing
} sensorData;

Preferences preferences;
WebServer server(80);
WebSocketsServer webSocket(81);
HardwareSerial TDSSerial(2);
const int LDR_AVERAGE_SAMPLES = 5;
const float TDS_SMOOTHING_FACTOR = 0.7; // For exponential moving average
int tdsReadFailures = 0;
const int MAX_TDS_FAILURES = 3;

void IRAM_ATTR flowPulse() {
  flowPulseCount++;
  lastFlowTime = millis();
}

bool initializeI2C() {
  Wire.begin(21, 22);
  Wire.setClock(100000);
  if (debugMode) Serial.println("Initializing I2C bus...");
  if (!scanI2C()) {
    if (debugMode) Serial.println("WARNING: No I2C devices found");
    return false;
  }
  return true;
}

bool scanI2C() {
  byte error, address;
  int nDevices = 0;
  if (debugMode) Serial.println("Scanning I2C bus...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      if (debugMode) {
        Serial.print("I2C device found at address 0x");
        if (address < 16) Serial.print("0");
        Serial.println(address, HEX);
      }
      nDevices++;
    }
  }
  return nDevices > 0;
}

bool initializeLCD() {
  if (debugMode) Serial.println("Attempting LCD initialization at address 0x27...");
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() != 0) {
    if (debugMode) Serial.println("ERROR: LCD not found at I2C address 0x27");
    return false;
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Eco-Go");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  if (debugMode) Serial.println("LCD initialized successfully");
  delay(100);
  return true;
}

void connectToWiFi() {
  if (debugMode) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
  }
  if (lcdInitialized) {
    lcd.setCursor(0, 2);
    lcd.print("Connecting WiFi...");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECTION_TIMEOUT) {
    if (millis() - lastAlertToggle > 500) {
      alertState = !alertState;
      digitalWrite(testBuzzerPin, alertState);
      digitalWrite(greenLedPin, alertState);
      lastAlertToggle = millis();
      if (debugMode) Serial.print(".");
    }
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    if (debugMode) {
      Serial.println("\nWiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    if (lcdInitialized) {
      lcd.setCursor(0, 3);
      lcd.print("IP: ");
      lcd.print(WiFi.localIP());
    }
    digitalWrite(testBuzzerPin, LOW);
    digitalWrite(greenLedPin, HIGH);
  } else {
    wifiConnected = false;
    if (debugMode) Serial.println("\nFailed to connect to WiFi");
    if (lcdInitialized) {
      lcd.setCursor(0, 3);
      lcd.print("WiFi Failed       ");
    }
    digitalWrite(testBuzzerPin, LOW);
    digitalWrite(greenLedPin, LOW);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      if (debugMode) Serial.printf("WebSocket client #%u disconnected\n", num);
      break;
    case WStype_CONNECTED:
      if (debugMode) Serial.printf("WebSocket client #%u connected\n", num);
      break;
    case WStype_TEXT:
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        if (debugMode) {
          Serial.print("JSON parse error: ");
          Serial.println(error.c_str());
        }
        return;
      }
      if (doc.containsKey("flowCalibration")) {
        FLOW_CALIBRATION_FACTOR = doc["flowCalibration"];
        saveSettings();
      }
      if (doc.containsKey("acsOffset")) {
        ACS712_offset = doc["acsOffset"];
        saveSettings();
      }
      if (doc.containsKey("tdsThreshold")) {
        tdsThreshold = doc["tdsThreshold"];
        saveSettings();
      }
      if (doc.containsKey("flowThreshold")) {
        flowThreshold = doc["flowThreshold"];
        saveSettings();
      }
      if (doc.containsKey("resetLiters")) {
        liters = 0.0;
        saveSettings();
      }
      if (doc.containsKey("calibrateTDS")) {
        calibrateTDS();
      }
      break;
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Eco-Go Water Monitor</title>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f4f4f9; margin: 0; padding: 20px; }";
  html += ".container { max-width: 1200px; margin: 0 auto; }";
  html += ".tabs { display: flex; margin-bottom: 20px; }";
  html += ".tab { padding: 10px 20px; cursor: pointer; background: #ddd; margin-right: 5px; border-radius: 5px 5px 0 0; }";
  html += ".tab.active { background: #fff; font-weight: bold; }";
  html += ".card { background: #fff; padding: 20px; margin: 10px 0; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  html += ".sensor-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; }";
  html += ".sensor-card { text-align: center; }";
  html += ".sensor-card h3 { margin: 0 0 10px; color: #333; }";
  html += ".sensor-value { font-size: 24px; color: #007bff; }";
  html += ".alert { color: #dc3545; font-weight: bold; }";
  html += ".button { padding: 10px 20px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; }";
  html += ".on { background-color: #28a745; color: white; }";
  html += ".off { background-color: #dc3545; color: white; }";
  html += ".settings-form { display: grid; gap: 10px; max-width: 400px; }";
  html += ".settings-form label { font-weight: bold; }";
  html += ".settings-form input { padding: 8px; border: 1px solid #ddd; border-radius: 4px; }";
  html += ".chart-container { position: relative; height: 200px; margin: 20px 0; }";
  html += ".notification { position: fixed; top: 20px; right: 20px; background: #dc3545; color: white; padding: 10px 20px; border-radius: 5px; display: none; }";
  html += "@media (max-width: 600px) { .sensor-grid { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Eco-Go Water Monitor</h1>";
  html += "<div class='tabs'>";
  html += "<div class='tab active' onclick='showTab(\"dashboard\")'>Dashboard</div>";
  html += "<div class='tab' onclick='showTab(\"history\")'>History</div>";
  html += "<div class='tab' onclick='showTab(\"settings\")'>Settings</div>";
  html += "</div>";
  html += "<div id='dashboard' class='tab-content'>";
  html += "<div class='sensor-grid'>";
  html += "<div class='sensor-card'><h3>Voltage</h3><div id='voltage' class='sensor-value'>0.0 V</div><div class='chart-container'><canvas id='voltageChart'></canvas></div></div>";
  html += "<div class='sensor-card'><h3>Current</h3><div id='current' class='sensor-value'>0.0 A</div><div class='chart-container'><canvas id='currentChart'></canvas></div></div>";
  html += "<div class='sensor-card'><h3>TDS</h3><div id='tds' class='sensor-value'>0.0 ppm</div><div id='tdsStatus' class='sensor-value'></div><div class='chart-container'><canvas id='tdsChart'></canvas></div></div>";
  html += "<div class='sensor-card'><h3>Temperature</h3><div id='temperature' class='sensor-value'>0.0 °C</div><div class='chart-container'><canvas id='tempChart'></canvas></div></div>";
  html += "<div class='sensor-card'><h3>Flow</h3><div id='flow' class='sensor-value'>0.0 L/min</div><div class='chart-container'><canvas id='flowChart'></canvas></div></div>";
  html += "<div class='sensor-card'><h3>Total Liters</h3><div id='liters' class='sensor-value'>0.0 L</div><div class='chart-container'><canvas id='litersChart'></canvas></div></div>";
  html += "<div class='sensor-card'><h3>Light</h3><div id='light' class='sensor-value'>0</div><div class='chart-container'><canvas id='lightChart'></canvas></div></div>";
  html += "</div>";
  html += "<div class='card'>";
  html += "<h3>Pump Status: <span id='pump'>" + String(sensorData.pumpState ? "ON" : "OFF") + "</span></h3>";
  html += "<button class='button " + String(sensorData.pumpState ? "on" : "off") + "' onclick='togglePump()'>Toggle Pump</button>";
  html += "</div>";
  html += "</div>";
  html += "<div id='history' class='tab-content' style='display:none;'>";
  html += "<div class='card'>";
  html += "<h3>Sensor History</h3>";
  html += "<div class='chart-container'><canvas id='historyChart'></canvas></div>";
  html += "</div>";
  html += "</div>";
  html += "<div id='settings' class='tab-content' style='display:none;'>";
  html += "<div class='card'>";
  html += "<h3>Settings</h3>";
  html += "<div class='settings-form'>";
  html += "<label>Flow Calibration Factor:</label><input type='number' id='flowCalibration' value='" + String(FLOW_CALIBRATION_FACTOR) + "' step='0.1'>";
  html += "<label>ACS712 Offset:</label><input type='number' id='acsOffset' value='" + String(ACS712_offset) + "' step='0.1'>";
  html += "<label>TDS Threshold (ppm):</label><input type='number' id='tdsThreshold' value='" + String(tdsThreshold) + "' step='1'>";
  html += "<label>Flow Threshold (L/min):</label><input type='number' id='flowThreshold' value='" + String(flowThreshold) + "' step='0.1'>";
  html += "<button class='button on' onclick='saveSettings()'>Save Settings</button>";
  html += "<button class='button off' onclick='resetLiters()'>Reset Total Liters</button>";
  html += "<button class='button on' onclick='calibrateTDS()'>Calibrate TDS Sensor</button>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "<div id='notification' class='notification'></div>";
  html += "<script>";
  html += "let ws = new WebSocket('ws://' + window.location.hostname + ':81/');";
  html += "let voltageData = [], currentData = [], tdsData = [], flowData = [], litersData = [], lightData = [], temperatureData = [], labels = [];";
  html += "let historyData = { voltage: [], current: [], tds: [], flow: [], liters: [], light: [], temperature: [], timestamps: [] };";
  html += "let voltageChart, currentChart, tdsChart, flowChart, litersChart, lightChart, historyChart, tempChart;";
  html += "ws.onmessage = function(event) {";
  html += "  let data = JSON.parse(event.data);";
  html += "  let now = new Date().toLocaleTimeString();";
  html += "  document.getElementById('voltage').innerText = data.voltage.toFixed(1) + ' V';";
  html += "  document.getElementById('current').innerText = data.current.toFixed(2) + ' A';";
  html += "  document.getElementById('tds').innerText = data.tds.toFixed(1) + ' ppm';";
  html += "  document.getElementById('tdsStatus').innerText = data.tdsStatus;";
  html += "  document.getElementById('temperature').innerText = data.temperature.toFixed(1) + ' °C';";
  html += "  document.getElementById('flow').innerText = data.flow.toFixed(1) + ' L/min';";
  html += "  document.getElementById('liters').innerText = data.liters.toFixed(1) + ' L';";
  html += "  document.getElementById('light').innerText = data.light;";
  html += "  document.getElementById('pump').innerText = data.pump ? 'ON' : 'OFF';";
  html += "  document.querySelector('.button').className = 'button ' + (data.pump ? 'on' : 'off');";
  html += "  if (data.alert) {";
  html += "    showNotification(data.alert);";
  html += "    document.getElementById(data.alert.includes('TDS') ? 'tds' : 'flow').classList.add('alert');";
  html += "  } else {";
  html += "    document.getElementById('tds').classList.remove('alert');";
  html += "    document.getElementById('flow').classList.remove('alert');";
  html += "  }";
  html += "  if (voltageData.length > 60) { voltageData.shift(); currentData.shift(); tdsData.shift(); flowData.shift(); litersData.shift(); lightData.shift(); temperatureData.shift(); labels.shift(); }";
  html += "  voltageData.push(data.voltage); currentData.push(data.current); tdsData.push(data.tds);";
  html += "  flowData.push(data.flow); litersData.push(data.liters); lightData.push(data.light); temperatureData.push(data.temperature); labels.push(now);";
  html += "  updateCharts();";
  html += "};";
  html += "function updateCharts() {";
  html += "  voltageChart.data.labels = labels; voltageChart.data.datasets[0].data = voltageData; voltageChart.update();";
  html += "  currentChart.data.labels = labels; currentChart.data.datasets[0].data = currentData; currentChart.update();";
  html += "  tdsChart.data.labels = labels; tdsChart.data.datasets[0].data = tdsData; tdsChart.update();";
  html += "  tempChart.data.labels = labels; tempChart.data.datasets[0].data = temperatureData; tdsChart.update();";
  html += "  flowChart.data.labels = labels; flowChart.data.datasets[0].data = flowData; flowChart.update();";
  html += "  litersChart.data.labels = labels; litersChart.data.datasets[0].data = litersData; litersChart.update();";
  html += "  lightChart.data.labels = labels; lightChart.data.datasets[0].data = lightData; lightChart.update();";
  html += "}";
  html += "function initCharts() {";
  html += "  voltageChart = new Chart(document.getElementById('voltageChart'), { type: 'line', data: { labels: [], datasets: [{ label: 'Voltage (V)', data: [], borderColor: '#007bff', fill: false }] }, options: { scales: { y: { beginAtZero: true } } } });";
  html += "  currentChart = new Chart(document.getElementById('currentChart'), { type: 'line', data: { labels: [], datasets: [{ label: 'Current (A)', data: [], borderColor: '#28a745', fill: false }] }, options: { scales: { y: { beginAtZero: true } } } });";
  html += "  tdsChart = new Chart(document.getElementById('tdsChart'), { type: 'line', data: { labels: [], datasets: [{ label: 'TDS (ppm)', data: [], borderColor: '#dc3545', fill: false }] }, options: { scales: { y: { beginAtZero: true } } } });";
  html += "  tempChart = new Chart(document.getElementById('tempChart'), { type: 'line', data: { labels: [], datasets: [{ label: 'Temperature (°C)', data: [], borderColor: '#fd7e14', fill: false }] }, options: { scales: { y: { beginAtZero: true } } } });";
  html += "  flowChart = new Chart(document.getElementById('flowChart'), { type: 'line', data: { labels: [], datasets: [{ label: 'Flow (L/min)', data: [], borderColor: '#ffc107', fill: false }] }, options: { scales: { y: { beginAtZero: true } } } });";
  html += "  litersChart = new Chart(document.getElementById('litersChart'), { type: 'line', data: { labels: [], datasets: [{ label: 'Total Liters (L)', data: [], borderColor: '#17a2b8', fill: false }] }, options: { scales: { y: { beginAtZero: true } } } });";
  html += "  lightChart = new Chart(document.getElementById('lightChart'), { type: 'line', data: { labels: [], datasets: [{ label: 'Light', data: [], borderColor: '#6f42c1', fill: false }] }, options: { scales: { y: { beginAtZero: true } } } });";
  html += "  historyChart = new Chart(document.getElementById('historyChart'), { type: 'line', data: { labels: historyData.timestamps, datasets: [";
  html += "    { label: 'Voltage (V)', data: historyData.voltage, borderColor: '#007bff', fill: false, hidden: true },";
  html += "    { label: 'Current (A)', data: historyData.current, borderColor: '#28a745', fill: false, hidden: true },";
  html += "    { label: 'TDS (ppm)', data: historyData.tds, borderColor: '#dc3545', fill: false },";
  html += "    { label: 'Temperature (°C)', data: historyData.temperature, borderColor: '#fd7e14', fill: false },";
  html += "    { label: 'Flow (L/min)', data: historyData.flow, borderColor: '#ffc107', fill: false },";
  html += "    { label: 'Total Liters (L)', data: historyData.liters, borderColor: '#17a2b8', fill: false, hidden: true },";
  html += "    { label: 'Light', data: historyData.light, borderColor: '#6f42c1', fill: false, hidden: true }";
  html += "  ] }, options: { scales: { x: { type: 'time', time: { unit: 'minute' } } } } });";
  html += "}";
  html += "function showTab(tab) {";
  html += "  document.querySelectorAll('.tab-content').forEach(content => content.style.display = 'none');";
  html += "  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));";
  html += "  document.getElementById(tab).style.display = 'block';";
  html += "  document.querySelector(`.tab[onclick=\"showTab('${tab}')\"]`).classList.add('active');";
  html += "}";
  html += "function togglePump() {";
  html += "  fetch('/togglePump', { method: 'POST' }).then(response => response.json()).then(data => {";
  html += "    document.getElementById('pump').innerText = data.pump ? 'ON' : 'OFF';";
  html += "    document.querySelector('.button').className = 'button ' + (data.pump ? 'on' : 'off');";
  html += "  }).catch(err => console.error('Error:', err));";
  html += "}";
  html += "function saveSettings() {";
  html += "  let settings = {";
  html += "    flowCalibration: parseFloat(document.getElementById('flowCalibration').value),";
  html += "    acsOffset: parseFloat(document.getElementById('acsOffset').value),";
  html += "    tdsThreshold: parseFloat(document.getElementById('tdsThreshold').value),";
  html += "    flowThreshold: parseFloat(document.getElementById('flowThreshold').value)";
  html += "  };";
  html += "  ws.send(JSON.stringify(settings));";
  html += "  showNotification('Settings saved!');";
  html += "}";
  html += "function resetLiters() {";
  html += "  ws.send(JSON.stringify({ resetLiters: true }));";
  html += "  showNotification('Total liters reset!');";
  html += "}";
  html += "function calibrateTDS() {";
  html += "  ws.send(JSON.stringify({ calibrateTDS: true }));";
  html += "  showNotification('TDS Sensor Calibration Started');";
  html += "}";
  html += "function showNotification(message) {";
  html += "  let notification = document.getElementById('notification');";
  html += "  notification.innerText = message;";
  html += "  notification.style.display = 'block';";
  html += "  let audio = new Audio('https://www.soundjay.com/buttons/beep-01a.mp3');";
  html += "  audio.play();";
  html += "  setTimeout(() => notification.style.display = 'none', 3000);";
  html += "}";
  html += "window.onload = function() {";
  html += "  initCharts();";
  html += "  fetch('/history').then(response => response.json()).then(data => {";
  html += "    historyData = data;";
  html += "    historyChart.data.labels = data.timestamps.map(t => new Date(t));";
  html += "    historyChart.data.datasets[0].data = data.voltage;";
  html += "    historyChart.data.datasets[1].data = data.current;";
  html += "    historyChart.data.datasets[2].data = data.tds;";
  html += "    historyChart.data.datasets[3].data = data.temperature;";
  html += "    historyChart.data.datasets[4].data = data.flow;";
  html += "    historyChart.data.datasets[5].data = data.liters;";
  html += "    historyChart.data.datasets[6].data = data.light;";
  html += "    historyChart.update();";
  html += "  });";
  html += "};";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleData() {
  StaticJsonDocument<256> doc;
  doc["voltage"] = sensorData.voltage;
  doc["current"] = sensorData.current;
  doc["tds"] = sensorData.smoothedTds;
  doc["flow"] = sensorData.flowRate;
  doc["liters"] = liters;
  doc["light"] = sensorData.ldrValue;
  doc["pump"] = sensorData.pumpState;
  doc["tdsStatus"] = sensorData.tdsStatus;
  doc["temperature"] = sensorData.smoothedTemp;
  if (sensorData.tdsValue > tdsThreshold && millis() - lastNotification > NOTIFICATION_DEBOUNCE) {
    doc["alert"] = "High TDS!";
    lastNotification = millis();
  } else if (sensorData.flowRate < flowThreshold && sensorData.flowRate > 0.1 && millis() - lastNotification > NOTIFICATION_DEBOUNCE) {
    doc["alert"] = "Low Flow!";
    lastNotification = millis();
  } else {
    doc["alert"] = "";
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleHistory() {
  StaticJsonDocument<4096> doc;
  JsonArray voltage = doc.createNestedArray("voltage");
  JsonArray current = doc.createNestedArray("current");
  JsonArray tds = doc.createNestedArray("tds");
  JsonArray flow = doc.createNestedArray("flow");
  JsonArray litersArray = doc.createNestedArray("liters");
  JsonArray light = doc.createNestedArray("light");
  JsonArray temperature = doc.createNestedArray("temperature");
  JsonArray timestamps = doc.createNestedArray("timestamps");

  preferences.begin("eco-go", true);
  for (int i = 0; i < MAX_LOG_ENTRIES; i++) {
    String key = "log_" + String(i);
    if (preferences.isKey(key.c_str())) {
      String log = preferences.getString(key.c_str());
      StaticJsonDocument<256> logDoc;
      deserializeJson(logDoc, log);
      voltage.add(logDoc["voltage"]);
      current.add(logDoc["current"]);
      tds.add(logDoc["tds"]);
      flow.add(logDoc["flow"]);
      litersArray.add(logDoc["liters"]);
      light.add(logDoc["light"]);
      temperature.add(logDoc["temperature"]);
      timestamps.add(logDoc["timestamp"]);
    }
  }
  preferences.end();

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleTogglePump() {
  sensorData.pumpState = !sensorData.pumpState;
  digitalWrite(pumpPin, sensorData.pumpState ? HIGH : LOW);
  StaticJsonDocument<64> doc;
  doc["pump"] = sensorData.pumpState;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  if (debugMode) {
    Serial.print("Pump toggled to ");
    Serial.println(sensorData.pumpState ? "ON" : "OFF");
  }
}

void sendHex(String hexString) {
  int len = hexString.length();
  if (debugMode) Serial.print("Sending hex command: ");
  for (int i = 0; i < len; i += 2) {
    String hexPair = hexString.substring(i, i + 2);
    byte hexValue = (byte)strtol(hexPair.c_str(), NULL, 16);
    TDSSerial.write(hexValue);
    if (debugMode) {
      Serial.print("0x");
      Serial.print(hexValue, HEX);
      Serial.print(" ");
    }
  }
  if (debugMode) Serial.println();
}

void requestTDSData() {
  if (debugMode) Serial.println("Requesting TDS data...");
  clearSerialBuffer(); // Clear buffer before request
  sendHex("A000000000A0");
}

void parseTDSData() {
  if (TDSSerial.available() >= 5) { // Wait for full packet
    if (debugMode) Serial.println("Data received from TDS sensor:");
    
    byte start = TDSSerial.read();
    if (start == 0xAA) {
      if (debugMode) Serial.println("Valid start byte received: 0xAA");
      
      byte tdsHi = TDSSerial.read();
      byte tdsLo = TDSSerial.read();
      byte tempHi = TDSSerial.read();
      byte tempLo = TDSSerial.read();
      
      float newTds = (tdsHi << 8) + tdsLo;
      float newTemp = ((tempHi << 8) + tempLo) / 100.0;
      
      if (newTds >= 0 && newTds <= 3000.0 && newTemp >= 0 && newTemp <= 100.0) {
        sensorData.tdsValue = newTds;
        sensorData.temperature = newTemp;
        // Apply exponential moving average
        if (sensorData.smoothedTds == 0) {
          sensorData.smoothedTds = newTds;
          sensorData.smoothedTemp = newTemp;
        } else {
          sensorData.smoothedTds = TDS_SMOOTHING_FACTOR * newTds + (1 - TDS_SMOOTHING_FACTOR) * sensorData.smoothedTds;
          sensorData.smoothedTemp = TDS_SMOOTHING_FACTOR * newTemp + (1 - TDS_SMOOTHING_FACTOR) * sensorData.smoothedTemp;
        }
        sensorData.tdsStatus = "UART";
        tdsReadFailures = 0;
        if (debugMode) {
          Serial.print("TDS: ");
          Serial.print(sensorData.smoothedTds);
          Serial.println(" ppm");
          Serial.print("Temp: ");
          Serial.print(sensorData.smoothedTemp);
          Serial.println(" °C");
        }
      } else {
        if (debugMode) Serial.println("Invalid TDS/Temp values");
        sensorData.tdsStatus = "Invalid";
        tdsReadFailures++;
      }
    } else {
      if (debugMode) {
        Serial.print("Invalid start byte: 0x");
        Serial.println(start, HEX);
      }
      sensorData.tdsStatus = "Invalid Start";
      tdsReadFailures++;
      clearSerialBuffer();
    }
  } else {
    if (debugMode) Serial.println("No or incomplete TDS data");
    sensorData.tdsStatus = "No Data";
    tdsReadFailures++;
  }
}

void calibrateTDS() {
  if (debugMode) Serial.println("Starting TDS Calibration...");
  clearSerialBuffer();
  sendHex("A600000000A6");
  if (debugMode) Serial.println("Calibration Command Sent");
  
  delay(500);
  while (TDSSerial.available()) {
    byte response = TDSSerial.read();
    if (debugMode) {
      Serial.print("Calibration response: 0x");
      Serial.println(response, HEX);
    }
  }
  if (debugMode) Serial.println("Calibration Complete!");
}

void clearSerialBuffer() {
  while (TDSSerial.available()) {
    TDSSerial.read();
  }
}

void readSensors() {
  int voltageRaw = analogRead(voltagePin);
  sensorData.voltage = (voltageRaw * (Vref / 4095.0)) * VOLTAGE_DIVIDER_RATIO;
  if (sensorData.voltage < 0.1 || sensorData.voltage > 30.0) sensorData.voltage = 0.0;

  int currentRaw = analogRead(currentPin);
  float voltageOut = (currentRaw * Vref) / 4095.0;
  sensorData.current = (voltageOut - ACS712_offset) / 0.185;
  if (sensorData.current < 0 || sensorData.current > 10.0) sensorData.current = 0.0;

  static unsigned long lastTdsRead = 0;
  if (useAnalogTds) {
    int tdsRaw = analogRead(analogTdsPin);
    float tdsVoltage = tdsRaw * (Vref / 4095.0);
    sensorData.tdsValue = (133.42 * tdsVoltage * tdsVoltage * tdsVoltage - 255.86 * tdsVoltage * tdsVoltage + 857.39 * tdsVoltage) * 0.5;
    if (sensorData.tdsValue < 0 || sensorData.tdsValue > 1000.0) sensorData.tdsValue = 0.0;
    sensorData.smoothedTds = sensorData.tdsValue;
    sensorData.temperature = 0.0;
    sensorData.smoothedTemp = 0.0;
    sensorData.tdsStatus = "Analog";
    lastTdsRead = millis();
  } else {
    if (millis() - lastTdsRequest >= TDS_REQUEST_INTERVAL) {
      requestTDSData();
      lastTdsRequest = millis();
    }
    if (TDSSerial.available() >= 5) {
      parseTDSData();
      if (sensorData.tdsStatus == "UART") {
        lastTdsRead = millis();
      }
    }
    if (millis() - lastTdsRead > TDS_TIMEOUT || tdsReadFailures >= MAX_TDS_FAILURES) {
      if (debugMode) Serial.println("TDS sensor timeout or repeated failures");
      sensorData.tdsStatus = "Timeout";
      int tdsRaw = analogRead(analogTdsPin);
      if (tdsRaw > 100) {
        useAnalogTds = true;
        if (debugMode) Serial.println("Switching to analog TDS sensor");
      }
    }
  }

  detachInterrupt(digitalPinToInterrupt(flowSensorPin));
  unsigned long currentTime = millis();
  float timeInterval = (currentTime - lastSensorRead) / 1000.0;

  if (timeInterval > 0) {
    float pulseFrequency = flowPulseCount / timeInterval;
    flowRate = pulseFrequency / FLOW_CALIBRATION_FACTOR;
    sensorData.flowRate = flowRate;
    float timeIntervalMinutes = timeInterval / 60.0;
    liters += flowRate * timeIntervalMinutes;
  } else {
    flowRate = 0.0;
    sensorData.flowRate = 0.0;
  }

  flowPulseCount = 0;
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowPulse, RISING);

  if (sensorData.smoothedTds > tdsThreshold || (flowRate < flowThreshold && flowRate > 0.1)) {
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(buzzerPin, LOW);
  }

  if (debugMode) Serial.println("Sensors read successfully");
}

void readLDR() {
  long sum = 0;
  for (int i = 0; i < LDR_AVERAGE_SAMPLES; i++) {
    sum += analogRead(ldrPin);
    delayMicroseconds(100);
  }
  sensorData.ldrValue = sum / LDR_AVERAGE_SAMPLES;
  if (sensorData.ldrValue < 0) sensorData.ldrValue = 0;
}

void updateLCD() {
  if (!lcdInitialized) return;

  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() != 0) {
    if (debugMode) Serial.println("ERROR: LCD communication lost, attempting reinitialization...");
    if (initializeLCD()) {
      lcdInitialized = true;
    } else {
      lcdInitialized = false;
      return;
    }
  }

  lcd.setCursor(0, 0);
  lcd.print("V:"); lcd.print(sensorData.voltage, 1);
  lcd.print(" I:"); lcd.print(sensorData.current, 1);
  lcd.print("    ");

  lcd.setCursor(0, 1);
  lcd.print("TDS:"); lcd.print((int)sensorData.smoothedTds);
  lcd.print("ppm "); lcd.print(sensorData.tdsStatus.substring(0, 2));
  lcd.print(" T:"); lcd.print(sensorData.smoothedTemp, 1);
  lcd.print("C  ");

  lcd.setCursor(0, 2);
  lcd.print("F:"); lcd.print(sensorData.flowRate, 1);
  lcd.print(" L:"); lcd.print(sensorData.ldrValue);
  lcd.print("   ");

  lcd.setCursor(0, 3);
  if (wifiConnected) {
    lcd.print("IP:"); lcd.print(WiFi.localIP());
  } else {
    lcd.print("WiFi: Disconnected");
  }

  lcd.setCursor(16, 3);
  lcd.print(sensorData.pumpState ? "ON " : "OFF");
}

void manageLEDsAndBuzzer() {
  unsigned long currentMillis = millis();
  if (!wifiConnected || sensorData.smoothedTds > tdsThreshold || 
      (sensorData.flowRate < flowThreshold && sensorData.flowRate > 0.1) || 
      sensorData.tdsStatus == "No Data" || sensorData.tdsStatus == "Timeout") {
    if (currentMillis - lastAlertToggle > 500) {
      alertState = !alertState;
      digitalWrite(testBuzzerPin, alertState);
      digitalWrite(greenLedPin, alertState);
      lastAlertToggle = currentMillis;
    }
  } else {
    digitalWrite(testBuzzerPin, LOW);
    digitalWrite(greenLedPin, HIGH);
  }
}

void logData() {
  StaticJsonDocument<256> doc;
  doc["voltage"] = sensorData.voltage;
  doc["current"] = sensorData.current;
  doc["tds"] = sensorData.smoothedTds;
  doc["flow"] = sensorData.flowRate;
  doc["liters"] = liters;
  doc["light"] = sensorData.ldrValue;
  doc["timestamp"] = millis();
  doc["temperature"] = sensorData.smoothedTemp;

  String logEntry;
  serializeJson(doc, logEntry);

  preferences.begin("eco-go", false);
  static int logIndex = 0;
  String key = "log_" + String(logIndex);
  preferences.putString(key.c_str(), logEntry);
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  preferences.end();

  if (debugMode) Serial.println("Data logged");
}

void saveSettings() {
  preferences.begin("eco-go", false);
  preferences.putFloat("flowCal", FLOW_CALIBRATION_FACTOR);
  preferences.putFloat("acsOffset", ACS712_offset);
  preferences.putFloat("tdsThresh", tdsThreshold);
  preferences.putFloat("flowThresh", flowThreshold);
  preferences.putFloat("liters", liters);
  preferences.end();

  if (debugMode) Serial.println("Settings saved");
}

void loadSettings() {
  preferences.begin("eco-go", true);
  FLOW_CALIBRATION_FACTOR = preferences.getFloat("flowCal", 7.5);
  ACS712_offset = preferences.getFloat("acsOffset", 2.5);
  tdsThreshold = preferences.getFloat("tdsThresh", 1000.0);
  flowThreshold = preferences.getFloat("flowThresh", 0.5);
  liters = preferences.getFloat("liters", 0.0);
  preferences.end();

  if (debugMode) Serial.println("Settings loaded");
}

void setup() {
  Serial.begin(115200);
  TDSSerial.begin(9600, SERIAL_8N1, TDS_RX_PIN, TDS_TX_PIN);

  pinMode(voltagePin, INPUT);
  pinMode(currentPin, INPUT);
  pinMode(flowSensorPin, INPUT_PULLUP);
  pinMode(ldrPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(testBuzzerPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(analogTdsPin, INPUT);

  digitalWrite(buzzerPin, LOW);
  digitalWrite(pumpPin, LOW);
  digitalWrite(testBuzzerPin, LOW);
  digitalWrite(greenLedPin, LOW);

  if (initializeI2C()) {
    lcdInitialized = initializeLCD();
  } else {
    if (debugMode) Serial.println("I2C initialization failed, skipping LCD");
  }

  attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowPulse, RISING);
  loadSettings();

  if (calibrateTdsOnStartup) {
    calibrateTDS();
  }

  connectToWiFi();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);
  server.on("/togglePump", handleTogglePump);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  if (debugMode) Serial.println("Setup complete");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readSensors();
    updateLCD();
    lastSensorRead = currentMillis;

    StaticJsonDocument<256> doc;
    doc["voltage"] = sensorData.voltage;
    doc["current"] = sensorData.current;
    doc["tds"] = sensorData.smoothedTds;
    doc["flow"] = sensorData.flowRate;
    doc["liters"] = liters;
    doc["light"] = sensorData.ldrValue;
    doc["pump"] = sensorData.pumpState;
    doc["tdsStatus"] = sensorData.tdsStatus;
    doc["temperature"] = sensorData.smoothedTemp;
    if (sensorData.smoothedTds > tdsThreshold && millis() - lastNotification > NOTIFICATION_DEBOUNCE) {
      doc["alert"] = "High TDS!";
      lastNotification = millis();
    } else if (sensorData.flowRate < flowThreshold && sensorData.flowRate > 0.1 && millis() - lastNotification > NOTIFICATION_DEBOUNCE) {
      doc["alert"] = "Low Flow!";
      lastNotification = millis();
    } else {
      doc["alert"] = "";
    }
    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);

    if (currentMillis - lastSerialOutput >= SERIAL_OUTPUT_INTERVAL) {
      Serial.println(json);
      lastSerialOutput = currentMillis;
    }
  }

  if (currentMillis - lastLdrRead >= LDR_READ_INTERVAL) {
    readLDR();
    lastLdrRead = currentMillis;
  }

  if (currentMillis - lastDataLog >= DATA_LOG_INTERVAL) {
    logData();
    lastDataLog = currentMillis;
  }

  manageLEDsAndBuzzer();
  webSocket.loop();
  server.handleClient();
}
