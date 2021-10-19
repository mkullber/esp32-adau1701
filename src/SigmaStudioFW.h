#ifndef __SIGMASTUDIO_FW_H__
#define __SIGMASTUDIO_FW_H__

#include <Arduino.h>
#include <Wire.h>

#define ADI_REG_TYPE uint8_t

#define SIGMA_WRITE_REGISTER_BLOCK i2cWriteData

void i2cWriteData(uint8_t devAddr, uint16_t regAddr, uint16_t len, uint8_t *data)
{
    char b[128];
    uint16_t total = 0;
    uint8_t partLen = 32;
    for (uint16_t i = 0; i < len; i += partLen)
    {
        uint16_t n;
        uint8_t status;
        Wire.beginTransmission(devAddr >> 1);
        n = Wire.write((regAddr + i) >> 8 & 0xFF);
        n = Wire.write((regAddr + i) & 0xFF);
        if (partLen > len - i)
        {
            partLen = len - i;
        }
        n = Wire.write(data + i, partLen);
        total += n;
        if (n < partLen)
        {
            sprintf(b, "n=%u len=%u i=%u partLen=%u total=%u\n", n, len, i, partLen, total);
            Serial.print(b);
            Serial.println("fail");
            break;
        }
        status = Wire.endTransmission();
        if (status != 0)
        {
            sprintf(b, "n=%u len=%u i=%u partLen=%u total=%u\n", n, len, i, partLen, total);
            Serial.print(b);
            Serial.print("status: ");
            Serial.println(status);
            break;
        }
    }
}

void i2cWriteDataCaca(uint8_t devAddr, uint16_t regAddr, uint16_t len, uint8_t *data)
{
    uint16_t n;
    uint8_t status;
    Wire.beginTransmission(devAddr >> 1);
    n = Wire.write(regAddr >> 8 & 0xFF);
    Serial.print("len r1: ");
    Serial.println(n);
    n = Wire.write(regAddr & 0xFF);
    Serial.print("len r2: ");
    Serial.println(n);
    uint16_t total = 0;
    uint8_t partLen = 32;
    for (uint16_t i = 0; i < len; i += partLen)
    {
        char b[128];
        if (partLen > len - i)
        {
            partLen = len - i;
        }
        n = Wire.write(data + i, partLen);
        total += n;
        sprintf(b, "n=%u len=%u i=%u partLen=%u total=%u\n", n, len, i, partLen, total);
        Serial.print(b);
        if (n < partLen)
        {
            Serial.println("fail");
            break;
        }
        // Serial.print("len data: ");
        // Serial.println(n);
    }
    Serial.print("len data total: ");
    Serial.println(total);
    status = Wire.endTransmission();
    Serial.print("status: ");
    Serial.println(status);
}

#endif
