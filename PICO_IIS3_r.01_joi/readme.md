# README — Implementação de Filtro de Vibração no IIS3DWB

## Objetivo

Esta branch implementa melhorias no processamento do acelerômetro IIS3DWB para reduzir ruído de piso, remover influência da gravidade (DC) e melhorar a detecção de vibração em tempo real utilizando Raspberry/Pico RP2040.

O objetivo principal foi estabilizar a leitura do sensor parado sobre a mesa e permitir que apenas vibrações reais fossem detectadas.

---

# Problema identificado

Inicialmente o sistema apresentava:

* Oscilação significativa mesmo com o sensor parado
* Magnitude (`mag_mg`) muito elevada em repouso
* Dificuldade para diferenciar vibração real de gravidade
* Presença de ruído contínuo e pequenos spikes

Foi identificado que:

* O cálculo da magnitude estava incluindo a componente gravitacional (DC)
* O acelerômetro mede aceleração + gravidade simultaneamente
* O sistema estava analisando o sinal bruto sem remoção de offset lento

---

# Estratégia adotada

Foi implementado um pipeline simples e leve para processamento em tempo real:

```text
Leitura RAW
    ↓
Estimativa lenta da gravidade (LPF)
    ↓
Remoção da gravidade/DC
    ↓
Filtro EMA (passa-baixa exponencial)
    ↓
Cálculo da magnitude da vibração
    ↓
Histerese de detecção
```

---

# Alterações implementadas

## 1. Adição da estimativa de gravidade/DC

Foram adicionadas variáveis globais para armazenar a componente lenta do sinal:

```cpp
static float gx = 0.0f;
static float gy = 0.0f;
static float gz = 0.0f;
```

Essa etapa funciona como um filtro passa-baixa lento para estimar a gravidade.

---

## 2. Implementação da constante BETA

Foi adicionada a constante:

```cpp
constexpr float BETA = 0.05f;
```

Responsabilidade:

* Controlar a velocidade de adaptação da estimativa da gravidade
* Valores menores tornam a convergência mais lenta
* Valores maiores removem a gravidade mais rapidamente

---

## 3. Remoção da gravidade (High-pass implícito)

Foi implementada a remoção da componente DC:

```cpp
float vib_x = x_mg - gx;
float vib_y = y_mg - gy;
float vib_z = z_mg - gz;
```

Na prática isso equivale a um filtro passa-alta digital.

Objetivo:

* Remover gravidade
* Remover drift lento
* Deixar apenas vibração real

---

## 4. Aplicação do filtro EMA

O sistema já possuía um filtro IIR/EMA, que foi mantido para suavização:

```cpp
fx_mg = (1.0f - ALPHA) * fx_mg + ALPHA * vib_x;
fy_mg = (1.0f - ALPHA) * fy_mg + ALPHA * vib_y;
fz_mg = (1.0f - ALPHA) * fz_mg + ALPHA * vib_z;
```

Constante utilizada:

```cpp
constexpr float ALPHA = 0.3f;
```

Responsabilidade:

* Reduzir ruído de alta frequência
* Suavizar pequenas oscilações
* Melhorar estabilidade visual no Teleplot

---

## 5. Recalibração dos thresholds

Os thresholds antigos estavam incorretos porque a magnitude incluía gravidade.

Antes:

```cpp
TH_ON_MG  = 500
TH_OFF_MG = 200
```

Depois:

```cpp
constexpr float TH_ON_MG  = 80.0f;
constexpr float TH_OFF_MG = 30.0f;
```

Objetivo:

* Detectar apenas vibração real
* Evitar falsos positivos
* Melhorar histerese

---

## 6. Aumento da taxa de amostragem

Foi reduzido o delay principal:

Antes:

```cpp
delay(120);
```

Depois:

```cpp
delay(10);
```

Resultado:

* Maior taxa de aquisição
* Menor aliasing
* Melhor resposta temporal
* Vibração mais fiel

---

## 7. Novas variáveis de debug no Teleplot

Foram adicionadas curvas auxiliares:

```cpp
>mag
>fx
>vx
>gx
>x_raw
```

Objetivo:

| Variável | Significado        |
| -------- | ------------------ |
| x_raw    | leitura bruta      |
| gx       | gravidade estimada |
| vx       | vibração sem DC    |
| fx       | vibração filtrada  |
| mag      | magnitude final    |

---

# Comportamento esperado

## Sensor parado

Esperado:

```text
mag ≈ 0 ~ 30 mg
```

O sinal deve permanecer relativamente estável.

---

## Sensor vibrando

Esperado:

* aumento rápido da magnitude
* detecção consistente de vibração
* resposta mais limpa

---

# Observações importantes

## Convergência inicial

Nos primeiros segundos após boot:

* o filtro ainda aprende a gravidade
* o valor pode oscilar ou “afundar”
* isso é esperado

---

# Próximas melhorias possíveis

Possíveis evoluções futuras:

* filtro mediana para spikes
* Butterworth passa-baixa
* análise FFT
* oversampling
* análise via osciloscópio
* ajuste dinâmico de threshold
* detector RMS por janela

---

# Resultado esperado da branch

Esta implementação transforma o sistema de:

```text
Detector de aceleração bruta
```

para:

```text
Detector real de vibração
```

com:

* remoção de gravidade
* suavização temporal
* melhor estabilidade
* menor ruído visual
* thresholds coerentes
* melhor detecção de eventos
