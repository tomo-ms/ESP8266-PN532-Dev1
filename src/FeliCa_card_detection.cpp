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
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUDP.h>
#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>
#include <PN532_debug.h>

#define MSG_LED         16
#define ERROR_LED       4
#define PN532_SS        5


// SCK->GOPI14(D5),  MISO->GPIO12(D6), MOSI->GPIO13(D7), SS->GPIO5(D1)
PN532_SPI pn532spi(SPI, PN532_SS);
PN532 nfc(pn532spi);
uint8_t        _prevIDm[8];
unsigned long  _prevTime;

static WiFiUDP wifiUdp;
const char* ssid     = "my_ssid";
const char* password = "my_password";

bool isError = false;

void connectWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if ( !WiFi.isConnected() ) {
    isError = true;
    return;
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  wifiUdp.begin(7000);
}

void initPN532() {
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata)
  {
    Serial.print("Didn't find PN53x board");
    isError = true;
    return;
  }
  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  // Set PN532 param
  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();
  memset(_prevIDm, 0, 8);
}

void requestURI() {
  HTTPClient http;
  http.begin("http://192.168.179.2:7000/");
  int httpCode = http.GET();
  if(httpCode) {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      if(httpCode == 200) {
          String payload = http.getString();
          Serial.println(payload);
      }
  } else {
      Serial.printf("[HTTP] GET... failed, error: %d\n", httpCode);
  }

  http.end();
  delay(5000);
}

void setup(void)
{
//  delay (1000);
  Serial.begin(115200);
  delay (1000); // wait Serial to be stabilized
  Serial.println("Hello!");

  initPN532();
  connectWifi();

  pinMode(ERROR_LED, OUTPUT);
  digitalWrite(ERROR_LED, LOW);
  pinMode(MSG_LED, OUTPUT);
  digitalWrite(MSG_LED, HIGH);

}


void loop(void)
{
  if (isError) {
    while(1) {
      for(int i=0; i<3; i++) {
        digitalWrite(ERROR_LED, HIGH);
        delay(200);
        digitalWrite(ERROR_LED, LOW);
        delay(200);
      }
      delay(1000);
    }
  }
  uint8_t ret;
  uint16_t systemCode = 0xFFFF;
  uint8_t requestCode = 0x01;       // System Code request
  uint8_t idm[8];
  uint8_t pmm[8];
  uint16_t systemCodeResponse;

  // Wait for an FeliCa type cards.
  // When one is found, some basic information such as IDm, PMm, and System Code are retrieved.
  Serial.print("Waiting for an FeliCa card...  ");
  analogWrite(MSG_LED, 64);
  ret = nfc.felica_Polling(systemCode, requestCode, idm, pmm, &systemCodeResponse, 5000);

  if (ret != 1)
  {
    Serial.println("Could not find a card");
    delay(500);
    return;
  }

  if ( memcmp(idm, _prevIDm, 8) == 0 ) {
    if ( (millis() - _prevTime) < 2000 ) {
        Serial.println("Same card");
      delay(500);
      return;
    }
  }
  for(int i=0; i<2; i++) {
    analogWrite(MSG_LED, 255);
    delay(150);
    analogWrite(MSG_LED, 0);
    delay(150);
  }
  Serial.println("Found a card!");
  Serial.print("  IDm: ");
  nfc.PrintHex(idm, 8);
  Serial.print("  PMm: ");
  nfc.PrintHex(pmm, 8);
  Serial.print("  System Code: ");
  Serial.print(systemCodeResponse, HEX);
  Serial.print("\n");

  wifiUdp.beginPacket("192.168.179.255", 9000);
  wifiUdp.write("Found card -> IDm:");
  for( int i=0; i<8; i++) {
    wifiUdp.printf(" %02x", idm[i]);
  }
  wifiUdp.write("\r\n");
  wifiUdp.endPacket();
  Serial.println("Send message");

  memcpy(_prevIDm, idm, 8);
  _prevTime = millis();

  // Wait 1 second before continuing
  Serial.println("Card access completed!\n");
  delay(1000);
}
