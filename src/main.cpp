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
#include <ArduinoSpotify.h> // https://github.com/witnessmenow/arduino-spotify-api
#include <ArduinoJson.h>    // https://github.com/bblanchon/ArduinoJson
// Spotify libraries

#include <TimeLib.h>
#include <WiFiUdp.h>
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = -8; // Pacific Standard Time (USA)
WiFiUDP Udp;
unsigned int localPort = 8888;

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
time_t prevDisplay = 0; // when the digital clock was displayed
char currentTimesWithZeros[12];
// clock

/* 
  SET THE BELOW VARIABLES TO YOUR OWN SPOTIFY CLIENT CREDENTIALS
*/
#define CLIENT_ID "a9ce5b430aca4e87ae43c8e4c243013e"
#define CLIENT_SECRET "67abed740a064cc88985d66cd3a7b7fa"

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
uint8_t oldButtonState = HIGH;
boolean settingMode;
boolean tokenReady = false;
String ssidList;
String localIP;

uint16_t nothingPlayingCount = 0;
uint16_t failedCount = 0;
boolean isPlaying = false;

String refresh_token = "";

DNSServer dnsServer;
ESP8266WebServer webServer(80);
WiFiClientSecure client;
ArduinoSpotify spotify(client, CLIENT_ID, CLIENT_SECRET);
SSD1306 display(0x3C, D2, D1);

String authUrl()
{
  String s = "https://accounts.spotify.com/authorize?client_id=";
  s += CLIENT_ID;
  s += "&response_type=code&redirect_uri=";
  s += "http://ardspot.local/callback";
  s += "&scope=user-modify-playback-state";

  return s;
}

String makePage(String title, String contents)
{
  String s = F("<!DOCTYPE html><html><head>");
  s += F("<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\"><title>");
  s += title;
  s += F("</title></head><body>");
  s += contents;
  s += F("</body></html>");
  return s;
}

String ip2Str(IPAddress ip)
{
  String s = "";
  for (int i = 0; i < 4; i++)
  {
    s += i ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

String urlDecode(String input)
{
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

void displayStatus(String header, String body = "", String body2 = "", String body3 = "")
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, header);
  display.drawString(64, 16, body);
  display.drawString(64, 32, body2);
  display.drawString(64, 48, body3);
  display.display();
}

void writeWifi(String ssid = "", String password = "")
{
  for (int i = 0; i < 96; i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  for (int i = 0; i < 32; i++)
  {
    EEPROM.write(i, ssid[i]);
  }
  for (int i = 0; i < 64; i++)
  {
    EEPROM.write(i + 32, password[i]);
  }
  EEPROM.commit();
}

boolean restoreConfig()
{
  Serial.println(F("Reading EEPROM..."));
  String ssid = "";
  String pass = "";
  String eeToken = "";
  if (EEPROM.read(0) != 0)
  {
    for (int i = 0; i < 32; ++i)
    {
      ssid += char(EEPROM.read(i));
    }
    for (int i = 32; i < 96; ++i)
    {
      pass += char(EEPROM.read(i));
    }
    for (int i = 96; i < 227; ++i)
    {
      eeToken += char(EEPROM.read(i));
    }
    Serial.print(F("Refresh Token: "));
    Serial.println(eeToken);

    if (eeToken[0] == 0)
    {
      refresh_token = "";
    }
    else
    {
      refresh_token = eeToken;
    }
    displayStatus(F("CONNECTING TO "), ssid);
    WiFi.begin(ssid.c_str(), pass.c_str());
    return true;
  }
  else
  {
    Serial.println(F("Config not found."));
    return false;
  }
}

boolean checkConnection()
{
  int count = 0;
  Serial.print(F("Waiting for Wi-Fi connection"));
  while (count < 20)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println();
      Serial.println(F("Connected!"));
      localIP = ip2Str(WiFi.localIP());
      Udp.begin(localPort);
      setSyncProvider(getNtpTime);
      setSyncInterval(300);
      return true;
    }
    delay(500);
    Serial.print(F("."));
    count++;
  }
  Serial.println(F("Timed out."));
  writeWifi("", "");
  return false;
}

void home()
{
  String s = F("<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>");
  webServer.send(200, F("text/html"), makePage(F("AP mode"), s));
}

