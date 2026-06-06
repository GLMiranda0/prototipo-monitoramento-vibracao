# Sistema de Monitoramento de Vibração com ESP32 e IA

## Objetivo

Desenvolver um sistema inteligente de monitoramento de vibração em equipamentos mecânicos (ex: esteira) capaz de detectar anomalias em tempo real utilizando aprendizado de máquina e classificar o estado operacional do equipamento como **Normal** ou **Anômalo**.

## Funcionamento

```
Equipamento → MPU6050 → ESP32 → Wi-Fi → API SaaS → Dashboard → Modelo IA → Classificação
                                   ↓
                             Interface Web local (http://vibra-XXXXXX.local)
```

### Etapas do Processamento

1. **Aquisição de Dados**: Leitura de acelerações (X, Y, Z) do MPU6050 via I2C a ~50 Hz
2. **Cálculo de Magnitude**: V = √(x² + y² + z²)
3. **Normalização pela Gravidade**: V_norm = V / grav_magnitude (repouso ≈ 1.0)
4. **Filtro EMA**: Suavização com filtro exponencial móvel (α configurável, padrão 0.2)
5. **Batching**: Agrupamento de N amostras (padrão 50, ~1 s) em um único payload
6. **Extração de Features**: RMS, Peak e Desvio Padrão calculados sobre o batch
7. **Transmissão**: HTTP POST para a API de ingestão com as features + leituras brutas
8. **Visualização**: Interface web embarcada no ESP32 — acesso pelo browser, sem app externo
9. **Classificação IA**: Modelo treinado para identificar Normal/Anomalia *(em desenvolvimento)*

---

## Interface Web Embarcada

O ESP32 serve uma interface web responsiva na porta 80. Acesse pelo browser:

- **mDNS**: `http://vibra-XXXXXX.local` (onde `XXXXXX` são os 3 bytes finais do MAC)
- **IP direto**: exibido no monitor serial após o boot

A interface exibe em tempo real as features de vibração (EMA, RMS, Peak, Std Dev), status da API e calibração, além de permitir ajustar os parâmetros de coleta sem recompilar.

### API REST do Dispositivo

| Método | Rota | Descrição |
|--------|------|-----------|
| GET | `/` | Interface web (HTML) |
| GET | `/api/status` | IP, hostname, SSID, status API, calibração, uptime |
| GET | `/api/readings` | Últimas features calculadas + timestamp |
| GET | `/api/config` | Parâmetros atuais de coleta |
| POST | `/api/config/sensor` | Atualiza parâmetros de coleta (persiste em flash) |
| POST | `/api/calibrate` | Dispara recalibração da gravidade |
| POST | `/api/wifi/reset` | Limpa credenciais Wi-Fi e reabre portal captivo |
| POST | `/api/restart` | Reinicia o dispositivo |

#### Parâmetros configuráveis (`POST /api/config/sensor`)

```json
{
  "batch_size":   50,
  "alpha":        0.2,
  "sample_delay": 20,
  "cal_samples":  200
}
```

---

## Configuração Wi-Fi (WiFiManager)

O firmware usa **WiFiManager** — não há `secrets.h` nem credenciais no código.

1. Na primeira execução (ou após reset de Wi-Fi), o ESP32 abre um **Access Point** com o nome `vibra-XXXXXX`
2. Conecte-se a esse AP pelo celular ou computador
3. Acesse o portal captivo (abre automaticamente, ou navegue para `192.168.4.1`)
4. Selecione sua rede Wi-Fi, insira a senha e confirme
5. O ESP32 se conecta, exibe o IP e hostname no serial e fica disponível em `http://vibra-XXXXXX.local`

O portal fecha automaticamente após 3 minutos sem configuração.

---

## Transmissão para a API

A cada batch coletado, o firmware envia um **POST HTTPS** para o endpoint de ingestão:

```
POST https://<api-host>/api/ingest
Content-Type: application/json
```

```json
{
  "device_token": "<token do dispositivo>",
  "recorded_at":  "2025-01-01T12:00:00.000Z",
  "payload": {
    "x":    -572,
    "y":    0,
    "z":    17468,
    "rms":  1.0018,
    "ema":  1.0021,
    "peak": 1.0153,
    "std":  0.0031
  }
}
```

---

## Features Extraídas

Todos os valores são calculados sobre as N amostras do batch (~1 s de sinal). A magnitude é **normalizada pela gravidade de repouso** — adimensional, com repouso ≈ 1.0.

| Feature | Fórmula | O que representa |
|---------|---------|-----------------|
| `ema` | `Vf = α·V_norm + (1-α)·Vf` | Tendência de longo prazo — sobe gradualmente com degradação |
| `rms` | `√(Σmag_norm² / n)` | Energia média da vibração — principal indicador de severidade |
| `peak` | `max(mag_norm)` | Valor máximo do batch — sensível a impactos e eventos impulsivos |
| `std` | `√(Σ(mag_norm - média)² / n)` | Variabilidade do sinal — aumenta com folgas e desequilíbrios |

