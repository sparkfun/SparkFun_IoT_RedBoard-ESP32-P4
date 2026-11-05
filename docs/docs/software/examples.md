# IoT RedBoard - ESP32-P4 Examples

We've written a few different test sketches and demos for the IoT RedBoard - ESP32-P4 for the supported code platforms. Let's take a closer look at them.

## Python Example

This simple example works right out of the box with the RedBoard's MicroPython firmware. Open your preferred Pythin interpreter and copy the code below into it. Depending on which version of the IoT RedBoard - ESP32-P4 you have you'll need to select the nework type:

``` python
# Select 'LAN' for Ethernet or 'WLAN' for Wi-Fi
network_if = 'LAN'
# network_if = 'WLAN'
```

If you're using the WiFi/BLE version, you'll need to replace `your-ssid` and `your-password` in the lines below with the SSID and password for your WiFi network:

``` python
wlan_ssid = 'your-ssid'
wlan_password = 'your-password'
```

After updating the code, select the board (MicroPython ESP32) and port and run the script. 

The example includes serial printouts for each step and also cycles the RGB LED from Red while connecting to the network, Blue while downloading the data and Green to indicate success. Once the download finishes, the code prints out the number of astronauts on the ISS along with their names.

``` python
################################################################################
# Configure the example using the settings below before running it!
################################################################################

# Select 'LAN' for Ethernet or 'WLAN' for Wi-Fi
network_if = 'LAN'
# network_if = 'WLAN'

# If you're using WLAN (Wi-Fi), set your SSID and password below
wlan_ssid = 'your-ssid'
wlan_password = 'your-password'

# Select whether to use the serial LCD display. To install with mip, run:
# mpremote mip install github:sparkfun/qwiic_serlcd_py
use_serlcd = False
# use_serlcd = True

################################################################################
# Configure the example using the settings above before running it!
################################################################################

# Import necessary modules
import network
import urequests
import time
import neopixel
import machine

# Set up the NeoPixel LED and LCD (if used)
led = neopixel.NeoPixel(machine.Pin.board.NEOPIXEL, 1)
if use_serlcd:
    import qwiic_serlcd
    myLCD = qwiic_serlcd.QwiicSerlcd()

# Set the LED to red while connecting
led[0] = (255, 0, 0)
led.write()
if use_serlcd:
    myLCD.setBacklight(255, 0, 0)

print("Waiting for connection...")
if use_serlcd:
    myLCD.clearScreen()
    myLCD.print("Waiting for")
    myLCD.setCursor(0,1)
    myLCD.print("connection...")

# Connect to the network
if network_if == 'LAN':
    net = network.LAN(
        phy_addr = 0,
        phy_type=network.PHY_GENERIC,
        mdc=22,
        mdio=21,
        crs_dv=28,
        rxd0=29,
        rxd1=30,
        tx_en=33,
        txd0=34,
        txd1=35,
        clk_out=23,
        clk_in=32,
    )
    net.active(True)
elif network_if == 'WLAN':
    net = network.WLAN(network.STA_IF)
    net.active(True)
    net.connect(wlan_ssid, wlan_password)

# Wait for the connection to complete
while not net.isconnected():
    time.sleep(1)
    print('.', sep='', end='', )

# Print IP address of the device
print('Connected! IP address:', net.ipconfig('addr4')[0])
if use_serlcd:
    myLCD.clearScreen()
    myLCD.print(net.ipconfig('addr4')[0])

# Set the LED to blue while downloading data
led[0] = (0, 0, 255)
led.write()
if use_serlcd:
    myLCD.setBacklight(0, 0, 255)

# Download the list of people currently in space
downloaded = False
while not downloaded:
    try:
        people = urequests.get("https://corquaid.github.io/international-space-station-APIs/JSON/people-in-space.json").json()
        downloaded = True
    except:
        print('Failed to download, retrying...')
        time.sleep(1)

# Set the LED to green to indicate success
led[0] = (0, 255, 0)
led.write()
if use_serlcd:
    myLCD.setBacklight(0, 255, 0)

# Print the number of people and their names
number = people['number']
print('There are', number, 'people in space.')
for i in range(number):
    print(i+1, people['people'][i]['name'])

# Display the number of people in space on the LCD if used
if use_serlcd:
    # Get the button to use for cycling through people in space
    btn = machine.Pin.board.BUTTON
    btn.init(btn.IN)

    # Show the initial number of people in space on the LCD
    myLCD.clearScreen()
    myLCD.print(net.ipconfig('addr4')[0])
    myLCD.setCursor(0,1)
    myLCD.print(str(number) + " ppl in space!")

    # Cycle through the people in space when the button is pressed
    idx = 0
    while True:
        # Wait for the button to be pressed and released
        pressed = False
        while btn() == 0:
            pressed = True

        # Update the display with the next person in space
        if pressed:
            # Increment index, wrapping around if necessary
            idx += 1
            if idx > number:
                idx = 1

            # Display the next person in space on the LCD
            myLCD.clearScreen()
            myLCD.setCursor(0,0)
            myLCD.print("Person #" + str(idx) + "/" + str(number))
            myLCD.setCursor(0,1)
            myLCD.print(people['people'][idx-1]['name'])


```

## Espressif IDF Examples

We have several examples [here](https://github.com/sparkfun/SparkFun_RedBoardIoT-ESP32-P4_Examples) that use the ESP32-P4 to control both a camera and display to answer the age-old question: "Can it run Doom?" along with a phone-style touch display. These examples do require extra hardware and are listed in the examples.

## Arduino Test Examples

The [ESP32-P4 Patch](https://github.com/sparkfun/SparkFun_IoT_RedBoard-ESP32-P4/releases/tag/v1.0.0) includes test examples for both PoE and WiFi. You can find them in the patch folder under **Arduino/tests**. These examples run hardware tests for the on-board components. The example boots up after ~10 seconds and prints out a serial menu with all the available tests. Enter the corresponding character for each test and it runs the test and prints out if it passed or failed.