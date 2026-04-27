# Especificação Técnica - Sistema de Monitoramento de Vibração

## 1. Visão Geral

Sistema embarcado de tempo real para monitoramento contínuo de vibração em equipamentos mecânicos, com capacidade de detecção de anomalias via Inteligência Artificial.

## 2. Requisitos Funcionais

### 2.1 Aquisição de Dados
- Leitura contínua de aceleração em 3 eixos (X, Y, Z)
- Taxa de amostragem: 50 Hz (período de 20ms)
- Precisão: 16-bit
- Range: ±16g (±156.96 m/s²)

### 2.2 Processamento
- Cálculo de magnitude: V = √(x² + y² + z²)
- Filtragem: Filtro Exponencial Móvel (EMA) com α = 0.3
- Extração de características: não implementada nesta versão (prevista: RMS, pico, desvio padrão)
- Classificação: baseada em limiar ou modelo de IA

### 2.3 Saída de Dados
- Transmissão serial: 115200 baud
- Formato estruturado: timestamp, valores de aceleração, magnitude, status
- Taxa de amostragem aproximada: 50 Hz (controlada por delay ou timer)

### 2.4 Alertas
- Detecção de anomalia baseada em variação da magnitude em relação ao valor de repouso
- Limiar inicial empírico (ex: > 15.0 m/s²), sujeito a calibração conforme aplicação
- Acionamento de alarme com proteção contra spam (máximo 1 a cada 5 segundos)

## 3. Requisitos Não-Funcionais

### 3.1 Desempenho
- Latência estimada: dependente da carga de processamento e comunicação serial
- Poder computacional: CPU: 240 MHz, RAM: 520 KB

### 3.2 Confiabilidade
- Verificação básica de inicialização do sensor (WHO_AM_I)
- Log de erros via serial

### 3.3 Escalabilidade
- Preparado para integração com modelo de IA
- Suporta múltiplos sensores (futuro)
- Compatível com sistemas de monitoramento remoto

## 4. Hardware

### 4.1 Microcontrolador
| Especificação | Valor |
|---|---|
| Modelo | ESP32 WROOM-32 |
| Processador | Dual-core 32-bit (240 MHz) |
| Memória RAM | 520 KB (SRAM) |
| Memória Flash | 4 MB |
| Pinos GPIO | 34 |
| I2C | 2 interfaces |
| UART | 3 interfaces |
| ADC | 12-bit, 12 canais |

### 4.2 Sensor de Vibração
| Especificação | Valor |
|---|---|
| Modelo | MPU6050 |
| Tipo | MEMS 6-DOF (Acelerômetro + Giroscópio) |
| Interface | I2C |
| Endereço I2C | 0x68 |
| Range (Aceleração) | ±2g, ±4g, ±8g, ±16g |
| Range (Configurado) | ±16g |
| Sensibilidade | 2048 LSB/g (em ±16g) |
| Resolução | 16-bit |
| Frequência de Amostragem | até 1000 Hz (config. 50 Hz) |
| Noise Density | ~40-50 μg/√Hz |
| Consumo de Corrente | ~5 mA (ativo) |

## 5. Conectividade

### 5.1 I2C (Sensor - Microcontrolador)
```
ESP32 GPIO21 (SDA) ←—→ MPU6050 SDA
ESP32 GPIO22 (SCL) ←—→ MPU6050 SCL
ESP32 3.3V      ←—→ MPU6050 VCC
ESP32 GND       ←—→ MPU6050 GND

Frequência I2C: 400 kHz (fast mode)
Pull-up: normalmente presente na placa GY-521 (4.7kΩ)
```

### 5.2 Serial (ESP32 - Computador)
```
Baud Rate: 115200
Data Bits: 8
Stop Bits: 1
Parity: None
Flow Control: None

Protocolo: USB (via CP2102 ou similar)
Latência: ~1-2ms
```

## 6. Software

### 6.1 Ambiente de Desenvolvimento
- **IDE**: PlatformIO (VS Code)
- **Framework**: Arduino
- **Linguagem**: C++ (compatível com Arduino framework)
- **Compilador**: GCC 8.2

