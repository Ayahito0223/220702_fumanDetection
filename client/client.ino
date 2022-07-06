#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <Wire.h>

#define PIR_MOTION_SENSOR 4
#define BUZZER 3

#include "arduino_secrets.h"
#include "rgb_lcd.h"

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
String arduino_pass = SECRET_ARDUINO_PASS;
int status = WL_IDLE_STATUS;

rgb_lcd lcd;

WiFiUDP Udp;
unsigned int localPort = 2390;

char packetBuffer[1024]; //buffer to hold incoming packet

int counter = 0;

bool DEBUG = false;

void setup() {
  pinMode(BUZZER, INPUT);
  pinMode(PIR_MOTION_SENSOR, INPUT);

  lcd.begin(16, 2);
  lcd.print("Begin Serial");

  Serial.begin(9600);
  while (!Serial) {
    if (!DEBUG) {
      break;
    }
    ; // wait for serial port to connect. Needed for native USB port only
  }
  while (true) {
    initWifi();
    if (!initUser()) {
      status = 6;
      continue;
    }
    break;
  }
  Serial.println("Start Detection");
  lcd.clear();
  lcd.setRGB(127, 255, 127);
  lcd.print("start detect!");

  delay(5000);
}

void loop() {
  if (WiFi.status() == 6) {
    digitalWrite(BUZZER, LOW);
    status = 6;
    while (true) {
      initWifi();
      if (!initUser()) {
        status = 6;
        continue;
      }
      break;
    }
    Serial.println("Start Detection");
    lcd.clear();
    lcd.setRGB(127, 255, 127);
    lcd.print("start detect!");

    delay(5000);
  }

  if (digitalRead(PIR_MOTION_SENSOR)) {
    counter += 1;
    if (counter >= 10) {
      lcd.clear();
      lcd.print("human detect!");
      lcd.setRGB(255, 127, 127);
      digitalWrite(BUZZER, HIGH);
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write("2");
      Serial.println("human detected!");
      Udp.endPacket();
      counter = 0;
    }
  } else {
    lcd.clear();
    lcd.setRGB(127, 255, 127);
    lcd.print("detecting...");
    digitalWrite(BUZZER, LOW);
    counter = 0;
  }

  int packetSize = Udp.parsePacket();
  if (packetSize) {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remoteIp = Udp.remoteIP();
    Serial.print(remoteIp);
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, 1023);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    Serial.print("Contents:");
    Serial.println(packetBuffer);

    if (String(packetBuffer).equals("3")) {
      while (true) {
        initWifi();
        if (!initUser()) {
          status = 6;
          continue;
        }
        break;
      }
      Serial.println("Start Detection");
      lcd.clear();
      lcd.setRGB(127, 255, 127);
      lcd.print("start detect!");

      delay(5000);
    }
  }

  delay(100);
}

void initWifi() {
  Udp.stop();

  // check for the WiFi module:
  lcd.clear();
  lcd.print("WiFi NO MODULE!");
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  lcd.clear();
  lcd.setRGB(255, 191, 127);
  lcd.print("wait WiFi...");
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    delay(5000);
  }

  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
  printCurrentNet();
  printWifiData();

  Udp.begin(localPort);
}

bool initUser() {
  bool findServer = false;
  lcd.clear();
  lcd.setRGB(255, 255, 127);
  lcd.print("wait valid user");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  while (!findServer) {
    if (WiFi.status() == 6) {
      return false;
    }
    int packetSize = Udp.parsePacket();
    if (packetSize) {
      Serial.print("Received packet of size ");
      Serial.println(packetSize);
      Serial.print("From ");
      IPAddress remoteIp = Udp.remoteIP();
      Serial.print(remoteIp);
      Serial.print(", port ");
      Serial.println(Udp.remotePort());

      // read the packet into packetBufffer
      int len = Udp.read(packetBuffer, 1023);
      if (len > 0) {
        packetBuffer[len] = 0;
      }
      Serial.print("Contents:");
      Serial.println(packetBuffer);

      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      if (arduino_pass.equals(String(packetBuffer))) {
        Udp.write("1");
        Serial.println("valid server!");
        findServer = true;
      } else {
        Udp.write("0");
        Serial.println("Invalid server");
      }
      Udp.endPacket();
      Serial.println("");
    }
  }
  return true;
}

void printWifiData() {
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print port
  Serial.print("Listening Port: ");
  Serial.println(localPort);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
  Serial.println("");
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
