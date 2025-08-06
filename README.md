# Lilygo TTGO T5 2.13" ESP32 ePaper weather station



This directory is a fork of David Bird’s ESP32-e-Paper Weather Display project  (© 2014 and beyond): https://github.com/G6EJD/ESP32-e-Paper-Weather-Display

## Features

- Updated display layout for improved readability.
- Cycling through "current day", "next day" and "4-day" forecast wiev on button press.
- Improved icons (night icons are displayed with a moon symbol instead of a sun).
- WiFi server for user friendly setup.
- Optimized for LilyGo 2.13" 250x122 e-Paper display.

## Housing for 3D print
The 3D design files for the housing are included in this repository. This design is based on [Sir.Puchtuning](https://makerworld.com/en/models/647684-lilygo-t5-2-13-small-case?from=search#profileId-1024510)'s project, with added feet and reinforced, optimised structure for 3D printing. There is space to fit a 1000mAh 3.7V battery as well.

## 📷 Images

![alt_text, width="200"](./LilyGo_213_weather_01.jpg)
![alt_text, width="200"](./LilyGo_213_weather_02.jpg)
![alt_text, width="200"](./LilyGo_213_weather_03.jpg)
![alt_text, width="200"](./LilyGo_213_weather_04.jpg)

## Setup server
The web interface enables setting up credentials, time zone, api key, etc. without reprogramming the controller. The values are stored in the EEPROM permanently (unless full erase using: http://192.168.4.1/erase_eeprom). 

Usage: For entering the Weather station setup page keep the "next" button pressed while switching the power ON. This will start a WiFi server called "weather_station_wifi". Connect to this and open http://192.168.4.1/

![Setup page, width="200"](./LilyGo_213_weather_station_setup.jpg)

## Hardware
1. ESP32 e-ink display module: https://lilygo.cc/en-pl/products/t5-2-13inch-e-paper
2. Optional: LiPo or LiIon battery with Micro JST 1.25 connector (3.7V, 100-1000mAh)

## Notes
- The default screen refresh period is set to 30 min. The controller is in deep sleep in between. This enables battery life for many months (possibly a year) on an 1000mAh 3.7V battery. 

- Recharge the battery via the micro USB plug.

- This port was built using PlatformIO in VScode.
