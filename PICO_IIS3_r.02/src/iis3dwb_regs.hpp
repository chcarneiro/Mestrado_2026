#pragma once
#include <Arduino.h>

static constexpr uint8_t REG_WHO_AM_I = 0x0F;
static constexpr uint8_t REG_CTRL1_XL = 0x10;   // Definido o valor 0xA0 para ODR=1.66 kHz e FS=±2 g
static constexpr uint8_t REG_CTRL3_C = 0x12;    // Definido o valor 0x44 para BDU=1 e IF_INC=1
static constexpr uint8_t REG_CTRL6_C = 0x15;    // Definido sem modo normal (0x00) e sem auto-reset de latência (0x00)
static constexpr uint8_t REG_CTRL8_XL   = 0x17;
static constexpr uint8_t REG_STATUS_REG = 0x1E;
static constexpr uint8_t REG_OUTX_L_XL = 0x28;

static constexpr uint8_t WHO_AM_I_VALUE = 0x7B;
static constexpr uint8_t STATUS_XLDA = 0x01;
static constexpr uint8_t IF_INC = 0x04;
static constexpr uint8_t BDU = 0x40;