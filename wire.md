
### Components and Pin Definitions
From the code, the components and their corresponding ESP32 pins are:
- **Voltage Sensor**: Pin 34 (GPIO 34)
- **Current Sensor (ACS712)**: Pin 35 (GPIO 35)
- **Flow Sensor**: Pin 26 (GPIO 26)
- **LDR (Light Sensor)**: Pin 33 (GPIO 33)
- **Buzzer**: Pin 25 (GPIO 25)
- **Pump**: Pin 13 (GPIO 13)
- **Test Buzzer**: Pin 27 (GPIO 27)
- **Green LED**: Pin 12 (GPIO 12)
- **Analog TDS Sensor (Fallback)**: Pin 32 (GPIO 32)
- **UART TDS Sensor**:
  - RX: Pin 16 (GPIO 16)
  - TX: Pin 17 (GPIO 17)
- **I2C LCD**:
  - SDA: Pin 21 (GPIO 21)
  - SCL: Pin 22 (GPIO 22)


#### 2. Voltage Sensor
- **Purpose**: Measures voltage (e.g., of a battery or power source).
- **Pin**: GPIO 34 (analog input).
- **Wiring**:
  - **Sensor Output (Signal)** → ESP32 GPIO 34 (Pin 34).
  - **Sensor VCC** → ESP32 3.3V or 5V (depending on sensor; check datasheet).
  - **Sensor GND** → ESP32 GND.
- **Notes**:
  - The code uses a voltage divider (`VOLTAGE_DIVIDER_RATIO = 5.0`), so ensure the sensor output is scaled to 0–3.3V (ESP32 ADC range).
  - Example: For a 0–25V sensor, use a voltage divider (e.g., R1 = 4kΩ, R2 = 1kΩ) to scale 25V to 5V, then ensure it’s within 3.3V for the ESP32.
  - If using a module like ZMPT101B, calibrate the scaling in the code.

#### 3. Current Sensor (ACS712)
- **Purpose**: Measures current (e.g., pump or load current).
- **Pin**: GPIO 35 (analog input).
- **Wiring**:
  - **ACS712 OUT** → ESP32 GPIO 35 (Pin 35).
  - **ACS712 VCC** → ESP32 5V (ACS712 typically requires 5V).
  - **ACS712 GND** → ESP32 GND.
  - **ACS712 IP+ and IP-** → Connect in series with the load (e.g., pump) to measure current.
- **Notes**:
  - The code uses `ACS712_offset = 2.5` and sensitivity `0.185V/A` (for ACS712 5A version). Confirm the ACS712 model (5A, 20A, or 30A) and adjust sensitivity if needed (e.g., 0.100V/A for 20A, 0.066V/A for 30A).
  - Ensure the load current is within the sensor’s range to avoid damage.

#### 4. Flow Sensor
- **Purpose**: Measures water flow rate (e.g., YF-S201 sensor).
- **Pin**: GPIO 26 (digital input with interrupt).
- **Wiring**:
  - **Flow Sensor Signal (Yellow)** → ESP32 GPIO 26 (Pin 26).
  - **Flow Sensor VCC (Red)** → ESP32 5V (most flow sensors require 5–24V; check datasheet).
  - **Flow Sensor GND (Black)** → ESP32 GND.
- **Notes**:
  - The code uses `INPUT_PULLUP` and an interrupt (`RISING`), so no external pull-up resistor is needed.
  - The flow sensor generates pulses proportional to flow rate. The code uses `FLOW_CALIBRATION_FACTOR = 7.5` pulses per liter, typical for YF-S201. Verify with your sensor’s datasheet.
  - Ensure the sensor is installed in the correct flow direction (arrow on sensor).

#### 5. LDR (Light-Dependent Resistor)
- **Purpose**: Measures ambient light intensity.
- **Pin**: GPIO 33 (analog input).
- **Wiring**:
  - **LDR One End** → ESP32 GPIO 33 (Pin 33).
  - **LDR Other End** → ESP32 3.3V.
  - **Resistor (e.g., 10kΩ)** → Connect from GPIO 33 to ESP32 GND (forms a voltage divider with the LDR).
