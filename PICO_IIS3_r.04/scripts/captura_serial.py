import csv
import math
import os
import threading
from collections import deque
from datetime import datetime

import matplotlib.pyplot as plt
import serial
import serial.tools.list_ports


# ============================================================
# CONFIGURAÇÕES
# ============================================================

BAUD_RATE = 115200

PASTA_SAIDA = "coletas"

# Quantos segundos aparecem na tela
JANELA_GRAFICO_S = 5

# Quantos pontos, no máximo, ficam no buffer do gráfico.
# Isso NÃO limita o CSV.
MAX_PONTOS_GRAFICO = 5000

# Intervalo entre atualizações do gráfico
# em milissegundos.
ATUALIZACAO_GRAFICO_MS = 50


# ============================================================
# PORTA SERIAL
# ============================================================

def escolher_porta():
    portas = list(serial.tools.list_ports.comports())

    if not portas:
        print("Nenhuma porta serial encontrada.")
        return None

    print("\nPortas seriais disponíveis:\n")

    for i, porta in enumerate(portas):
        print(
            f"[{i}] {porta.device}"
            f" - {porta.description}"
        )

    while True:
        try:
            escolha = int(
                input("\nEscolha a porta: ")
            )

            if 0 <= escolha < len(portas):
                return portas[escolha].device

            print("Opção inválida.")

        except ValueError:
            print("Digite apenas o número.")


# ============================================================
# INTERPRETAÇÃO DAS LINHAS
# ============================================================

def interpretar_linha(linha):
    """
    Linha esperada:

    t_us,x_mg,y_mg,z_mg

    Exemplo:

    421,12.322,-8.906,1003.511
    """

    linha = linha.strip()

    if not linha:
        return None

    # Metadados do Arduino
    if linha.startswith("#"):
        return None

    # Dados destinados ao Teleplot antigo
    if linha.startswith(">"):
        return None

    # Cabeçalho
    if linha.startswith("t_us"):
        return None

    partes = linha.split(",")

    if len(partes) != 4:
        return None

    try:
        t_us = int(partes[0])

        x_mg = float(partes[1])
        y_mg = float(partes[2])
        z_mg = float(partes[3])

        return t_us, x_mg, y_mg, z_mg

    except ValueError:
        return None


# ============================================================
# CAPTURA
# ============================================================