void startAP()
{
  Serial.print(F("Starting Web Server at "));
  Serial.println(apIP);

  displayStatus(F("STARTING AP"));

  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  delay(100);

  Serial.println("");
  for (int i = 0; i < n; ++i)
  {
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
    for (int i = 0; i < 96; ++i)
    {
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
    if (checkConnection())
    {
      webServer.send(200, F("text/html"), makePage(F("Wi-Fi Settings"), s));
      delay(1000);
      WiFi.softAPdisconnect();
      delay(100);
      ESP.restart();
    }
    else
    {
      home();
    }
  });

  webServer.on("/", home);

  webServer.onNotFound(home);

  webServer.begin();

  // displayStatus(F("WIFI CONFIG"), F("Connect to:"), apSSID, F("http://ardspot.local/"));
  settingMode = true;
}

void startWifi()
{
  webServer.on("/", []() {
    String s = F("<h1>Arduino Spotify</h1>");
    s += F("<form action=\"https://accounts.spotify.com/authorize\"><input type=\"hidden\" name=\"client_id\" value=\"");
    s += CLIENT_ID;
    s += ("\" /><input type=\"hidden\" name=\"response_type\" value=\"code\" /><input type=\"hidden\" name=\"redirect_uri\" value=\"http://ardspot.local/callback\" /><input type=\"hidden\" name=\"scope\" value=\"user-modify-playback-state\" /><input type=\"checkbox\" name=\"show_dialog\" value=\"true\" />Manual <input type=\"submit\" value=\"Sign In\" /></form>");
    s += F("</p><p><a href=\"/wifireset\">Reset Wi-Fi Settings</a><br/><a href=\"/tokenreset\">Sign Out</a><br/><a href=\"/factoryreset\">Factory Reset<a></p>");
    webServer.send(200, F("text/html"), makePage(F("Ard Home"), s));
  });

  webServer.on(F("/factoryreset"), []() {
    for (int i = 0; i < 227; i++)
    {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();

    webServer.send(200, F("text/html"), makePage(F("Factory Reset"), F("<h1>All settings have been reset</h1><br><h2>Please close this window</h2>")));
    displayStatus(F("FACTORY RESET"), F("Restarting..."));
    delay(1500);
    ESP.restart();
  });

  webServer.on(F("/wifireset"), []() {
    for (int i = 0; i < 96; i++)
    {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();

    webServer.send(200, F("text/html"), makePage(F("Wi-Fi Reset"), F("<h1>Wifi settings have been reset</h1><br><h2>Please close this window</h2>")));
    displayStatus(F("WIFI RESET"), F("Restarting..."));
    delay(1500);
    ESP.restart();
  });

  webServer.on(F("/tokenreset"), []() {
    for (int i = 0; i < 131; ++i)
    {
      EEPROM.write(i + 96, 0);
    }
    EEPROM.commit();

    webServer.send(200, F("text/html"), makePage(F("Signed Out"), F("<h1>You have been signed out</h1><br><h2>Please close this window</h2>")));
    displayStatus(F("SIGNED OUT"), F("Restarting..."));

    ESP.restart();
  });

  webServer.on(F("/tokenset"), []() {
    String refreshToken = webServer.arg("refresh_token");
    Serial.print(F("Got token: "));
    Serial.println(refreshToken);
    tokenReady = false;

    webServer.send(200, F("text/html"), makePage(F("Set Refresh Token"), F("<h1>Refresh token was set</h1>")));
    for (int i = 0; i < 131; ++i)
    {
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
    displayStatus(F("VERIFYING..."));
    String refreshToken = spotify.requestAccessTokens(authCode.c_str(), "http://ardspot.local/callback");

    for (int i = 0; i < 131; ++i)
    {
      EEPROM.write(i + 96, refreshToken[i]);
    }
    EEPROM.commit();

    webServer.send(200, F("text/html"), makePage(F("Spotify Authentication"), F("<h1>Please close this window</h1>")));

    displayStatus(F("SAVED!"), F("Restarting..."));
    delay(200);
    ESP.restart();
  });

  webServer.begin();
  settingMode = false;
}

void displayCurrentlyPlaying(CurrentlyPlaying currentlyPlaying)
{
  if (!currentlyPlaying.error)
  {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    switch (currentlyPlaying.statusCode)
    {
    case 200:
    {
      nothingPlayingCount = 0;
      sprintf(currentTimesWithZeros, "%02u:%02u", hourFormat12(), minute());
      display.drawString(0, 0, currentlyPlaying.trackName);
      display.drawString(0, 16, currentlyPlaying.firstArtistName);
      display.drawString(0, 40, String(currentTimesWithZeros) + " " + String(isAM() ? "am" : "pm"));

      isPlaying = currentlyPlaying.isPlaying;
    }
    break;

    case 204:
      if (nothingPlayingCount < 20)
      {
        nothingPlayingCount++;
        display.drawString(0, 0, F("Nothing playing"));
      }
      break;

    default:
      display.drawString(0, 0, F("ERROR"));
      display.drawString(0, 16, F("Please reset"));
      break;
    }
  }
  else
  {
    if (currentlyPlaying.statusCode == -1)
    {
      display.setPixel(127, 0);
      if (failedCount > 20)
      {
        ESP.restart();
      }
      else
      {
        failedCount++;
      }
    }
  }
  display.display();
}

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512);

  pinMode(BUTTON, INPUT_PULLUP);

  display.init();
  display.flipScreenVertically();
  // display.setFont(Open_Sans_Hebrew_Condensed_Light_12);
  display.setFont(Roboto_Condensed_13);

  spotify.currentlyPlayingBufferSize = 5000;
  spotify.playerDetailsBufferSize = 5000;

  // Restore wifi & spotify credentials
  if (restoreConfig())
  {
    // Try wifi connection
    if (checkConnection())
    {
      // Start normal wifi connection as device
      startWifi();
#ifndef USING_AXTLS
      client.setFingerprint(SPOTIFY_FINGERPRINT);
#endif
      // Try to revive saved spotify token else prompt user to sign in
      if (refresh_token != "")
      {
        spotify.setRefreshToken(refresh_token.c_str());
        displayStatus(F("SIGNING IN..."));
        Serial.println(F("Refreshing saved tokens.."));

        if (spotify.refreshAccessToken())
        {
          displayStatus(("STARTING..."));
          Serial.println(F("Tokens refreshed!"));
          tokenReady = true;
        }
        else
        {
          displayStatus(F("SPOTIFY ERROR"), F("Go to:"), F("http://ardspot.local/  or"), localIP + "/");
          Serial.println(F("Failed to refresh tokens"));
          Serial.print(F("Connect to "));
          Serial.println(WiFi.localIP());
          tokenReady = false;
        }
      }
      else
      {
        displayStatus(F("NOT SIGNED IN"), String("On ") + WiFi.SSID() + F(", go to"), F("http://ardspot.local/  or"), String("http://") + localIP + F("/"));
        tokenReady = false;
      }
    }
    else
    {
      displayStatus(F("FAILED TO CONNECT"));
      WiFi.disconnect();
      delay(1300);
      // If wifi exists but failed, start softAP
      startAP();
    }
  }
  else
  {
    // If wifi credentials not saved, start softAP
    startAP();
  }

  // configure http://ardspot.local/
  if (!MDNS.begin(F("ardspot")))
  {
    Serial.println(F("Error setting up MDNS responder!"));
  }
  else
  {
    Serial.println(F("mDNS responder started"));
    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
  }
}

void loop()
{
  int buttonState = digitalRead(BUTTON); // Very unstable right now
  MDNS.update();
  webServer.handleClient();
  // If is in softAP mode
  if (settingMode)
  {
    dnsServer.processNextRequest();
    if (buttonState == LOW)
    {
      displayStatus(F("SETUP INFO"), "SSID: " + String(apSSID), "GW: " + ip2Str(apIP), F("http://ardspot.local/"));
    }
    else
    {
      displayStatus(F("WIFI CONFIG"), F("Connect to:"), apSSID, F("http://ardspot.local/"));
    }
    // Else if the Spotify token is valid then start
  }
  else if (tokenReady)
  {
    if (buttonState == LOW)
    {
      displayStatus(F("NETWORK INFO"), F("http://ardspot.local/"), String("http://") + localIP + F("/"));
    }
    else
    {
      if (millis() > requestDueTime)
      {
        requestDueTime = millis() + delayBetweenRequests;
        unsigned long requestMilli = millis();
        Serial.print(F("Free heap: "));
        Serial.print(ESP.getFreeHeap());
        Serial.print(F(" : "));
        displayCurrentlyPlaying(spotify.getCurrentlyPlaying(SPOTIFY_MARKET));
        Serial.print(ESP.getFreeHeap());
        Serial.print(F("  "));
        Serial.print(millis() - requestMilli);
        Serial.println(F("ms"));
      }
    }
  }
}

const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}