### Referência de valores normalizados

| Situação | EMA / RMS | Peak | Std Dev |
|----------|-----------|------|---------|
| Repouso | ~1.0 | ~1.01–1.02 | ~0.001–0.005 |
| Vibração leve | ~1.05–1.1 | ~1.1–1.2 | ~0.01–0.05 |
| Vibração severa | >1.3 | >1.5 | >0.1 |

### Como as features se complementam na detecção de anomalia

| Situação | EMA | RMS | Peak | Std |
|----------|-----|-----|------|-----|
| Normal | Estável ~1.0 | ~1.0 estável | Proporcional ao RMS | Muito baixo |
| Degradação gradual | Sobe devagar | Sobe gradualmente | Acompanha RMS | Aumenta levemente |
| Impacto / folga | Pouco afetado | Sobe um pouco | Pico isolado alto | Sobe bastante |
| Falha severa | Alto | Alto | Muito alto | Alto e instável |

---

## Calibração da Gravidade

No boot, o firmware captura N amostras em repouso (padrão 200) e calcula a **magnitude média de repouso** (`grav_magnitude`). Todas as amostras subsequentes são divididas por esse valor.

Para recalibrar sem reiniciar:
- **Serial**: envie o comando `CAL`
- **Interface web**: clique no botão de calibração

> **Atenção:** execute a calibração sempre com o sensor parado e fixado na posição definitiva de montagem.

---

## Tecnologias

| Componente | Tecnologia |
|-----------|-----------|
| Microcontrolador | ESP32 WROOM-32 |
| Sensor | MPU6050 (Acelerômetro 6-DOF, I2C) |
| Framework | Arduino + PlatformIO |
| Gerenciamento Wi-Fi | WiFiManager (portal captivo) |
| Config persistente | ESP32 Preferences (flash) |
| Interface web | HTML/CSS/JS embarcado no firmware |
| Descoberta de rede | mDNS (`vibra-XXXXXX.local`) |
| Sincronização de tempo | NTP (`pool.ntp.org`, `time.google.com`) |
| Transmissão de dados | HTTPS POST → API SaaS |
| Machine Learning | Scikit-learn — detecção de anomalia *(em desenvolvimento)* |

---

## Estrutura do Projeto

```
manutencaoPreditivaVibracao/
├── src/
│   ├── main.cpp          # Firmware principal
│   └── webui.h           # HTML da interface web embarcada
├── saas-dashboard/       # Dashboard SaaS (FastAPI + React)
├── platformio.ini
├── .gitignore
└── README.md
```

---

## Como Executar

### Pré-requisitos

- VS Code com extensão PlatformIO
- ESP32 WROOM-32 conectado via USB
- Rede Wi-Fi 2.4 GHz disponível

### Firmware

1. Clone o repositório:
```bash
git clone https://github.com/GLMiranda0/prototipo-monitoramento-vibracao.git
cd prototipo-monitoramento-vibracao
```

2. Compile e faça upload via PlatformIO:
```bash
pio run -t upload
```

3. Monitore a saída serial:
```bash
pio device monitor -b 115200
```

4. Na primeira execução, conecte-se ao AP `vibra-XXXXXX` e configure o Wi-Fi pelo portal captivo.

5. Após a conexão, acesse a interface em `http://vibra-XXXXXX.local` (ou pelo IP exibido no serial).

### Conexão de Hardware

| Pino MPU6050 | Pino ESP32 |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | 3.3V |
| GND | GND |

---

## Histórico de Versões

| Versão | Tag | Descrição |
|--------|-----|-----------|
| 0.1  | `v0.1`  | Leitura Serial básica, cálculo de magnitude e filtro EMA |
| 0.11 | `v0.11` | Conexão Wi-Fi multi-rede, timestamp NTP e secrets.h |
| 0.12 | `v0.12` | Batch JSON (50 amostras/1 s), EMA no payload, dashboard Streamlit |
| 0.13 | `v0.13` | Features no firmware: RMS, Peak e Desvio Padrão |
| 0.14 | `v0.14` | Normalização pela gravidade: magnitude adimensional, calibração no boot, comando CAL |
| 0.2  | `v0.2`  | MQTT: publicação das features no Home Assistant, Last Will (status online/offline) |
| 0.3  | `v0.3`  | Interface web embarcada + WiFiManager + envio HTTP para API SaaS |
| 1.0  | `v1.0`  | Entrega final à faculdade *(planejado)* |

## Roadmap

- [x] Leitura e filtragem de dados do MPU6050
- [x] Transmissão com timestamp NTP
- [x] Payload em batch JSON
- [x] Features de domínio do tempo: EMA, RMS, Peak, Desvio Padrão
- [x] Normalização pela gravidade (independente de orientação, saída adimensional)
- [x] Interface web embarcada com configuração em runtime
- [x] Gerenciamento Wi-Fi via portal captivo (WiFiManager)
- [x] mDNS — acesso por hostname sem precisar saber o IP
- [x] Configuração persistente em flash (Preferences)
- [x] Transmissão HTTP para API SaaS
