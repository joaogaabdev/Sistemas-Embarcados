#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ================= CONFIG WiFi =================
const char* WIFI_SSID = "SEU_WIFI_AQUI";
const char* WIFI_PASS = "SUA_SENHA_AQUI";

// ================= CONFIG Firebase =================
// URL base do Realtime Database (SEM barra no final)
const char* FIREBASE_HOST = "https://testeproject-9a60a-default-rtdb.firebaseio.com";

// Caminho raiz do sistema
const char* FIREBASE_ROOT = "/irrigacao";

// ================= SENSOR DHT =================
#define DHTPIN 4
#define DHTTYPE DHT11   // troque para DHT22 se for o seu
DHT dht(DHTPIN, DHTTYPE);

// ================= RELÉ =================
const int RELAY_PIN = 23;

// Umidade abaixo disso = "seco" -> irrigar
const int LIMITE_UMIDADE = 40;

// Estados
bool estadoIrrigadorFirebase = false;   // o que veio do Firebase
bool estadoIrrigadorAplicado = false;   // o que já está no relé

// Timers
unsigned long lastSensorRead = 0;
unsigned long lastDBRead = 0;
const unsigned long sensorReadInterval = 10000; // 10s
const unsigned long dbReadInterval = 3000;      // 3s

// =============== FUNÇÕES AUXILIARES ===============

String makeFirebaseUrl(const String& pathWithoutJson) {
  // pathWithoutJson deve começar com "/"
  return String(FIREBASE_HOST) + pathWithoutJson + ".json";
}

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

// =============== LÓGICA: DHT ➜ Firebase =================

// 1) Ler umidade, enviar para /sensor e decidir se irrigar
//    Depois mandar TRUE/FALSE para /atuador/estado
void lerUmidadeEAtualizarFirebase() {
  float umidade = dht.readHumidity();

  if (isnan(umidade)) {
    Serial.println("Falha na leitura do DHT");
    return;
  }

  Serial.print("Umidade do ar: ");
  Serial.print(umidade);
  Serial.println(" %");

  // Enviar umidade para /irrigacao/sensor
  String jsonSensor = "{";
  jsonSensor += "\"umidade_percentual\":" + String(umidade, 1);
  jsonSensor += "}";
  firebasePUT(String(FIREBASE_ROOT) + "/sensor", jsonSensor);

  // Decidir se deve irrigar com base na umidade
  bool irrigar = (umidade < LIMITE_UMIDADE);

  Serial.print("Decisao local (baseada na umidade): ");
  Serial.println(irrigar ? "IRRIGAR (true)" : "NAO IRRIGAR (false)");

  // Mandar esse true/false para /irrigacao/atuador/estado
  // Enviamos um booleano simples (JSON: true ou false)
  firebasePUT(String(FIREBASE_ROOT) + "/atuador/estado",
              irrigar ? "true" : "false");
}

// 2) Ler /irrigacao/atuador/estado do Firebase
//    e atualizar variavel estadoIrrigadorFirebase
void lerEstadoDoFirebase() {
  String payload = firebaseGET(String(FIREBASE_ROOT) + "/atuador/estado");

  if (payload == "null" || payload.length() == 0) {
    Serial.println("Estado ainda nao definido no Firebase, mantendo false.");
    estadoIrrigadorFirebase = false;
    return;
  }

  payload.trim();
  if (payload.indexOf("true") != -1) {
    estadoIrrigadorFirebase = true;
  } else {
    estadoIrrigadorFirebase = false;
  }

  Serial.print("Estado recebido do Firebase: ");
  Serial.println(estadoIrrigadorFirebase ? "true" : "false");
}

// 3) Aplicar o estado vindo do Firebase no relé
//    e dar o print de LIGADO / DESLIGADO
void aplicarEstadoNoRele() {
  if (estadoIrrigadorFirebase != estadoIrrigadorAplicado) {
    // Houve mudança -> atualiza
    estadoIrrigadorAplicado = estadoIrrigadorFirebase;

    // Relé ativo em LOW (mais comum)
    digitalWrite(RELAY_PIN, estadoIrrigadorAplicado ? LOW : HIGH);

    Serial.print("IRRIGADOR: ");
    Serial.println(estadoIrrigadorAplicado ? "LIGADO" : "DESLIGADO");
    Serial.println("--------------------------------------");
  }
}

// =============== SETUP / LOOP ===============

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  // relé começa desligado
  digitalWrite(RELAY_PIN, HIGH); // para relé ativo em LOW

  conectarWiFi();

  Serial.println("Sistema de Irrigacao Inteligente iniciado");
}

void loop() {
  unsigned long agora = millis();

  // A cada 10s: ler umidade, mandar para Firebase e escrever estado (true/false)
  if (agora - lastSensorRead > sensorReadInterval) {
    lastSensorRead = agora;
    lerUmidadeEAtualizarFirebase();
  }

  // A cada 3s: ler o estado do Firebase e aplicar no relé
  if (agora - lastDBRead > dbReadInterval) {
    lastDBRead = agora;
    lerEstadoDoFirebase();
    aplicarEstadoNoRele();
  }
}