- **Notes**:
  - The LDR and resistor create a voltage divider. The voltage at GPIO 33 varies with light intensity.
  - Adjust the resistor value (e.g., 1kΩ to 100kΩ) based on the LDR’s resistance range for optimal sensitivity.
  - The code averages 5 samples (`LDR_AVERAGE_SAMPLES`) to reduce noise.

#### 6. Buzzer
- **Purpose**: Alerts for high TDS or low flow.
- **Pin**: GPIO 25 (digital output).
- **Wiring**:
  - **Buzzer Positive (+)** → ESP32 GPIO 25 (Pin 25).
  - **Buzzer Negative (-)** → ESP32 GND.
- **Notes**:
  - Use an active buzzer (5V or 3.3V, depending on model) that works with a digital HIGH/LOW signal.
  - If using a passive buzzer, you’ll need to generate a PWM signal, which requires code changes.
  - Add a 100Ω resistor in series to limit current if the buzzer draws too much.

#### 7. Pump
- **Purpose**: Controls a water pump.
- **Pin**: GPIO 13 (digital output).
- **Wiring**:
  - **Pump Control (via Relay or MOSFET)** → ESP32 GPIO 13 (Pin 13).
  - **Pump VCC** → External power supply (e.g., 12V or 24V, depending on pump).
  - **Pump GND** → External power supply GND (shared with ESP32 GND).
- **Notes**:
  - The ESP32 GPIO cannot directly drive a pump due to high current/voltage. Use a **relay module** or **N-channel MOSFET** (e.g., IRLZ44N) with a flyback diode.
  - **Relay Example**:
    - Relay IN → ESP32 GPIO 13.
    - Relay VCC → ESP32 5V or 3.3V (check relay module).
    - Relay GND → ESP32 GND.
    - Relay COM → Pump positive.
    - Relay NO → External power supply positive.
  - **MOSFET Example**:
    - MOSFET Gate → ESP32 GPIO 13 (with 100Ω resistor).
    - MOSFET Drain → Pump negative.
    - MOSFET Source → External power supply GND.
    - Flyback diode across pump (cathode to pump positive, anode to pump negative).
  - Ensure the pump’s voltage matches the power supply.

#### 8. Test Buzzer
- **Purpose**: Alerts for WiFi or sensor issues.
- **Pin**: GPIO 27 (digital output).
- **Wiring**:
  - **Test Buzzer Positive (+)** → ESP32 GPIO 27 (Pin 27).
  - **Test Buzzer Negative (-)** → ESP32 GND.
- **Notes**:
  - Same as the main buzzer; use an active buzzer.
  - Add a 100Ω resistor in series if needed.

#### 9. Green LED
- **Purpose**: Indicates system status (e.g., WiFi connected).
- **Pin**: GPIO 12 (digital output).
- **Wiring**:
  - **LED Anode (+)** → ESP32 GPIO 12 (Pin 12) via a 220Ω resistor.
  - **LED Cathode (-)** → ESP32 GND.
- **Notes**:
  - The resistor limits current to ~10mA (safe for ESP32 GPIO).
  - Ensure correct polarity (longer leg is anode).

#### 10. Analog TDS Sensor (Fallback)
- **Purpose**: Measures Total Dissolved Solids (TDS) if UART TDS fails.
- **Pin**: GPIO 32 (analog input).
- **Wiring**:
  - **TDS Sensor Signal (A)** → ESP32 GPIO 32 (Pin 32).
  - **TDS Sensor VCC** → ESP32 3.3V or 5V (check sensor datasheet).
  - **TDS Sensor GND** → ESP32 GND.
- **Notes**:
  - The code uses a polynomial formula to convert voltage to TDS (`133.42 * V^3 - 255.86 * V^2 + 857.39 * V * 0.5`).
  - Ensure the sensor output is 0–3.3V to avoid damaging the ESP32 ADC.
  - Calibrate using a known TDS solution (e.g., 500 ppm).

