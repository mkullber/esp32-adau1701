#ifndef _ADAU1701_H_
#define _ADAU1701_H_

#include <Arduino.h>
#include <Wire.h>

#include "sigmastudio system files/teh_DSP_IC_1_PARAM.h"
#include "sigmastudio system files/teh_DSP_IC_1_REG.h"

class ADAU1701
{
public:
    ADAU1701();

    bool init(uint8_t i2cAddr = 0x68);
    bool isInitialized();
    bool connect();
    void disconnect();
    bool isConnected();
    String isConnectedJSON();
    bool checkConnection();
    bool readReg(uint16_t addr, uint8_t *buf, uint8_t len);
    bool safeloadWrite(uint8_t safeloadNum, uint16_t addr, uint32_t value);
    bool writeCoreRegister(uint16_t value);
    bool safeloadApply();

    bool setMasterVolume(uint32_t value);
    bool setSubLevel(uint32_t value);
    bool setMute(uint16_t addr, bool value);
    bool setInv(uint16_t addr, bool value);

    bool readValues();
    String valuesJSON();

private:
    uint8_t _i2cAddr;
    uint8_t _buf[32];
    bool _initialized;
    bool _connected;

    bool loadU32(uint16_t addr, uint32_t *value);
    bool loadMute(uint16_t addr, bool *value);
    bool loadInv(uint16_t addr, bool *value);
    bool _readValues();

    uint32_t _masterVolumeMain;
    uint32_t _masterVolumeSub;
    uint32_t _subLevel;
    bool _inputMuteADC;
    bool _inputMuteI2S;
    bool _mainMute;
    bool _subMute;
    uint32_t _mainEqBypass;
    uint32_t _subEqBypass;
    bool _mainInvL;
    bool _mainInvR;
    bool _subInv;
};

#endif
