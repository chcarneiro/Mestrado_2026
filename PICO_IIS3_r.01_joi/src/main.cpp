#include <Arduino.h>
#include <math.h>
#include "iis3dwb_driver.hpp"
#include "iis3dwb_regs.hpp"
#include "pinout.hpp"


// -----------------------------------------------------------------------------
// Constantes do IIS3DWB - Criando um Filtro Simples + Histerese para Detecção de Vibração
// -----------------------------------------------------------------------------

// Sensibilidade típica em FS = ±4 g (datasheet: ~0,122 mg/LSB) [web:185][web:192]
constexpr float IIS3DWB_SENS_MG_LSB = 0.122f; // de 0.122f para ~0,061 mg/LSB com +-2g de ajuste

// Parâmetros do filtro IIR e histerese
constexpr float ALPHA = 0.3f;       // 0 < ALPHA <= 1, menor = mais suave, alterando o formato do sinal apenas.
constexpr float BETA = 0.05f;

// Criando uma histerese:
constexpr float TH_ON_MG  = 80.0f;
constexpr float TH_OFF_MG = 30.0f;

// Estado global do filtro e histerese
static float fx_mg = 0.0f;
static float fy_mg = 0.0f;
static float fz_mg = 0.0f;

// Estimativa lenta da gravidade/DC
static float gx = 0.0f;
static float gy = 0.0f;
static float gz = 0.0f;

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
  /*Serial.println("A");
  Serial.println("BOOT,IIS3DWB,Pico RP2040");
  Serial.println("B");*/

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
// -----------------------------------------------------------------------------
// loop()
// -----------------------------------------------------------------------------
void loop()
{
    // -------------------------------------------------------------------------
    // 1) Leitura raw
    // -------------------------------------------------------------------------

    uint8_t r28 = sensor.readRegister(REG_OUTX_L_XL);
    uint8_t r29 = sensor.readRegister(REG_OUTX_L_XL + 1);

    uint8_t r2A = sensor.readRegister(REG_OUTX_L_XL + 2);
    uint8_t r2B = sensor.readRegister(REG_OUTX_L_XL + 3);

    uint8_t r2C = sensor.readRegister(REG_OUTX_L_XL + 4);
    uint8_t r2D = sensor.readRegister(REG_OUTX_L_XL + 5);

    int16_t x_raw = (int16_t)((int16_t)r29 << 8 | r28);
    int16_t y_raw = (int16_t)((int16_t)r2B << 8 | r2A);
    int16_t z_raw = (int16_t)((int16_t)r2D << 8 | r2C);

    // -------------------------------------------------------------------------
    // 2) Converter para mg
    // -------------------------------------------------------------------------

    float x_mg = x_raw * IIS3DWB_SENS_MG_LSB;
    float y_mg = y_raw * IIS3DWB_SENS_MG_LSB;
    float z_mg = z_raw * IIS3DWB_SENS_MG_LSB;

    // -------------------------------------------------------------------------
    // 3) Estima gravidade lenta
    // -------------------------------------------------------------------------

    gx = (1.0f - BETA) * gx + BETA * x_mg;
    gy = (1.0f - BETA) * gy + BETA * y_mg;
    gz = (1.0f - BETA) * gz + BETA * z_mg;

    // -------------------------------------------------------------------------
    // 4) Remove gravidade
    // -------------------------------------------------------------------------

    float vib_x = x_mg - gx;
    float vib_y = y_mg - gy;
    float vib_z = z_mg - gz;

    // -------------------------------------------------------------------------
    // 5) EMA da vibração
    // -------------------------------------------------------------------------

    fx_mg = (1.0f - ALPHA) * fx_mg + ALPHA * vib_x;
    fy_mg = (1.0f - ALPHA) * fy_mg + ALPHA * vib_y;
    fz_mg = (1.0f - ALPHA) * fz_mg + ALPHA * vib_z;

    // -------------------------------------------------------------------------
    // 6) Magnitude REAL da vibração
    // -------------------------------------------------------------------------

    float mag_mg = sqrtf(
        fx_mg * fx_mg +
        fy_mg * fy_mg +
        fz_mg * fz_mg
    );

    // -------------------------------------------------------------------------
    // 7) Histerese
    // -------------------------------------------------------------------------

    if (!evento_ativo && mag_mg > TH_ON_MG)
    {
        evento_ativo = true;
    }

    if (evento_ativo && mag_mg < TH_OFF_MG)
    {
        evento_ativo = false;
    }

    // -------------------------------------------------------------------------
    // 8) TELEPLOT
    // -------------------------------------------------------------------------

    Serial.print(">mag:");
    Serial.println(mag_mg);

    Serial.print(">fx:");
    Serial.println(fx_mg);

    Serial.print(">vx:");
    Serial.println(vib_x);

    Serial.print(">gx:");
    Serial.println(gx);

    Serial.print(">x_raw:");
    Serial.println(x_raw);

    // -------------------------------------------------------------------------
    // 9) Delay
    // -------------------------------------------------------------------------

    delay(10);
}
