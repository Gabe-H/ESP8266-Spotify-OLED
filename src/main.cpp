#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
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

/* 
  SET THE BELOW VARIABLES TO YOUR OWN SPOTIFY CLIENT CREDENTIALS
*/
//#define CLIENT_ID ""
//#define CLIENT_SECRET ""

// Country code, including this is advisable
#define SPOTIFY_MARKET "US"
#define BUTTON 2
//------- ---------------------- ------

// Used to hide client variables on Github
#ifndef CLIENT_ID
  #include <creds.h>
#endif

unsigned long delayBetweenRequests = 2000; // Time between requests
unsigned long requestDueTime = 2000;       // Time when request due

const IPAddress apIP(192, 168, 1, 1);
const char apSSID[] = "Ard Connect";
int oldButtonState = HIGH;
boolean settingMode;
boolean tokenReady = false;
String ssidList;
String localIP;

uint16_t nothingPlayingCount = 0;
boolean isPlaying = false;

String refresh_token = "";

DNSServer dnsServer;
ESP8266WebServer webServer(80);
WiFiClientSecure client;
ArduinoSpotify spotify(client, CLIENT_ID, CLIENT_SECRET);
SSD1306  display(0x3C, D2, D1);

String authUrl() {
  String s = "https://accounts.spotify.com/authorize?client_id=";
  s += CLIENT_ID;
  s += "&response_type=code&redirect_uri=";
  s += "http://ardspot.local/callback";
  s += "&scope=user-modify-playback-state";

  return s;
}

String makePage(String title, String contents) {
  String s = F("<!DOCTYPE html><html><head>");
  s += F("<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\"><title>");
  s += title;
  s += F("</title></head><body>");
  s += contents;
  s += F("</body></html>");
  return s;
}

String ip2Str(IPAddress ip) {
    String s="";
    for (int i=0; i<4; i++) {
        s += i  ? "." + String(ip[i]) : String(ip[i]);
    }
    return s;
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

  for (int i=0; i<32; i++) {
    EEPROM.write(i, ssid[i]);
  }
  for (int i=0; i<64; i++) {
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
  while ( count < 20 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println(F("Connected!"));
      localIP = ip2Str(WiFi.localIP());
      return true;
    }
    delay(500);
    Serial.print(F("."));
    count++;
  }
  Serial.println(F("Timed out."));
  displayStatus(F("WIFI CONFIG"), F("Connection failed"), F("Reconnect and"), F("try again"));
  writeWifi("", "");
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

    displayStatus(F("CONNECTING TO "), ssid);
    WiFi.begin(ssid.c_str(), pass.c_str());
    if (checkConnection()) {
      webServer.send(200, F("text/html"), makePage(F("Wi-Fi Settings"), s));
      delay(1000);
      WiFi.softAPdisconnect();
      delay(100);
      ESP.restart();
    } else {
      home();
    }

  });

  webServer.on("/", home);

  webServer.onNotFound(home);

  webServer.begin();

  // displayStatus(F("WIFI CONFIG"), F("Connect to:"), apSSID, F("http://ardspot.local/"));
  settingMode = true;
}

