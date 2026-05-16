#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// =========================
// OLED PINS
// =========================
#define SDA_PIN 14
#define SCL_PIN 12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);

// =========================
// SETTINGS
// =========================
const unsigned long SCAN_INTERVAL = 2000;

// =========================
// TRACK TARGET
// =========================
bool trackingMode = false;

String targetMAC = "";
String targetSSID = "";

bool targetFound = false;
int targetRSSI = -100;

// =========================
// CALIBRATION
// =========================
float A = -45.0;
float n = 2.6;

// =========================
// TREND
// =========================
int lastRSSI = -100;
String trend = "-----";

// =========================
// TIME
// =========================
unsigned long lastScanTime = 0;

// =========================
// NETWORK STRUCT
// =========================
struct NetworkInfo {
  String ssid;
  String bssid;
  int rssi;
  String encryption;
};

NetworkInfo topNetworks[3];

// =========================
// SMOOTH
// =========================
int buffer[6];
int idx = 0;
bool full = false;

int smoothRSSI(int v) {

  buffer[idx++] = v;

  if(idx >= 6) {
    idx = 0;
    full = true;
  }

  int count = full ? 6 : idx;

  int sum = 0;

  for(int i = 0; i < count; i++) {
    sum += buffer[i];
  }

  return sum / count;
}

// =========================
// ENCRYPTION
// =========================
String getEncryptionType(int encryptionType) {

  switch(encryptionType) {

    case ENC_TYPE_NONE:
      return "OP";

    case ENC_TYPE_TKIP:
      return "WP";

    case ENC_TYPE_CCMP:
      return "W2";

    default:
      return "SC";
  }
}

// =========================
// TREND
// =========================
void updateTrend(int rssi) {

  int d = rssi - lastRSSI;

  if(d > 1) trend = "^^^^^";
  else if(d < -1) trend = "vvvvv";
  else trend = "-----";

  lastRSSI = rssi;
}

// =========================
// DISTANCE
// =========================
float calcDistance(int rssi) {

  return pow(10.0, (A - rssi) / (10.0 * n));
}

// =========================
// WEB SIGNAL BAR
// =========================
String makeBar(int rssi) {

  int strength = map(constrain(rssi, -90, -30), -90, -30, 0, 10);

  String bar = "[";

  for(int i = 0; i < 10; i++) {
    bar += (i < strength) ? "=" : " ";
  }

  bar += "]";

  return bar;
}

// =========================
// WEB PAGE
// =========================
String makePage() {

  String html = "";

  html += "<html>";
  html += "<head>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<style>";
  html += "body{font-family:Arial;background:#111;color:#0f0;padding:10px;}";
  html += ".box{border:1px solid #0f0;padding:10px;margin-bottom:10px;}";
  html += "button{padding:10px;background:#0f0;border:none;font-weight:bold;}";
  html += "</style>";
  html += "</head>";
  html += "<body>";

  html += "<h2>RF Finder</h2>";

  // =========================
  // TRACK MODE
  // =========================
  if(trackingMode) {

    html += "<div class='box'>";

    html += "<h3>TRACKING</h3>";

    html += "<b>SSID:</b> " + targetSSID + "<br>";
    html += "<b>MAC:</b> " + targetMAC + "<br>";
    html += "<b>RSSI:</b> " + String(targetRSSI) + "<br>";
    html += "<b>TREND:</b> " + trend + "<br>";
    html += "<b>DIST:</b> " + String(calcDistance(targetRSSI), 1) + " m<br>";

    html += "<br>";
    html += makeBar(targetRSSI);

    html += "<br><br>";

    html += "<a href='/reset'>";
    html += "<button>STOP TRACKING</button>";
    html += "</a>";

    html += "</div>";

  } else {

    // =========================
    // TOP 3
    // =========================
    for(int i = 0; i < 3; i++) {

      if(topNetworks[i].ssid == "") continue;

      html += "<div class='box'>";

      html += "<h3>TOP ";
      html += String(i + 1);
      html += "</h3>";

      html += "<b>SSID:</b> ";
      html += topNetworks[i].ssid;
      html += "<br>";

      html += "<b>MAC:</b> ";
      html += topNetworks[i].bssid;
      html += "<br>";

      html += "<b>RSSI:</b> ";
      html += String(topNetworks[i].rssi);
      html += " dBm<br>";

      html += "<b>ENC:</b> ";
      html += topNetworks[i].encryption;
      html += "<br><br>";

      html += makeBar(topNetworks[i].rssi);

      html += "<br><br>";

      html += "<a href='/select?mac=";
      html += topNetworks[i].bssid;
      html += "&ssid=";
      html += topNetworks[i].ssid;
      html += "'>";

      html += "<button>TRACK</button>";
      html += "</a>";

      html += "</div>";
    }
  }

  html += "</body></html>";

  return html;
}

// =========================
// WEB HANDLERS
// =========================
void handleRoot() {

  server.send(200, "text/html", makePage());
}

