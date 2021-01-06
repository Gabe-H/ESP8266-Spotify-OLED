#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
// Wifi libraries

#include <SPI.h>
#include <Wire.h>
#include <SSD1306.h>
// SSD1306 OLED libraries

#include <WiFiClientSecure.h>
#include <ArduinoSpotify.h>   // https://github.com/witnessmenow/arduino-spotify-api
#include <ArduinoJson.h>      // https://github.com/bblanchon/ArduinoJson
// Spotify libraries

#define CLIENT_ID "" // Your client ID of your spotify APP
#define CLIENT_SECRET "" // Your client Secret of your spotify APP (Do Not share this!)

// Country code, including this is advisable
#define SPOTIFY_MARKET "US"
//------- ---------------------- ------



unsigned long delayBetweenRequests = 1000; // Time between requests (1 second)
unsigned long requestDueTime;              // Time when request due

const IPAddress apIP(192, 168, 1, 1);
const char apSSID[] = "Ard Connect";
boolean settingMode;
boolean tokenReady = false;
String ssidList;
String localIP;

int nothingPlayingCount = 0;
int countForWifiShutdown = 0;

String refresh_token = "";

DNSServer dnsServer;
ESP8266WebServer webServer(80);
WiFiClientSecure client;
ArduinoSpotify spotify(client, CLIENT_ID, CLIENT_SECRET);
SSD1306  display(0x3C, D2, D1);

String makePage(String title, String contents) {
  String s = F("<!DOCTYPE html><html><head>");
  s += F("<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\"><title>");
  s += title;
  s += F("</title></head><body>");
  s += contents;
  s += F("</body></html>");
  return s;
}

void ip2Str(IPAddress ip) {
    String s="";
    for (int i=0; i<4; i++) {
        s += i  ? "." + String(ip[i]) : String(ip[i]);
    }
    localIP = s;
}

String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}

void displayStatus(String header, String body="", String body2="", String body3="") {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, header);
  display.drawString(64, 16, body);
  display.drawString(64, 32, body2);
  display.drawString(64, 48, body3);
  display.display();
}

void writeWifi(String ssid = "", String password = "") {
  for (int i=0; i<96; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  for (int i=0; i<ssid.length(); i++) {
    EEPROM.write(i, ssid[i]);
  }
  for (int i=0; i<password.length(); i++) {
    EEPROM.write(i+32, password[i]);
  }
  EEPROM.commit();
}

boolean restoreConfig() {
  Serial.println(F("Reading EEPROM..."));
  String ssid = "";
  String pass = "";
  String eeToken = "";
  if (EEPROM.read(0) != 0) {
    for (int i = 0; i < 32; ++i) {
      ssid += char(EEPROM.read(i));
    }
    Serial.print(F("SSID: "));
    Serial.println(ssid);
    for (int i = 32; i < 96; ++i) {
      pass += char(EEPROM.read(i));
    }
    Serial.print(F("Password: "));
    Serial.println(pass);
    for (int i = 96; i < 227; ++i) {
      eeToken += char(EEPROM.read(i));
    }
    Serial.print(F("Refresh Token: "));
    Serial.println(eeToken);

    if (eeToken[0] == 0) {
      refresh_token = "";
    } else {
      refresh_token = eeToken;
    }
    displayStatus(F("CONNECTING TO "), ssid);
    WiFi.begin(ssid.c_str(), pass.c_str());
    return true;
  }
  else {
    Serial.println(F("Config not found."));
    return false;
  }
}

boolean checkConnection() {
  int count = 0;
  Serial.print(F("Waiting for Wi-Fi connection"));
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println(F("Connected!"));
      return true;
    }
    delay(500);
    Serial.print(F("."));
    count++;
  }
  Serial.println(F("Timed out."));
  return false;
}

void home() {
  String s = F("<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>");
  webServer.send(200, F("text/html"), makePage(F("AP mode"), s));
}

void startAP() {
  Serial.print(F("Starting Web Server at "));
  Serial.println(apIP);

  displayStatus(F("STARTING AP"));

  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  delay(100);

  Serial.println("");
  for (int i = 0; i < n; ++i) {
    ssidList += F("<option value=\"");
    ssidList += WiFi.SSID(i);
    ssidList += F("\">");
    ssidList += WiFi.SSID(i);
    ssidList += F("</option>");
  }
  delay(100);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID);
  dnsServer.start(53, "*", apIP);

  webServer.on("/settings", []() {
    String s = F("<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>");
    s += F("<form method=\"get\" action=\"wifiset\"><label>SSID: </label><select name=\"ssid\">");
    s += ssidList;
    s += F("</select><br>Password: <input name=\"pass\" length=64 type=\"password\"><input type=\"submit\"></form>");
    webServer.send(200, F("text/html"), makePage(F("Wi-Fi Settings"), s));
  });

  webServer.on("/wifiset", []() {
    for (int i = 0; i < 96; ++i) {
      EEPROM.write(i, 0);
    }
    String ssid = urlDecode(webServer.arg("ssid"));
    Serial.print(F("SSID: "));
    Serial.println(ssid);
    String pass = urlDecode(webServer.arg("pass"));
    Serial.print(F("Password: "));
    Serial.println(pass);

    writeWifi(ssid, pass);

    Serial.println(F("Wifi EEPROM write done!"));
    String s = F("<h1>Setup complete.</h1><p>device will be connected to \"");
    s += ssid;
    s += F("\" after the restart.");
    webServer.send(200, F("text/html"), makePage(F("Wi-Fi Settings"), s));

    WiFi.softAPdisconnect();
    delay(100);
    ESP.restart();
  });

  webServer.on("/", home);

  webServer.onNotFound(home);

  webServer.begin();

  displayStatus(F("WIFI CONFIG"), F("Connect to:"), apSSID);
  settingMode = true;
}

