#include "ADAU1701.h"

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
    delay(100);

    _initialized = true;
    return connect();
}

bool ADAU1701::connect()
{
    if (!_initialized)
    {
        return false;
    }
    _connected = checkConnection();
    return _connected;
}

void ADAU1701::disconnect()
{
    _connected = false;
}

bool ADAU1701::isConnected()
{
    return _connected;
}

String ADAU1701::isConnectedJSON()
{
    _jsonDoc["connected"] = isConnected();
    String valuesStr;
    serializeJson(_jsonDoc, valuesStr);
    ADAU1701_PRINTLN(valuesStr);
    return valuesStr;
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

bool ADAU1701::readValues()
{
    if (!_connected)
    {
        return false;
    }
    if (!loadU32(MOD_MASTERVOLUMEMAIN_ALG0_TARGET_ADDR, &_masterVolumeMain))
        return false;
    if (!loadU32(MOD_MASTERVOLUMESUB_ALG0_TARGET_ADDR, &_masterVolumeSub))
        return false;
    if (!loadU32(MOD_SUBLEVEL_ALG0_TARGET_ADDR, &_subLevel))
        return false;
    if (!loadMute(MOD_INPUTMUTEADC_ALG0_MUTEONOFF_ADDR, &_inputMuteADC))
        return false;
    if (!loadMute(MOD_INPUTMUTEI2S_ALG0_MUTEONOFF_ADDR, &_inputMuteI2S))
        return false;
    if (!loadMute(MOD_MAINMUTE_ALG0_MUTEONOFF_ADDR, &_mainMute))
        return false;
    if (!loadMute(MOD_SUBMUTE_ALG0_MUTEONOFF_ADDR, &_subMute))
        return false;
    if (!loadU32(MOD_MAINEQBYPASS_STEREOSWSLEW_ADDR, &_mainEqBypass))
        return false;
    if (!loadU32(MOD_SUBEQBYPASS_MONOSWSLEW_ADDR, &_subEqBypass))
        return false;
    if (!loadInv(MOD_MAININVL_EQ1940INVERT1GAIN_ADDR, &_mainInvL))
        return false;
    if (!loadInv(MOD_MAININVR_EQ1940INVERT2GAIN_ADDR, &_mainInvR))
        return false;
    if (!loadInv(MOD_SUBINV_EQ1940INVERT3GAIN_ADDR, &_subInv))
        return false;
    return true;
}

String ADAU1701::valuesJSON()
{
    _jsonDoc["connected"] = _connected;
    if (_connected)
    {
        _jsonDoc["masterVolumeMain"] = _masterVolumeMain;
        _jsonDoc["masterVolumeSub"] = _masterVolumeSub;
        _jsonDoc["subLevel"] = _subLevel;
        _jsonDoc["inputMuteADC"] = _inputMuteADC;
        _jsonDoc["inputMuteI2S"] = _inputMuteI2S;
        _jsonDoc["mainMute"] = _mainMute;
        _jsonDoc["subMute"] = _subMute;
        _jsonDoc["mainEqBypass"] = _mainEqBypass;
        _jsonDoc["subEqBypass"] = _subEqBypass;
        _jsonDoc["mainInvL"] = _mainInvL;
        _jsonDoc["mainInvR"] = _mainInvR;
        _jsonDoc["subInv"] = _subInv;
    }
    String valuesStr;
    serializeJson(_jsonDoc, valuesStr);
    ADAU1701_PRINTLN(valuesStr);
    return valuesStr;
}

bool ADAU1701::setMasterVolume(uint32_t value)
{
    if (!_connected)
    {
        return false;
    }
    if (safeloadWrite(0, MOD_MASTERVOLUMEMAIN_ALG0_TARGET_ADDR, value))
    {
        if (safeloadWrite(1, MOD_MASTERVOLUMESUB_ALG0_TARGET_ADDR, value))
        {
            if (safeloadApply())
            {
                return true;
            }
        }
    }
    return false;
}

bool ADAU1701::setSubLevel(uint32_t value)
{
    if (!_connected)
    {
        return false;
    }
    if (safeloadWrite(0, MOD_SUBLEVEL_ALG0_TARGET_ADDR, value))
    {
        if (safeloadApply())
        {
            return true;
        }
    }
    return false;
}

bool ADAU1701::setMute(uint16_t addr, bool value)
{
    if (!_connected)
    {
        return false;
    }
    uint32_t muteU32 = 0x00800000;
    if (value)
    {
        muteU32 = 0x00000000;
    }
    if (safeloadWrite(0, addr, muteU32))
    {

        if (safeloadApply())
        {
            return true;
        }
    }
    return false;
}

bool ADAU1701::setInv(uint16_t addr, bool value)
{
    if (!_connected)
    {
        return false;
    }
    uint32_t invU32 = 0x00800000;
    if (value)
    {
        invU32 = 0x0F800000;
    }
    if (safeloadWrite(0, addr, invU32))
    {

        if (safeloadApply())
        {
            return true;
        }
    }
    return false;
}

bool ADAU1701::safeloadWrite(uint8_t safeloadNum, uint16_t addr, uint32_t value)
{
    if (!_connected)
    {
        return false;
    }
    size_t nBytes;
    uint8_t status;
    // data
    Wire.beginTransmission(_i2cAddr);
    _buf[0] = 0x08;
    _buf[1] = 0x10 + safeloadNum;
    _buf[2] = 0x00; // TODO: writable, get from value somehow
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
    if (!_connected)
    {
        return false;
    }
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
    if (!_connected)
    {
        return false;
    }
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

bool ADAU1701::loadMute(uint16_t addr, bool *value)
{
    // 0x00800000 = false
    // 0x00000000 = true
    if (!readReg(addr, _buf, 4))
    {
        return false;
    }
    uint32_t regValue = (_buf[0] << 24) | (_buf[1] << 16) | (_buf[2] << 8) | _buf[3];
    *value = !(regValue && 0x00800000);
    return true;
}

bool ADAU1701::loadInv(uint16_t addr, bool *value)
{
    // 0x00800000 = false
    // 0x0F800000 = true
    if (!readReg(addr, _buf, 4))
    {
        return false;
    }
    uint32_t regValue = (_buf[0] << 24) | (_buf[1] << 16) | (_buf[2] << 8) | _buf[3];
    *value = (regValue & 0x0F000000) > 0;
    return true;
}

void i2cScan()
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
