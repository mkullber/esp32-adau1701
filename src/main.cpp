// curl -v -F "update=@.pio/build/esp32dev/firmware.bin" http://admin:password@192.168.8.187/firmware

#include <Arduino.h>

#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfESP32HTTPUpdateServer.h>

const char thingName[] = "teh DSP";
const char wifiInitialApPassword[] = "password";
#define CONFIG_VERSION "dsp2"
#define CONFIG_PIN 4
#define STATUS_PIN 2

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;
WiFiClient net;

#define STRING_LEN 64
char switchAddressValue[STRING_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfTextParameter switchAddressParam = IotWebConfTextParameter("Switch address", "switchAddress", switchAddressValue, STRING_LEN);

void wifiConnected();
bool isWifiConnected = false;
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);
void handleRoot();

void handleGetValues();
void handleFileList();

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

#include "ADAU1701.h"
ADAU1701 adau;

char serial2Buf[128] = {0};
uint8_t serial2BufPtr = 0;
void handleSerial2Line();

void setup()
{
    Serial.begin(115200);
    Serial2.begin(115200);

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
    }

    iotWebConf.setStatusPin(STATUS_PIN);
    iotWebConf.setConfigPin(CONFIG_PIN);
    iotWebConf.addSystemParameter(&switchAddressParam);
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
    server.serveStatic("/", SPIFFS, "/index.html");
    server.on("/config", []
              { iotWebConf.handleConfig(); });
    server.onNotFound([]()
                      {
                          if (!handleFileRead(server.uri()))
                              iotWebConf.handleNotFound();
                      });

    server.on("/i2c_scan", handle_i2cScan);

    server.on("/connection", handleConnection);
    server.on("/get_values", handleGetValues);
    server.on("/switch", handleSwitch);

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

    if (!adau.init())
    {
        delay(500);
        if (!adau.connect())
        {
            Serial.println("ADAU1701 init/connect fail");
        }
    }
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

    while (Serial2.available())
    {
        char ch;
        int nBytes;
        nBytes = Serial2.read(&ch, 1);
        if (nBytes > 0)
        {
            if (ch == '\n')
            {
                serial2Buf[serial2BufPtr] = '\0';
                handleSerial2Line();
                serial2BufPtr = 0;
            }
            else if (ch != '\r')
            {
                serial2Buf[serial2BufPtr++] = ch;
                if (serial2BufPtr >= sizeof(serial2Buf))
                {
                    // buffer overflow
                    serial2BufPtr = 0;
                }
            }
        }
    }
}

void wifiConnected()
{
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    ArduinoOTA.begin();

    isWifiConnected = true;
}

void handleSerial2Line()
{
    char buf[32];
    if (strncmp(serial2Buf, "VOL = ", 6) == 0)
    {
        int16_t volume;
        if (sscanf(serial2Buf + 6, "%hi", &volume) == 1)
        {
            // macOS volume adjustment: sqrt( linear 0..1 )
            uint32_t volumePositive = volume + 24576;
            uint32_t volumeScaled = volumePositive * volumePositive / 72; // 24576^2 / 0x00800000 = 72
            Serial.printf("Volume: %i -> %u (0x%08X)\n", volume, volumeScaled, volumeScaled);
            adau.setMasterVolume(volumeScaled);
            if (isWifiConnected)
            {
                snprintf(buf, sizeof(buf) - 1, "{\"volume\": %u}", volumeScaled);
                webSocket.broadcastTXT(buf);
            }
        }
    }
    else if (strncmp(serial2Buf, "MUTE = ", 7) == 0)
    {
        uint8_t muteNum;
        boolean mute;
        if (sscanf(serial2Buf + 7, "%hhu", &muteNum) == 1)
        {
            mute = muteNum == 1;
            Serial.printf("Mute: %i\n", mute);
            adau.setMute(MOD_MAINMUTE_ALG0_MUTEONOFF_ADDR, mute);
            adau.setMute(MOD_SUBMUTE_ALG0_MUTEONOFF_ADDR, mute);
            if (isWifiConnected)
            {
                snprintf(buf, sizeof(buf) - 1, "{\"mute\": %u}", mute);
                webSocket.broadcastTXT(buf);
            }
        }
    }
    else if (strncmp(serial2Buf, "PLAY = ", 7) == 0)
    {
        uint8_t playNum;
        boolean play;
        if (sscanf(serial2Buf + 7, "%hhu", &playNum) == 1)
        {
            play = playNum == 1;
            Serial.printf("Play: %i\n", play);
            if (isWifiConnected)
            {
                snprintf(buf, sizeof(buf) - 1, "{\"play\": %u}", play);
                webSocket.broadcastTXT(buf);
            }
            // TODO: save play timestamp, turn switch on/off (off delay)
        }
    }
    else
    {
        Serial.print("Serial2: ");
        Serial.println(serial2Buf);
    }
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
