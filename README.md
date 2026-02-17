#     ESP32_weather-chart

Code running on ESP32-WROOM-32E (dual core, wifi capable).

The hardware consist of:
 - one PCB to control daisy-chained WS2818 LEDs
 - one plexiglass/acrylic board with a laminated FAA VFR map which one has create holes at desired airport and added 5mm RGB LED in each (WS2818), all daisy chained to one another.
 - Enclosure that can be 3D printed and glued on the back of the plexiglass/acrylic map.

The PCB has these hardware:
- one input for 5V wall-wart barrel connector
- 3 wire output for the WS2818 LED daisy chain (GND-5V and one control wire)
- one thumbwheel used for brightness control
- one optional switch for user option
- one on-board RGB LED for status

  The code performs these actions:
  - Fetch the airport data every minute and identify the flight category for the airport (VFR, MVFR, IFR, LIFR)
  - send color signal to the specific LED of the airport and lights up with the appropriate color
  - No light if no flight category is retrieved
  - Wheel: adjust the brightness of the LEDs
  - act as a Access Portal for the initial Wifi setup (password, etc)
  - act as a web server to allow the user to change settings via a webpage, such as the list of airport and their order/id in the daisy-chain
  - Let the user to set a time for the device to reset daily, to keep the device from freezing/crashing
  - Avoid the use of String to limit memory fragmentation
  - Runs the wifi on one core, the other task on the other core, and have regular check from one core to the other to see if one is not responding, and reset if a crash is imminent
 
PCB files are under 'board' folder
3D printable enclosure can be downloaded via https://www.printables.com/model/1605885-esp32_weather-chart-enclosure
 
