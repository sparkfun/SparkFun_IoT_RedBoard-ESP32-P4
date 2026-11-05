# Hardware Overview

## ESP32-P4 SoC

The main processor on this IoT RedBoard, the ESP32-P4 is a high-performance dual-core RISC-V CPU that includes a host of impressive features including AI instruction extensions, advanced memory subsystem and high-speed peripherals. The ESP32-P4's dual RISC-V cores run at speeds up to 400M Hz for high performance applications but also has an integrated low-power core that runs up to 40 MHz for ultra-low power applications. The P4 boasts a wide range of peripheral options including I2C, SPI, I2S, LED PWM, TWAI<sup>&trade;</sup> as well as full and high-speed USB and high-speed SDIO capability. For a complete overview of the ESP32-P4, refer to the [datasheet](/ref/esp32-p4_datasheet_en.pdf)

## Networking

The main difference between the two versions of the IoT RedBoard ESP32-P4 is how they handle networking. The WiFi/BLE version has an ESP32-C5 WROOM module for wireless connectivity, the PoE version features an Ethernet connector, transceiver and buck regulator for a wired internet connection that can also power the board.

### ESP32-C5 (WiFi/BLE Version Only)

The WiFi/BLE version uses an ESP32-C5 WROOM module to connect to wireless networks. The ESP32-C5 supports dual-band 2.4 and 5 GHz WiFi, Bluetooth<sup>&trade;</sup> 5 (LE), Zibee and Thread networks. It features an IEEE standard 802.15.4 subsystem for common software stacks. The ESP32-C5's primary use on this board is network connectivity but the board does also include a 0.1"-spaced serial header connected to the C5 to access the chip's UART interface for reprogramming. For complete information on the ESP32-C5, refer to the [datasheet](/ref/esp32-c5-wroom-1_wroom-1u_datasheet_en.pdf)

### Ethernet (PoE Version Only)

The PoE version includes a MagJack for an Ethernet connection to the internet as well as powering the board over PoE. The board has a 10/100Mbps transceiver for a wired internet connection over Ethernet. 

## Power

### Barrel Jack

The barrel jack connector on the RedBoard works with standard DC wall supplies and have a 5.5mm jack, with a 2.1mm center pole diameter. It accepts a supply voltage between 7 and 15 V.

### USB-C

The pair of USB-C connectors on the RedBoard connect to the ESP32-P4's serial/JTAG interface (labeled Serial/JTAG) and the high-speed OTG USB interface (labeled 400Mbps OTG). The Serial/JTAG USB-C connector's primary use is programming and interfacing with the board over USB. The 400Mbps OTG USB-C connector's primary use is for using the board as a USB host or USB device. As a USB Host, this IoT RedBoard supports connecting USB devices like a USB camera to stream video over USB.

### LiPo Battery & Charging

The 2-pin JST connector lets you power the board with a single-cell lithium ion (LiPo) battery. It also has battery charge and monitoring circuits. 

The MCP73831 charge IC receives power from the RedBoard's 5V rail. It charges an attached battery with a charge current of 455 mA. The MAX17048 fuel gauge tracks the battery voltage and state of charge (SoC) over I<sup>2</sup>C. The fuel gauge includes an alert pin that connects to IO46 on the ESP32-P4. 

### Power Over Ethernet (PoE Version Only)

The PoE version can power the RedBoard over an Ethernet connection through the MagJack connector and a 5V/1.8A buck regulator.

### Serial Basic Header (WiFi/BLE Version Only)

**Advanced Users Only**

The WiFi/BLE version routes the ESP32-C5's serial interface with the appropriate power and boot pins to a 0.1"-spaced plated through-hole (PTH) header that matches the pinout of a [Serial Basic](https://www.sparkfun.com/sparkfun-serial-basic-breakout-ch340c-and-usb-c.html). Most users will never need to use this header as it's only for overwriting the pre-programmed radio firmware on the ESP32-C5.

## Pinout