void startWifi() {
  webServer.on("/", []() {
    String s = F("<h1>Arduino Spotify</h1><p><form action=\"tokenset\"><input type=\"text\" name=\"refresh_token\" /><input type=\"submit\" value=\"Set Refresh Token\"/></form>");
    s += F("</p><p><a href=\"/wifireset\">Reset Wi-Fi Settings</a><br/><a href=\"/tokenreset\">Reset Spotify Token</a><br/><a href=\"/factoryreset\">Factory Reset<a></p>");
    webServer.send(200, F("text/html"), makePage(F("STA mode"), s));
  });

  webServer.on(F("/factoryreset"), []() {
    for (int i = 0; i < 227; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    String s = F("<h1>All settings have been reset.</h1>");
    webServer.send(200, F("text/html"), makePage(F("Factory Reset"), s));

    ESP.restart();
  });

  webServer.on("/wifireset", []() {
    for (int i=0; i<96; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();

    String s = "<h1>Wifi settings have been reset</h1>";
    webServer.send(200, F("text/html"), makePage(F("Wifi Settings Reset"), s));

    ESP.restart();
  });

  webServer.on("/tokenreset", []() {
    for (int i = 0; i < 131; ++i) {
      EEPROM.write(i + 96, 0);
    }
    EEPROM.commit();

    String s = F("<h1>Spotify token has been reset");
    webServer.send(200, F("text/html"), makePage(F("Token Reset"), s));

    ESP.restart();
  });

  webServer.on("/tokenset", []() {
    String refreshToken = webServer.arg("refresh_token");
    Serial.print("Got token: ");
    Serial.println(refreshToken);
    for (int i = 0; i < 131; ++i) {
      EEPROM.write(i + 96, refreshToken[i]);
    }
    EEPROM.commit();
    webServer.send(200, "text/html", makePage(F("Set Refresh Token"), F("<h1>Refresh token was set</h1>")));
    tokenReady = false;
    
    refresh_token = refreshToken;
    spotify.setRefreshToken(refresh_token.c_str());

    displayStatus("CONNECTING SPOTIFY");
    if (spotify.refreshAccessToken()) {
      displayStatus(F("READY"), "", F("Starting..."));
      tokenReady = true;
    } else {
      displayStatus(F("TOKEN ERROR"), F("Go to:"), localIP);
      tokenReady = false;
    }
  });

  webServer.begin();
  settingMode = false;
}

void displayCurrentlyPlaying(CurrentlyPlaying currentlyPlaying)
{
  if (!currentlyPlaying.error) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    switch (currentlyPlaying.statusCode) {
      case 200: {
        nothingPlayingCount = 0;
        display.drawString(0, 0, currentlyPlaying.trackName);
        display.drawString(0, 16, currentlyPlaying.firstArtistName);
        display.drawString(0, 32, currentlyPlaying.albumName);

        float precentage = ((float) currentlyPlaying.progressMs / (float) currentlyPlaying.duraitonMs) * 100;
        display.drawProgressBar(1, 50, 126, 10, (int)precentage);
      }
        break;

      case 204:
        if (nothingPlayingCount < 20) {
          nothingPlayingCount++;
          display.drawString(0, 0, F("Nothing playing"));
        }
        break;
      
      default:
        display.drawString(0, 0, F("ERROR"));
        display.drawString(0, 16, F("Please reboot"));
        break;
    }
    display.display();
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  display.init();
  display.flipScreenVertically();
  // display.setFont(Open_Sans_Hebrew_Condensed_Light_12);
  display.setFont(Roboto_Condensed_13);

  spotify.currentlyPlayingBufferSize = 5000;
  spotify.playerDetailsBufferSize = 5000;
  
  if (restoreConfig()) {
    if (checkConnection()) {
      startWifi();
      client.setFingerprint(SPOTIFY_FINGERPRINT);
      
      if (refresh_token != "") {
        spotify.setRefreshToken(refresh_token.c_str());
        displayStatus(F("AUTHENTICATING"));
        Serial.println(F("Refreshing saved tokens.."));

        if (spotify.refreshAccessToken()) {
          displayStatus("READY", "", "Starting...");
          Serial.println(F("Tokens refreshed!"));
          tokenReady = true;
        } else {
          displayStatus("TOKEN ERROR", "Go to:", localIP);
          Serial.println(F("Failed to refresh tokens"));
          Serial.print(F("Connect to "));
          Serial.println();
          tokenReady = false;
        }
      } else {
        displayStatus(F("NO TOKEN SET"), F("Go to:"), localIP);
        tokenReady = false;
      }
      
    } else {
      displayStatus(F("FAILED TO CONNECT TO WIFI"));
      startAP();
    }
  } else {
    startAP();
  }
}

void loop() {
  if (settingMode) {
    dnsServer.processNextRequest();
  } else if (tokenReady) {
    if (millis() > requestDueTime) {
      Serial.print(F("Free heap: "));
      Serial.println(ESP.getFreeHeap());

      displayCurrentlyPlaying(spotify.getCurrentlyPlaying(SPOTIFY_MARKET));
    }
  }
  webServer.handleClient();
}
