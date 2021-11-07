#include <Arduino.h>
#include "util.h"

#include <HTTPClient.h>

#include "ADAU1701.h"
extern ADAU1701 adau;

extern char switchAddressValue[STRING_LEN];

void handleGetValues()
{
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", adau.valuesJSON());
}

bool tasmotaSwitch(String state, bool *switchStatus)
{
    bool status = false;

    String url = "http://" + String(switchAddressValue) + "/cm?cmnd=Power";
    if (state != NULL && state.length() > 0)
    {
        url += "%20" + state;
    }
    HTTPClient http;
    String result;
    if (http.begin(url))
    {
        int httpStatus = http.GET();
        status = httpStatus > 0;
        if (httpStatus > 0)
        {
            result = http.getString();
        }
    }
    http.end();

    if (status && switchStatus != NULL)
    {
        DynamicJsonDocument doc(32);
        DeserializationError error = deserializeJson(doc, result);
        if (error)
        {
            status = false;
        }
        else
        {
            if (strcmp(doc["POWER"], "ON") == 0)
            {
                *switchStatus = true;
            }
            else
            {
                *switchStatus = false;
            }
        }
    }

    return status;
}

void handleSwitch()
{
    String stateStr = String();
    if (server.hasArg("state"))
    {
        stateStr = server.arg("state");
    }

    bool state;
    DynamicJsonDocument doc(32);
    if (tasmotaSwitch(stateStr, &state))
    {
        doc["switch"] = state;
    }
    String resp;
    serializeJson(doc, resp);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", resp);
}

void handleConnection()
{
    String stateStr = String();
    if (server.hasArg("state"))
    {
        stateStr = server.arg("state");
        if (stateStr == "1")
        {
            if (!adau.isConnected())
            {
                if (!adau.init())
                {
                    debugPrintln("ADAU1701.init() fail");
                }
            }
        }
        else
        {
            adau.disconnect();
        }
    }

    DynamicJsonDocument doc(32);
    doc["connected"] = adau.isConnected();
    String resp;
    serializeJson(doc, resp);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", resp);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    bool status = false;
    DynamicJsonDocument doc(1024);
    DeserializationError error;

    switch (type)
    {
    case WStype_DISCONNECTED:
        debugPrintf("[%u] Disconnected!\n", num);
        break;
    case WStype_CONNECTED:
    {
        IPAddress ip = webSocket.remoteIP(num);
        debugPrintf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "WS connected");
    }
    break;
    case WStype_TEXT:
        debugPrintf("WS[%u] text: %s\n", num, payload);

        error = deserializeJson(doc, payload);
        if (!error)
        {
            status = true;
            if (doc.containsKey("masterVolume"))
            {
                debugPrintf("masterVolume: %08x\n", doc["masterVolume"].as<uint32_t>());
                status = adau.setMasterVolume(doc["masterVolume"].as<uint32_t>());
            }
            if (doc.containsKey("subLevel"))
            {
                debugPrintf("subLevel: %08x\n", doc["subLevel"].as<uint32_t>());
                status = adau.setSubLevel(doc["subLevel"].as<uint32_t>());
            }
            if (doc.containsKey("mute"))
            {
                debugPrintf("mute: %u\n", doc["mute"].as<bool>());
                status = adau.setMute(MOD_MAINMUTE_ALG0_MUTEONOFF_ADDR, doc["mute"].as<bool>());
            }
            if (doc.containsKey("subMute"))
            {
                debugPrintf("subMute: %u\n", doc["subMute"].as<bool>());
                status = adau.setMute(MOD_SUBMUTE_ALG0_MUTEONOFF_ADDR, doc["subMute"].as<bool>());
            }
        }

        if (status)
        {
            String resp = adau.valuesJSON();
            webSocket.sendTXT(num, resp);
        }
        else
        {
            webSocket.sendTXT(num, "FAIL");
        }

        // send data to all connected clients
        // webSocket.broadcastTXT("message here");
        break;
    case WStype_BIN:
        debugPrintf("WS[%u] binary (%u)\n", num, length);
        hexdump(payload, length);

        // send message to client
        // webSocket.sendBIN(num, payload, length);
        break;
    case WStype_PING:
        debugPrintf("WS[%u] ping (%u)\n", num, length);
        hexdump(payload, length);
        break;
    case WStype_PONG:
    case WStype_ERROR:
        debugPrintln("WStype_ERROR");
        break;
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
        break;
    }
}

void handleMinimalUpload()
{
    char temp[1500];

    snprintf(temp, 1500,
             "<!DOCTYPE html>\
    <html>\
      <head>\
        <title>ESP8266 Upload</title>\
        <meta charset=\"utf-8\">\
        <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\
        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
      </head>\
      <body>\
        <form action=\"/edit\" method=\"post\" enctype=\"multipart/form-data\">\
          <input type=\"file\" name=\"data\">\
          <input type=\"text\" name=\"path\" value=\"/\">\
          <button>Upload</button>\
         </form>\
      </body>\
    </html>");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", temp);
}

void i2cScan(bool send = false)
{
    byte error, address;
    int nDevices;
    char sendBuf[40];
    debugPrintln("Scanning...");
    nDevices = 0;
    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0)
        {
            snprintf(sendBuf, sizeof(sendBuf) - 1, "I2C device found at address 0x%02X\n", address);
            debugPrint(sendBuf);
            if (send)
            {
                server.sendContent(sendBuf);
            }
            nDevices++;
        }
        else if (error == 4)
        {
            snprintf(sendBuf, sizeof(sendBuf) - 1, "Unknow error at address 0x%02X\n", address);
            debugPrint(sendBuf);
            if (send)
            {
                server.sendContent(sendBuf);
            }
        }
    }
    if (nDevices == 0)
    {
        debugPrintln("No I2C devices found\n");
    }
    else
    {
        debugPrintln("done\n");
    }
}

void handle_i2cScan()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    debugPrintln("Starting I2C scan");
    server.send(200, "text/plain", "I2C scan\n");
    i2cScan(true);
    server.sendContent("");
}
