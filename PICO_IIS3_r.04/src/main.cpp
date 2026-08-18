#include <Arduino.h>
#include <math.h>

#include "iis3dwb_driver.hpp"
#include "iis3dwb_regs.hpp"
#include "pinout.hpp"

// =============================================================================
// CONFIGURACOES GERAIS
// =============================================================================

// Serial
constexpr uint32_t SERIAL_BAUD = 115200;

// -----------------------------------------------------------------------------
// DURACAO DA COLETA
//
// 0  = coleta continua / infinita
// 10 = coleta por 10 segundos
// 30 = coleta por 30 segundos
// 60 = coleta por 1 minuto
// -----------------------------------------------------------------------------

constexpr uint32_t DURACAO_COLETA_S = 0;

// =============================================================================
// CONFIGURACOES DO IIS3DWB
// =============================================================================

// O driver configura CTRL1_XL = 0xA0
// Full Scale = ±2 g
//
// Sensibilidade correspondente:
// 0.061 mg/LSB
constexpr float IIS3DWB_SENS_MG_LSB = 0.061f;

// =============================================================================
// CONFIGURACOES DO TELEPLOT
// =============================================================================

// O CSV tenta registrar toda nova amostra disponível.
//
// O Teleplot NAO precisa ser atualizado na mesma velocidade.
// Aqui ele recebe uma atualizacao a cada 50 ms:
//
// 50 ms -> aproximadamente 20 atualizacoes/s
constexpr uint32_t INTERVALO_TELEPLOT_US = 50000UL;

// Habilita/desabilita apenas a deteccao de evento.
// Isso NAO filtra nem altera X, Y ou Z.
constexpr bool ATIVAR_EVENTO = true;

// Histerese para indicacao visual de evento.
//
// O valor usado sera o desvio da magnitude em relacao
// a uma linha de base medida no inicio da coleta.
//
// Ajuste posteriormente observando os dados reais.
constexpr float TH_ON_MG  = 500.0f;
constexpr float TH_OFF_MG = 200.0f;

// =============================================================================
// CALIBRACAO DA LINHA DE BASE
// =============================================================================

// Durante o primeiro segundo, calculamos a magnitude media.
//
// Exemplo:
// sensor parado -> magnitude proxima da gravidade;
// sensor montado em uma bancada -> referencia real daquela condicao.
//
// A deteccao de evento so comeca depois dessa calibracao.
constexpr uint32_t TEMPO_BASELINE_US = 1000000UL;

// =============================================================================
// VARIAVEIS GLOBAIS
// =============================================================================

// Controle da coleta
static bool coleta_iniciada = false;
static bool coleta_finalizada = false;

static uint32_t tempo_inicio_us = 0;
static uint32_t ultimo_t_us = 0;
static uint32_t numero_amostras = 0;

// Teleplot
static uint32_t ultimo_teleplot_us = 0;

// Evento
static bool evento_ativo = false;

// Baseline
static bool baseline_pronta = false;
static double soma_baseline = 0.0;
static uint32_t amostras_baseline = 0;
static float baseline_mag_mg = 0.0f;

// =============================================================================
// FINALIZAR COLETA
// =============================================================================

void finalizarColeta()
{
    if (coleta_finalizada)
    {
        return;
    }

    coleta_finalizada = true;

    Serial.println("#FIM_COLETA");

    Serial.print("#amostras=");
    Serial.println(numero_amostras);

    Serial.print("#duracao_us=");
    Serial.println(ultimo_t_us);

    if (numero_amostras > 1 && ultimo_t_us > 0)
    {
        float fs_estimada =
            ((numero_amostras - 1) * 1000000.0f) /
            (float)ultimo_t_us;

        Serial.print("#fs_estimada_hz=");
        Serial.println(fs_estimada, 3);
    }

    if (baseline_pronta)
    {
        Serial.print("#baseline_mag_mg=");
        Serial.println(baseline_mag_mg, 3);
    }
}

// =============================================================================
// SETUP
// =============================================================================

