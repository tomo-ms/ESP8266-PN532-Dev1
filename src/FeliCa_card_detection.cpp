#include <Arduino.h>
#include <stdio.h>
#include <PN532.h>
#include <PN532_debug.h>
#include <PN532_SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <aJson.h>
#include "password.h"

// Msg LED: GPIO16(D0)   Error LED: GPIO4(D2)
#define MSG_LED         16
#define ERROR_LED       4
#define PN532_SS        5

// SCK->GOPI14(D5),  MISO->GPIO12(D6), MOSI->GPIO13(D7), SS->GPIO5(D1)
PN532_SPI pn532spi(SPI, PN532_SS);
PN532 nfc(pn532spi);
uint8_t        _prevIDm[8];
unsigned long  _prevTime;

ESP8266WiFiMulti WiFiMulti;
long wifiTimeout = 60000;

bool isError = false;

void blinkLED(uint8_t pin, uint16_t onTime, uint16_t offTime, uint8_t loop ) {
  for(int i=0; i<loop; i++) {
    analogWrite(pin, 255);
    delay(onTime);
    analogWrite(pin, 0);
    delay(offTime);
  }
}

bool initPN532() {
  Serial.println();
  Serial.println("[PN532] Connecting PN532 board");
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata)
  {
    Serial.println("[PN532] Didn't find PN532 board");
    isError = true;
    return false;
  }
  Serial.print("[PN532] Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("[PN532] Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  // Set PN532 param
  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();
  memset(_prevIDm, 0, 8);
  return true;
}

bool connectWifi() {
  Serial.println();
  Serial.println("[WiFi] Connecting WiFi");

  // wait for WiFi connection
  WiFiMulti.addAP(MY_SSID1, MY_PASSWORD1);
  WiFiMulti.addAP(MY_SSID2, MY_PASSWORD2);

  long start = millis();
  while (WiFiMulti.run() != WL_CONNECTED) {
    if ( (millis() - start) >wifiTimeout ) {
      Serial.println("[WiFi] WiFi connection failed");
      isError = true;
      return false;
    }
    Serial.print(".");
    delay(1000);
  }

  Serial.println();
  Serial.println("[WiFi] WiFi connected");
  Serial.print("[WiFi] IP address: "); Serial.println(WiFi.localIP());
  return true;
}

void sendNotification(uint8_t *idm) {
  Serial.println("[HTTP] Sending card data to the server...");

  aJsonObject *root, *msg;
  root = aJson.createObject();
  aJson.addStringToObject(root, "to", "/topics/news");
  aJson.addItemToObject(root, "notification", msg = aJson.createObject());
  char buf[40];
  sprintf(buf, "Card: %02X%02X%02X%02X%02X%02X%02X%02X",
    idm[0], idm[1], idm[2], idm[3], idm[4], idm[5], idm[6], idm[7]);
  aJson.addStringToObject(msg, "text", buf);
  aJson.addStringToObject(msg, "sound", "default");
  char *payload = aJson.print(root);

  const char *url = "http://fcm.googleapis.com/fcm/send";
  HTTPClient http;
  http.begin(url);
  http.addHeader("Authorization", FIREBASE_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.printf("[HTTP] POST response code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.print("[HTTP] response body: ");
      Serial.println(payload);
    }
  }
  http.end();
}

void setup(void)
{
  pinMode(ERROR_LED, OUTPUT);
  analogWrite(ERROR_LED, 0);
  pinMode(MSG_LED, OUTPUT);
  analogWrite(MSG_LED, 255);

  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println();
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    blinkLED(MSG_LED, 100, 100, 5); // wait Serial to be stabilized
  }

  if ( !initPN532() )
    return;
  if ( !connectWifi() )
    return;
}

void loop(void)
{
  if (isError) {
    analogWrite(MSG_LED, 0);
    while(1) {
      blinkLED(ERROR_LED, 400, 100, 3);
      delay(1000);
    }
  }
  int8_t ret;
  uint16_t systemCode = 0xFFFF;
  uint8_t requestCode = 0x01;       // System Code request
  uint8_t idm[8];
  uint8_t pmm[8];
  uint16_t systemCodeResponse;

  // Wait for an FeliCa type cards.
  // When one is found, some basic information such as IDm, PMm, and System Code are retrieved.
  Serial.print("[PN532] Waiting for an FeliCa card...  ");
  analogWrite(MSG_LED, 32);
  ret = nfc.felica_Polling(systemCode, requestCode, idm, pmm, &systemCodeResponse, 5000);

  if ( (ret == 0) || (ret == -2) )
  {
    Serial.println("Timeout");
    delay(500);
    return;
  }
  if (ret != 1)
  {
    Serial.println();
    Serial.print("[PN532] Card access error: code "); Serial.println(ret);
    analogWrite(MSG_LED, 0);
    blinkLED(ERROR_LED, 100, 50, 3);
    delay(500);
    return;
  }

  if ( memcmp(idm, _prevIDm, 8) == 0 ) {
    if ( (millis() - _prevTime) < 2000 ) {
      Serial.println("Same card");
      analogWrite(ERROR_LED, 255);
      analogWrite(MSG_LED, 0);
      delay(500);
      analogWrite(ERROR_LED, 0);
      return;
    }
  }
  blinkLED(MSG_LED, 100, 50, 2);
  Serial.println("Found a card!");
  Serial.print("[PN532]  IDm: ");
  nfc.PrintHex(idm, 8);
  Serial.print("[PN532]  PMm: ");
  nfc.PrintHex(pmm, 8);
  Serial.print("[PN532]  System Code: ");
  Serial.print(systemCodeResponse, HEX);
  Serial.println();

  memcpy(_prevIDm, idm, 8);
  _prevTime = millis();

  sendNotification(idm);

  // Wait 1 second before continuing
  Serial.println("Card access completed!\n");
  delay(1000);
}