void startWifi() {
  webServer.on("/", []() {
    String s = F("<h1>Arduino Spotify</h1>");
    s += F("<form action=\"https://accounts.spotify.com/authorize\"><input type=\"hidden\" name=\"client_id\" value=\"");
    s += CLIENT_ID;
    s += ("\" /><input type=\"hidden\" name=\"response_type\" value=\"code\" /><input type=\"hidden\" name=\"redirect_uri\" value=\"http://ardspot.local/callback\" /><input type=\"hidden\" name=\"scope\" value=\"user-modify-playback-state\" /><input type=\"checkbox\" name=\"show_dialog\" value=\"true\" />Manual <input type=\"submit\" value=\"Sign In\" /></form>");
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
    displayStatus(F("FACTORY RESET"), F("Restarting..."));
    delay(1000);
    ESP.restart();
  });

  webServer.on(F("/wifireset"), []() {
    for (int i=0; i<96; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();

    String s = F("<h1>Wifi settings have been reset</h1>");
    webServer.send(200, F("text/html"), makePage(F("Wifi Settings Reset"), s));
    displayStatus(F("WIFI RESET"), F("Restarting..."));
    delay(1000);
    ESP.restart();
  });

  webServer.on(F("/tokenreset"), []() {
    for (int i = 0; i < 131; ++i) {
      EEPROM.write(i + 96, 0);
    }
    EEPROM.commit();

    String s = F("<h1>Spotify token has been reset");
    webServer.send(200, F("text/html"), makePage(F("Token Reset"), s));

    ESP.restart();
  });

  webServer.on(F("/tokenset"), []() {
    String refreshToken = webServer.arg("refresh_token");
    Serial.print(F("Got token: "));
    Serial.println(refreshToken);
    tokenReady = false;

    webServer.send(200, F("text/html"), makePage(F("Set Refresh Token"), F("<h1>Refresh token was set</h1>")));
    for (int i = 0; i < 131; ++i) {
      EEPROM.write(i + 96, refreshToken[i]);
    }
    EEPROM.commit();
    displayStatus(F("SAVING..."));
    delay(2500);
    
    refresh_token = refreshToken;
    displayStatus(F("RESTARTING..."));
    delay(200);
    ESP.restart();
  });

  webServer.on(F("/callback"), []() {
    String authCode = webServer.arg(F("code"));
    Serial.print(F("Got code: "));
    Serial.println(authCode);
    displayStatus(F("GOT CODE"));
    String refreshToken = spotify.requestAccessTokens(authCode.c_str(), "http://ardspot.local/callback");
    
    for (int i = 0; i < 131; ++i) {
      EEPROM.write(i + 96, refreshToken[i]);
    }
    EEPROM.commit();

    webServer.send(200, F("text/html"), makePage(F("Spotify Authentication"), F("<h1>You may close this window</h1>")));
    displayStatus(F("SAVING..."));
    delay(2500);
    
    refresh_token = refreshToken;
    displayStatus(F("RESTARTING..."));
    delay(200);
    ESP.restart();
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
        isPlaying = currentlyPlaying.isPlaying;
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

  pinMode(BUTTON, INPUT_PULLUP);

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
          displayStatus(("READY"), F("Starting..."));
          Serial.println(F("Tokens refreshed!"));
          tokenReady = true;
        } else {
          displayStatus(F("TOKEN ERROR"), F("Go to:"), F("http://ardspot.local/  or"), localIP+"/");
          Serial.println(F("Failed to refresh tokens"));
          Serial.print(F("Connect to "));
          Serial.println(WiFi.localIP());
          tokenReady = false;
        }
      } else {
        displayStatus(F("NO TOKEN SET"), F("On ")+WiFi.SSID()+F(", go to"), F("http://ardspot.local/  or"), F("http://")+localIP+F("/"));
        tokenReady = false;
      }
      
    } else {
      displayStatus(F("FAILED TO CONNECT TO WIFI"));
      WiFi.disconnect();
      delay(1300);
      startAP();
    }
  } else {
    startAP();
  }
  
  if (!MDNS.begin(F("ardspot"))) {
    Serial.println(F("Error setting up MDNS responder!"));
  } else {
    Serial.println(F("mDNS responder started"));
    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
  }
}

void loop() {
  int buttonState = digitalRead(BUTTON);
  MDNS.update();
  webServer.handleClient();
  if (settingMode) {
    dnsServer.processNextRequest();
    if (buttonState == LOW) {
      displayStatus(F("SETUP INFO"), F("SSID: ")+String(apSSID), F("GW: ")+ip2Str(apIP), F("http://ardspot.local/"));
    } else {
      displayStatus(F("WIFI CONFIG"), F("Connect to:"), apSSID, F("http://ardspot.local/"));
  }
  } else if (tokenReady) {
    if (buttonState == LOW) {
      displayStatus("NETWORK INFO", "http://ardspot.local/", F("http://")+localIP+F("/"));
    } else {
      if (millis() > requestDueTime) {
        requestDueTime = millis() + delayBetweenRequests;
        Serial.print(F("Free heap: "));
        Serial.println(ESP.getFreeHeap());

        displayCurrentlyPlaying(spotify.getCurrentlyPlaying(SPOTIFY_MARKET));
      }
    }
  }
}
