#ifndef _ADAU1701_H_
#define _ADAU1701_H_

#include <Arduino.h>
#include <Wire.h>

class ADAU1701
{
public:
    ADAU1701();

    bool init(uint8_t i2cAddr = 0x68);
    bool connect();
    void disconnect();
    bool isConnected();
    String isConnectedJSON();
    bool checkConnection();
    bool readReg(uint16_t addr, uint8_t *buf, uint8_t len);
    bool safeloadWrite(uint8_t safeloadNum, uint16_t addr, uint32_t value);
    bool safeloadApply();

    bool setMasterVolume(uint32_t value);

    bool readValues();
    String valuesJSON();

private:
    uint8_t _i2cAddr;
    uint8_t _buf[32];
    bool _initialized;
    bool _connected;

    bool loadU32(uint16_t addr, uint32_t *value);
    bool loadBool(uint16_t addr, bool *value);

    uint32_t _masterVolumeMain;
    uint32_t _masterVolumeSub;
    uint32_t _subVolume;
    bool _inputMuteADC;
    bool _inputMuteI2S;
    bool _mainMute;
    bool _subMute;
    bool _mainBypass;
    bool _subBypass;
    bool _mainInvL;
    bool _mainInvR;
    /*
    uint32_t _subVolume2;
    uint32_t _mainVolumeDAC1;
    uint32_t _mainVolumeDAC2;
    uint32_t _mainVolumeI2S1;
    uint32_t _mainVolumeI2S2;
    */
};

void i2cScan();

#endif
