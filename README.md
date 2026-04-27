# Sistema de Monitoramento de Vibração com ESP32 e IA

##  Objetivo

Desenvolver um sistema inteligente de monitoramento de vibração em equipamentos mecânicos (ex: esteira) capaz de detectar anomalias em tempo real utilizando aprendizado de máquina e classificar o estado operacional do equipamento.

##  Funcionamento

O sistema opera seguindo este pipeline:

```
Equipamento → Sensor MPU6050 → ESP32 → Processamento → Modelo IA → Classificação → Alarme
```

### Etapas do Processamento:

1. **Aquisição de Dados**: Leitura de acelerações (X, Y, Z) do MPU6050 via I2C
2. **Cálculo de Magnitude**: V = √(x² + y² + z²)
3. **Filtro EMA**: Suavização dos dados com filtro exponencial móvel
4. **Extração de Padrões**: Características dos sinais de vibração
5. **Classificação IA**: Modelo treinado para identificar Normal/Anomalia
6. **Acionamento de Alarme**: Notificação em caso de detecção anômala

##  Tecnologias

- **Microcontrolador**: ESP32 (WROOM-32)
- **Sensor**: MPU6050 (Acelerômetro 6-DOF)
- **Framework**: Arduino + PlatformIO
- **Comunicação**: Serial (115200 baud)
- **Protocolo de Sensor**: I2C (SDA: GPIO21, SCL: GPIO22)
- **Machine Learning**: TensorFlow Lite / Scikit-learn (modelos treinados offline)

##  Estrutura do Projeto

```
prototipo-monitoramento-vibracao/
├── firmware/
│   └── esp32/
│       ├── src/
│       │   ├── main.cpp
│       │   ├── mpu6050.h
│       │   └── filters.h
│       ├── include/
│       ├── platformio.ini
│       └── .gitignore
├── data/
│   ├── treino/
│   │   └── .gitkeep
│   └── teste/
│       └── .gitkeep
├── models/
│   ├── anomaly_detector.py
│   └── .gitkeep
├── docs/
│   ├── diagramas/
│   │   └── sistema_arquitetura.md
│   └── relatorio/
│       └── especificacao_tecnica.md
├── assets/
│   └── imagens/
│       └── .gitkeep
├── README.md
└── LICENSE
```

##  Como Executar

### Pré-requisitos

- PlatformIO IDE instalado (VS Code + extensão PlatformIO)
- ESP32 WROOM-32 conectado via USB
- Dependências: Adafruit MPU6050, Adafruit Unified Sensor

### Instalação do Firmware

1. Clone o repositório:
```bash
git clone https://github.com/GLMiranda0/prototipo-monitoramento-vibracao.git
cd prototipo-monitoramento-vibracao/firmware/esp32
```

2. Instale as dependências (PlatformIO gerencia automaticamente):
```bash
pio run -t install
```

3. Compile e faça upload:
```bash
pio run -t upload
```

4. Monitore a saída serial:
```bash
pio device monitor -b 115200
```

### Configuração do Sensor

O MPU6050 deve ser conectado ao ESP32 nos seguintes pinos:
- **SDA**: GPIO21
- **SCL**: GPIO22
- **VCC**: 3.3V
- **GND**: GND
