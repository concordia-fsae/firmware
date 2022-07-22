# Aero Sensor Unit

## Contains Information Relating to sensor requirements and implementation notes

#### List of Hardware

###### Sensors

1. 16x Gauge Pressure Sensors
    -MPRLS0300YG00001B
    -I2C: Address 0x18 << 1
    -Command: 0xaa, 0x00, 0x00 (Start Conversion)
    -Reply:
        1. Status (bit 5 = 1) -> Busy, nack, stop (Takes upto 5 ms)
        2. Status, Data23:16, Data15:8, Data7:0
2. 1x air temperature sensor + Differential Dual Port Pressure Sensors
    -NPA-700B-030D
    -I2C: Address 0x28 << 1
    -Always read, can fetch 2, 3, or 4 bytes depending on location of nack
    -Reply:
        1. Status bit 1:0, Bridge Data 13:8
        2. Bridge data 7:0
        3. (Optional) Temperature 10:3
        4. (Optional) Temperature 2:0, X 4:0

__Long Term__
3. 1x air density (can be local to environment)
4. (vehicle speed reading - ideally from CAN bus)
5. Attack Angle of wing (If possible/necessary)
6. Gyroscope for Angle of attack and lateral velocity of wing (if necessary/possible)
7. 4x Linear potentiometer (suspension compression)

###### Electronics

1. STM32F103C8T6
2. Digilent 410-123
    -SD card slot over SPI
3. MAX7356EUG+
    -1x8 I2C multiplexer
    -Incremental r/w of internal registers in enhance mode
    -Single r/w of first register in basic mode
    -I2C Address: 01110,A2,A1,A0b << 1 **IMPLEMENTATION USES 0x70 << 1 FOR BOTH**
    -Configurable address lines upto 8 different addresses
4. Can Transceiver TJA1050

#### Hardware Implementation

#### Sensors

| Sensor | Model No | Quantity |

#### Module Locations
1. Sensors (all) -> Front Wing
2. PCB board -> Front wing
3. Air speed sensor -> in a free stream environment

#### Firmware Implementation


#### Technical Questions
1. The length of each sample?
2. Amount of debouncing?
3. Sampling frequency of ->
    1. Surface taps
    2. Wind speed
    3. Air temperature
    4. Air density
    5. Vehicle velocity (**If implemented**)
    6. Accelerometer (**If implemented**)