void handleSelect() {

  if(server.hasArg("mac")) {

    targetMAC = server.arg("mac");
    targetSSID = server.arg("ssid");

    targetMAC.toUpperCase();

    trackingMode = true;
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleReset() {

  trackingMode = false;

  targetMAC = "";
  targetSSID = "";

  server.sendHeader("Location", "/");
  server.send(303);
}

// =========================
// SETUP
// =========================
void setup() {

  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }

  WiFi.mode(WIFI_STA);

  WiFi.softAP("RFFinder");

  server.on("/", handleRoot);
  server.on("/select", handleSelect);
  server.on("/reset", handleReset);

  server.begin();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("RF Finder");

  display.setCursor(0, 10);
  display.println("192.168.4.1");

  display.display();

  delay(1500);
}

// =========================
// LOOP
// =========================
void loop() {

  server.handleClient();

  if(millis() - lastScanTime >= SCAN_INTERVAL) {

    lastScanTime = millis();

    int networks = WiFi.scanNetworks(false, true);

    // RESET TOP
    for(int i = 0; i < 3; i++) {

      topNetworks[i].ssid = "";
      topNetworks[i].bssid = "";
      topNetworks[i].rssi = -100;
      topNetworks[i].encryption = "";
    }

    targetFound = false;

    // =========================
    // SCAN
    // =========================
    for(int i = 0; i < networks; i++) {

      int rssi = WiFi.RSSI(i);

      String ssid = WiFi.SSID(i);

      if(ssid == "") {
        ssid = "<HIDDEN>";
      }

      uint8_t* bssid = WiFi.BSSID(i);

      char mac[18];

      snprintf(
        mac,
        18,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        bssid[0],
        bssid[1],
        bssid[2],
        bssid[3],
        bssid[4],
        bssid[5]
      );

      // =========================
      // TOP 3
      // =========================
      for(int j = 0; j < 3; j++) {

        if(rssi > topNetworks[j].rssi) {

          for(int k = 2; k > j; k--) {
            topNetworks[k] = topNetworks[k - 1];
          }

          topNetworks[j].ssid = ssid;
          topNetworks[j].bssid = String(mac);
          topNetworks[j].rssi = rssi;
          topNetworks[j].encryption = getEncryptionType(WiFi.encryptionType(i));

          break;
        }
      }

      // =========================
      // TRACK
      // =========================
      if(trackingMode) {

        String currentMAC = String(mac);
        currentMAC.toUpperCase();

        if(currentMAC == targetMAC) {

          targetFound = true;

          targetRSSI = smoothRSSI(rssi);

          updateTrend(targetRSSI);
        }
      }
    }

    // =========================
    // OLED
    // =========================
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // =========================
    // TRACK SCREEN
    // =========================
    if(trackingMode) {

      if(targetFound) {

        display.setCursor(0, 0);

        String ssid = targetSSID;

        if(ssid.length() > 12) {
          ssid = ssid.substring(0, 12);
        }

        display.println(ssid);

        display.setCursor(0, 8);
        display.println(targetMAC);

        display.setCursor(0, 20);
        display.print("RSSI:");
        display.println(targetRSSI);

        int strength = map(
          constrain(targetRSSI, -90, -30),
          -90,
          -30,
          0,
          100
        );

        display.setCursor(0, 30);

        display.print("[");

        for(int i = 0; i < 10; i++) {
          display.print(i < strength / 10 ? "=" : " ");
        }

        display.println("]");

        display.setCursor(0, 42);

        display.print(calcDistance(targetRSSI), 1);
        display.println(" m");

        display.setTextSize(2);

        display.setCursor(20, 52);
        display.println(trend);

      } else {

        display.setCursor(0, 0);
        display.println("TARGET LOST");
      }

    } else {

      // =========================
      // TOP3 SCREEN (ADVANCED)
      // =========================

      // TOP 1
      String ssid1 = topNetworks[0].ssid;

      if(ssid1 == "") {
        ssid1 = "<HIDDEN>";
      }

      if(ssid1.length() > 12) {
        ssid1 = ssid1.substring(0, 12);
      }

      display.setCursor(0, 0);

      display.print(ssid1);

      display.setCursor(95, 0);
      display.println(topNetworks[0].rssi);

      // BSSID
      display.setCursor(0, 8);
      display.println(topNetworks[0].bssid);

      // SIGNAL BAR
      int strength = map(
        constrain(topNetworks[0].rssi, -90, -30),
        -90,
        -30,
        0,
        100
      );

      strength = constrain(strength, 0, 100);

      display.print(topNetworks[0].encryption);

      display.print(" [");

      for(int i = 0; i < 10; i++) {
        display.print(i < strength / 10 ? "=" : " ");
      }

      display.print("] ");

      display.print(strength);

      display.println("%");

      // LINE
      display.println("-------------");

      // TOP 2
      display.setCursor(0, 42);

      if(topNetworks[1].ssid != "") {

        String ssid2 = topNetworks[1].ssid;

        if(ssid2.length() > 10) {
          ssid2 = ssid2.substring(0, 10);
        }

        display.print(ssid2);

        display.setCursor(95, 42);
        display.println(topNetworks[1].rssi);

      } else {

        display.println("---");
      }

      // TOP 3
      display.setCursor(0, 50);

      if(topNetworks[2].ssid != "") {

        String ssid3 = topNetworks[2].ssid;

        if(ssid3.length() > 10) {
          ssid3 = ssid3.substring(0, 10);
        }

        display.print(ssid3);

        display.setCursor(95, 50);
        display.println(topNetworks[2].rssi);

      } else {

        display.println("---");
      }
    }

    display.display();

    WiFi.scanDelete();
  }
}