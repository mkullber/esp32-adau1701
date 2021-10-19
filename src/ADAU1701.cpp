#include "ADAU1701.h"

#include "APM2_PCUICtrl_IC_1_PARAM.h"
#include "APM2_PCUICtrl_IC_1_REG.h"

#include <ArduinoJson.h>
DynamicJsonDocument _jsonDoc(4096);

#define ADAU1701_DEBUG
#ifdef ADAU1701_DEBUG
#define ADAU1701_PRINTF(...) Serial.printf(__VA_ARGS__)
#define ADAU1701_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define ADAU1701_PRINTF(...)
#define ADAU1701_PRINTLN(...)
#endif

ADAU1701::ADAU1701()
{
}

bool ADAU1701::init(uint8_t i2cAddr)
{
    _i2cAddr = i2cAddr >> 1;
    Wire.begin();
    Wire.setClock(400000);
    //pinMode(21, INPUT_PULLUP);
    //pinMode(22, INPUT_PULLUP);

    return checkConnection();
}

bool ADAU1701::checkConnection()
{
    Wire.beginTransmission(_i2cAddr);
    if (Wire.endTransmission() == 0)
    {
        return true;
    }
    return false;
}

bool ADAU1701::loadValues()
{
    if (!loadU32(MOD_MASTERVOLUME_ALG0_GAIN1940ALGNS1_ADDR, &_masterVolume1))
        return false;
    if (!loadU32(MOD_MASTERVOLUME_ALG1_GAIN1940ALGNS2_ADDR, &_masterVolume2))
        return false;
    if (!loadU32(MOD_SUBVOLUME_ALG0_GAIN1940ALGNS3_ADDR, &_subVolume1))
        return false;
    if (!loadU32(MOD_SUBVOLUME_ALG1_GAIN1940ALGNS4_ADDR, &_subVolume2))
        return false;
    if (!loadU32(MOD_MAINVOLUMEDAC_ALG0_GAIN1940ALGNS17_ADDR, &_mainVolumeDAC1))
        return false;
    if (!loadU32(MOD_MAINVOLUMEDAC_ALG1_GAIN1940ALGNS18_ADDR, &_mainVolumeDAC2))
        return false;
    if (!loadU32(MOD_MAINVOLUMEI2S_ALG0_GAIN1940ALGNS15_ADDR, &_mainVolumeI2S1))
        return false;
    if (!loadU32(MOD_MAINVOLUMEI2S_ALG1_GAIN1940ALGNS16_ADDR, &_mainVolumeI2S2))
        return false;
    return true;
}

String ADAU1701::valuesJSON()
{
    _jsonDoc["masterVolume1"] = _masterVolume1;
    _jsonDoc["masterVolume2"] = _masterVolume2;
    _jsonDoc["subVolume1"] = _subVolume1;
    _jsonDoc["subVolume2"] = _subVolume2;
    _jsonDoc["mainVolumeDAC1"] = _mainVolumeDAC1;
    _jsonDoc["mainVolumeDAC2"] = _mainVolumeDAC2;
    _jsonDoc["mainVolumeI2S1"] = _mainVolumeI2S1;
    _jsonDoc["mainVolumeI2S2"] = _mainVolumeI2S2;
    String valuesStr;
    serializeJson(_jsonDoc, valuesStr);
    ADAU1701_PRINTLN(valuesStr);
    return valuesStr;
}

bool ADAU1701::setMasterVolume(uint32_t value)
{
    safeloadWrite(0, MOD_MASTERVOLUME_ALG0_GAIN1940ALGNS1_ADDR, value);
    safeloadWrite(1, MOD_MASTERVOLUME_ALG1_GAIN1940ALGNS2_ADDR, value);
    safeloadApply();
    return false;
}