These boards match the standard RedBoard pinout with six analog pins, sixteen GPIO pins and various power/ground pins for powering peripherals. Included in this pinout is one UART (UART0), SPI and I<sup>2</sup>C interface all mapped to work with compatible RedBoard/R3 shields. The pin labels on the back of the IoT RedBoard indicate the IO pin it connects to on the ESP32-P4 along with any alternate functionalities.

## Memory

### Flash

This IoT RedBoard includes 16MB of external Flash memory.

### MicroSD Card Slot

The &micro;SD card slot on the board lets you expand the board's memory with an external SD card.

## Peripheral Connectors

### Camera MIPI Connector

The board routes out the P4's MIPI camera pins to this vertical ribbon cable connector to easily add a compatible MIDI camera. The P4.

### Display MIPI Connector

This vertical ribbon cable connector is tied to the P4's MIPI display pins to connect to a compatible MIPI display. 

### Qwiic Connector

The Qwiic connect makes it easy to connect Qwiic devices to the RedBoard to communicate over I<sup>2</sup>C. This Qwiic connector routes out the SDA (IO26) and SCL (IO27) pins along with power (3.3V) and ground.

## Solder Jumpers

The following list outlines the solder jumpers shared by both versions of the IoT RedBoard ESP32-P4. It covers their labels, function, default state and any notes on their use.

* <b>SHLD1</b> - The SHLD1 jumper connects the Serial/JTAG USB-C connector's shield pin to the board's ground and is CLOSED by default. Open this jumper to isolate the shield pin from the board.
* <b>SHLD2</b> - The SHLD2 jumper connects the High Speed/OTG USB-C connector's shield pin to the board's ground and is CLOSED by default. Open this jumper to isolate the shield pin from the board.
* <b>I2C</b> - The I2C jumper pulls the SDA (IO26) and SCL (IO27) pins to 3.3V_P through a pair of 2.2k&ohm; resistors. This three-way jumper is CLOSED by default. Open the jumper completely to disable pullups on these pins.
* <b>CHG</b> - The CHG jumper completes the charge status LED circuit and is CLOSED by default. Open the jumper to disable the charge LED.
* <b>PWR</b> - The PWR jumper completes the power LED circuit and is CLOSED by default. Open the jumper to disable the power indicator LED.
* <b>LED_DO</b> - The LED_DO jumper connects the WS2812 LED's data output signal to the NC pin on the RedBoard header. It is CLOSED by default. Open it to disable the data output on the NC pin.

### PoE Jumpers

The PoE version of the IoT RedBoard includes three extra solder jumpers labeled <b>POE</b>, <b>GRN</b> and <b>YLW</b>. The list below outlines their functionality, default state and any notes on their use:

* <b>POE</b> - The POE jumper creates a dummy load on the buck regulator to maintain the minimum PoE load and is CLOSED by default. Open this jumper to disable the dummy load.
* <b>GRN</b> - The GRN jumper completes the circuit for the green LED on the Ethernet jack and is CLOSED by default. Open the jumper to disable the green LED on the jack.
* <b>YLW</b> - The YLW jumper completes the circuit for the yellow LED on the Ethernet jack and is CLOSED by default. Open the jumper to disable the yellow LED on the jack.

## LEDs

There are three LEDs on this RedBoard labeled <b>CHG</b>, <b>PWR</b> and <b>RGB IO12</b>. The yellow charge (CHG) LED illuminates while the board charges an attached LiPo battery. The red power (PWR) LED indicates when the board has power. The RGB LED is a WS2812 RGB LED and its data input signal connects to IO12 on the P4. Users can write to the IO12 pin to change the color of the LED.

## Buttons

Both versions of the IoT RedBoard - ESP32-P4 include a pair of buttons labeled <b>RESET</b> and <b>BOOT</b>. These buttons connect to the reset and boot pins on the ESP32-P4 to reset the module and force it into bootloader mode.

### WiFi/BLE Version

The WiFi/BLE version includes a second set of buttons connected to the ESP32-C5 module labeled <b>RESET</b> and <b>BOOT</b>. These connect to their respective pins on the C5 to reset the module and put it into bootloader mode.

## Board Dimensions