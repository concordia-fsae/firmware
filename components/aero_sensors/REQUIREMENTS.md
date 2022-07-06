# Aero Sensor Unit

## Contains Information Relating to sensor requirements and implementation notes

#### List of Sensors
1. 16x differential pressure sensors
2. 1x air temperature sensor (can be local to environment)
3. 1x air density (can be local to environment)
4. (air speed - local to direction of travel)
5. (vehicle speed reading - ideally from CAN bus)
6. Attack Angle of wing (If possible/necessary)
7. Gyroscope for Angle of attack and lateral velocity of wing (if necessary/possible)
8. 4x Linear potentiometer (suspension compression)

#### Hardware Implementation
- 16 Analog differential pressure signals into multiplexer
- 16:1 Analog multiplexer into ~10x Analog Amplifier
- Analog Amplifier into ADC of microcontroller 
- Range into Microcontroller ADC would be 500-1000mV for pressure sensors
- Additional sensor reading inputs to be determined
- Output to storage (Initial: SD; Long Term: CAN bus to DAQ)

#### Sensors

| Sensor | Model No | Quantity |
| DPS    | MPS20N0040D-D 5VDC | 16 | - Mandatory
| ATS    | MPL3115A2 | 1 | - Skip for now
| ADS    | MPL3115A2 | ^ | - Skip for now
| ASS    | HV120-SM02-R | 1 | - Mandatory
| Linear | ? | 4 | - Seperate module 

#### Module Locations
1. Sensors (all) -> Front Wing
2. PCB board -> Front wing
3. Air speed sensor -> in a free stream environment

#### Firmware Implementation
- ADC Channel 0-3 for 4 seperate AMUX
- DMA in circular mode
    - At every HAL_ADC_ConvCpltCallback(), the mux line selection will have to be changed
    - **May need to be temporarily delayed for circuit to apply**


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

