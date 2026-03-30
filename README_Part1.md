# Telemetria com Go, RabbitMQ e PostgreSQL

> _Documentação gerada com auxílio de IA._

Sistema de telemetria distribuído que recebe dados de sensores via HTTP, enfileira no RabbitMQ e persiste no PostgreSQL. Inclui teste de carga com k6.

## Arquitetura

```
k6 / Cliente HTTP
      │
      ▼ POST /telemetry
┌─────────────┐       AMQP        ┌─────────────┐       SQL        ┌──────────────┐
│   Backend   │ ────────────────► │  RabbitMQ   │ ◄──────────────  │   Consumer   │
│  (Go :8080) │                   │ (:5672)     │                  │     (Go)     │
└─────────────┘                   └─────────────┘                  └──────┬───────┘
                                                                          │
                                                                          ▼
                                                                   ┌──────────────┐
                                                                   │  PostgreSQL  │
                                                                   │   (:5432)    │
                                                                   └──────────────┘
```

### Fluxo de dados

1. Um cliente envia um `POST /telemetry` com payload JSON para o **Backend**
2. O Backend publica a mensagem na fila `telemetry_queue` do **RabbitMQ**
3. O **Consumer** consome as mensagens da fila e insere os registros no **PostgreSQL**
4. Os dados ficam persistidos na tabela `telemetry_readings`

---

## Estrutura do projeto

```
.
├── docker-compose.yaml
├── backend/
│   ├── backend.Dockerfile
│   ├── main.go          # Entry point: inicia RabbitMQ e servidor HTTP
│   ├── handler.go       # Handler HTTP POST /telemetry
│   ├── model.go         # Struct Telemetry
│   └── rabbitmq.go      # Conexão, declaração de fila e publicação
├── consumer/
│   ├── consumer.Dockerfile
│   └── consumer.go      # Consome fila e persiste no PostgreSQL
├── db/
│   └── 01-create.sql    # Script de inicialização da tabela
└── k6/
    └── load_test.js     # Teste de carga (50 VUs, 30s)
```

---

## Pré-requisitos

