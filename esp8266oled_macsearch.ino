#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define SDA_PIN 14
#define SCL_PIN 12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);

// ===== TARGET =====
String targetMAC = "50:FF:20:BD:5A:D5";

// ===== STATE =====
bool targetFound = false;
int targetRSSI = -100;
String targetSSID = "";

int lastRSSI = -100;
String trend = "-----";

unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 1000;

// ===== RSSI SMOOTH =====
int smoothRSSI(int newValue) {
  static int buffer[5] = {-100, -100, -100, -100, -100};
  static int index = 0;

  buffer[index++] = newValue;
  if(index >= 5) index = 0;

  int sum = 0;
  for(int i = 0; i < 5; i++) sum += buffer[i];

  return sum / 5;
}

// ===== TREND =====
void updateTrend(int currentRSSI) {
  int delta = currentRSSI - lastRSSI;

  if(delta > 2) trend = "^^^";
  else if(delta < -2) trend = "vvv";
  else trend = "---";

  lastRSSI = currentRSSI;
}

// ===== BAR GRAPH =====
void drawBar(int x, int y, int value) {
  int bars = map(value, 0, 100, 0, 10);

  display.setCursor(x, y);
  display.print("[");

  for(int i = 0; i < 10; i++) {
    if(i < bars) display.print("#");
    else display.print(" ");
  }

  display.print("]");
}

// ===== WEB UI =====
void handleRoot() {
  String html =
    "<h2>WiFi Tracker</h2>"
    "<form action='/set'>"
    "MAC: <input name='mac' value='" + targetMAC + "'><br><br>"
    "<input type='submit' value='Set MAC'>"
    "</form>";

  server.send(200, "text/html", html);
}

void handleSet() {
  if(server.hasArg("mac")) {
    targetMAC = server.arg("mac");
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }

  // WiFi scan mode
  WiFi.mode(WIFI_STA);

  // WEB SERVER
  WiFi.softAP("WiFiTracker");
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("WiFi Tracker Ready");
  display.display();

  delay(1000);
}

// ===== LOOP =====
void loop() {
  server.handleClient();

  if(millis() - lastScanTime >= SCAN_INTERVAL) {
    lastScanTime = millis();

    int networks = WiFi.scanNetworks();

    targetFound = false;

    for(int i = 0; i < networks; i++) {
      uint8_t* bssid = WiFi.BSSID(i);

      char mac[18];
      snprintf(mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
               bssid[0], bssid[1], bssid[2],
               bssid[3], bssid[4], bssid[5]);

      String currentMAC = String(mac);

      if(currentMAC == targetMAC) {
        targetFound = true;

        targetSSID = WiFi.SSID(i);
        if(targetSSID == "") targetSSID = "<hidden>";

        int rawRSSI = WiFi.RSSI(i);
        targetRSSI = smoothRSSI(rawRSSI);

        updateTrend(targetRSSI);
        break;
      }
    }

    // ===== DISPLAY =====
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    if(targetFound) {

      String ssid = targetSSID;
      if(ssid.length() > 14) ssid = ssid.substring(0, 14);

      display.setCursor(0, 0);
      display.println(ssid);

      display.setCursor(0, 8);
      display.println(targetMAC);

      display.setCursor(0, 18);
      display.print("RSSI: ");
      display.println(targetRSSI);

      int strength = map(constrain(targetRSSI, -90, -30), -90, -30, 0, 100);
      strength = constrain(strength, 0, 100);

      display.setCursor(0, 28);
      display.print("SIG:");

      drawBar(30, 28, strength);

      display.setCursor(0, 42);
      display.setTextSize(2);
      display.println(trend);

    } else {
      display.setCursor(0, 25);
      display.println("TARGET NOT FOUND");
    }

    display.display();
    WiFi.scanDelete();
  }
}
