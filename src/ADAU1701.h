#ifndef _ADAU1701_H_
#define _ADAU1701_H_

#include <Arduino.h>
#include <Wire.h>

class ADAU1701
{
public:
    ADAU1701();

    bool init(uint8_t i2cAddr = 0x68);
    bool checkConnection();
    bool readReg(uint16_t addr, uint8_t *buf, uint8_t len);
    bool safeloadWrite(uint8_t safeloadNum, uint16_t addr, uint32_t value);
    bool safeloadApply();

    bool setMasterVolume(uint32_t value);

    bool loadValues();
    String valuesJSON();

    void i2cScan();

private:
    uint8_t _i2cAddr;
    uint8_t _buf[32];

    bool loadU32(uint16_t addr, uint32_t *value);

    uint32_t _masterVolume1;
    uint32_t _masterVolume2;
    uint32_t _subVolume1;
    uint32_t _subVolume2;
    uint32_t _mainVolumeDAC1;
    uint32_t _mainVolumeDAC2;
    uint32_t _mainVolumeI2S1;
    uint32_t _mainVolumeI2S2;
};

#endif
