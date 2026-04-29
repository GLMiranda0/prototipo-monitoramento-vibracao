import streamlit as st
import serial
import serial.tools.list_ports
import json
import time
from collections import deque

# ── Configuração da página ────────────────────────────────────────────────────
st.set_page_config(
    page_title="Monitor de Vibração",
    page_icon="📡",
    layout="wide"
)

st.title("📡 Monitor de Vibração — ESP32")
st.caption("Leitura em tempo real via Serial")

# ── Sidebar: configurações ────────────────────────────────────────────────────
with st.sidebar:
    st.header("Configurações")

    portas = [p.device for p in serial.tools.list_ports.comports()]
    porta = st.selectbox("Porta Serial", portas if portas else ["Nenhuma encontrada"])
    baud  = st.selectbox("Baud Rate", [115200, 9600], index=0)
    janela = st.slider("Janela do gráfico (batches)", min_value=10, max_value=120, value=30)
    conectar = st.button("Conectar", type="primary")
    st.divider()
    st.caption("Cada batch = 50 amostras (~1s)")

# ── Estado da sessão ──────────────────────────────────────────────────────────
if "rodando" not in st.session_state:
    st.session_state.rodando = False
if "ema_hist" not in st.session_state:
    st.session_state.ema_hist = deque(maxlen=120)
if "mag_hist" not in st.session_state:
    st.session_state.mag_hist = deque(maxlen=6000)  # 120s × 50 amostras
if "ax_hist" not in st.session_state:
    st.session_state.ax_hist = deque(maxlen=6000)
if "ay_hist" not in st.session_state:
    st.session_state.ay_hist = deque(maxlen=6000)
if "az_hist" not in st.session_state:
    st.session_state.az_hist = deque(maxlen=6000)

if conectar:
    st.session_state.rodando = True

# ── Layout principal ──────────────────────────────────────────────────────────
col1, col2, col3 = st.columns(3)
metric_ema  = col1.empty()
metric_max  = col2.empty()
metric_bat  = col3.empty()

graf_mag  = st.empty()
graf_eixos = st.empty()
status_box = st.empty()

# ── Loop de leitura ───────────────────────────────────────────────────────────
if st.session_state.rodando:
    try:
        ser = serial.Serial(porta, baud, timeout=2)
        status_box.success(f"Conectado em {porta} @ {baud} baud")
        batch_count = 0

        while True:
            linha = ser.readline().decode("utf-8", errors="ignore").strip()
            if not linha:
                continue

            try:
                data = json.loads(linha)
            except json.JSONDecodeError:
                continue

            if "batch" not in data or "ema" not in data:
                continue

            ema_val = data["ema"]
            batch   = data["batch"]
            batch_count += 1

            # Acumula histórico
            st.session_state.ema_hist.append(ema_val)
            for s in batch:
                st.session_state.mag_hist.append(s.get("mag", 0))
                st.session_state.ax_hist.append(s.get("ax", 0))
                st.session_state.ay_hist.append(s.get("ay", 0))
                st.session_state.az_hist.append(s.get("az", 0))

            mag_list = list(st.session_state.mag_hist)
            ax_list  = list(st.session_state.ax_hist)
            ay_list  = list(st.session_state.ay_hist)
            az_list  = list(st.session_state.az_hist)

            # Métricas
            metric_ema.metric("EMA atual", f"{ema_val:,.1f}")
            metric_max.metric("Mag. máx (janela)", f"{max(mag_list):,.1f}")
            metric_bat.metric("Batches recebidos", batch_count)

            # Gráfico magnitude
            graf_mag.line_chart(
                {"Magnitude bruta": mag_list, "EMA": list(st.session_state.ema_hist) + [None] * (len(mag_list) - len(st.session_state.ema_hist))},
                height=300,
                use_container_width=True
            )

            # Gráfico eixos
            graf_eixos.line_chart(
                {"AX": ax_list, "AY": ay_list, "AZ": az_list},
                height=250,
                use_container_width=True
            )

    except serial.SerialException as e:
        status_box.error(f"Erro na Serial: {e}")
    except Exception as e:
        status_box.error(f"Erro inesperado: {e}")
else:
    status_box.info("Selecione a porta e clique em Conectar para iniciar.")