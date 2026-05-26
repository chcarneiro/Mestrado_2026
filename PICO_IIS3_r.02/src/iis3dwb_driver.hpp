#pragma once
#include <Arduino.h>
#include <SPI.h>

class IIS3DWBDriver
{
public:
    IIS3DWBDriver() : _spiMode(SPI_MODE3) {}

    bool begin();
    bool dataReady();
    void readXYZraw(int16_t &x, int16_t &y, int16_t &z);
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);

private:
    uint8_t _spiMode;

    uint8_t readRegisterMode(uint8_t reg, uint8_t spiMode);
    void writeRegisterMode(uint8_t reg, uint8_t value, uint8_t spiMode);
    void readBurstMode(uint8_t startReg, uint8_t *dst, size_t len, uint8_t spiMode);
    void readBurst(uint8_t startReg, uint8_t *dst, size_t len);
};

extern IIS3DWBDriver sensor;