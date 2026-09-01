#include <Arduino.h>
#include "Seeed_Arduino_mmWave.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ====== CONFIGURAÇÃO WI-FI ======
const char* ssid = "NomeInternet";
const char* password = "Password";

// ====== CONFIGURAÇÃO FIREBASE ======
#define API_KEY "API KEY"
#define DATABASE_URL "DATA BASE FIREBASE URL"

// ====== SENSOR ======
#ifdef ESP32
  #include <HardwareSerial.h>
  HardwareSerial mmWaveSerial(0);
#else
  #define mmWaveSerial Serial1
#endif

SEEED_MR60BHA2 mmWave;

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// Variáveis do sensor
float respiracao = 0;
float batimento = 0;
float distancia = 0;

unsigned long lastSend = 0;
const unsigned long INTERVALO_ENVIO = 10000;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== MR60BHA2 + Firebase ===");

  // Iniciar o sensor
  mmWave.begin(&mmWaveSerial);
  Serial.println("Sensor inicializado!");

  // Conectar ao Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("A conectar ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado! IP: " + WiFi.localIP().toString());

  // Configurar a Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Autenticação anónima
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase: autenticação anónima OK");
    signupOK = true;
  } else {
    Serial.printf("Erro Firebase: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // Atualizar os dados do sensor
  if (mmWave.update(100)) {
    mmWave.getBreathRate(respiracao);
    mmWave.getHeartRate(batimento);
    mmWave.getDistance(distancia);
  }

  if (Firebase.ready() && signupOK && (millis() - lastSend > INTERVALO_ENVIO)) 
  {
      lastSend = millis();

      // Criar JSON com os dados da leitura
      FirebaseJson json;
      json.set("batimento", batimento);
      json.set("respiracao", respiracao);
      json.set("distancia", distancia);
      json.set("timestamp/.sv", "timestamp");

      // Adicionar nova entrada em /leituras/
      if (Firebase.RTDB.pushJSON(&fbdo, "/leituras", &json)) {
        Serial.printf("Leitura guardada — BPM: %.1f | RPM: %.1f | Dist: %.1f cm\n",
                      batimento, respiracao, distancia);
      } else {
        Serial.println("Erro ao guardar: " + fbdo.errorReason());
      }
  }
}