import streamlit as st
import serial
import serial.tools.list_ports
import json
from collections import deque

# ── Configuração da página ────────────────────────────────────────────────────
st.set_page_config(
    page_title="Monitor de Vibração",
    page_icon="📡",
    layout="wide"
)

st.title("📡 Monitor de Vibração — ESP32")
st.caption("Leitura em tempo real via Serial | 1 ponto = 1 batch (~1s)")

# ── Sidebar ───────────────────────────────────────────────────────────────────
with st.sidebar:
    st.header("Configurações")
    portas   = [p.device for p in serial.tools.list_ports.comports()]
    porta    = st.selectbox("Porta Serial", portas if portas else ["Nenhuma encontrada"])
    baud     = st.selectbox("Baud Rate", [115200, 9600], index=0)
    janela   = st.slider("Janela do gráfico (batches)", min_value=10, max_value=300, value=60)
    conectar = st.button("Conectar", type="primary")
    st.divider()
    st.caption("Cada batch = 50 amostras (~1s)")

# ── Estado da sessão ──────────────────────────────────────────────────────────
def init_deque(key):
    if key not in st.session_state:
        st.session_state[key] = deque(maxlen=300)  # até 300 batches (~5 min)

for key in ["ema_hist", "rms_hist", "peak_hist", "std_hist",
            "mag_hist", "ax_hist", "ay_hist", "az_hist"]:
    init_deque(key)

if "rodando" not in st.session_state:
    st.session_state.rodando = False
if conectar:
    st.session_state.rodando = True

# ── Layout ────────────────────────────────────────────────────────────────────
st.subheader("Features do Batch")
col_ema, col_rms, col_peak, col_std, col_bat = st.columns(5)
metric_ema  = col_ema.empty()
metric_rms  = col_rms.empty()
metric_peak = col_peak.empty()
metric_std  = col_std.empty()
metric_bat  = col_bat.empty()

st.divider()

st.subheader("Tendência das Features")
graf_features = st.empty()

st.subheader("Magnitude média por batch")
graf_mag = st.empty()

st.subheader("Média dos Eixos X · Y · Z por batch")
graf_eixos = st.empty()

status_box = st.empty()

# ── Helpers ───────────────────────────────────────────────────────────────────
def delta(hist):
    h = list(hist)
    if len(h) < 2:
        return None
    return round(h[-1] - h[-2], 2)

def janelado(hist):
    """Retorna apenas os últimos N batches conforme slider."""
    return list(hist)[-janela:]

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

            batch_count += 1

            # Features do batch
            ema_val  = data.get("ema",  0)
            rms_val  = data.get("rms",  0)
            peak_val = data.get("peak", 0)
            std_val  = data.get("std",  0)

            st.session_state.ema_hist.append(ema_val)
            st.session_state.rms_hist.append(rms_val)
            st.session_state.peak_hist.append(peak_val)
            st.session_state.std_hist.append(std_val)

            # Médias dos eixos e magnitude por batch
            amostras = data["batch"]
            n = len(amostras) if amostras else 1
            st.session_state.mag_hist.append(sum(s.get("mag", 0) for s in amostras) / n)
            st.session_state.ax_hist.append( sum(s.get("ax",  0) for s in amostras) / n)
            st.session_state.ay_hist.append( sum(s.get("ay",  0) for s in amostras) / n)
            st.session_state.az_hist.append( sum(s.get("az",  0) for s in amostras) / n)

            # Métricas
            metric_ema.metric("EMA",     f"{ema_val:,.1f}",  delta=delta(st.session_state.ema_hist))
            metric_rms.metric("RMS",     f"{rms_val:,.1f}",  delta=delta(st.session_state.rms_hist))
            metric_peak.metric("Peak",   f"{peak_val:,.1f}", delta=delta(st.session_state.peak_hist))
            metric_std.metric("Std Dev", f"{std_val:,.2f}",  delta=delta(st.session_state.std_hist))
            metric_bat.metric("Batches", batch_count)

            # Gráfico features
            graf_features.line_chart(
                {
                    "EMA":  janelado(st.session_state.ema_hist),
                    "RMS":  janelado(st.session_state.rms_hist),
                    "Peak": janelado(st.session_state.peak_hist),
                    "Std":  janelado(st.session_state.std_hist),
                },
                height=280,
                use_container_width=True
            )

            # Gráfico magnitude média
            graf_mag.line_chart(
                {"Magnitude média": janelado(st.session_state.mag_hist)},
                height=220,
                use_container_width=True
            )

            # Gráfico eixos médios
            graf_eixos.line_chart(
                {
                    "AX médio": janelado(st.session_state.ax_hist),
                    "AY médio": janelado(st.session_state.ay_hist),
                    "AZ médio": janelado(st.session_state.az_hist),
                },
                height=220,
                use_container_width=True
            )

    except serial.SerialException as e:
        status_box.error(f"Erro na Serial: {e}")
    except Exception as e:
        status_box.error(f"Erro inesperado: {e}")
else:
    status_box.info("Selecione a porta e clique em Conectar para iniciar.")