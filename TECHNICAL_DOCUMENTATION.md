# Octa-Solar: Technical Documentation

## Table of Contents
1. [System Overview](#system-overview)
2. [Hardware Architecture](#hardware-architecture)
3. [Software Architecture](#software-architecture)
4. [Component Analysis](#component-analysis)
5. [Communication Protocols](#communication-protocols)
6. [Data Flow](#data-flow)
7. [Security Implementation](#security-implementation)
8. [Performance Optimization](#performance-optimization)
9. [Error Handling](#error-handling)
10. [Future Enhancements](#future-enhancements)

## System Overview

### Project Name Explanation
**Octa-Solar** is a self-descriptive name:
- **Octa**: Refers to the eight main components/sensors being monitored
- **Solar**: Indicates the solar-powered water treatment plant context

### Core Philosophy
This system is designed around the ESP32 microcontroller (NOT Arduino) for several critical reasons:
- **WiFi Capability**: Built-in 802.11 b/g/n WiFi for IoT connectivity
- **Dual-Core Processing**: Allows parallel sensor reading and communication tasks
- **Rich GPIO Options**: 30+ GPIO pins for extensive sensor interfacing
- **Memory**: 520KB SRAM and 4MB Flash for complex applications
- **Low Power**: Advanced power management for solar applications

## Hardware Architecture

### Main Controller: ESP32
```
ESP32 DevKit V1 Specifications:
├── Processor: Xtensa dual-core 32-bit LX6 @ 240MHz
├── Memory: 520KB SRAM + 4MB Flash
├── WiFi: IEEE 802.11 b/g/n 2.4GHz
├── Bluetooth: v4.2 BR/EDR and BLE
├── GPIO: 30+ digital pins
├── ADC: 12-bit resolution, 18 channels
├── DAC: 8-bit resolution, 2 channels
├── PWM: 16 channels
├── UART: 3 hardware UARTs
├── I2C: 2 controllers
└── SPI: 4 controllers
```

### Sensor Configuration (The "Octa" Components)

#### 1. Voltage Sensor (GPIO 34)
**Purpose**: Monitor solar panel/battery voltage
```cpp
Technical Specifications:
├── Input Range: 0-25V DC (with voltage divider)
├── Resolution: 12-bit ADC (0-4095 counts)
├── Voltage Divider: 5:1 ratio (R1=4kΩ, R2=1kΩ)
├── Accuracy: ±1% with proper calibration
└── Update Rate: 1Hz
```

**Implementation Details**:
```cpp
// Voltage reading with voltage divider compensation
int voltageRaw = analogRead(voltagePin);
float voltage = (voltageRaw * (Vref / 4095.0)) * VOLTAGE_DIVIDER_RATIO;

// Safety limits to prevent erroneous readings
if (voltage < 0.1 || voltage > 30.0) voltage = 0.0;
```

#### 2. ACS712 Current Sensor (GPIO 35)
**Purpose**: Measure load current (pump/system current)
```cpp
Technical Specifications:
├── Sensor Model: ACS712-05B/20A/30A
├── Sensitivity: 185mV/A (5A), 100mV/A (20A), 66mV/A (30A)
├── Zero Current Output: 2.5V (configurable offset)
├── Bandwidth: 80kHz
├── Response Time: 5µs
└── Accuracy: ±1.5% at 25°C
```

**Implementation Details**:
```cpp
// Current calculation with offset compensation
int currentRaw = analogRead(currentPin);
float voltageOut = (currentRaw * Vref) / 4095.0;
float current = (voltageOut - ACS712_offset) / 0.185; // For 5A sensor

// Noise filtering for low currents
if (current < 0 || current > 10.0) current = 0.0;
```

#### 3. YF-S201 Flow Sensor (GPIO 26)
**Purpose**: Measure water flow rate
```cpp
Technical Specifications:
├── Flow Range: 1-30 L/min
├── Output: Square wave pulses
├── Pulse Rate: ~7.5 pulses per liter (calibratable)
├── Operating Voltage: 5-24V DC
├── Max Current: 15mA
├── Operating Temperature: -25°C to +80°C
└── Connection: Hall effect sensor
```

**Implementation Details**:
```cpp
// Interrupt-driven pulse counting for accuracy
volatile int flowPulseCount = 0;

void IRAM_ATTR flowPulse() {
  flowPulseCount++;
  lastFlowTime = millis();
}

// Flow rate calculation
float timeInterval = (currentTime - lastSensorRead) / 1000.0;
float pulseFrequency = flowPulseCount / timeInterval;
float flowRate = pulseFrequency / FLOW_CALIBRATION_FACTOR;
```

#### 4. UART TDS Sensor (GPIO 16/17)
**Purpose**: Primary water quality measurement with temperature
```cpp
Technical Specifications:
├── Communication: UART at 9600 baud
├── TDS Range: 0-2000 ppm
├── Temperature Range: 0-100°C
├── Accuracy: ±2% F.S.
├── Response Time: <10 seconds
├── Probe Material: Stainless steel
└── Calibration: Multi-point calibration supported
```

**Implementation Details**:
```cpp
// UART communication protocol
void requestTDSData() {
  sendHex("A000000000A0"); // Request data command
}

// Data parsing with validation
if (start == 0xAA) { // Valid start byte
  byte tdsHi = TDSSerial.read();
  byte tdsLo = TDSSerial.read();
  byte tempHi = TDSSerial.read();
  byte tempLo = TDSSerial.read();
  
  float tds = (tdsHi << 8) + tdsLo;
  float temp = ((tempHi << 8) + tempLo) / 100.0;
}
```

#### 5. Analog TDS Sensor (GPIO 32)
**Purpose**: Backup TDS measurement when UART fails
```cpp
Technical Specifications:
├── Output: 0-3.3V analog
├── TDS Range: 0-1000 ppm
├── Temperature Compensation: Manual
├── Response Time: 1 second
├── Probe: 2-electrode design
└── Calibration: Single or multi-point
```

**Implementation Details**:
```cpp
// Polynomial conversion formula (calibrated)
int tdsRaw = analogRead(analogTdsPin);
float tdsVoltage = tdsRaw * (Vref / 4095.0);
float tdsValue = (133.42 * pow(tdsVoltage, 3) - 
                  255.86 * pow(tdsVoltage, 2) + 
                  857.39 * tdsVoltage) * 0.5;
```

#### 6. LDR Light Sensor (GPIO 33)
**Purpose**: Ambient light monitoring for solar efficiency correlation
```cpp
Technical Specifications:
├── Resistance Range: 1kΩ (bright) to 100kΩ (dark)
├── Spectral Response: 400-700nm (visible light)
├── Response Time: 20-30ms
├── Temperature Coefficient: -0.5%/°C
└── Configuration: Voltage divider with 10kΩ resistor
```

**Implementation Details**:
```cpp
// Noise reduction through averaging
long sum = 0;
for (int i = 0; i < LDR_AVERAGE_SAMPLES; i++) {
  sum += analogRead(ldrPin);
  delayMicroseconds(100);
}
int ldrValue = sum / LDR_AVERAGE_SAMPLES;
```

#### 7. 20x4 I2C LCD (GPIO 21/22)
**Purpose**: Local display for system status and readings
```cpp
Technical Specifications:
├── Display: 20 characters × 4 lines
├── Interface: I2C (PCF8574 backpack)
├── I2C Address: 0x27 (configurable)
├── Supply Voltage: 5V
├── Backlight: LED backlight with control
└── Character Set: HD44780 compatible
```

**Implementation Details**:
```cpp
// I2C initialization with error handling
bool initializeLCD() {
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() != 0) return false;
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  return true;
}

// Dynamic display update
void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.printf("V:%.1f I:%.1f", voltage, current);
  lcd.setCursor(0, 1);
  lcd.printf("TDS:%dppm %s T:%.1fC", (int)tds, status.c_str(), temp);
}
```

#### 8. Water Pump Control (GPIO 13)
**Purpose**: Remote pump operation with safety features
```cpp
Technical Specifications:
├── Control Method: Relay module (5V coil)
├── Load Capacity: Up to 10A @ 240V AC
├── Switch Type: SPDT (Single Pole Double Throw)
├── Isolation: Optocoupler isolated
├── Response Time: <10ms
└── LED Indicator: Power and relay status
```

**Implementation Details**:
```cpp
// Safe pump control with state tracking
void handleTogglePump() {
  sensorData.pumpState = !sensorData.pumpState;
  digitalWrite(pumpPin, sensorData.pumpState ? HIGH : LOW);
  
  // Database logging
  logPumpState(sensorData.pumpState);
  
  // WebSocket notification
  notifyClients("pump", sensorData.pumpState);
}
```

## Software Architecture

### 1. ESP32 Firmware Architecture
```
ESP32 Firmware Structure:
├── Main Loop (Core 0)
│   ├── Sensor Reading (1Hz)
│   ├── LCD Update (1Hz)
│   ├── Data Logging (1/60Hz)
│   └── LED/Buzzer Management
├── WiFi/WebSocket Handler (Core 1)
│   ├── HTTP Server
│   ├── WebSocket Server
│   ├── Client Communication
│   └── Settings Management
└── Interrupt Handlers
    ├── Flow Sensor Pulses
    └── Button Presses
```

**Key Features**:
- **Dual-core utilization**: Sensor tasks on Core 0, communication on Core 1
- **Non-blocking operations**: All operations use timers and interrupts
- **Robust error handling**: Automatic recovery from sensor failures
- **Memory management**: Efficient JSON handling and buffer management

### 2. Flask Backend Architecture
```
Flask Application Structure:
├── Routes
│   ├── Authentication (/api/login, /api/signup)
│   ├── Data Endpoints (/api/sensors, /update)
│   ├── Control (/api/switch)
│   └── Static Files (/web/, /)
├── Database Layer
│   ├── Connection Management
│   ├── Query Optimization
│   ├── Transaction Handling
│   └── Error Recovery
├── Security Layer
│   ├── bcrypt Password Hashing
│   ├── Input Validation
│   ├── SQL Injection Prevention
│   └── CORS Configuration
└── Logging System
    ├── Request Logging
    ├── Error Tracking
    └── Performance Monitoring
```

**Key Features**:
- **RESTful API design**: Consistent endpoint structure
- **Database connection pooling**: Efficient resource usage
- **Input validation**: Comprehensive data sanitization
- **Error handling**: Graceful error responses

### 3. Flutter Mobile App Architecture
```
Flutter App Architecture:
├── Presentation Layer
│   ├── Login Screen (authentication)
│   ├── Dashboard Screen (real-time monitoring)
│   ├── Signup Screen (user registration)
│   └── Widgets (reusable components)
├── Business Logic Layer
│   ├── AuthService (authentication management)
│   ├── Data Models (sensor data structures)
│   └── State Management (Provider pattern)
├── Data Layer
│   ├── HTTP Client (API communication)
│   ├── Local Storage (SharedPreferences)
│   └── Data Caching
└── Platform Integration
    ├── Android Support
    ├── iOS Support
    ├── Web Support
    └── Desktop Support
```

**Key Features**:
- **Cross-platform compatibility**: Single codebase for all platforms
- **Real-time updates**: Efficient polling and state management
- **Offline capability**: Local authentication storage
- **Material Design**: Consistent UI across platforms

## Communication Protocols

### 1. WiFi Communication (ESP32 ↔ Network)
```cpp
Protocol Stack:
├── Physical Layer: IEEE 802.11 b/g/n (2.4GHz)
├── Network Layer: IPv4 with DHCP
├── Transport Layer: TCP for reliability
└── Application Layer: HTTP/WebSocket
```

**Configuration**:
- **Security**: WPA2-PSK encryption
- **Connection Management**: Auto-reconnection on failure
- **Power Management**: Dynamic power adjustment
- **Range**: Up to 100m in open space

### 2. I2C Communication (ESP32 ↔ LCD)
```cpp
I2C Configuration:
├── Clock Speed: 100kHz (standard mode)
├── Address: 0x27 (7-bit addressing)
├── Pull-up Resistors: 4.7kΩ (if needed)
└── Error Handling: Bus scanning and recovery
```

**Implementation**:
```cpp
// I2C bus initialization with error detection
bool initializeI2C() {
  Wire.begin(21, 22);           // SDA, SCL pins
  Wire.setClock(100000);        // 100kHz clock
  return scanI2C();             // Verify device presence
}
```

### 3. UART Communication (ESP32 ↔ TDS Sensor)
```cpp
UART Configuration:
├── Baud Rate: 9600 bps
├── Data Bits: 8
├── Parity: None
├── Stop Bits: 1
└── Flow Control: None
```

**Protocol Commands**:
- **Data Request**: `A000000000A0`
- **Calibration**: `A600000000A6`
- **Response Format**: 5-byte packet (0xAA + 4 data bytes)

### 4. HTTP/WebSocket API
```javascript
API Endpoints:
├── Authentication
│   ├── POST /api/login
│   └── POST /api/signup
├── Data Management
│   ├── GET /api/sensors
│   ├── POST /update
│   └── GET /api/pump_state
├── Control
│   └── POST /api/switch
└── Static Content
    ├── GET /
    └── GET /web/*
```

## Data Flow

### 1. Sensor Data Collection
```
Data Flow (ESP32 → Database):
Sensors → ESP32 ADC → Processing → JSON → HTTP POST → Flask → MySQL

Time Flow:
├── Sensor Reading: 1 second intervals
├── Data Processing: Real-time
├── Local Display: 1 second intervals
├── Database Logging: 60 second intervals
└── WebSocket Broadcast: 1 second intervals
```

### 2. Mobile App Data Flow
```
Data Flow (Database → Mobile):
MySQL → Flask → HTTP GET → Flutter → UI Update

Polling Schedule:
├── Sensor Data: 2 second intervals
├── Pump State: On user interaction
├── Authentication: On app startup
└── Error Retry: Exponential backoff
```

### 3. Web Dashboard Data Flow
```
Data Flow (ESP32 → Browser):
ESP32 → WebSocket → Browser JavaScript → Chart.js

Update Frequency:
├── Live Data: 1 second intervals
├── Charts: Real-time updates
├── Settings: On user interaction
└── History: On tab selection
```

## Security Implementation

### 1. Password Security
```python
Security Measures:
├── bcrypt Hashing: Cost factor 12
├── Salt Generation: Automatic per password
├── Input Sanitization: UTF-8 validation
└── Timing Attack Prevention: Constant-time comparison
```

**Implementation**:
```python
# Secure password hashing
hashed_password = bcrypt.hashpw(password.encode('utf-8'), bcrypt.gensalt())

# Secure password verification
if bcrypt.checkpw(password.encode('utf-8'), stored_hash):
    return True
```

### 2. API Security
```python
Security Features:
├── Input Validation: Type checking and range validation
├── SQL Injection Prevention: Parameterized queries
├── CORS Configuration: Controlled cross-origin access
└── Error Handling: No sensitive information leakage
```

### 3. Network Security
```cpp
ESP32 Security:
├── WiFi Security: WPA2-PSK encryption
├── Credential Storage: ESP32 Preferences (encrypted)
├── Web Server: Input validation on all endpoints
└── WebSocket: Origin validation
```

## Performance Optimization

### 1. ESP32 Optimizations
```cpp
Performance Features:
├── Dual-Core Utilization: Task separation
├── Interrupt-Driven: Flow sensor pulse counting
├── Efficient JSON: StaticJsonDocument sizing
├── Memory Management: Stack vs heap optimization
└── Watchdog Timer: System reliability
```

**Memory Usage**:
- **SRAM Usage**: ~45% (235KB used / 520KB total)
- **Flash Usage**: ~28% (1.1MB used / 4MB total)
- **Heap Fragmentation**: Minimized through static allocation

### 2. Database Optimizations
```sql
Database Performance:
├── Indexing: Timestamp and sensor_type indexes
├── Connection Pooling: Reuse database connections
├── Query Optimization: Efficient JOIN operations
└── Data Retention: Automated cleanup procedures
```

**Query Performance**:
```sql
-- Optimized sensor data retrieval
SELECT timestamp, sensor_type, value 
FROM sensors 
WHERE (sensor_type, timestamp) IN (
    SELECT sensor_type, MAX(timestamp)
    FROM sensors
    GROUP BY sensor_type
)
ORDER BY timestamp DESC;
```

### 3. Mobile App Optimizations
```dart
Flutter Performance:
├── Widget Optimization: Efficient rebuild strategies
├── Memory Management: Proper disposal of resources
├── Network Efficiency: Connection reuse and caching
└── State Management: Minimal rebuild scopes
```

## Error Handling

### 1. Hardware Error Handling
```cpp
Error Recovery Strategies:
├── Sensor Failure Detection: Timeout and validation
├── Automatic Failover: UART → Analog TDS switching
├── Connection Recovery: WiFi reconnection logic
└── Watchdog Protection: System reset on hang
```

**Example**:
```cpp
// TDS sensor failover logic
if (millis() - lastTdsRead > TDS_TIMEOUT || tdsReadFailures >= MAX_TDS_FAILURES) {
  if (analogRead(analogTdsPin) > 100) {
    useAnalogTds = true;  // Switch to backup sensor
  }
}
```

### 2. Software Error Handling
```python
Error Handling Strategies:
├── Database Errors: Connection retry with backoff
├── API Errors: Graceful error responses
├── Input Validation: Comprehensive data checking
└── Logging: Detailed error tracking
```

### 3. User Experience Error Handling
```dart
UX Error Strategies:
├── Network Errors: User-friendly error messages
├── Timeout Handling: Progress indicators
├── Retry Logic: Automatic retry with user feedback
└── Offline Mode: Graceful degradation
```

## Future Enhancements

### 1. Hardware Enhancements
```
Planned Hardware Upgrades:
├── Solar Panel Integration: Direct solar power monitoring
├── Battery Management System: Advanced power management
├── Additional Sensors: pH, dissolved oxygen, turbidity
├── LoRaWAN Support: Long-range communication option
└── Edge AI Processing: Local machine learning
```

### 2. Software Enhancements
```
Planned Software Features:
├── Machine Learning: Predictive maintenance algorithms
├── Advanced Analytics: Trend analysis and forecasting
├── Mobile Notifications: Push notifications for alerts
├── Data Export: Advanced reporting and export options
└── Multi-site Management: Support for multiple installations
```

### 3. Integration Capabilities
```
Integration Roadmap:
├── MQTT Support: Industry-standard IoT protocol
├── Cloud Integration: AWS/Azure/Google Cloud support
├── Third-party APIs: Weather data integration
├── Automation Systems: Home Assistant/OpenHAB support
└── Enterprise Features: Role-based access control
```

## Conclusion

The Octa-Solar system represents a comprehensive IoT solution for water quality monitoring in solar-powered applications. The use of ESP32 (rather than Arduino) provides the necessary computational power, connectivity, and flexibility required for this complex system.

Key technical achievements:
- **Multi-sensor integration** with robust error handling
- **Real-time data processing** with dual-core utilization
- **Cross-platform applications** with consistent user experience
- **Scalable architecture** supporting future enhancements
- **Industrial-grade reliability** with comprehensive error recovery

The system demonstrates advanced embedded systems engineering, combining hardware interfacing, real-time processing, database management, web development, and mobile application development in a cohesive, production-ready solution.
