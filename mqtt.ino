#include <WiFi.h>
#include <PubSubClient.h>
#include <max6675.h>

// Konfigurasi PIN Sensor - PERBAIKAN: Urutan parameter yang benar
int ktcSO = 19;  // MISO
int ktcCS = 5;   // CS
int ktcCLK = 18; // SCK

// PERBAIKAN: Constructor MAX6675 dengan urutan yang benar (CLK, CS, SO)
MAX6675 thermocouple(ktcCLK, ktcCS, ktcSO);

// Setup WiFi
const char* ssid = "WIJAYA";
const char* password = "wijaya21";

// Setup MQTT
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* topic = "esp32/suhu";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Menghubungkan ke WiFi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi terhubung");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("Terhubung!");
      // Optional: subscribe jika perlu menerima data
      // client.subscribe(topic);
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 5 detik");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Memulai ESP32 MAX6675 MQTT...");
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // PERBAIKAN: Berikan waktu yang cukup untuk MAX6675 initialization
  Serial.println("Menunggu MAX6675 siap...");
  delay(3000);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // PERBAIKAN: Baca suhu dengan error checking
  float suhu = thermocouple.readCelsius();
  
  // PERBAIKAN: Validasi pembacaan sensor
  if (isnan(suhu) || suhu == 0.0) {
    Serial.println("Error: Tidak bisa membaca sensor MAX6675!");
    delay(000);
    return;
  }

  // PERBAIKAN: Cek apakah suhu dalam range wajar
  if (suhu < -50 || suhu > 1000) {
    Serial.println("Error: Pembacaan suhu tidak valid!");
    Serial.print("Suhu terbaca: ");
    Serial.println(suhu);
    delay(1000);
    return;
  }

  Serial.print("Suhu: ");
  Serial.print(suhu);
  Serial.println(" °C");

  // Kirim ke MQTT
  char msg[50];
  dtostrf(suhu, 6, 2, msg);
  
  if (client.publish(topic, msg)) {
    Serial.print("Data terkirim ke MQTT: ");
    Serial.println(msg);
  } else {
    Serial.println("Gagal kirim data ke MQTT!");
  }

  // PERBAIKAN: Delay yang cukup untuk MAX6675 (minimal 250ms antar pembacaan)
  delay(3000);
}
