# ESP8266 Spotify OLED
For use with an SSD1306 128x64, designed in mind for blue-yellow displays.
<br>
<img src="https://github.com/Gabe-H/ESP8266-Spotify-OLED/blob/master/images/image_3.JPG?raw=true" alt="Demo pic" width="500"></img>

## Requirements
- VS Code with PlatformIO installed
- ESP8266 (D1 mini configured)
- 128x64 SSD1306
- (OPTIONAL) <a href="https://www.thingiverse.com/thing:3080488">3D printed case</a>

## Uploading
### Precompiled
Download the latest <a href="https://github.com/gabe-h/esp8266-spotify-oled/releases/latest">Release</a> 
binary for D1 Mini, and use <a href="https://github.com/espressif/esptool">esptool.py</a> to upload.
<br>
Then go to __Device Setup__
### Compile yourself
You will need to supply your own Spotify client credentials, created from
<a href="https://developers.spotify.com/dashboard">the Spotify Application Dashboard </a>.
<br />
Click create an app, and give it a name, description, and check the boxes. Once created click
"Edit Settings" and add a redirect uri of
```
http://ardspot.local/
```
You can put the Client ID and Client Secret in the code in 2 ways:
1. Add them to `main.cpp` in the `#define`'s and uncomment
2. If you want to keep your credentials git-ignored, you may create file `src/cred.h` like this:
```cpp
#define CLIENT_ID "your_client_id"
#define CLIENT_SECRET "your_client_secret"
```
Then upload to the ESP8266 using PlatformIO, or compile and use `esptool.py`

## Device Setup
When you boot the first time, connect to "Ard Connect" wifi. 
If the captive page doesn't load automatically, go to `http://ardspot.local/` 
and sign into your wifi network.
<br /> <br />
The device will reboot, and you can continue to sign into Spotify by first connecting to the same network as the device, 
then again going to `http://ardspot.local/` and clicking sign in.

## The Device
As of this version, the device only has the ESP8266 board, and the OLED. Future versions may have support for playback control buttons and more features. Only point to bring up here is that besides displaying the current playback of your Spotify account, the screen will show a small dot in the top right if it has lost connection. It is nothing to restart the device over, but just a reminder that the device will not have quick updates if the dot is visible.

### Home page
The Home page (http://ardspot.local/) has a some buttons:
- A Spotify sign in button with a checkbox next to it. In most cases you can simply press the button and sign in, but if you press the check mark it allows "show dialog" with the api authorization, so you can switch accounts if needed, and see the requested scopes
- A Wi-Fi reset button. This will reset only the WiFi SSID and password, and upon reboot will start access point (AP) mode
- A Sign Out button. This will remove the saved Spotify refresh token, and reboot giving the message to sign in by going back to the home page.
- A Factory Reset button. This will wipe the EEPROM and comepletely restart the device.

### AP Mode
Will start a WiFi network called "Ard Connect". Connect to that network and use screen to set up.

### Connections
<img src="https://www.electronics-lab.com/wp-content/uploads/2020/01/OLED-Schematics.jpeg" alt="wiring diagram" />
Wiring diagram by <a href="https://bitsnblobs.com/">Bits and Bobs</a>
