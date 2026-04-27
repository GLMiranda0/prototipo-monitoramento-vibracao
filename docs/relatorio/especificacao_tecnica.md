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
- Extração de características: RMS, pico, desvio padrão
- Classificação: baseada em limiar ou modelo de IA

### 2.3 Saída de Dados
- Transmissão serial: 115200 baud
- Formato estruturado: timestamp, valores de aceleração, magnitude, status
- Frequência: a cada amostra (50 Hz)

### 2.4 Alertas
- Detecção de anomalia quando magnitude > 15.0 m/s²
- Acionamento de alarme com proteção contra spam (máximo 1 a cada 5 segundos)

## 3. Requisitos Não-Funcionais

### 3.1 Desempenho
- Latência de processamento: < 10ms
- Latência total: < 35ms
- Poder computacional: CPU: 240 MHz, RAM: 520 KB

### 3.2 Confiabilidade
- Sensor fault-tolerant com verificação de inicialização
- Reinicialização automática em caso de erro de I2C
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
Pull-up: 4.7kΩ (interno do MPU6050)
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
- **Linguagem**: C++17
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

#### mpu6050.h
- Abstração do sensor MPU6050
- Funções de inicialização e leitura
- Configuração de range e bandwidth

#### filters.h
- Classe EMAFilter: filtro exponencial móvel
- Classe MovingAverageFilter: média móvel
- Classe StatisticsCalculator: cálculo de RMS, desvio padrão, pico

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

**Lógica Atual**:
```
if (Magnitude_Filtrada > 15.0 m/s²) {
    Status = ANOMALIA
    Acionare Alarme
} else {
    Status = NORMAL
}
```

**Limiar Típico**: 15.0 m/s² (configurável)

## 8. Pipeline de Dados

```
Tempo      Entrada   Mag.Bruta  Mag.Filtrada  Status
(ms)       (X,Y,Z)   (m/s²)     (m/s²)        
────────────────────────────────────────────────────
0          (0,0,9.81)  9.81      9.81          NORMAL
20         (0.2,0,9.83) 9.83      9.82          NORMAL
40         (0.1,0.1,9.82) 9.82   9.82          NORMAL
60         (5,5,10)    12.25     10.72          NORMAL
80         (8,8,11)    16.33     12.62          ANOMALIA ⚠️
100        (3,3,9.9)   11.16     12.10          ANOMALIA ⚠️
120        (0.1,0,9.81) 9.82     11.36          NORMAL
```

## 9. Integração com IA (Futuro)

### 9.1 Etapas de Implementação

1. **Coleta de Dados**
   - Capturar dados de operação normal e anômala
   - Armazenar em `data/treino/` e `data/teste/`
   - Mínimo 1000 amostras por classe

2. **Treinamento de Modelo**
   - Usar Python + scikit-learn ou TensorFlow
   - Extrair features: RMS, pico, desvio padrão, curtose, skewness
   - Algoritmos: Logistic Regression, Random Forest, SVM
   - Validação cruzada (5-fold)

3. **Quantização e Deploy**
   - Converter modelo para TensorFlow Lite
   - Otimizar para baixa memória (< 100 KB)
   - Testar em ESP32

4. **Inferência No Microcontroller**
   - Carregar modelo em flash
   - Executar em tempo real
   - Retornar probabilidade de anomalia

### 9.2 Features Propostas

- **RMS** (Root Mean Square): √(Σx²/N)
- **Pico** (Peak): max(|aceleração|)
- **Desvio Padrão**: √(Σ(x-μ)²/N)
- **Kurtosis**: medida de picos anormais
- **Skewness**: assimetria da distribuição
- **Crest Factor**: pico / RMS
- **Entropy**: complexidade do sinal

## 10. Formato de Dados Serial

### 10.1 Estrutura de Mensagem

```
[TIMESTAMP] X=0.12, Y=0.08, Z=9.81, Magnitude=9.85, Filtrada=9.84, Status=NORMAL
```

### 10.2 Parsing
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

## 11. Testes

### 11.1 Teste Unitário
- Verificar inicialização do sensor
- Validar cálculo de magnitude
- Testar filtro EMA com valores conhecidos

### 11.2 Teste de Integração
- Comunicação I2C com MPU6050
- Transmissão serial contínua
- Taxa de amostragem (50 Hz ±5%)

### 11.3 Teste de Campo
- Capturar dados reais em equipamento
- Comparar com medições de referência
- Validar detecção de anomalias

## 12. Documentação de Referência

- [MPU6050 Datasheet](https://invensense.tdk.com/products/motion-tracking/6-axis/)
- [Adafruit MPU6050 Library](https://github.com/adafruit/Adafruit_MPU6050)
- [ESP32 Technical Reference](https://docs.espressif.com/projects/esp-idf/)
- [PlatformIO Documentation](https://docs.platformio.org/)

---
**Documento**: Especificacao_Tecnica.md  
**Data**: 2026-04-27  
**Versão**: 1.0  
**Autor**: GLMiranda0
