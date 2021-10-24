#include <Arduino.h>
#include "util.h"

#include <HTTPClient.h>

#include "ADAU1701.h"
extern ADAU1701 adau;

extern char switchAddressValue[STRING_LEN];

void handleGetValues()
{
    server.sendHeader("Access-Control-Allow-Origin", "*");
    // jsonDoc["value1"] = "foo";
    // String valuesStr;
    // serializeJson(jsonDoc, valuesStr);
    // server.send(200, "application/json", valuesStr);
    if (!adau.readValues())
        Serial.println("adau.loadValues() fail");
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
            adau.connect();
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
        webSocket.sendTXT(num, "WS connected");
    }
    break;
    case WStype_TEXT:
        Serial.printf("WS[%u] text: %s\n", num, payload);

        if (strncmp((const char *)payload, "masterVolume: ", 14) == 0)
        {
            uint32_t value;
            if (sscanf((const char *)(payload + 14), "%u", &value) == 1)
            {
                Serial.printf("slider value: %u -> %08x\n", value, value);
                status = adau.setMasterVolume(value);
            }
        }

        if (strncmp((const char *)payload, "heartbeat", 9) != 0)
        {
            webSocket.sendTXT(num, status ? "OK" : "FAIL");
        }

        // send data to all connected clients
        // webSocket.broadcastTXT("message here");
        break;
    case WStype_BIN:
        Serial.printf("WS[%u] binary (%u)\n", num, length);
        hexdump(payload, length);

        // send message to client
        // webSocket.sendBIN(num, payload, length);
        break;
    case WStype_PING:
        Serial.printf("WS[%u] ping (%u)\n", num, length);
        hexdump(payload, length);
        break;
    case WStype_PONG:
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