void setup()
{
    Serial.begin(SERIAL_BAUD);

    delay(3000);

    // Espera opcional pelo Serial por no maximo 8 segundos
    while (!Serial && millis() < 8000)
    {
        delay(10);
    }

    // -------------------------------------------------------------------------
    // Inicializa IIS3DWB
    // -------------------------------------------------------------------------

    if (!sensor.begin())
    {
        Serial.println("#ERRO:IIS3DWB_NAO_ENCONTRADO");

        while (true)
        {
            delay(1000);
        }
    }

    // -------------------------------------------------------------------------
    // Informacoes da aquisicao
    // -------------------------------------------------------------------------

    Serial.println();
    Serial.println("#IIS3DWB_AQUISICAO");
    Serial.println("#FS=+-2g");
    Serial.println("#sensibilidade=0.061mg/LSB");
    Serial.println("#filtro_IIR=desativado");

    if (DURACAO_COLETA_S == 0)
    {
        Serial.println("#duracao=continua");
    }
    else
    {
        Serial.print("#duracao_s=");
        Serial.println(DURACAO_COLETA_S);
    }

    Serial.println("#teleplot=ativado");

    if (ATIVAR_EVENTO)
    {
        Serial.println("#evento=ativado");

        Serial.print("#TH_ON_MG=");
        Serial.println(TH_ON_MG);

        Serial.print("#TH_OFF_MG=");
        Serial.println(TH_OFF_MG);
    }
    else
    {
        Serial.println("#evento=desativado");
    }

    // -------------------------------------------------------------------------
    // Descarta uma leitura antiga, caso exista
    // -------------------------------------------------------------------------

    if (sensor.dataReady())
    {
        int16_t x_dummy;
        int16_t y_dummy;
        int16_t z_dummy;

        sensor.readXYZraw(
            x_dummy,
            y_dummy,
            z_dummy
        );
    }

    // -------------------------------------------------------------------------
    // Cabecalho CSV
    // -------------------------------------------------------------------------

    Serial.println("#INICIO_COLETA");

    Serial.println(
        "t_us,x_mg,y_mg,z_mg"
    );
}

// =============================================================================
// LOOP
// =============================================================================

