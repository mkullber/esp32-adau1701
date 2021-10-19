// curl -v -F "update=@.pio/build/esp32dev/firmware.bin" http://admin:password@192.168.8.187/firmware

#include <Arduino.h>

#include <IotWebConf.h>
#include <IotWebConfESP32HTTPUpdateServer.h>

const char thingName[] = "teh DSP";
const char wifiInitialApPassword[] = "password";
#define CONFIG_VERSION "dsp1"
#define CONFIG_PIN 4
#define STATUS_PIN 2

void wifiConnected();
bool isWifiConnected = false;
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);
void handleRoot();

void handleGetValues();
void handleFileList();

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;
WiFiClient net;

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
// IotWebConfParameter mqttServerParam = IotWebConfParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);

#include <SPIFFS.h>

#include <ArduinoJson.h>
DynamicJsonDocument jsonDoc(1024);

#include <WebSocketsServer.h>
WebSocketsServer webSocket = WebSocketsServer(81);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

#include <ArduinoOTA.h>

boolean needReset = false;

#include "request_handlers.h"
#include "spiffs_webserver.h"

//#include "APM2_PCUICtrl_IC_1.h"
#include "ADAU1701.h"
ADAU1701 adau;

void setup()
{
  Serial.begin(115200);

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
  }

  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  // iotWebConf.addParameter(&mqttServerParam);
  // iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  char updatePath[32];
  httpUpdater.setup(&server, updatePath);
  iotWebConf.setupUpdateServer(
      [](const char *updatePath)
      { httpUpdater.setup(&server, updatePath); },
      [](const char *userName, char *password)
      { httpUpdater.updateCredentials(userName, password); });

  iotWebConf.skipApStartup();

  // -- Initializing the configuration.
  boolean validConfig = iotWebConf.init();
  if (!validConfig)
  {
    // mqttServerValue[0] = '\0';
  }

  // -- Set up required URL handlers on the web server.
  // server.on("/", handleRoot);
  // server.serveStatic("/", SPIFFS, "/index.html");
  // server.serveStatic("/favicon.ico", SPIFFS, "/favicon.ico");
  server.on("/config", []
            { iotWebConf.handleConfig(); });
  server.onNotFound([]()
                    {
                      if (!handleFileRead(server.uri()))
                        iotWebConf.handleNotFound();
                    });

  server.on("/get_values", handleGetValues);

  server.on("/list", handleFileList);
  server.on("/edit", HTTP_PUT, handleFileCreate);
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  server.on(
      "/edit", HTTP_POST, []()
      {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", "");
      },
      handleFileUpload);
  server.on("/upload", handleMinimalUpload);

  server.on("/edit", HTTP_GET, [&]()
            {
              if (!handleFileRead(server.uri()))
                iotWebConf.handleNotFound();
            });

  ArduinoOTA
      .onStart([]()
               {
                 String type;
                 if (ArduinoOTA.getCommand() == U_FLASH)
                   type = "sketch";
                 else // U_SPIFFS
                   type = "filesystem";

                 // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                 Serial.println("Start updating " + type);
               })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
                 Serial.printf("Error[%u]: ", error);
                 if (error == OTA_AUTH_ERROR)
                   Serial.println("Auth Failed");
                 else if (error == OTA_BEGIN_ERROR)
                   Serial.println("Begin Failed");
                 else if (error == OTA_CONNECT_ERROR)
                   Serial.println("Connect Failed");
                 else if (error == OTA_RECEIVE_ERROR)
                   Serial.println("Receive Failed");
                 else if (error == OTA_END_ERROR)
                   Serial.println("End Failed");
               });

  if (!adau.init(0x68))
  {
    Serial.println("ADAU1701.init() fail");
  }
  // adau.i2cScan();
}

void loop()
{
  iotWebConf.doLoop();
  if (isWifiConnected)
  {
    webSocket.loop();
    ArduinoOTA.handle();
  }

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }

  // Serial.print("Configuring ADAU1701...");
  // default_download_IC_1();
  // Serial.println(" done!");

  // delay(5000);
}

void wifiConnected()
{
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  ArduinoOTA.begin();

  isWifiConnected = true;
}

