# Software Setup

## MicroPython Firmware

The IoT RedBoard - ESP32-P4 comes flashed with MicroPython firmware along with a simple test script that cycles the RGB LED. If you overwrite the pre-installed firmware or need to update it, you can find the firmware files and installation instructions on the [MicroPython downloads page](https://micropython.org/download/?vendor=SparkFun).

## Supported Platforms

### Python/MicroPython

With the pre-installed firmware you can jump right into running Python scripts on the IoT RedBoard. If you've never used Python or MicroPython before, you can learn more about them here: [Python.org](https://www.python.org/) & [MicroPython.org](https://micropython.org/).

### Espressif ESP-IDF

This RedBoard also works great with the espressif ESP-IDF and they have some examples included to really show off the power of the ESP32-P4. To get started with the IDF, take a look at their [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/index.html). SparkFun has some demo examples that use the IoT RedBoard - ESP32-P4 with MIPI displays and cameras hosted on this [GitHub repository](https://github.com/sparkfun/SparkFun_RedBoardIoT-ESP32-P4_Examples).

### Arduino 

The latest main release of the espressif ESP32 core supports the Ethernet version but does *not* currently support the WiFi/BLE version. In order to use the WiFi/BLE version with Arduino you'll need to perform the following steps to install SparkFun's patch for the core:

* Install esp32 core v3.3.12.
* Download the SparkFun patch from the IoT RedBoard - ESP32-P4 GitHub repository [here](https://github.com/sparkfun/SparkFun_IoT_RedBoard-ESP32-P4/releases/tag/v1.0.0)
* Locate the file "arduino-c5-patch"
* Install the patch file:
    * Linux/macOS: `./install.sh`
    * Windows Powershell: `powershell -ExecutionPolicy Bypass -File .\install.ps1`
* After installation, restart Arduino (if necessary) and in the "Boards" menu you'll find both versions of the IoT RedBoard - ESP32-P4 at the very bottom of the menu.

The patch downloaded from the repository includes hardware test examples for both versions of the IoT RedBoard. We cover those in some more detail in the following examples section.