void loop()
{
    // -------------------------------------------------------------------------
    // Caso uma coleta finita tenha terminado
    // -------------------------------------------------------------------------

    if (coleta_finalizada)
    {
        delay(100);
        return;
    }

    // -------------------------------------------------------------------------
    // Espera o IIS3DWB indicar que existe uma nova amostra
    // -------------------------------------------------------------------------

    if (!sensor.dataReady())
    {
        return;
    }

    // -------------------------------------------------------------------------
    // Timestamp
    // -------------------------------------------------------------------------

    uint32_t agora_us = micros();

    // A primeira amostra define t = 0
    if (!coleta_iniciada)
    {
        tempo_inicio_us = agora_us;
        ultimo_teleplot_us = agora_us;

        coleta_iniciada = true;
    }

    uint32_t t_us =
        agora_us - tempo_inicio_us;

    // -------------------------------------------------------------------------
    // Verifica se uma coleta finita terminou
    //
    // DURACAO_COLETA_S = 0 -> ignora esta verificacao
    // -------------------------------------------------------------------------

    if (DURACAO_COLETA_S > 0)
    {
        const uint32_t duracao_us =
            DURACAO_COLETA_S * 1000000UL;

        if (t_us >= duracao_us)
        {
            finalizarColeta();
            return;
        }
    }

    // -------------------------------------------------------------------------
    // Leitura RAW
    // -------------------------------------------------------------------------

    int16_t x_raw;
    int16_t y_raw;
    int16_t z_raw;

    sensor.readXYZraw(
        x_raw,
        y_raw,
        z_raw
    );

    // -------------------------------------------------------------------------
    // Conversao RAW -> mg
    //
    // IMPORTANTE:
    // Nao existe filtro IIR aqui.
    // Sao os valores diretamente convertidos da leitura do sensor.
    // -------------------------------------------------------------------------

    float x_mg =
        x_raw * IIS3DWB_SENS_MG_LSB;

    float y_mg =
        y_raw * IIS3DWB_SENS_MG_LSB;

    float z_mg =
        z_raw * IIS3DWB_SENS_MG_LSB;

    // -------------------------------------------------------------------------
    // Magnitude
    //
    // Utilizada SOMENTE para visualizacao/deteccao.
    //
    // X, Y e Z continuam inalterados.
    // -------------------------------------------------------------------------

    float mag_mg = sqrtf(
        x_mg * x_mg +
        y_mg * y_mg +
        z_mg * z_mg
    );

    // =========================================================================
    // BASELINE
    // =========================================================================

    if (!baseline_pronta)
    {
        soma_baseline += mag_mg;
        amostras_baseline++;

        // Depois do primeiro segundo,
        // calcula a magnitude media da condicao inicial.
        if (t_us >= TEMPO_BASELINE_US)
        {
            if (amostras_baseline > 0)
            {
                baseline_mag_mg =
                    (float)(
                        soma_baseline /
                        (double)amostras_baseline
                    );
            }

            baseline_pronta = true;

            Serial.print("#baseline_mag_mg=");
            Serial.println(baseline_mag_mg, 3);
        }
    }

    // =========================================================================
    // DETECCAO DE EVENTO
    // =========================================================================

    float vib_mg = 0.0f;

    if (baseline_pronta)
    {
        // Quanto a magnitude atual se afastou
        // da condicao media inicial
        vib_mg =
            fabsf(mag_mg - baseline_mag_mg);

        if (ATIVAR_EVENTO)
        {
            // Liga evento
            if (!evento_ativo &&
                vib_mg > TH_ON_MG)
            {
                evento_ativo = true;
            }

            // Desliga evento
            if (evento_ativo &&
                vib_mg < TH_OFF_MG)
            {
                evento_ativo = false;
            }
        }
    }

    // =========================================================================
    // CSV
    //
    // Esta parte ocorre em TODA amostra capturada.
    //
    // Formato:
    // t_us,x_mg,y_mg,z_mg
    // =========================================================================

    Serial.print(t_us);

    Serial.print(",");
    Serial.print(x_mg, 3);

    Serial.print(",");
    Serial.print(y_mg, 3);

    Serial.print(",");
    Serial.println(z_mg, 3);

    // -------------------------------------------------------------------------
    // Estatisticas da coleta
    // -------------------------------------------------------------------------

    ultimo_t_us = t_us;
    numero_amostras++;

    // =========================================================================
    // TELEPLOT
    //
    // Atualizado em uma taxa MENOR que a coleta.
    //
    // Isso reduz o trafego desnecessario de visualizacao.
    // =========================================================================

    if ((uint32_t)(agora_us - ultimo_teleplot_us) >=
        INTERVALO_TELEPLOT_US)
    {
        ultimo_teleplot_us = agora_us;

        // -------------------------------------------------------------
        // Eixos
        // -------------------------------------------------------------

        Serial.print(">x_mg:");
        Serial.println(x_mg, 3);

        Serial.print(">y_mg:");
        Serial.println(y_mg, 3);

        Serial.print(">z_mg:");
        Serial.println(z_mg, 3);

        // -------------------------------------------------------------
        // Magnitude
        // -------------------------------------------------------------

        Serial.print(">mag_mg:");
        Serial.println(mag_mg, 3);

        // -------------------------------------------------------------
        // Vibracao em relacao a baseline
        // -------------------------------------------------------------

        if (baseline_pronta)
        {
            Serial.print(">vib_mg:");
            Serial.println(vib_mg, 3);
        }

        // -------------------------------------------------------------
        // Evento
        // -------------------------------------------------------------

        if (ATIVAR_EVENTO)
        {
            Serial.print(">evento:");
            Serial.println(
                evento_ativo ? 1 : 0
            );
        }
    }
}