#### 11. UART TDS Sensor
- **Purpose**: Primary TDS and temperature measurement via UART.
- **Pins**: RX (GPIO 16), TX (GPIO 17).
- **Wiring**:
  - **TDS Sensor TX** → ESP32 GPIO 16 (RX, Pin 16).
  - **TDS Sensor RX** → ESP32 GPIO 17 (TX, Pin 17).
  - **TDS Sensor VCC** → ESP32 5V or 3.3V (check datasheet; many TDS sensors require 5V).
  - **TDS Sensor GND** → ESP32 GND.
- **Notes**:
  - The code uses `HardwareSerial` on Serial2 (9600 baud, 8N1).
  - If the TDS sensor is 5V, use a **logic level converter** (e.g., 3.3V to 5V) for TX/RX lines to protect ESP32 GPIO.
  - Example Level Converter:
    - Low side (3.3V): Connect to ESP32 GPIO 16 (RX) and GPIO 17 (TX), 3.3V, GND.
    - High side (5V): Connect to TDS sensor TX and RX, 5V, GND.
  - Verify the sensor’s protocol (`A000000000A0` for data, `A600000000A6` for calibration).

#### 12. I2C LCD (20x4)
- **Purpose**: Displays sensor data.
- **Pins**: SDA (GPIO 21), SCL (GPIO 22).
- **Wiring**:
  - **LCD SDA** → ESP32 GPIO 21 (Pin 21).
  - **LCD SCL** → ESP32 GPIO 22 (Pin 22).
  - **LCD VCC** → ESP32 5V (most I2C LCDs require 5V).
  - **LCD GND** → ESP32 GND.
- **Notes**:
  - Add **4.7kΩ pull-up resistors** from SDA and SCL to 3.3V if I2C communication is unstable.
  - Confirm the I2C address (`0x27` in code) using an I2C scanner (common alternatives: `0x3F`).
  - Ensure the `LiquidCrystal_I2C` library is ESP32-compatible (e.g., Frank de Brabander’s version).

### Wiring Diagram Summary

| **Component**         | **ESP32 Pin** | **Connection Details**                                                                 |
|-----------------------|---------------|--------------------------------------------------------------------------------------|
| **Voltage Sensor**    | GPIO 34       | Signal → GPIO 34, VCC → 3.3V/5V, GND → GND                                           |
| **Current Sensor**    | GPIO 35       | OUT → GPIO 35, VCC → 5V, GND → GND, IP+/IP- → Load                                   |
| **Flow Sensor**       | GPIO 26       | Signal → GPIO 26, VCC → 5V, GND → GND                                                |
| **LDR**               | GPIO 33       | LDR → GPIO 33 & 3.3V, 10kΩ resistor → GPIO 33 & GND                                  |
| **Buzzer**            | GPIO 25       | Positive → GPIO 25, Negative → GND (optional 100Ω resistor)                          |
| **Pump**              | GPIO 13       | Relay/MOSFET IN → GPIO 13, Pump → External power via relay/MOSFET, GND → Shared GND  |
| **Test Buzzer**       | GPIO 27       | Positive → GPIO 27, Negative → GND (optional 100Ω resistor)                          |
| **Green LED**         | GPIO 12       | Anode → GPIO 12 via 220Ω resistor, Cathode → GND                                     |
| **Analog TDS**        | GPIO 32       | Signal → GPIO 32, VCC → 3.3V/5V, GND → GND                                           |
| **UART TDS TX**       | GPIO 16 (RX)  | TDS TX → GPIO 16 (via level converter if 5V), VCC → 3.3V/5V, GND → GND               |
| **UART TDS RX**       | GPIO 17 (TX)  | TDS RX → GPIO 17 (via level converter if 5V)                                         |
| **LCD SDA**           | GPIO 21       | SDA → GPIO 21, VCC → 5V, GND → GND, 4.7kΩ pull-up to 3.3V if needed                  |
| **LCD SCL**           | GPIO 22       | SCL → GPIO 22, 4.7kΩ pull-up to 3.3V if needed                                       |

