/**************************************************************************/
/*!
    This example will attempt to connect to an FeliCa
    card or tag and retrieve some basic information about it
    that can be used to determine what type of card it is.

    Note that you need the baud rate to be 115200 because we need to print
    out the data and read from the card at the same time!

    To enable debug message, define DEBUG in PN532/PN532_debug.h

 */
/**************************************************************************/
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

#define DEBUG_ESP_WIFI

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
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata)
  {
    Serial.print("Didn't find PN53x board");
    isError = true;
    return false;
  }
  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  // Set PN532 param
  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();
  memset(_prevIDm, 0, 8);
  return true;
}

bool connectWifi() {
  Serial.println();
  Serial.println("Connecting to WiFi AP");

  // wait for WiFi connection
  WiFiMulti.addAP(MY_SSID1, MY_PASSWORD1);
  WiFiMulti.addAP(MY_SSID2, MY_PASSWORD2);

  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("   IP address: "); Serial.println(WiFi.localIP());
  return true;
}


/*
void requestURI2(uint8_t *idm) {
  Serial.println("Sending card data to the server...");

  aJsonObject *root, *msg;
  root=aJson.createObject();
  aJson.addStringToObject(root,"to", "/topics/news");
  aJson.addItemToObject(root,"notification", msg = aJson.createObject());
  char buf[40];
  sprintf(buf, "Card: %02X%02X%02X%02X%02X%02X%02X%02X",
    idm[0], idm[1], idm[2], idm[3], idm[4], idm[5], idm[6], idm[7]);
  aJson.addStringToObject(msg, "text", buf);
  aJson.addStringToObject(msg, "sound", "default");
  char *payload=aJson.print(root);
  char payloadLen[5];
  sprintf(payloadLen, "%d", strlen(payload));

  const char *host = "fcm.googleapis.com";
  WiFiClient client;
  if (!client.connect(host, 80)) {
    Serial.println("Connection failed.");
    return;
  }
  Serial.println("Connected to server.");

  client.print("POST /fcm/send HTTP/1.1\n");
  client.print("Host: fcm.googleapis.com\n");
  client.print("Connection: close\n");
  client.print("Authorization: ");
  client.print(FIREBASE_KEY);
  client.print("\nContent-Type: application/json\n");
  client.print("Content-Length: ");
  client.print(payloadLen);
  client.print("\n\n");
  client.print(payload);

  // long interval = 4000;
  // unsigned long currentMillis = millis(), previousMillis = millis();
  // while(!client.available()){
  //   if( (currentMillis - previousMillis) > interval ){
  //     Serial.println("Timeout");
  //     blinkLED(ERROR_LED, 300, 200, 3);
  //     client.stop();
  //     return;
  //   }
  //   currentMillis = millis();
  // }

  long interval = 4000;
  unsigned long currentMillis = millis(), previousMillis = millis();

  Serial.println("Server response:");
  while (client.connected()) {
    if ( client.available() ) {
      char str=client.read();
      Serial.print(str);
    }
    if( (currentMillis - previousMillis) > interval ){
      Serial.println("Timeout");
      blinkLED(ERROR_LED, 300, 200, 3);
      client.stop();
      return;
    }
    currentMillis = millis();
  }
  client.stop();
}
*/
void requestURI2(uint8_t *idm) {
  Serial.println("Sending card data to the server...");

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
  char payloadLen[5];
  sprintf(payloadLen, "%d", strlen(payload));

//  const char *host =       "fcm.googleapis.com";
  const char *url = "http://fcm.googleapis.com/fcm/send";
  HTTPClient http;
  http.begin(url);
  http.addHeader("Authorization", FIREBASE_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
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
  blinkLED(MSG_LED, 100, 100, 4); // wait Serial to be stabilized
  Serial.println("Hello!");

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
  Serial.print("Waiting for an FeliCa card...  ");
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
    Serial.print("Card access error: code "); Serial.println(ret);
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
  Serial.print("  IDm: ");
  nfc.PrintHex(idm, 8);
  Serial.print("  PMm: ");
  nfc.PrintHex(pmm, 8);
  Serial.print("  System Code: ");
  Serial.print(systemCodeResponse, HEX);
  Serial.print("\n");

  memcpy(_prevIDm, idm, 8);
  _prevTime = millis();

  requestURI2(idm);

  // Wait 1 second before continuing
  Serial.println("Card access completed!\n");
  delay(1000);
}
