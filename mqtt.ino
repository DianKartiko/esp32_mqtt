#include <WiFi.h>
#include <PubSubClient.h>
#include <max6675.h>

// Konfigurasi PIN Sensor
int ktcSO = 15;  // D6
int ktcCS = 2;  // D8
int ktcCLK = 4; // D5

MAX6675 thermocouple(ktcSO, ktcCS, ktcCLK);

// Setup WiFi
const char* ssid = "WIJAYA";
const char* password = "wijaya21";

// Setup MQTT
const char* mqtt_server = "broker.emqx.io"; // atau IP broker lokal
const int mqtt_port = 1883;
const char* topic = "esp32/suhu";

// Register WiFi Client and PubSubClien
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
  // Loop sampai terkoneksi
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    // ClientID harus unik, jadi tambahkan random number
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);

    // connect tanpa username & password
    if (client.connect(clientId.c_str())) {
      Serial.println("Terhubung!");
      client.subscribe(topic);
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
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Paksa gunakan MQTT 3.1.1
  #ifdef MQTT_VERSION
  #undef MQTT_VERSION
  #define MQTT_VERSION MQTT_VERSION_3_1_1
  #endif

  delay(2000); // MAX6675 butuh waktu start
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Baca suhu dari MAX6675
  float suhu = thermocouple.readCelsius();
  Serial.print("Suhu: ");
  Serial.println(suhu);

  // Kirim ke MQTT
  char msg[50];
  dtostrf(suhu, 6, 2, msg);
  client.publish(topic, msg);

  delay(2000); // kirim setiap 2 detik
}