void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>teh DSP</title></head><body>";
  s += "teh DSP<br/>>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void handleGetValues()
{
  server.sendHeader("Access-Control-Allow-Origin", "*");
  // jsonDoc["value1"] = "foo";
  // String valuesStr;
  // serializeJson(jsonDoc, valuesStr);
  // server.send(200, "application/json", valuesStr);
  if (!adau.loadValues())
    Serial.println("pååppa");
  server.send(200, "application/json", adau.valuesJSON());
}

void configSaved()
{
  Serial.println("Configuration was updated.");
  needReset = true;
}

bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
{
  Serial.println("Validating form.");
  boolean valid = true;

  // int l = webRequestWrapper->arg(stringParam.getId()).length();
  // if (l < 3)
  // {
  //   stringParam.errorMessage = "Please provide at least 3 characters for this test!";
  //   valid = false;
  // }

  return valid;
}

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16)
{
  const uint8_t *src = (const uint8_t *)mem;
  Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
  for (uint32_t i = 0; i < len; i++)
  {
    if (i % cols == 0)
    {
      Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
    }
    Serial.printf("%02X ", *src);
    src++;
  }
  Serial.printf("\n");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[%u] Disconnected!\n", num);
    break;
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

    // send message to client
    webSocket.sendTXT(num, "Connected");
  }
  break;
  case WStype_TEXT:
    Serial.printf("[%u] get Text: %s\n", num, payload);

    if (strncmp((const char *)payload, "slider: ", 8) == 0)
    {
      uint32_t value;
      if (sscanf((const char *)(payload + 8), "%u", &value) == 1)
      {
        uint32_t mapped = map(value, 0, 100, 0, 0x00800000);
        Serial.printf("slider value: %u -> %08x\n", value, mapped);
        adau.setMasterVolume(mapped);
      }
    }

    // send message to client
    webSocket.sendTXT(num, "OK");

    // send data to all connected clients
    // webSocket.broadcastTXT("message here");
    break;
  case WStype_BIN:
    Serial.printf("[%u] get binary length: %u\n", num, length);
    hexdump(payload, length);

    // send message to client
    // webSocket.sendBIN(num, payload, length);
    break;
  case WStype_PING:
    Serial.println("ws ping");
    break;
  case WStype_PONG:
    Serial.println("ws pong");
    Serial.printf("[%u] pong length: %u\n", num, length);
    hexdump(payload, length);
    break;
  case WStype_ERROR:
    Serial.println("ws error");
    break;
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    break;
  }
}

/*
  char buf[256] = {0};

  size_t n;
  uint8_t status;

  // mute
  uint8_t mute[2] = {0x00, 0x18};
  Wire.beginTransmission(ADAU1701_ADDR);
  n = Wire.write(mute, 2);
  status = Wire.endTransmission();
  sprintf(buf, "mute - len: %u, status: %u\n", n, status);
  Serial.print(buf);

  // program data

  // param

  // hw config
  uint8_t hw_conf[24] = {
      0x00, 0x18,       // dsp core (mute)
      0x08,             // reserved
      0x00, 0x00,       // serial output
      0x00,             // serial input
      0x44, 0xFF, 0x44, // mp pin config 0
      // mp5: gpio in debounce
      // mp4: gpio in debounce
      // mp3: aux adc
      // mp2: aux adc
      // mp1: gpio in debounce
      // mp0: gpio in debounce
      0xCC, 0xFF, 0x4C, // mp pin config 1
      // mp11: serial port inverted
      // mp10: serial port inverted
      // mp9: aux adc
      // mp8: aux adc
      // mp7: gpio in debounce
      // mp6: serial port inverted
      0x00, 0x00, // aux adc & power
      0x00, 0x00, // reserved
      0x80, 0x00, // aux adc en (enable)
      0x00, 0x00, // reserved
      0x00, 0x00, // osc pwr-dn
      0x00, 0x01  // dac setup (initialize)
  };

  // mute + clear registers
  uint8_t mute_cr[2] = {0x00, 0x1C};
  Wire.beginTransmission(ADAU1701_ADDR);
  n = Wire.write(mute_cr, 2);
  status = Wire.endTransmission();
  sprintf(buf, "mute_cr - len: %u, status: %u\n", n, status);
  Serial.print(buf);
  */
