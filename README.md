# Sistema de Monitoramento de Vibração com ESP32 e IA

## Objetivo

Desenvolver um sistema inteligente de monitoramento de vibração em equipamentos mecânicos (ex: esteira) capaz de detectar anomalias em tempo real utilizando aprendizado de máquina e classificar o estado operacional do equipamento como **Normal** ou **Anômalo**.

## Funcionamento

O sistema opera seguindo este pipeline:

```
Equipamento → Sensor MPU6050 → ESP32 → Wi-Fi → Dashboard → Modelo IA → Classificação
```

### Etapas do Processamento

1. **Aquisição de Dados**: Leitura de acelerações (X, Y, Z) do MPU6050 via I2C a 50 Hz
2. **Cálculo de Magnitude**: V = √(x² + y² + z²)
3. **Filtro EMA**: Suavização dos dados com filtro exponencial móvel (α = 0.2)
4. **Batching**: Agrupamento de 50 amostras (~1s) em um único payload JSON
5. **Transmissão**: Envio via Serial com timestamp Unix em milissegundos (sincronizado via NTP)
6. **Visualização**: Dashboard Streamlit em tempo real
7. **Classificação IA**: Modelo treinado para identificar Normal/Anomalia *(em desenvolvimento)*

### Formato do Payload JSON

A cada ~1 segundo a ESP32 envia um JSON com o seguinte formato:

```json
{
  "ema": 17305.4,
  "batch": [
    { "ts": 1714300000020, "ax": -572, "ay": 0, "az": 17468, "mag": 17249.2 },
    { "ts": 1714300000040, "ax": -591, "ay": -12, "az": 17401, "mag": 17298.3 }
  ]
}
```

| Campo | Descrição |
|-------|-----------|
| `ema` | Valor do filtro EMA ao final do batch — principal indicador de tendência |
| `ts`  | Timestamp Unix em milissegundos (sincronizado via NTP) |
| `ax` `ay` `az` | Aceleração bruta nos 3 eixos (unidade: LSB do MPU6050) |
| `mag` | Magnitude bruta da amostra: √(ax² + ay² + az²) |

## Tecnologias

- **Microcontrolador**: ESP32 (WROOM-32)
- **Sensor**: MPU6050 (Acelerômetro 6-DOF)
- **Framework**: Arduino + PlatformIO
- **Comunicação**: Serial (115200 baud) → MQTT *(em desenvolvimento)*
- **Protocolo de Sensor**: I2C (SDA: GPIO21, SCL: GPIO22)
- **Sincronização de Tempo**: NTP (`pool.ntp.org`)
- **Dashboard**: Python + Streamlit
- **Machine Learning**: Scikit-learn — detecção de anomalia Normal/Anômalo *(em desenvolvimento)*

## Estrutura do Projeto

```
prototipo-monitoramento-vibracao/
├── src/
│   ├── main.cpp              # Firmware principal
│   ├── secrets.h             # Credenciais Wi-Fi (ignorado pelo git)
│   └── secrets.h.example     # Template de credenciais
├── dashboard/
│   ├── dashboard.py          # Dashboard Streamlit tempo real
│   └── requirements.txt      # Dependências Python
├── data/
│   ├── treino/               # CSVs de coleta para treinamento
│   └── teste/
├── models/
│   └── anomaly_detector.py   # Modelo de detecção de anomalia
├── docs/
│   └── relatorio/
├── platformio.ini
├── .gitignore
└── README.md
```

## Como Executar

### Pré-requisitos

- VS Code com extensão PlatformIO
- Python 3.8+
- ESP32 WROOM-32 conectado via USB

### Firmware

1. Clone o repositório:
```bash
git clone https://github.com/GLMiranda0/prototipo-monitoramento-vibracao.git
cd prototipo-monitoramento-vibracao
```

2. Configure as credenciais Wi-Fi:
```bash
cp src/secrets.h.example src/secrets.h
# Edite src/secrets.h com seus SSIDs e senhas
```

3. Compile e faça upload via PlatformIO:
```bash
pio run -t upload
```

4. Monitore a saída serial:
```bash
pio device monitor -b 115200
```

### Dashboard

1. Instale as dependências:
```bash
pip install -r dashboard/requirements.txt
```

2. Rode o dashboard:
```bash
streamlit run dashboard/dashboard.py
```

3. Acesse `http://localhost:8501`, selecione a porta COM da ESP32 na sidebar e clique em **Conectar**.

### Configuração do Sensor

O MPU6050 deve ser conectado ao ESP32 nos seguintes pinos:

| Pino MPU6050 | Pino ESP32 |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | 3.3V |
| GND | GND |

## Histórico de Versões

O projeto segue versionamento semântico simplificado:
- **0.x** — Desenvolvimento e coleta de dados
- **1.0** — Entrega final à faculdade

| Versão | Tag | Descrição |
|--------|-----|-----------|
| 0.1 | `v0.1` | Leitura Serial básica, cálculo de magnitude e filtro EMA |
| 0.11 | `v0.11` | Conexão Wi-Fi multi-rede, timestamp NTP e secrets.h |
| 0.12 | `v0.12` | Batch JSON (50 amostras/1s), EMA no payload, dashboard Streamlit |
| 0.2 | `v0.2` | Integração MQTT *(planejado)* |
| 0.3 | `v0.3` | Coleta de dataset e modelo de detecção de anomalia *(planejado)* |
| 1.0 | `v1.0` | Entrega final à faculdade *(planejado)* |

## Roadmap

- [x] Leitura e filtragem de dados do MPU6050
- [x] Transmissão com timestamp NTP
- [x] Payload em batch JSON
- [x] Dashboard Streamlit em tempo real
- [ ] Broker MQTT para transmissão sem fio
- [ ] Acoplamento à esteira motorizada (impressão 3D)
- [ ] Coleta de dataset Normal/Anômalo
- [ ] Treinamento do modelo de detecção de anomalia
- [ ] Inferência embarcada na ESP32