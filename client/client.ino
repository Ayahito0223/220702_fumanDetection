#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Crypto.h>
#include <SHA256.h>
#include <string.h>

#include "arduino_secrets.h"
#include "rgb_lcd.h"

// MOTION SENSORのpinは4
#define PIR_MOTION_SENSOR 4

// BUZZERのpinは3
#define BUZZER 3

// SSID
char ssid[] = SECRET_SSID;

// SSIDのパスワード
char pass[] = SECRET_PASS;

// Arduinoからデータを受け取るためのパスワード
String arduino_pass = SECRET_ARDUINO_PASS;

// データを受け取るポート番号
unsigned int localPort = 2390;

// WiFiのステータスを管理する変数
int status = WL_IDLE_STATUS;

// UDPを使うための変数
WiFiUDP Udp;

// UDPで送られて来たデータを入れるための変数
char packetBuffer[1024];

// LCDを使うための変数
rgb_lcd lcd;

// 暗号通信をするためのpythonから送られて来たランダムな文字列を入れる変数
const int STREAM_PASS_SIZE = 32;
char streamPass[STREAM_PASS_SIZE];
int streamPassCounter = 0;

// 人を検知しすぎないように制御するカウンター
int counter = 0;

// 処理を制御する変数
int processNum = 0;

// SHA256でハッシュ化するための変数
SHA256 sha256;

// DEBUG用のフラグ（Serialの内容を読みたい時Trueに、Arudinoを独立して動かす時にはFalse）
bool DEBUG = false;

#define USER_INVALID "0"
#define USER_VALID "1"
#define HUMAN_DETECTED "2"
#define SERVER_OK "5"
#define SERVER_QUIT "9"

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
    ;
  }
//  Serial.println((2 + 1) % 3);
}

void loop() {
  // 必ずやる処理
  if (WiFi.status() == 6) {
    processNum = 0;
  }

  switch (processNum) {
    //wifiのinit処理
    case 0:
      digitalWrite(BUZZER, LOW);
      status = 6;
      initWifi();

      processNum = 1;

      break;

    // ユーザーのinit処理
    case 1:
      digitalWrite(BUZZER, LOW);
      if (!initUser()) {
        processNum = 0;

      } else {
        Serial.println("Start Detection");
        lcd.clear();
        lcd.setRGB(127, 255, 127);
        lcd.print("start detect!");

        processNum = 2;
        delay(2500);
      }
      break;

    // メインの処理
    case 2:
      // 人を検知してpythonの方に送る
      if (digitalRead(PIR_MOTION_SENSOR)) {
        counter += 1;
        if (counter >= 10) {
          lcd.clear();
          lcd.print("human detect!");
          lcd.setRGB(255, 127, 127);
          digitalWrite(BUZZER, HIGH);
          Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
          Udp.write(useHashMessage(HUMAN_DETECTED));
          Serial.println("human detected!");
          Udp.endPacket();
          counter = 0;

          delay(1000);
          int quitCount = 0;
          while (true) {
            if (WiFi.status() == 6) {
              processNum = 0;
              break;
            }
            if (quitCount >= 40000) {
              processNum = 1;
              break;
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

              if (String(packetBuffer).equals(checkHashMessage(SERVER_OK))) {
                break;
              }
            }
            quitCount += 1;
          }
        }
      } else {
        lcd.clear();
        lcd.setRGB(127, 255, 127);
        lcd.print("detecting...");
        digitalWrite(BUZZER, LOW);
        counter = 0;

        // pythonからのデータを受け取る。
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

          if (String(packetBuffer).equals(checkHashMessage(SERVER_QUIT))) {
            processNum = 1;
          }
        }
      }

      delay(100);

      break;
  }
}

#define HASH_SIZE 32
#define BLOCK_SIZE 64
char hex[256];
byte buffer[128];

char *btoh(char *dest, uint8_t *src, int len) {
 char *d = dest;
 while( len-- ) sprintf(d, "%02x", (unsigned char)*src++), d += 2;
 return dest;
}

char* hashed(Hash *hash, const char* hashvalue)
{
  uint8_t value[HASH_SIZE];

  hash->reset();
  hash->update(hashvalue, strlen(hashvalue));
  hash->finalize(value, sizeof(value));
  
  return btoh(hex, value, 32);
}

char* checkHashMessage(char* original) {
  String num = original;
  String pass = (String)streamPass[streamPassCounter];
  String str = num + pass;
  streamPassCounter = (streamPassCounter + 1) % STREAM_PASS_SIZE;
  return hashed(&sha256, str.c_str());
}

char* useHashMessage(char* message) {
  String num = message;
  String pass = (String)streamPass[streamPassCounter];
  String str = num + pass;
  streamPassCounter = (streamPassCounter + 1) % STREAM_PASS_SIZE;
  return hashed(&sha256, str.c_str());
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
  bool findUser = false;

  Serial.println("wait valid user");

  lcd.clear();
  lcd.setRGB(255, 255, 127);
  lcd.print("wait valid user");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  while (!findUser) {
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
        Udp.write(USER_VALID);
        findUser = true;
      } else {
        Udp.write(USER_INVALID);
        Serial.println("Invalid server");
      }
      Udp.endPacket();
      Serial.println("");
    }
  }
  delay(1500);
  while (true) {
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

      int len = Udp.read(streamPass, STREAM_PASS_SIZE);
      if (len > 0) {
        streamPass[len] = 0;
      }
      Serial.print("Contents:");
      Serial.println(streamPass);
      Serial.println(strlen(streamPass));
      streamPassCounter = 0;

      Serial.println("valid server!");
      Serial.println("");
      break;
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
