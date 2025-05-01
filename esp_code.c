#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>

// Wi-Fi credentials
const char* ssid = "Hidden";
const char* password = "abdullah1122";
const char* serverName = "http://192.168.205.6:5000/update"; // Replace with your Flask server IP

// LCD setup
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Pin definitions for ESP32
const int voltagePin = 34;  // Analog pin (GPIO34)
const int currentPin = 35;  // Analog pin (GPIO35)
const int tdsPin = 32;      // Analog pin (GPIO32)
const int flowSensorPin = 26; // Digital pin (GPIO26)
const int ldrPin = 33;      // Analog pin (GPIO33)
const int buzzerPin = 25;   // Digital pin (GPIO25) - Original buzzer
const int switchPin = 13;   // Digital pin (GPIO13) for switch from web app
const int testBuzzerPin = 27; // New buzzer for connection test
const int greenLedPin = 12;   // Green LED for connection status

// Flow sensor variables
volatile int flowPulseCount = 0;
unsigned long oldTime = 0;
float flowRate = 0.0;
float liters = 0.0;

// Sensor calibration
float ACS712_offset = 2.5;
float ecCalibration = 1.0;
float Vref = 3.3; // ESP32 uses 3.3V reference
float calibrationFactor = 0.909;

// Connection variables
bool serverConnected = false;
bool wifiConnected = false;
unsigned long lastConnectionAttempt = 0;
unsigned long lastAlertToggle = 0;
bool alertState = false;
unsigned long wifiReconnectAttempt = 0;

// Non-blocking timer variables
unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
unsigned long lastSerialOutput = 0;

void IRAM_ATTR flowPulse() {
  flowPulseCount++;
}

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(flowSensorPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(switchPin, OUTPUT);
  pinMode(testBuzzerPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  
  // Initial state
  digitalWrite(switchPin, LOW);
  digitalWrite(testBuzzerPin, LOW);
  digitalWrite(greenLedPin, LOW);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Eco-Go");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  
  // Start WiFi connection
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 2);
  lcd.print("Connecting to WiFi");
  
  // Wait for WiFi with quick timeout and alert pattern
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    // Quick alert pattern during initial connection
    if (millis() - lastAlertToggle > 50) { // Very fast 50ms toggle
      alertState = !alertState;
      digitalWrite(testBuzzerPin, alertState);
      digitalWrite(greenLedPin, alertState);
      lastAlertToggle = millis();
      Serial.print(".");
    }
    // Don't block CPU - just yield
    yield();
  }
  
  // Check if WiFi connected successfully
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nConnected to WiFi");
    Serial.println(WiFi.localIP());
    lcd.setCursor(0, 3);
    lcd.print("WiFi: " + WiFi.localIP().toString());
    // Connection successful - buzzer off, green LED stable ON
    digitalWrite(testBuzzerPin, LOW);
    digitalWrite(greenLedPin, HIGH);
  } else {
    // Failed to connect - will continue trying in the loop
    lcd.setCursor(0, 3);
    lcd.print("WiFi: Connecting...");
  }

  // Attach interrupt for flow sensor
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowPulse, RISING);
}

