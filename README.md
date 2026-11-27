# SmartDrill OS — STM32F407 Industrial AC Motor & Drill Controller

An industrial-grade closed-loop AC phase-angle motor controller built around the **STM32F407VET6** microcontroller, featuring a responsive **ILI9341 TFT Touch UI**, real-time RPM monitoring, and a dual-layer hardware/software **Latching Emergency Stop (E-Stop)** system.

## Key Features
* **Precision AC Phase-Angle Control:** 100Hz Zero-Cross detection (H11L1S + SN74LVC1G17) synchronized with hardware timer TIM2 to generate isolated Triac gate pulses (MOC3052M + BTA24-600B).
* **Real-Time RPM & Frequency Telemetry:** Captures Hall-effect sensor pulses via external interrupts (EXTI3) to display live motor RPM and frequency (Hz).
* **Dual-Layer Latching E-Stop (ERR E01):**
  * **Hardware Layer:** Cuts Triac gate emitter path (Q2) and clamps Hall sensor signal (Q3) immediately upon pressing the physical NC E-Stop switch (SW2).
  * **Firmware Layer:** Instant interrupt latching (PC2) that locks the system until the physical E-Stop is released and the on-screen STOP button is pressed.
* **Touch & Physical Control:** Supports both TFT touch (XPT2046 over SPI2) and external industrial pushbuttons (PB0 Start / PC2 E-Stop).

## Hardware Pinout (STM32F407VET6)

| Pin | Function | Peripheral / Mode | Description |
| :--- | :--- | :--- | :--- |
| **PA0** | Zero-Cross Input | TIM2_CH1 (Input Capture) | 100Hz AC mains sync pulses |
| **PA1** | Speed Setpoint | ADC1_IN1 (Analog) | Potentiometer speed control (0-100%) |
| **PA3** | Hall Sensor Input | EXTI3 (Falling Edge) | Motor RPM feedback pulses |
| **PA5** | Triac Gate Output | GPIO / TIM2 Compare | Isolated gate drive via Q2 & MOC3052M |
| **PB0** | External Start | GPIO Input (Pull-Up) | Active-Low physical START button (SW1) |
| **PC2** | Emergency Stop | EXTI2 (Rising Edge) | NC E-Stop loop (0V = OK, 3.3V = Latch Fault) |
| **PB12-15** | Touch Controller | SPI2 (XPT2046) | Touchscreen coordinates (PC5 = T_IRQ) |
| **FSMC** | TFT Display | 16-bit Parallel (ILI9341) | 240x320 graphical user interface |

## How to Flash
Flash `build/SmartDrill_F407.hex` to the **STM32F407VET6** using **STM32CubeProgrammer** via ST-Link V2.
