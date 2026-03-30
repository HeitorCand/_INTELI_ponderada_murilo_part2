#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino.h>

// ---- Configurações Wi-Fi ----
const char* SSID     = "Wokwi-GUEST";
const char* PASSWORD = "";

// ---- Configurações do backend ----
const char* BACKEND_URL = "http://192.168.0.121:8080/telemetry";
const char* DEVICE_ID   = "pico-w-01";

// ---- Pinos ----
const int ANALOG_PIN  = 26; // GP26 = ADC0 (ex: potenciômetro / sensor de temperatura)
const int DIGITAL_PIN = 15; // GP15 (ex: botão / sensor de presença)

// ---- Intervalo de envio ----
const int SEND_INTERVAL_MS = 5000; // envia a cada 5 segundos

// ---- Variáveis de suavização (média móvel simples) ----
const int NUM_SAMPLES = 10;
int analogSamples[NUM_SAMPLES];
int sampleIndex = 0;
bool bufferFull = false;

// ---- Debounce do digital ----
bool lastDigitalState      = LOW;
unsigned long lastDebounce = 0;
const int DEBOUNCE_MS      = 50;

// ---- Timer de envio ----
unsigned long lastSendTime = 0;

// ============================================================
void setup() {
  Serial1.begin(115200);
  delay(1000);

  pinMode(DIGITAL_PIN, INPUT_PULLUP);

  // Inicializa buffer de amostras em 0
  for (int i = 0; i < NUM_SAMPLES; i++) {
    analogSamples[i] = 0;
  }

  connectWiFi();
}

// ============================================================
void loop() {
  // 1. Leitura analógica com suavização
  int rawValue = analogRead(ANALOG_PIN);
  analogSamples[sampleIndex] = rawValue;
  sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;
  if (sampleIndex == 0) bufferFull = true;

  float smoothedValue = calcMovingAverage();
  // Converte o valor ADC (0-4095 no Pico W) para tensão (0.0 - 3.3V)
  float voltage = smoothedValue * (3.3f / 4095.0f);

  // 2. Leitura digital com debounce
  bool digitalReading = digitalRead(DIGITAL_PIN) == LOW; // LOW = pressionado (pullup)
  if (millis() - lastDebounce > DEBOUNCE_MS) {
    lastDigitalState = digitalReading;
    lastDebounce     = millis();
  }

  // 3. Reconexão Wi-Fi automática
  if (WiFi.status() != WL_CONNECTED) {
    Serial1.println("[WiFi] Desconectado, reconectando...");
    connectWiFi();
  }

  // 4. Envia telemetria no intervalo configurado
  if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = millis();

    sendTelemetry("temperature", "analog",   voltage,          false, false);
    sendTelemetry("presence",   "discrete",  0.0,              lastDigitalState, true);
  }

  delay(10);
}

// ============================================================
// Calcula a média móvel das amostras analógicas
float calcMovingAverage() {
  int count = bufferFull ? NUM_SAMPLES : sampleIndex;
  if (count == 0) return 0;

  long sum = 0;
  for (int i = 0; i < count; i++) {
    sum += analogSamples[i];
  }
  return (float)sum / count;
}

// ============================================================
// Conecta (ou reconecta) ao Wi-Fi
void connectWiFi() {
  Serial1.print("[WiFi] Conectando a ");
  Serial1.println(SSID);

  WiFi.begin("Wokwi-GUEST", "");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial1.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial1.println();
    Serial1.print("[WiFi] Conectado! IP: ");
    Serial1.println(WiFi.localIP());
  } else {
    Serial1.println();
    Serial1.println("[WiFi] Falha ao conectar. Tentará novamente no próximo ciclo.");
  }
}

// ============================================================
// Envia um pacote de telemetria para o backend
// isBoolean = true  → value é bool (discrete)
// isBoolean = false → value é float (analog)
void sendTelemetry(const char* sensorType,
                   const char* readingType,
                   float       floatValue,
                   bool        boolValue,
                   bool        isBoolean) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial1.println("[HTTP] Sem Wi-Fi, pulando envio.");
    return;
  }

  // Monta o timestamp simples (ms desde boot, pois Pico W não tem RTC interno)
  // Em produção, use NTP para obter a hora real
  char timestamp[30];
  unsigned long ms = millis();
  snprintf(timestamp, sizeof(timestamp), "2026-03-29T%02lu:%02lu:%02lu.000Z",
           (ms / 3600000UL) % 24,
           (ms / 60000UL)   % 60,
           (ms / 1000UL)    % 60);

  // Monta o JSON manualmente (simples, sem biblioteca extra)
  char body[256];
  if (isBoolean) {
    snprintf(body, sizeof(body),
      "{\"device_id\":\"%s\","
      "\"timestamp\":\"%s\","
      "\"sensor_type\":\"%s\","
      "\"reading_type\":\"%s\","
      "\"value\":%s}",
      DEVICE_ID, timestamp, sensorType, readingType,
      boolValue ? "true" : "false");
  } else {
    snprintf(body, sizeof(body),
      "{\"device_id\":\"%s\","
      "\"timestamp\":\"%s\","
      "\"sensor_type\":\"%s\","
      "\"reading_type\":\"%s\","
      "\"value\":%.4f}",
      DEVICE_ID, timestamp, sensorType, readingType, floatValue);
  }

  Serial1.print("[HTTP] Enviando: ");
  Serial1.println(body);

  // Tenta enviar com até 3 tentativas (retry)
  const int MAX_RETRIES = 3;
  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    HTTPClient http;
    http.begin(BACKEND_URL);
    http.addHeader("Content-Type", "application/json");

    int statusCode = http.POST(body);

    if (statusCode == 202) {
      Serial1.printf("[HTTP] OK (tentativa %d) → %d\n", attempt, statusCode);
      http.end();
      return; // sucesso, sai do loop
    } else {
      Serial1.printf("[HTTP] Falha (tentativa %d) → código %d\n", attempt, statusCode);
      http.end();
      delay(1000); // aguarda 1s antes de tentar novamente
    }
  }

  Serial1.println("[HTTP] Todas as tentativas falharam.");
}