bool ADAU1701::safeloadWrite(uint8_t safeloadNum, uint16_t addr, uint32_t value)
{
    size_t nBytes;
    uint8_t status;
    // data
    Wire.beginTransmission(_i2cAddr);
    _buf[0] = 0x08;
    _buf[1] = 0x10 + safeloadNum;
    _buf[2] = 0x00;
    _buf[3] = (value >> 24) & 0xFF;
    _buf[4] = (value >> 16) & 0xFF;
    _buf[5] = (value >> 8) & 0xFF;
    _buf[6] = value & 0xFF;
    nBytes = Wire.write(_buf, 7);
    if (nBytes != 7)
    {
        ADAU1701_PRINTF("write(): %u\n", nBytes);
        return false;
    }
    status = Wire.endTransmission();
    if (status != 0)
    {
        ADAU1701_PRINTF("endTr2(): %u\n", status);
        return false;
    }
    // address
    Wire.beginTransmission(_i2cAddr);
    _buf[0] = 0x08;
    _buf[1] = 0x15 + safeloadNum;
    _buf[2] = (addr >> 8) & 0xFF;
    _buf[3] = addr & 0xFF;
    nBytes = Wire.write(_buf, 4);
    if (nBytes != 4)
    {
        ADAU1701_PRINTF("write(): %u\n", nBytes);
        return false;
    }
    status = Wire.endTransmission();
    if (status != 0)
    {
        ADAU1701_PRINTF("endTr2(): %u\n", status);
        return false;
    }
    return true;
}

bool ADAU1701::safeloadApply()
{
    size_t nBytes;
    uint8_t status;
    Wire.beginTransmission(_i2cAddr);
    _buf[0] = (REG_COREREGISTER_IC_1_ADDR >> 8) & 0xFF;
    _buf[1] = REG_COREREGISTER_IC_1_ADDR & 0xFF;
    _buf[2] = 0x00;
    _buf[3] = R13_REGISTER_ZERO_IC_1_MASK | R13_MUTE_DAC_IC_1_MASK | R13_MUTE_ADC_IC_1_MASK | R13_SAFELOAD_IC_1_MASK;
    nBytes = Wire.write(_buf, 4);
    if (nBytes != 4)
    {
        ADAU1701_PRINTF("write(): %u\n", nBytes);
        return false;
    }
    status = Wire.endTransmission();
    if (status != 0)
    {
        ADAU1701_PRINTF("endTr2(): %u\n", status);
        return false;
    }
    return true;
}

bool ADAU1701::readReg(uint16_t addr, uint8_t *buf, uint8_t len)
{
    size_t nBytes;
    uint8_t status;
    Wire.beginTransmission(_i2cAddr);
    uint8_t addrBuf[2];
    addrBuf[0] = (addr >> 8) & 0xFF;
    addrBuf[1] = addr & 0xFF;
    nBytes = Wire.write(addrBuf, 2);
    if (nBytes != 2)
    {
        ADAU1701_PRINTF("write(): %u\n", nBytes);
        return false;
    }
    status = Wire.endTransmission(false);
    if (status != 0)
    {
        ADAU1701_PRINTF("endTr1(): %u\n", status);
        return false;
    }

    Wire.beginTransmission(_i2cAddr);
    nBytes = Wire.requestFrom(_i2cAddr, len);
    if (nBytes != len)
    {
        ADAU1701_PRINTF("reqFrom(): %u\n", nBytes);
        return false;
    }
    nBytes = Wire.readBytes(buf, len);
    if (nBytes != len)
    {
        ADAU1701_PRINTF("readBytes(): %u\n", nBytes);
        return false;
    }
    status = Wire.endTransmission();
    if (status != 0)
    {
        ADAU1701_PRINTF("endTr2(): %u\n", status);
        return false;
    }
    return true;
}

bool ADAU1701::loadU32(uint16_t addr, uint32_t *value)
{
    if (!readReg(addr, _buf, 4))
    {
        return false;
    }
    *value = (_buf[0] << 24) | (_buf[1] << 16) | (_buf[2] << 8) | _buf[3];
    return true;
}

void ADAU1701::i2cScan()
{
    byte error, address;
    int nDevices;
    Serial.println("Scanning...");
    nDevices = 0;
    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0)
        {
            Serial.print("I2C device found at address 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.println(address, HEX);
            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("Unknow error at address 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0)
    {
        Serial.println("No I2C devices found\n");
    }
    else
    {
        Serial.println("done\n");
    }
}
