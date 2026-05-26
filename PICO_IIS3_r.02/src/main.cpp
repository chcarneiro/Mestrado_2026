// Tentei novos ajustes de alpha de histerese. Apresentou pequena melhora.
#include <Arduino.h>
#include <math.h>
#include "iis3dwb_driver.hpp"
#include "iis3dwb_regs.hpp"
#include "pinout.hpp"


// -----------------------------------------------------------------------------
// Constantes do IIS3DWB - Criando um Filtro Simples + Histerese para Detecção de Vibração
// -----------------------------------------------------------------------------

// Sensibilidade típica em FS = ±4 g (datasheet: ~0,122 mg/LSB) [web:185][web:192]
constexpr float IIS3DWB_SENS_MG_LSB = 0.122f; // com +-2g de ajuste

// Parâmetros do filtro IIR e histerese
constexpr float ALPHA = 0.3f;       // 0 < ALPHA <= 1, menor = mais suave, alterando o formato do sinal apenas.

// Criando uma histerese:
constexpr float TH_ON_MG  = 300.0f; // ativa evento acima desse nível de vibração 300 > 500
constexpr float TH_OFF_MG = 200.0f; // desativa quando cair abaixo disso          sem alterar

// Estado global do filtro e histerese
static float fx_mg = 0.0f;
static float fy_mg = 0.0f;
static float fz_mg = 0.0f;
static bool evento_ativo = false;

// -----------------------------------------------------------------------------
// setup()
// -----------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(3000);

  // Espera opcional pelo monitor serial (até 8 s)
  while (!Serial && millis() < 8000)
  {
    delay(10);
  }

  Serial.println("t_ms,x_mg_f,y_mg_f,z_mg_f,mag_mg,evt");
  // // Serial.println("A");
  Serial.println("BOOT,IIS3DWB,Pico RP2040");     //habilitou na rev_02
  Serial.println("B");

  // Inicializa o driver do sensor
  if (!sensor.begin())
  {
    Serial.println("C");
    Serial.println("ERROR,WHO_AM_I");
    while (true)
    {
      delay(1000); // trava aqui se nao conseguir falar com o sensor
    }
  }

  // Leitura de registradores de configuração para conferência
  Serial.println("D");

  Serial.print("WHO_AM_I=0x");
  Serial.println(sensor.readRegister(REG_WHO_AM_I), HEX);

  Serial.print("CTRL1_XL=0x");
  Serial.println(sensor.readRegister(REG_CTRL1_XL), HEX);

  Serial.print("CTRL3_C=0x");
  Serial.println(sensor.readRegister(REG_CTRL3_C), HEX);

  Serial.print("CTRL6_C=0x");
  Serial.println(sensor.readRegister(REG_CTRL6_C), HEX);

  Serial.println("E");

  // Cabeçalho do log contínuo:
  // t_ms,x_raw,y_raw,z_raw,mag_mg,evento_ativo
  Serial.println("t_ms,x_raw,y_raw,z_raw,mag_mg,evt");
}

// -----------------------------------------------------------------------------
// loop()
// -----------------------------------------------------------------------------
void loop()
{
  // 1) Ler 6 registradores consecutivos a partir de OUTX_L_A (0x28)
  uint8_t r28 = sensor.readRegister(REG_OUTX_L_XL);     // OUTX_L_A (LSB X)
  uint8_t r29 = sensor.readRegister(REG_OUTX_L_XL + 1); // OUTX_H_A (MSB X)
  uint8_t r2A = sensor.readRegister(REG_OUTX_L_XL + 2); // OUTY_L_A (LSB Y)
  uint8_t r2B = sensor.readRegister(REG_OUTX_L_XL + 3); // OUTY_H_A (MSB Y)
  uint8_t r2C = sensor.readRegister(REG_OUTX_L_XL + 4); // OUTZ_L_A (LSB Z)
  uint8_t r2D = sensor.readRegister(REG_OUTX_L_XL + 5); // OUTZ_H_A (MSB Z)

  // 2) Reconstruir int16_t (little-endian: MSB << 8 | LSB) [web:185][web:188]
  int16_t x_raw = (int16_t)((int16_t)r29 << 8 | r28);
  int16_t y_raw = (int16_t)((int16_t)r2B << 8 | r2A);
  int16_t z_raw = (int16_t)((int16_t)r2D << 8 | r2C);

  // 3) Converter para mg usando FS = ±2 g
  float x_mg = x_raw * IIS3DWB_SENS_MG_LSB;
  float y_mg = y_raw * IIS3DWB_SENS_MG_LSB;
  float z_mg = z_raw * IIS3DWB_SENS_MG_LSB;

  // 4) Filtro IIR simples em cada eixo
  fx_mg = (1.0f - ALPHA) * fx_mg + ALPHA * x_mg;
  fy_mg = (1.0f - ALPHA) * fy_mg + ALPHA * y_mg;
  fz_mg = (1.0f - ALPHA) * fz_mg + ALPHA * z_mg;

  // 5) Magnitude filtrada (aprox. RMS instantâneo)
  float mag_mg = sqrtf(fx_mg * fx_mg + fy_mg * fy_mg + fz_mg * fz_mg);

  // 6) Limiar + histerese para tornar o sistema menos sensível e plotável pelo Teleplot
  if (!evento_ativo && mag_mg > TH_ON_MG)
  {
    evento_ativo = true;
    Serial.print("EVT_ON,");
    Serial.print(millis());
    Serial.print(",");
    Serial.println(mag_mg, 1);
  }

  if (evento_ativo && mag_mg < TH_OFF_MG)
  {
    evento_ativo = false;
    Serial.print("EVT_OFF,");
    Serial.print(millis());
    Serial.print(",");
    Serial.println(mag_mg, 1);
  }

  // 6.b) Envio para Teleplot (curvas em tempo real)

  // Valores filtrados em mg
  Serial.print(">mag:");
  Serial.println(mag_mg);   // curva 'mag'

  Serial.print(">fx:");
  Serial.println(fx_mg);    // curva 'fx'
  // Serial.print(">fy:"); Serial.println(fy_mg);
  // Serial.print(">fz:"); Serial.println(fz_mg);

  // Valores raw dos 3 eixos (contagens do ADC)
  Serial.print(">x_raw:");
  Serial.println(x_raw);    // curva 'x_raw'

  Serial.print(">y_raw:");
  Serial.println(y_raw);    // curva 'y_raw'

  Serial.print(">z_raw:");
  Serial.println(z_raw);    // curva 'z_raw'

  // 6.c) Log contínuo em formato CSV para análise offline
  // t_ms,x_mg_f,y_mg_f,z_mg_f,mag_mg,evt
  Serial.print(millis());
  Serial.print(",");
  Serial.print(fx_mg, 1);
  Serial.print(",");
  Serial.print(fy_mg, 1);
  Serial.print(",");
  Serial.print(fz_mg, 1);
  Serial.print(",");
  Serial.print(mag_mg, 1);
  Serial.print(",");
  Serial.println(evento_ativo ? 1 : 0);

  // Ajuste de taxa de atualização visual
  delay(120);
}

