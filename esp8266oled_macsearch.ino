#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define SDA_PIN 14
#define SCL_PIN 12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);

// ===== TARGET =====
String targetMAC = "76:FF:FC:A2:D1:C5";

// ===== SPEED =====
const unsigned long SCAN_INTERVAL = 500; // быстрее чем раньше

// ===== CALIBRATION =====
float A = -45.0;
float n = 2.6;

// ===== STATE =====
bool targetFound = false;
int targetRSSI = -100;
String targetSSID = "";

int lastRSSI = -100;
String trend = "-----";

unsigned long lastScanTime = 0;

// ===== SMOOTH (6 samples) =====
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

  for(int i = 0; i < count; i++) sum += buffer[i];

  return sum / count;
}

// ===== TREND FAST =====
void updateTrend(int rssi) {
  int d = rssi - lastRSSI;

  if(d > 1) trend = "^^^^^";
  else if(d < -1) trend = "vvvvv";
  else trend = "-----";

  lastRSSI = rssi;
}

// ===== DISTANCE =====
float calcDistance(int rssi) {
  return pow(10.0, (A - rssi) / (10.0 * n));
}

// ===== BAR =====
void drawBar(int x, int y, int val) {
  int bars = map(val, 0, 100, 0, 10);

  display.setCursor(x, y);
  display.print("[");

  for(int i = 0; i < 10; i++) {
    display.print(i < bars ? "#" : " ");
  }

  display.print("]");
}

// ===== WEB =====
const char PAGE[] PROGMEM = R"rawliteral(
<h3>RF Finder</h3>
<p><b>MAC:</b> %MAC%</p>
<form action="/set">
<input name="mac">
<br><br>
<input type="submit" value="SET">
</form>
)rawliteral";

void handleRoot() {
  String page = PAGE;
  page.replace("%MAC%", targetMAC);
  server.send(200, "text/html", page);
}

void handleSet() {
  if(server.hasArg("mac")) {
    targetMAC = server.arg("mac");
    targetMAC.toUpperCase();
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

  WiFi.mode(WIFI_STA);
  WiFi.softAP("RFFinder");

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("RF Finder");
  display.display();

  delay(800);
}

// ===== LOOP =====
void loop() {
  server.handleClient();

  // ===== FAST SCAN =====
  if(millis() - lastScanTime >= SCAN_INTERVAL) {
    lastScanTime = millis();

    int n = WiFi.scanNetworks(false, true); // fast scan

    targetFound = false;

    for(int i = 0; i < n; i++) {

      // фильтр слабых сигналов (ускоряет)
      if(WiFi.RSSI(i) < -85) continue;

      String mac = WiFi.BSSIDstr(i);
      mac.toUpperCase();

      if(mac != targetMAC) continue;

      targetFound = true;

      targetSSID = WiFi.SSID(i);
      if(targetSSID == "") targetSSID = "<hidden>";

      int raw = WiFi.RSSI(i);
      targetRSSI = smoothRSSI(raw);

      updateTrend(targetRSSI);

      break; // ранний выход = быстрее
    }

    WiFi.scanDelete();
  }

  // ===== DISPLAY =====
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if(targetFound) {

    String ssid = targetSSID;
    if(ssid.length() > 14) ssid = ssid.substring(0, 14);

    display.setCursor(0, 0);
    display.println(ssid);

    display.setCursor(0, 7);
    display.println(targetMAC);

    display.setCursor(0, 18);
    display.print("RSSI: ");
    display.println(targetRSSI);

    int sig = map(constrain(targetRSSI, -90, -30), -90, -30, 0, 100);
    sig = constrain(sig, 0, 100);

    display.setCursor(0, 28);
    display.print("SIG:");
    drawBar(30, 28, sig);

    float dist = calcDistance(targetRSSI);

    display.setCursor(0, 38);
    display.print("D: ");
    display.print(dist, 1);
    display.println(" m");

    display.setTextSize(2);
    display.setCursor(10, 50);
    display.println(trend);

  } else {

    display.setCursor(0, 0);
    display.println("SEARCHING...");

    display.setCursor(0, 10);
    display.println(targetMAC);

    display.setCursor(0, 25);
    display.println("SEARCH MODE");
  }

  display.display();
}