### 6.2 Dependências
```
Adafruit MPU6050 v2.2.4
Adafruit Unified Sensor v1.1.14
Arduino Framework ESP32
```

### 6.3 Estrutura de Código

#### main.cpp
- Loop principal com amostragem a 50 Hz
- Integração com sensor, filtros e classificação
- Transmissão de dados via serial

## 7. Algoritmos

### 7.1 Filtro Exponencial Móvel (EMA)

**Equação**: Vf(t) = α × V(t) + (1 - α) × Vf(t-1)

- **α**: 0.3 (fator de suavização)
- **Vf(t)**: valor filtrado no tempo t
- **V(t)**: valor bruto no tempo t
- **Efeito**: reduz ruído e suaviza oscilações

**Exemplo**:
```
V(0) = 10.5 m/s²
α = 0.3

Vf(1) = 0.3 × 10.8 + 0.7 × 10.5 = 3.24 + 7.35 = 10.59
Vf(2) = 0.3 × 10.2 + 0.7 × 10.59 = 3.06 + 7.413 = 10.47
```

### 7.2 Cálculo de Magnitude

**Equação**: V = √(x² + y² + z²)

- **x, y, z**: componentes de aceleração em m/s²
- **V**: magnitude resultante

**Interpretação**:
- Combina acelerações em 3 eixos em um único valor
- Elimina viés direcional
- Facilita detecção de anomalias globais

### 7.3 Classificação (Limiar)

**Limiar inicial empírico (ajustável conforme aplicação)**

## 8. Integração com IA (Futuro)

**Integração com IA prevista (não implementada nesta versão)**

## 9. Formato de Dados Serial

### 9.1 Estrutura de Mensagem

```
[TIMESTAMP] X=0.12, Y=0.08, Z=9.81, Magnitude=9.85, Filtrada=9.84, Status=NORMAL
```

### 9.2 Parsing
```
Campo         | Tipo    | Unidade | Descrição
──────────────│─────────│─────────│────────────────────────
TIMESTAMP     | uint32  | ms      | Tempo desde boot
X             | float   | m/s²    | Aceleração eixo X
Y             | float   | m/s²    | Aceleração eixo Y
Z             | float   | m/s²    | Aceleração eixo Z
Magnitude     | float   | m/s²    | Valor bruto
Filtrada      | float   | m/s²    | Valor após filtro EMA
Status        | string  | -       | NORMAL ou ANOMALIA
```

## 10. Testes

### 10.1 Teste Unitário
- Verificar inicialização do sensor
- Validar cálculo de magnitude
- Testar filtro EMA com valores conhecidos

### 10.2 Teste de Integração
- Comunicação I2C com MPU6050
- Transmissão serial contínua
- Taxa de amostragem (50 Hz ±5%)

### 10.3 Teste de Campo
- Capturar dados reais em equipamento
- Comparar com medições de referência
- Validar detecção de anomalias

## 11. Limitações do Sistema

- Não realiza análise no domínio da frequência (FFT)
- Sensível à orientação do sensor (efeito da gravidade)
- Classificação baseada em limiar simples (sem modelo treinado)
- Taxa de amostragem não determinística (dependente do loop)

## 12. Trabalhos Futuros

- Implementação de análise espectral (FFT)
- Treinamento de modelo de IA embarcado
- Detecção automática de baseline de vibração
- Comunicação via Wi-Fi/MQTT
- Interface gráfica para monitoramento

## 13. Documentação de Referência

- [MPU6050 Datasheet](https://invensense.tdk.com/products/motion-tracking/6-axis/)
- [Adafruit MPU6050 Library](https://github.com/adafruit/Adafruit_MPU6050)
- [ESP32 Technical Reference](https://docs.espressif.com/projects/esp-idf/)
- [PlatformIO Documentation](https://docs.platformio.org/)

  
---
**Documento**: Especificacao_Tecnica.md  
**Data**: 2026-04-27  
**Versão**: 1.0  
**Autor**: GLMiranda0