void loop() {
  unsigned long currentMillis = millis();
  
  // ========================
  // CONNECTION MANAGEMENT - NON-BLOCKING
  // ========================
  
  // Check WiFi connection status (fast response)
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      // WiFi just connected
      wifiConnected = true;
      Serial.println("WiFi connected!");
      lcd.setCursor(0, 3);
      lcd.print("WiFi: " + WiFi.localIP().toString());
    }
  } else {
    if (wifiConnected) {
      // WiFi just disconnected
      wifiConnected = false;
      serverConnected = false;
      Serial.println("WiFi disconnected!");
      lcd.setCursor(0, 3);
      lcd.print("WiFi: Disconnected");
    }
    
    // Try reconnect every 5 seconds without blocking
    if (currentMillis - wifiReconnectAttempt > 5000) {
      Serial.println("Attempting WiFi reconnection...");
      WiFi.begin(ssid, password);
      wifiReconnectAttempt = currentMillis;
    }
  }

  // Modified Connection alert pattern (non-blocking)
  if (!wifiConnected) {
    // When WiFi is not connected: Both buzzer and LED toggle quickly (250ms)
    if (currentMillis - lastAlertToggle > 250) {
      alertState = !alertState;
      digitalWrite(testBuzzerPin, alertState);
      digitalWrite(greenLedPin, alertState);
      lastAlertToggle = currentMillis;
    }
  } else {
    // WiFi is connected: Check server status
    if (serverConnected) {
      // Server connected: Buzzer OFF, LED steady ON
      digitalWrite(testBuzzerPin, LOW);
      digitalWrite(greenLedPin, HIGH);
    } else {
      // Server not connected: LED toggles quickly (250ms), Buzzer OFF
      digitalWrite(testBuzzerPin, LOW);
      if (currentMillis - lastAlertToggle > 250) {
        alertState = !alertState;
        digitalWrite(greenLedPin, alertState);
        lastAlertToggle = currentMillis;
      }
    }
  }
  
  // ========================
  // SENSOR READINGS - NON-BLOCKING
  // ========================
  
  if (currentMillis - lastSensorRead > 250) { // Read sensors every 250ms
    // Calculate flow rate (detach interrupt briefly)
    detachInterrupt(digitalPinToInterrupt(flowSensorPin));
    flowRate = (flowPulseCount / 7.5);
    liters += flowRate / (60.0 * 4.0); // Adjust for 250ms interval
    flowPulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowPulse, RISING);
    
    // Read voltage
    int voltageRaw = analogRead(voltagePin);
    float voltage = (voltageRaw * (3.3 / 4095.0)) * 5.0;
    
    // Read current
    int currentRaw = analogRead(currentPin);
    float voltageOut = (currentRaw * 3.3) / 4095.0;
    float current = (voltageOut - ACS712_offset) / 0.185;
    
    // Read TDS
    float waterTemp = 25.0;
    int tdsRaw = analogRead(tdsPin);
    float tdsVoltage = tdsRaw * (Vref / 4095.0);
    float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
    float ec = (tdsVoltage / temperatureCoefficient) * ecCalibration;
    float tdsValue = ((133.42 * pow(ec, 3)) - (255.86 * pow(ec, 2)) + (857.39 * ec)) * 0.5 * calibrationFactor;
    
    // Read LDR
    int ldrValue = analogRead(ldrPin);
    
    // Original buzzer logic for water quality
    if (tdsValue > 1000 || flowRate < 0.5) {
      digitalWrite(buzzerPin, HIGH);
    } else {
      digitalWrite(buzzerPin, LOW);
    }
    
    // Update LCD (don't clear whole screen - update specific positions for less flicker)
    lcd.setCursor(0, 0);
    lcd.print("V:"); lcd.print(voltage, 1); lcd.print(" I:"); lcd.print(current, 1);
    lcd.setCursor(0, 1);
    lcd.print("TDS:"); lcd.print((int)tdsValue); lcd.print("ppm F:"); lcd.print(flowRate, 1);
    lcd.setCursor(0, 2);
    lcd.print("Light:"); lcd.print(ldrValue);
    lcd.setCursor(0, 3);
    if (wifiConnected) {
      lcd.print("Total:"); lcd.print(liters, 1); lcd.print("L");
    } else {
      lcd.print("WiFi: Connecting...");
    }
    
    // Show switch status
    lcd.setCursor(16, 3);
    lcd.print(digitalRead(switchPin) ? "ON " : "OFF");
    
    lastSensorRead = currentMillis;
  }
  
  // ========================
  // SERVER COMMUNICATION - NON-BLOCKING
  // ========================
  
  // Send data to server every 2 seconds if WiFi is connected
  if (wifiConnected && (currentMillis - lastDataSend > 2000)) {
    static HTTPClient http;
    
    // Get the latest readings
    int voltageRaw = analogRead(voltagePin);
    float voltage = (voltageRaw * (3.3 / 4095.0)) * 5.0;
    
    int currentRaw = analogRead(currentPin);
    float voltageOut = (currentRaw * 3.3) / 4095.0;
    float current = (voltageOut - ACS712_offset) / 0.185;
    
    float waterTemp = 25.0;
    int tdsRaw = analogRead(tdsPin);
    float tdsVoltage = tdsRaw * (Vref / 4095.0);
    float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
    float ec = (tdsVoltage / temperatureCoefficient) * ecCalibration;
    float tdsValue = ((133.42 * pow(ec, 3)) - (255.86 * pow(ec, 2)) + (857.39 * ec)) * 0.5 * calibrationFactor;
    
    int ldrValue = analogRead(ldrPin);
    
    // Begin HTTP request (non-blocking)
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    
    String jsonData = "{\"voltage\":" + String(voltage, 1) + 
                      ",\"current\":" + String(current, 2) + 
                      ",\"tds\":" + String(tdsValue, 1) + 
                      ",\"flow\":" + String(flowRate, 1) + 
                      ",\"liters\":" + String(liters, 1) + 
                      ",\"light\":" + String(ldrValue) + 
                      ",\"switch\":" + String(digitalRead(switchPin)) + "}";
    
    // Send the request with short timeout (500ms)
    int httpResponseCode = http.POST(jsonData);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      
      // Update server connection status
      if (!serverConnected) {
        serverConnected = true;
        Serial.println("Server connected!");
      }
      
      // Process switch control commands
      if (response.indexOf("switch_on") != -1) {
        digitalWrite(switchPin, HIGH);
        lcd.setCursor(16, 3);
        lcd.print("ON ");
      } else if (response.indexOf("switch_off") != -1) {
        digitalWrite(switchPin, LOW);
        lcd.setCursor(16, 3);
        lcd.print("OFF");
      }
    } else {
      // Server error
      if (serverConnected) {
        serverConnected = false;
        Serial.println("Server connection failed!");
      }
    }
    
    http.end();
    lastDataSend = currentMillis;
  }
  
  // ========================
  // SERIAL OUTPUT - NON-BLOCKING
  // ========================
  
  // Output debug information every second
  if (currentMillis - lastSerialOutput > 1000) {
    // Get latest readings for console output
    int voltageRaw = analogRead(voltagePin);
    float voltage = (voltageRaw * (3.3 / 4095.0)) * 5.0;
    
    int currentRaw = analogRead(currentPin);
    float voltageOut = (currentRaw * 3.3) / 4095.0;
    float current = (voltageOut - ACS712_offset) / 0.185;
    
    float waterTemp = 25.0;
    int tdsRaw = analogRead(tdsPin);
    float tdsVoltage = tdsRaw * (Vref / 4095.0);
    float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
    float ec = (tdsVoltage / temperatureCoefficient) * ecCalibration;
    float tdsValue = ((133.42 * pow(ec, 3)) - (255.86 * pow(ec, 2)) + (857.39 * ec)) * 0.5 * calibrationFactor;
    
    int ldrValue = analogRead(ldrPin);
    
    Serial.print("Voltage: "); Serial.print(voltage, 1); Serial.print("V | ");
    Serial.print("Current: "); Serial.print(current, 2); Serial.print("A | ");
    Serial.print("TDS: "); Serial.print(tdsValue, 1); Serial.print("ppm | ");
    Serial.print("Flow: "); Serial.print(flowRate, 1); Serial.print(" L/min | ");
    Serial.print("Total: "); Serial.print(liters, 1); Serial.print("L | ");
    Serial.print("Light: "); Serial.print(ldrValue); Serial.print(" | ");
    Serial.print("WiFi: "); Serial.print(wifiConnected ? "Connected" : "Disconnected"); Serial.print(" | ");
    Serial.print("Server: "); Serial.print(serverConnected ? "Connected" : "Disconnected"); Serial.print(" | ");
    Serial.print("Switch: "); Serial.println(digitalRead(switchPin));
    
    lastSerialOutput = currentMillis;
  }
  
  // Always yield at the end of the loop to prevent watchdog timer issues
  yield();
}
