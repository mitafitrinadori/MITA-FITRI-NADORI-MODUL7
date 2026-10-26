/*
 * ==============================================================================
 * PRAKTIKUM 7: SIMPUL SENSOR-AKTUATOR IOT UNTUK BROKER LOKAL
 * ==============================================================================
 * Membaca sensor DHT22, mengirim telemetri JSON ke broker Mosquitto lokal,
 * serta menerima instruksi kendali LED melalui MQTT.
 * ==============================================================================
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// 1. Kredensial Wi-Fi
const char* ssid     = "OPPO A54";
const char* password = "yyyyyyyy";

// 2. Konfigurasi Broker MQTT Lokal
const char* mqttServer = "172.31.190.1";
const int mqttPort = 1883;

// 3. Identitas Simpul dan Topik MQTT
const char* clientId = "ESP8266_Local_Node01";
const char* topicPubTele = "kampus/labiot/telemetri";
const char* topicSubCmd = "kampus/labiot/kendali";

// 4. Pin Perangkat Keras
const byte dhtPin = 2; // D4 (GPIO 2)
const byte ledPin = 5; // D1 (GPIO 5)

#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTeleTime = 0;
const unsigned long teleInterval = 4000;

// Penanganan pesan kendali dari broker MQTT
void onMessageCallback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("\n----------------------------------------");
  Serial.printf("[INSTRUKSI MASUK] Topik: %s\n", topic);
  Serial.printf("[PAYLOAD]         : %s\n", incomingBuffer);

  // Format JSON kendali: {"led": 1} atau {"led": 0}
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.printf("[ERROR] Gagal parsing JSON instruksi: %s\n", err.c_str());
    return;
  }

  if (doc.containsKey("led")) {
    int stateLED = doc["led"];
    digitalWrite(ledPin, stateLED == 1 ? HIGH : LOW);

    Serial.printf(
      "--> [AKTUATOR] Status LED diubah: %s\n",
      stateLED == 1 ? "MENYALA (ON)" : "PADAM (OFF)"
    );
  } else {
    Serial.println("[WARNING] Kunci 'led' tidak ditemukan dalam payload!");
  }

  Serial.println("----------------------------------------");
}

// Menghubungkan ESP8266 ke hotspot
void connectToWiFi() {
  Serial.println();
  Serial.print("[Wi-Fi] Menghubungkan ke ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi] Berhasil Terhubung!");
  Serial.print("[Wi-Fi] IP NodeMCU: ");
  Serial.println(WiFi.localIP());
}

// Menghubungkan ESP8266 ke broker Mosquitto
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("[MQTT] Menghubungkan ke Broker Lokal ");
    Serial.print(mqttServer);
    Serial.print("... ");

    if (client.connect(clientId, "admin", "rahasia123")) {
      Serial.println("Terhubung dengan Autentikasi Sah!");

      client.subscribe(topicSubCmd);
      Serial.printf(
        "[MQTT] Berlangganan ke topik: %s\n",
        topicSubCmd
      );
    } else {
      Serial.printf(
        "Gagal (rc=%d). Coba lagi dalam 4 detik...\n",
        client.state()
      );
      delay(4000);
    }
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();
  connectToWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageCallback);
  client.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  // Pengiriman telemetri setiap 4 detik
  unsigned long now = millis();

  if (now - lastTeleTime >= teleInterval) {
    lastTeleTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      StaticJsonDocument<256> docOut;

      docOut["node_id"] = clientId;
      docOut["suhu"] = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);

      char teleBuffer[256];
      serializeJson(docOut, teleBuffer);

      bool success = client.publish(topicPubTele, teleBuffer);

      if (success) {
        Serial.printf(
          "[TELEMETRI TERKIRIM] %s\n",
          teleBuffer
        );
      } else {
        Serial.println(
          "[ERROR] Gagal mengirim telemetri ke broker lokal!"
        );
      }
    } else {
      Serial.println(
        "[ERROR] Gagal membaca data dari sensor DHT fisik!"
      );
    }
  }
}