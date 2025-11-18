/*
 * Sistema de Irrigação Inteligente (Protótipo)
 * ESP32 + Sensor de Umidade do AR (DHT) + Relé (simulando irrigador)
 * Envio de dados e comandos via Firebase Realtime Database (API REST)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ======================= CONFIGURAÇÕES BÁSICAS =======================

// --- Wi-Fi ---
const char* WIFI_SSID = "SEU_WIFI_AQUI";
const char* WIFI_PASS = "SENHA_DO_WIFI";

// --- Firebase ---
// URL base do Realtime Database (SEM barra no final)
const char* FIREBASE_HOST = "https://SEU_PROJETO-default-rtdb.firebaseio.com";

// Caminho raiz do sistema de irrigação
const char* FIREBASE_ROOT = "/irrigacao";

// ======================= PINOS E SENSOR =======================

// DHT (umidade do ar)
#define DHTPIN 4
#define DHTTYPE DHT11      // troque para DHT22 se for o seu
DHT dht(DHTPIN, DHTTYPE);

// Relé (simula irrigador)
const int RELAY_PIN = 23;

// ======================= CONTROLE DE LÓGICA =======================

// Estado desejado do irrigador (lido do Firebase)
bool estadoIrrigador = false;
// Estado atualmente aplicado ao relé (pra detectar mudança)
bool estadoIrrigadorAtual = false;

// Controle de tempo para leituras e consultas
unsigned long lastSensorRead = 0;
unsigned long lastDBRead = 0;
const unsigned long sensorReadInterval = 10000; // 10s
const unsigned long dbReadInterval     = 3000;  // 3s

// Para logs de duração
unsigned long momentoUltimaMudanca = 0;
float ultimaUmidadeLida = 0.0;

// ======================= FUNÇÕES AUXILIARES =======================

// Monta a URL completa do Firebase para um caminho (sem .json)
String makeFirebaseUrl(const String& pathWithoutJson) {
  // pathWithoutJson deve começar com "/"
  return String(FIREBASE_HOST) + pathWithoutJson + ".json";
}

// Envia um JSON via PUT (substitui o nó)
bool firebasePUT(const String& path, const String& jsonPayload) {
  String url = makeFirebaseUrl(path);

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.PUT(jsonPayload);

  Serial.print("[PUT] ");
  Serial.print(url);
  Serial.print(" -> HTTP ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    String resp = http.getString();
    Serial.println("Resposta: " + resp);
  }

  http.end();
  return (httpCode >= 200 && httpCode < 300);
}

// Envia um JSON via POST (cria nó com ID automático, usado em logs)
bool firebasePOST(const String& path, const String& jsonPayload) {
  String url = makeFirebaseUrl(path);

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(jsonPayload);

  Serial.print("[POST] ");
  Serial.print(url);
  Serial.print(" -> HTTP ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    String resp = http.getString();
    Serial.println("Resposta: " + resp);
  }

  http.end();
  return (httpCode >= 200 && httpCode < 300);
}

// Faz um GET e retorna o payload como String
String firebaseGET(const String& path) {
  String url = makeFirebaseUrl(path);

  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();
  Serial.print("[GET] ");
  Serial.print(url);
  Serial.print(" -> HTTP ");
  Serial.println(httpCode);

  String payload = "";
  if (httpCode > 0) {
    payload = http.getString();
    Serial.println("Payload: " + payload);
  }

  http.end();
  return payload;
}

// ======================= FUNÇÕES DE LÓGICA =======================

// Conexão Wi-Fi
void conectarWiFi() {
  Serial.print("Conectando ao WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// Lê umidade do DHT, envia ao Firebase em /irrigacao/sensor
void lerEEnviarUmidade() {
  float umidade = dht.readHumidity();

  if (isnan(umidade)) {
    Serial.println("Falha na leitura do DHT");
    return;
  }

  ultimaUmidadeLida = umidade;

  Serial.print("Umidade do ar: ");
  Serial.print(umidade);
  Serial.println(" %");

  // Monta JSON simples
  String json = "{";
  json += "\"umidade_percentual\":" + String(umidade, 1);
  json += "}";

  // PUT em /irrigacao/sensor
  firebasePUT(String(FIREBASE_ROOT) + "/sensor", json);
}

// Lê o comando do atuador em /irrigacao/atuador/estado
void escutarComandosFirebase() {
  String payload = firebaseGET(String(FIREBASE_ROOT) + "/atuador/estado");

  // Se ainda não existir, inicializa como false
  if (payload == "null" || payload.length() == 0) {
    firebasePUT(String(FIREBASE_ROOT) + "/atuador", "{\"estado\":false}");
    estadoIrrigador = false;
    return;
  }

  // Payload vem como "true" ou "false"
  payload.trim();
  if (payload.indexOf("true") != -1) {
    estadoIrrigador = true;
  } else {
    estadoIrrigador = false;
  }

  Serial.print("Estado do irrigador (Firebase): ");
  Serial.println(estadoIrrigador ? "LIGADO" : "DESLIGADO");
}

// Registra log em /irrigacao/logs
void registrarLog(const String& evento, float umidade, unsigned long duracaoMs) {
  // Aqui, por simplicidade, o "timestamp" é o tempo desde que a placa ligou (millis)
  String json = "{";
  json += "\"timestamp_ms\":" + String(millis());
  json += ",\"evento\":\"" + evento + "\"";
  json += ",\"duracao_ms\":" + String(duracaoMs);
  json += ",\"umidade\":" + String(umidade, 1);
  json += "}";

  firebasePOST(String(FIREBASE_ROOT) + "/logs", json);
}

// Atualiza o relé e registra logs quando há mudança de estado
void atualizarRele() {
  if (estadoIrrigador != estadoIrrigadorAtual) {
    // Houve mudança de estado -> gerar log
    unsigned long agora = millis();
    unsigned long duracao = (momentoUltimaMudanca == 0) ? 0 : (agora - momentoUltimaMudanca);
    momentoUltimaMudanca = agora;

    String evento = estadoIrrigador ? "Irrigador Ligado" : "Irrigador Desligado";
    registrarLog(evento, ultimaUmidadeLida, duracao);

    // Atualiza também /irrigacao/atuador/ultimo_acionamento
    String json = "{";
    json += "\"estado\":" + String(estadoIrrigador ? "true" : "false");
    json += ",\"ultimo_acionamento_ms\":" + String(agora);
    json += "}";
    firebasePUT(String(FIREBASE_ROOT) + "/atuador", json);

    estadoIrrigadorAtual = estadoIrrigador;
  }

  // Aqui assumo relé ativo em HIGH (ajuste se o seu for ao contrário)
  digitalWrite(RELAY_PIN, estadoIrrigador ? HIGH : LOW);
}

// ======================= SETUP E LOOP =======================

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // começa desligado

  conectarWiFi();

  Serial.println("Sistema de Irrigacao Inteligente - Iniciado");
  momentoUltimaMudanca = millis();
}

void loop() {
  unsigned long agora = millis();

  // 1. Ler e enviar umidade periodicamente
  if (agora - lastSensorRead > sensorReadInterval) {
    lastSensorRead = agora;
    lerEEnviarUmidade();
  }

  // 2. Buscar comando de estado do irrigador no Firebase
  if (agora - lastDBRead > dbReadInterval) {
    lastDBRead = agora;
    escutarComandosFirebase();
  }

  // 3. Atualizar relé e logs
  atualizarRele();
}