def main():
    porta = escolher_porta()

    if porta is None:
        return

    # --------------------------------------------------------
    # Pasta/arquivo
    # --------------------------------------------------------

    os.makedirs(
        PASTA_SAIDA,
        exist_ok=True
    )

    agora = datetime.now().strftime(
        "%Y-%m-%d_%H-%M-%S"
    )

    nome_arquivo = (
        f"coleta_{agora}.csv"
    )

    caminho = os.path.join(
        PASTA_SAIDA,
        nome_arquivo
    )

    # --------------------------------------------------------
    # Serial
    # --------------------------------------------------------

    try:
        ser = serial.Serial(
            porta,
            BAUD_RATE,
            timeout=0.2
        )

    except serial.SerialException as erro:
        print(f"\nErro ao abrir {porta}:")
        print(erro)
        return

    print()
    print("==============================")
    print("AQUISIÇÃO IIS3DWB")
    print("==============================")
    print(f"Porta:   {porta}")
    print(f"Baud:    {BAUD_RATE}")
    print(f"Arquivo: {caminho}")
    print()
    print("A aquisição começou.")
    print(
        "Feche o gráfico ou pressione Ctrl+C "
        "para encerrar."
    )
    print()

    # ========================================================
    # BUFFERS DO GRÁFICO
    # ========================================================

    tempos = deque(
        maxlen=MAX_PONTOS_GRAFICO
    )

    valores_x = deque(
        maxlen=MAX_PONTOS_GRAFICO
    )

    valores_y = deque(
        maxlen=MAX_PONTOS_GRAFICO
    )

    valores_z = deque(
        maxlen=MAX_PONTOS_GRAFICO
    )

    valores_mag = deque(
        maxlen=MAX_PONTOS_GRAFICO
    )

    lock = threading.Lock()

    parar = threading.Event()
    coleta_finalizada = threading.Event()

    estatisticas = {
        "amostras": 0,
        "primeiro_t_us": None,
        "ultimo_t_us": None,
    }

    # ========================================================
    # THREAD DE LEITURA
    # ========================================================

    def leitor_serial():

        primeiro_timestamp_sensor = None

        with open(
            caminho,
            "w",
            newline="",
            encoding="utf-8"
        ) as arquivo:

            writer = csv.writer(arquivo)

            writer.writerow([
                "t_us",
                "x_mg",
                "y_mg",
                "z_mg"
            ])

            while not parar.is_set():

                try:
                    linha = ser.readline().decode(
                        "utf-8",
                        errors="ignore"
                    ).strip()

                except serial.SerialException:
                    print(
                        "\nConexão serial perdida."
                    )
                    break

                if not linha:
                    continue

                # --------------------------------------------
                # Metadados
                # --------------------------------------------

                if linha.startswith("#"):

                    print(linha)

                    if linha.startswith(
                        "#FIM_COLETA"
                    ):
                        coleta_finalizada.set()
                        break

                    continue

                # --------------------------------------------
                # Ignora Teleplot antigo
                # --------------------------------------------

                if linha.startswith(">"):
                    continue

                # --------------------------------------------
                # Amostra
                # --------------------------------------------

                dados = interpretar_linha(linha)

                if dados is None:
                    continue

                t_sensor_us, x, y, z = dados

                # --------------------------------------------
                # Faz a captura começar em t = 0
                #
                # Mesmo que o Arduino já estivesse ligado.
                # --------------------------------------------

                if primeiro_timestamp_sensor is None:
                    primeiro_timestamp_sensor = (
                        t_sensor_us
                    )

                t_us = (
                    t_sensor_us
                    - primeiro_timestamp_sensor
                )

                t_s = t_us / 1_000_000.0

                # --------------------------------------------
                # Magnitude apenas para visualização
                # --------------------------------------------

                mag = math.sqrt(
                    x * x +
                    y * y +
                    z * z
                )

                # --------------------------------------------
                # CSV
                #
                # TODAS as amostras entram aqui.
                # --------------------------------------------

                writer.writerow([
                    t_us,
                    x,
                    y,
                    z
                ])

                estatisticas["amostras"] += 1

                if (
                    estatisticas["primeiro_t_us"]
                    is None
                ):
                    estatisticas[
                        "primeiro_t_us"
                    ] = t_us

                estatisticas[
                    "ultimo_t_us"
                ] = t_us

                # Flush periódico
                if (
                    estatisticas["amostras"]
                    % 500 == 0
                ):
                    arquivo.flush()

                # --------------------------------------------
                # BUFFER PARA VISUALIZAÇÃO
                # --------------------------------------------

                with lock:

                    tempos.append(t_s)

                    valores_x.append(x)
                    valores_y.append(y)
                    valores_z.append(z)

                    valores_mag.append(mag)

            arquivo.flush()

        coleta_finalizada.set()

    # ========================================================
    # INICIA LEITURA EM PARALELO
    # ========================================================

    thread = threading.Thread(
        target=leitor_serial,
        daemon=True
    )

    thread.start()

    # ========================================================
    # GRÁFICO EM TEMPO REAL
    # ========================================================

    plt.ion()

    figura, (ax_xyz, ax_mag) = plt.subplots(
        2,
        1,
        figsize=(13, 8)
    )

    # --------------------------------------------------------
    # X Y Z
    # --------------------------------------------------------

    linha_x, = ax_xyz.plot(
        [],
        [],
        label="X"
    )

    linha_y, = ax_xyz.plot(
        [],
        [],
        label="Y"
    )

    linha_z, = ax_xyz.plot(
        [],
        [],
        label="Z"
    )

    ax_xyz.set_title(
        "IIS3DWB — aquisição em tempo real"
    )

    ax_xyz.set_xlabel(
        "Tempo (s)"
    )

    ax_xyz.set_ylabel(
        "Aceleração (mg)"
    )

    ax_xyz.legend()

    ax_xyz.grid()

    # --------------------------------------------------------
    # MAGNITUDE
    # --------------------------------------------------------

    linha_mag, = ax_mag.plot(
        [],
        [],
        label="Magnitude"
    )

    ax_mag.set_xlabel(
        "Tempo (s)"
    )

    ax_mag.set_ylabel(
        "Magnitude (mg)"
    )

    ax_mag.legend()

    ax_mag.grid()

    plt.tight_layout()

    # ========================================================
    # LOOP DO GRÁFICO
    # ========================================================

    try:

        while plt.fignum_exists(
            figura.number
        ):

            with lock:

                if len(tempos) > 1:

                    t = list(tempos)

                    x = list(valores_x)
                    y = list(valores_y)
                    z = list(valores_z)

                    mag = list(valores_mag)

                else:
                    t = []

            if len(t) > 1:

                # --------------------------------------------
                # Mostra apenas os últimos N segundos
                # --------------------------------------------

                tempo_final = t[-1]

                tempo_inicial = max(
                    0,
                    tempo_final
                    - JANELA_GRAFICO_S
                )

                inicio = 0

                for i, valor in enumerate(t):

                    if valor >= tempo_inicial:
                        inicio = i
                        break

                t_plot = t[inicio:]

                x_plot = x[inicio:]
                y_plot = y[inicio:]
                z_plot = z[inicio:]

                mag_plot = mag[inicio:]

                # --------------------------------------------
                # Atualiza linhas
                # --------------------------------------------

                linha_x.set_data(
                    t_plot,
                    x_plot
                )

                linha_y.set_data(
                    t_plot,
                    y_plot
                )

                linha_z.set_data(
                    t_plot,
                    z_plot
                )

                linha_mag.set_data(
                    t_plot,
                    mag_plot
                )

                # --------------------------------------------
                # Atualiza limites
                # --------------------------------------------

                ax_xyz.relim()
                ax_xyz.autoscale_view()

                ax_mag.relim()
                ax_mag.autoscale_view()

                # --------------------------------------------
                # Informações na tela
                # --------------------------------------------

                n = estatisticas["amostras"]

                if t[-1] > 0 and n > 1:

                    fs_media = (
                        (n - 1)
                        / t[-1]
                    )

                    figura.suptitle(
                        f"Amostras: {n}   |   "
                        f"Tempo: {t[-1]:.2f} s   |   "
                        f"Fs média: {fs_media:.1f} Hz"
                    )

            figura.canvas.draw_idle()

            figura.canvas.flush_events()

            plt.pause(
                ATUALIZACAO_GRAFICO_MS
                / 1000.0
            )

            # Se o Arduino terminou uma coleta finita,
            # mantém o último gráfico visível.
            if coleta_finalizada.is_set():
                figura.suptitle(
                    "Coleta finalizada — "
                    "feche a janela para sair"
                )

    except KeyboardInterrupt:

        print(
            "\nCaptura interrompida "
            "pelo usuário."
        )

    finally:

        parar.set()

        thread.join(
            timeout=2
        )

        ser.close()

        plt.ioff()

        try:
            plt.close(figura)
        except Exception:
            pass

    # ========================================================
    # RESUMO FINAL
    # ========================================================

    n = estatisticas["amostras"]

    print()
    print("==============================")
    print("RESUMO")
    print("==============================")

    print(
        f"Amostras salvas: {n}"
    )

    ultimo = estatisticas[
        "ultimo_t_us"
    ]

    if (
        ultimo is not None
        and ultimo > 0
        and n > 1
    ):

        duracao_s = (
            ultimo / 1_000_000
        )

        fs_media = (
            (n - 1) / duracao_s
        )

        print(
            f"Duração: {duracao_s:.3f} s"
        )

        print(
            f"Fs média: {fs_media:.2f} Hz"
        )

    print()
    print(
        f"Arquivo salvo em:\n{caminho}"
    )


# ============================================================
# EXECUÇÃO
# ============================================================

if __name__ == "__main__":
    main()