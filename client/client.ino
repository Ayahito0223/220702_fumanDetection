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

/* ----- Wifi関連の変数定義 -----*/
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

// サーバーのIPアドレスとポートを保持
IPAddress serverIp;
int serverPort;

// python側からの接続が返ってくるまでの制限時間の最大値
const int SERVER_TIME_LIMIT = 50000;

/* ----- ↑Wifi関連の変数定義↑ -----*/

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

// DEBUG用のフラグ（Serialの内容を読みたい時Trueに、Arudinoを独立して動かす時にはFalse）
bool DEBUG = false;

// 通信内容の番号
#define USER_INVALID "0"
#define USER_VALID "1"
#define HUMAN_DETECTED "2"
#define SERVER_OK "5"
#define SERVER_QUIT "9"

/* ----- SHA256でのハッシュ化に関連する変数と処理 -----*/
// SHA256でハッシュ化するための変数
SHA256 sha256;
#define HASH_SIZE 32
#define BLOCK_SIZE 64
char hex[256];

char *btoh(char *dest, uint8_t *src, int len) {
  char *d = dest;
  while ( len-- ) sprintf(d, "%02x", (unsigned char)*src++), d += 2;
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
/* ----- ↑SHA256でのハッシュ化に関連する変数と処理↑ -----*/

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
          Udp.beginPacket(serverIp, serverPort);
          char* sendStr = useHashMessage(HUMAN_DETECTED);
          Udp.write(sendStr);
          Udp.endPacket();

          Serial.println("human detected!");
          sendLogPrint(serverIp, serverPort, sendStr);
          counter = 0;

          // serverからデータを受け取ったというレスポンスを得る。
          delay(1000);
          int quitCount = 0;
          while (true) {
            if (WiFi.status() == 6) {
              processNum = 0;
              break;
            }
            if (quitCount >= SERVER_TIME_LIMIT) {
              processNum = 1;
              break;
            }
            int packetSize = Udp.parsePacket();
            if (packetSize) {
              int len = Udp.read(packetBuffer, 1023);
              if (len > 0) {
                packetBuffer[len] = 0;
              }

              recieveLogPrint(packetBuffer);

              if (((String)serverIp).equals((String)Udp.remoteIP()) && serverPort == Udp.remotePort()) {
                if (checkHashMessage(SERVER_OK, packetBuffer)) {
                  break;
                }
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

        // pythonからのデータを受け取る。（python側のプログラムを終了するという情報を受け取る）
        int packetSize = Udp.parsePacket();
        if (packetSize) {
          int len = Udp.read(packetBuffer, 1023);
          if (len > 0) {
            packetBuffer[len] = 0;
          }
          recieveLogPrint(packetBuffer);

          if (((String)serverIp).equals((String)Udp.remoteIP()) && serverPort == Udp.remotePort()) {
            if (checkHashMessage(SERVER_QUIT, packetBuffer)) {
              processNum = 1;
            }
          }
        }
      }

      delay(100);

      break;
  }
}

bool checkHashMessage(char* original, char* sendMessage) {
  String num = original;
  String pass = (String)streamPass[streamPassCounter];
  String str = num + pass;
  String result = hashed(&sha256, str.c_str());
  if (result.equals((String)sendMessage)) {
    streamPassCounter = (streamPassCounter + 1) % STREAM_PASS_SIZE;
    return true;
  } else {
    return false;
  }
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
  serverIp = IPAddress();
  serverPort = -1;

  Serial.println("wait valid user...");
  Serial.println("");

  lcd.clear();
  lcd.setRGB(255, 255, 127);
  lcd.print("wait valid user");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  // python側からのパスワード受け取りと認証
  // 認証に成功したら、1を送信、失敗したら2を送信。
  while (!findUser) {
    if (WiFi.status() == 6) {
      return false;
    }
    int packetSize = Udp.parsePacket();
    if (packetSize) {
      int len = Udp.read(packetBuffer, 1023);
      if (len > 0) {
        packetBuffer[len] = 0;
      }

      recieveLogPrint(packetBuffer);

      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      if (arduino_pass.equals(String(packetBuffer))) {
        Udp.write(USER_VALID);
        findUser = true;
        serverIp = Udp.remoteIP();
        serverPort = Udp.remotePort();

        sendLogPrint(serverIp, serverPort, USER_VALID);
      } else {
        Udp.write(USER_INVALID);
        sendLogPrint(Udp.remoteIP(), Udp.remotePort(), USER_INVALID);
        Serial.println("Invalid server");
        Serial.println("");
      }
      Udp.endPacket();
    }
  }

  // 認証に成功したら、python側から作成されたstreamPassを受け取る。
  delay(1500);
  int quitCount = 0;
  while (true) {
    if (WiFi.status() == 6) {
      return false;
    }
    if (quitCount >= SERVER_TIME_LIMIT) {
      return false;
    }
    int packetSize = Udp.parsePacket();
    if (packetSize) {
      if (((String)serverIp).equals((String)Udp.remoteIP()) && serverPort == Udp.remotePort()) {
        int len = Udp.read(streamPass, STREAM_PASS_SIZE);
        if (len > 0) {
          streamPass[len] = 0;
        }
        recieveLogPrint(streamPass);

        streamPassCounter = 0;

        Serial.println("valid server!");
        Serial.println("");
        break;
      } else {
        int len = Udp.read(packetBuffer, 1023);
        if (len > 0) {
          packetBuffer[len] = 0;
        }
        recieveLogPrint(packetBuffer);

        Serial.println("No Match IP and Port");
        Serial.println("");
      }
    }
    quitCount += 1;
  }
  return true;
}

void recieveLogPrint(char* packetMessage) {
  Serial.print("From ");
  Serial.print(Udp.remoteIP());
  Serial.print(", port ");
  Serial.println(Udp.remotePort());
  Serial.print("Contents:");
  Serial.println(packetMessage);
  Serial.println("");
}

void sendLogPrint(IPAddress ip, int port, char* packetMessage) {
  Serial.print("To ");
  Serial.print(ip);
  Serial.print(", port ");
  Serial.println(port);
  Serial.print("Content: ");
  Serial.println(packetMessage);
  Serial.println("");
}

// Wifiの情報出力
void printWifiData() {
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Serial.print("Listening Port: ");
  Serial.println(localPort);

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
  Serial.println("");
}

// ネットワークの情報出力
void printCurrentNet() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

// MacアドレスをSerialに出力
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
