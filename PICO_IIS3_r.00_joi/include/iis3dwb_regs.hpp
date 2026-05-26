#pragma once
#include <Arduino.h>

static constexpr uint8_t REG_WHO_AM_I = 0x0F;
static constexpr uint8_t REG_CTRL1_XL = 0x10;
static constexpr uint8_t REG_CTRL3_C = 0x12;
static constexpr uint8_t REG_CTRL6_C = 0x15;
static constexpr uint8_t REG_STATUS_REG = 0x1E;
static constexpr uint8_t REG_OUTX_L_XL = 0x28;

static constexpr uint8_t WHO_AM_I_VALUE = 0x7B;
static constexpr uint8_t STATUS_XLDA = 0x01;
static constexpr uint8_t IF_INC = 0x04;
static constexpr uint8_t BDU = 0x40;