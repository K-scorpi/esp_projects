#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// Пины для HW-364A
#define SDA_PIN 14
#define SCL_PIN 12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct NetworkInfo {
  String ssid;
  String bssid;
  int rssi;
  String encryption;
};

NetworkInfo topNetworks[3];
unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 2000;

String getEncryptionType(int encryptionType) {
  switch(encryptionType) {
    case ENC_TYPE_NONE: return "OP";
    case ENC_TYPE_TKIP: return "WP";
    case ENC_TYPE_CCMP: return "W2";
    default: return "SC";
  }
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }
  
  WiFi.mode(WIFI_STA);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Scanning WiFi...");
  display.display();
  
  delay(1000);
}

void loop() {
  if(millis() - lastScanTime >= SCAN_INTERVAL) {
    lastScanTime = millis();
    
    int networks = WiFi.scanNetworks();
    
    if(networks == 0) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("NO NETWORKS");
      display.display();
    } else {
      // Поиск топ-3
      for(int i = 0; i < 3; i++) {
        topNetworks[i].ssid = "";
        topNetworks[i].bssid = "";
        topNetworks[i].rssi = -100;
      }
      
      for(int i = 0; i < networks; i++) {
        int rssi = WiFi.RSSI(i);
        String ssid = WiFi.SSID(i);
        uint8_t* bssid = WiFi.BSSID(i);
        char mac[18];
        snprintf(mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
        
        for(int j = 0; j < 3; j++) {
          if(rssi > topNetworks[j].rssi) {
            for(int k = 2; k > j; k--) {
              topNetworks[k] = topNetworks[k-1];
            }
            topNetworks[j].ssid = ssid;
            topNetworks[j].bssid = String(mac);
            topNetworks[j].rssi = rssi;
            topNetworks[j].encryption = getEncryptionType(WiFi.encryptionType(i));
            break;
          }
        }
      }
      
      // Вывод на дисплей
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      
      // TOP 1
      String ssid1 = topNetworks[0].ssid;
      if(ssid1 == "") ssid1 = "<HIDDEN>";
      if(ssid1.length() > 12) ssid1 = ssid1.substring(0, 12);
      display.print(ssid1);
      display.setCursor(95, 0);
      display.println(topNetworks[0].rssi);
      
      display.setCursor(0, 8);
      display.println(topNetworks[0].bssid);
      
      int strength = map(constrain(topNetworks[0].rssi, -90, -30), -90, -30, 0, 100);
      strength = constrain(strength, 0, 100);
      
      display.print(topNetworks[0].encryption);
      display.print(" [");
      for(int i = 0; i < 10; i++) {
        display.print(i < strength/10 ? "=" : " ");
      }
      display.print("] ");
      display.print(strength);
      display.println("%");
      
      display.println("-------------");
      
      // TOP 2
      display.setCursor(0, 42);
      if(topNetworks[1].ssid != "") {
        String ssid2 = topNetworks[1].ssid;
        if(ssid2.length() > 10) ssid2 = ssid2.substring(0, 10);
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
        if(ssid3.length() > 10) ssid3 = ssid3.substring(0, 10);
        display.print(ssid3);
        display.setCursor(95, 50);
        display.println(topNetworks[2].rssi);
      } else {
        display.println("---");
      }
      
      display.display();
      
      // Лог в Serial
      Serial.println("\n=== SCAN ===");
      for(int i = 0; i < 3; i++) {
        if(topNetworks[i].ssid != "") {
          Serial.print(i+1);
          Serial.print(". ");
          Serial.print(topNetworks[i].ssid);
          Serial.print(" | ");
          Serial.print(topNetworks[i].bssid);
          Serial.print(" | ");
          Serial.print(topNetworks[i].rssi);
          Serial.println(" dBm");
        }
      }
    }
    
    WiFi.scanDelete();
  }
}