- [Docker](https://www.docker.com/) e Docker Compose
- [k6](https://k6.io/) (apenas para rodar o teste de carga)

```bash
# Instalar k6 no macOS
brew install k6
```

---

## Como executar

### 1. Subir todos os serviços

```bash
docker compose up --build
```

Isso irá:
- Compilar as imagens Go do backend e do consumer
- Subir RabbitMQ, PostgreSQL, backend e consumer
- Executar o script SQL de inicialização do banco

Aguarde até ver no log:

```
backend-1  | Server running on :8080
```

### 2. Testar manualmente (opcional)

```bash
curl -X POST http://localhost:8080/telemetry \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "sensor-01",
    "timestamp": "2026-03-22T12:00:00Z",
    "sensor_type": "temperature",
    "reading_type": "analog",
    "value": 23.5
  }'
```

Resposta esperada: `HTTP 202 Accepted`

### 3. Rodar o teste de carga com k6

```bash
k6 run k6/load_test.js
```

O teste envia requisições com **50 usuários virtuais simultâneos** durante **30 segundos**, gerando ~14.000 requisições.

### 4. Verificar os dados no banco

```bash
docker compose exec postgres psql -U postgres -d telemetry -c "SELECT COUNT(*) FROM telemetry_readings;"
```

Resultado esperado após o teste de carga:

```
 count
-------
 14340
(1 row)
```

---

## Serviços

| Serviço    | Imagem / Linguagem    | Porta          | Descrição                              |
|------------|-----------------------|----------------|----------------------------------------|
| `backend`  | Go 1.22               | `8080`         | API HTTP que publica no RabbitMQ       |
| `consumer` | Go 1.22               | —              | Consome fila e insere no PostgreSQL    |
| `rabbitmq` | `rabbitmq:3-management` | `5672`, `15672` | Message broker (UI em `:15672`)      |
| `postgres` | `postgres:15`         | `5432`         | Banco de dados relacional              |

### Painel do RabbitMQ

Acesse `http://localhost:15672` com login `guest` / `guest` para monitorar filas em tempo real.

---

## Modelo de dados

### Payload da requisição (`POST /telemetry`)

```json
{
  "device_id":    "sensor-01",
  "timestamp":    "2026-03-22T12:00:00Z",
  "sensor_type":  "temperature",
  "reading_type": "analog",
  "value":        23.5
}
```

| Campo         | Tipo              | Descrição                                  |
|---------------|-------------------|--------------------------------------------|
| `device_id`   | `string`          | Identificador único do dispositivo         |
| `timestamp`   | `string` (ISO8601)| Data e hora da leitura                     |
| `sensor_type` | `string`          | Tipo do sensor (ex: `temperature`)         |
| `reading_type`| `string`          | `analog` (numérico) ou `discrete` (boolean)|
| `value`       | `number \| bool`  | Valor da leitura                           |

### Tabela `telemetry_readings` (PostgreSQL)

```sql
CREATE TABLE IF NOT EXISTS telemetry_readings (
    id            SERIAL PRIMARY KEY,
    device_id     VARCHAR(100),
    timestamp     TIMESTAMP,
    sensor_type   VARCHAR(50),
    reading_type  VARCHAR(20),
    value_numeric DOUBLE PRECISION,
    value_boolean BOOLEAN,
    created_at    TIMESTAMP DEFAULT NOW()
);
```

O campo `value` do JSON é mapeado para `value_numeric` (quando `float64`) ou `value_boolean` (quando `bool`).

---

## Teste de carga (k6)

Configuração em `k6/load_test.js`:

| Parâmetro  | Valor |
|------------|-------|
| VUs        | 50    |
| Duração    | 30s   |
| Sleep/iter | 100ms |

Resultado obtido:

```
http_req_failed: 0.00%   ✓ (0 falhas em 14.340 requisições)
http_req_duration: avg=3.96ms  p(90)=6.92ms  p(95)=12.41ms
```

---

## Variáveis de ambiente (PostgreSQL)

Definidas no `docker-compose.yaml`:

| Variável            | Valor       |
|---------------------|-------------|
| `POSTGRES_DB`       | `telemetry` |
| `POSTGRES_USER`     | `postgres`  |
| `POSTGRES_PASSWORD` | `postgres`  |

---

## Firmware – Raspberry Pi Pico W (Arduino)

O firmware está em `pico_firmware/src/main.ino` e roda sobre o **Arduino Framework** com o core oficial do Raspberry Pi Pico W.

### Funcionalidades implementadas

| Critério | Implementação |
|---|---|
| Leitura digital | `digitalRead(GP15)` com debounce de 50 ms |
| Leitura analógica | `analogRead(GP26)` com média móvel de 10 amostras e conversão para Volts |
| Conectividade Wi-Fi | `WiFi.begin()` com reconexão automática a cada ciclo |
| Envio de telemetria | `HTTPClient POST /telemetry` com retry de até 3 tentativas |

### Circuito sugerido

```
Pico W GP26 (ADC0) ──── Potenciômetro / Sensor analógico
Pico W GP15         ──── Botão / Sensor de presença (pull-up interno)
Pico W GND          ──── GND dos sensores
```

### Como gravar no Pico W

1. Instale o Arduino IDE e adicione o core **Raspberry Pi Pico/RP2040** via Board Manager  
   (URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`)
2. Selecione a placa **"Raspberry Pi Pico W"**
3. Edite as constantes no topo do arquivo:
   ```cpp
   const char* SSID        = "SUA_REDE_WIFI";
   const char* PASSWORD    = "SUA_SENHA_WIFI";
   const char* BACKEND_URL = "http://<IP_DO_SERVIDOR>:8080/telemetry";
   const char* DEVICE_ID   = "pico-w-01";
   ```
4. Clique em **Upload** (Pico W deve estar em modo BOOTSEL ou conectado via USB)

O dispositivo enviará telemetria analógica (`temperature`) e discreta (`presence`) a cada **5 segundos**.

---

## Comandos úteis

```bash
# Ver logs de um serviço específico
docker compose logs backend
docker compose logs consumer
docker compose logs rabbitmq

# Reiniciar apenas o consumer
docker compose restart consumer

# Parar todos os containers
docker compose down

# Parar e remover volumes (apaga os dados do banco)
docker compose down -v

# Acessar o PostgreSQL diretamente
docker compose exec postgres psql -U postgres -d telemetry

# Ver os últimos registros inseridos
docker compose exec postgres psql -U postgres -d telemetry \
  -c "SELECT * FROM telemetry_readings ORDER BY created_at DESC LIMIT 10;"
```


