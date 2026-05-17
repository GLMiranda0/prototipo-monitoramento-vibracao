# Sistema de Monitoramento de Vibração com ESP32

## Objetivo

Desenvolver um sistema de monitoramento de vibração em equipamentos mecânicos (ex: esteira) capaz de detectar anomalias em tempo real e classificar o estado operacional como **Normal** ou **Anômalo**.

## Arquitetura

```
Equipamento → MPU6050 → ESP32 ─── MQTT ──→ Home Assistant / Broker
                                └── Wi-Fi → Interface Web de Configuração
```

## Funcionamento

### Etapas do Processamento

1. **Aquisição**: Leitura de acelerações (X, Y, Z) do MPU6050 via I2C
2. **Normalização pela Gravidade**: `V_norm = √(x²+y²+z²) / grav_magnitude` (repouso ≈ 1.0)
3. **Filtro EMA**: Suavização com filtro exponencial móvel configurável (α)
4. **Batching**: Agrupamento de N amostras em um payload JSON
5. **Extração de Features**: RMS, Peak e Desvio Padrão sobre o batch
6. **Transmissão MQTT**: Features publicadas com timestamp NTP

### Features Extraídas

Todos os valores são adimensionais (normalizados pela gravidade de repouso).

| Feature | Fórmula | O que representa |
|---------|---------|-----------------|
| `ema`  | `Vf = α·V + (1-α)·Vf` | Tendência de longo prazo |
| `rms`  | `√(Σmag² / n)` | Energia média da vibração |
| `peak` | `max(mag)` | Valor máximo — sensível a impactos |
| `std`  | `√(Σ(mag - média)² / n)` | Variabilidade — detecta folgas e desequilíbrios |

#### Referência de valores

| Situação | EMA / RMS | Peak | Std Dev |
|----------|-----------|------|---------|
| Repouso | ~1.0 | ~1.01–1.02 | ~0.001–0.005 |
| Vibração leve | ~1.05–1.1 | ~1.1–1.2 | ~0.01–0.05 |
| Vibração severa | >1.3 | >1.5 | >0.1 |

### Payload MQTT

```json
{ "ema": 1.002, "rms": 1.001, "peak": 1.015, "std": 0.003 }
```

Publicado no tópico `vibration/esp32/features` (configurável).

### Calibração da Gravidade

No boot, o firmware captura N amostras em repouso e calcula a magnitude média (`grav_magnitude`). Todas as leituras subsequentes são divididas por esse valor.

Para recalibrar sem reiniciar:
- **Interface web**: botão **Calibrar** na página de configuração
- **Serial**: envie `CAL` (115200 baud)

> Sempre calibre com o sensor parado e fixado na posição definitiva de montagem.

## Interface Web de Configuração

Após conectar ao Wi-Fi, acesse `http://<IP-do-dispositivo>` no navegador.

### Funcionalidades

- **Última leitura**: EMA, RMS, Pico e Desvio Padrão com atualização automática a cada 5s, com código de cores (verde = normal, vermelho = elevado)
- **MQTT**: broker, porta, usuário, senha, client ID e tópicos
- **Sensor**: tamanho do batch, intervalo de amostragem, amostras de calibração e α (K do EMA)
- **Sistema**: redefinir Wi-Fi (volta ao modo AP) e reiniciar dispositivo

Todas as configurações são persistidas no flash (NVS) e sobrevivem a reinicializações.

## Provisionamento Wi-Fi

No primeiro boot (ou após "Redefinir Wi-Fi"), o dispositivo cria um Access Point:

```
Rede: VibracaoSensor-XXXXXX
```

Conecte-se a essa rede, acesse `192.168.4.1`, configure o Wi-Fi e salve. O dispositivo reinicia e conecta automaticamente. Nos próximos boots o Wi-Fi salvo é usado sem intervenção.

## Hardware

| Pino MPU6050 | Pino ESP32 |
|---|---|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 3.3V |
| GND | GND |

## Tecnologias

- **Microcontrolador**: ESP32 (WROOM-32)
- **Sensor**: MPU6050 (acelerômetro 6-DOF via I2C)
- **Framework**: Arduino + PlatformIO
- **Comunicação**: MQTT (PubSubClient), Interface Web (WebServer)
- **Configuração**: WiFiManager + Preferences (NVS)
- **Sincronização de Tempo**: NTP (`pool.ntp.org`, UTC-3)

## Estrutura do Projeto

```
├── src/
│   ├── main.cpp       # Firmware principal
│   └── webui.h        # Interface web (HTML/CSS/JS embutido)
├── docs/
│   └── relatorio/
├── platformio.ini
├── .gitignore
└── README.md
```

## Como Executar

### Pré-requisitos

- VS Code com extensão PlatformIO
- ESP32 WROOM-32 conectado via USB

### Upload

```bash
pio run -t upload
```

### Monitor Serial

```bash
pio device monitor -b 115200
```

O IP do dispositivo é exibido no monitor após a conexão Wi-Fi.

## Histórico de Versões

| Versão | Descrição |
|--------|-----------|
| 0.1  | Leitura Serial básica, magnitude e filtro EMA |
| 0.11 | Wi-Fi multi-rede, timestamp NTP |
| 0.12 | Batch JSON, EMA no payload |
| 0.13 | Features no firmware: RMS, Peak, Desvio Padrão |
| 0.14 | Normalização pela gravidade, calibração no boot, comando CAL Serial |
| 0.2  | Integração MQTT com Home Assistant |
| 0.3  | Interface web de configuração, WiFiManager, configuração persistente via NVS |

## Roadmap

- [x] Leitura e filtragem do MPU6050
- [x] Timestamp NTP
- [x] Batch JSON com features de domínio do tempo
- [x] Normalização pela gravidade (adimensional, independente de orientação)
- [x] Transmissão MQTT
- [x] Interface web de configuração (sem recompilar)
- [x] Provisionamento Wi-Fi via portal AP
- [ ] Acoplamento à esteira motorizada
- [ ] Coleta de dataset Normal/Anômalo
- [ ] Treinamento e inferência do modelo de detecção de anomalia
