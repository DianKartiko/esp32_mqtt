#include <WiFi.h>
#include <PubSubClient.h>
#include <max6675.h>
#include <FS.h>
#include "SPIFFS.h"

// =================================================================
// --- KONFIGURASI SENSOR & JARINGAN (SESUAIKAN DI SINI) ---
// =================================================================

// --- Sensor Suhu Dryer 3 ---
int ktcSO = 19; // Pin MISO/SO
int ktcCS = 5;  // Pin Chip Select (CS)
int ktcCLK = 18; // Pin Clock (SCK)
MAX6675 thermocouple(ktcCLK, ktcCS, ktcSO);
const char* topic = "TOPIC";     // Topik MQTT untuk sensor
const char* data_file = "/data_suhu3.txt";  // File penyimpanan untuk sensor

// --- Konfigurasi Jaringan ---
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* mqtt_server = "SERVER"; // Ganti dengan IP Broker MQTT lokal Anda
const int mqtt_port = PORT;

// =================================================================
// --- VARIABEL GLOBAL & TIMER ---
// =================================================================
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long last_wifi_check_millis = 0;
unsigned long last_mqtt_check_millis = 0;
unsigned long last_sensor_read_millis = 0;

const long wifi_reconnect_interval = 20000;
const long mqtt_reconnect_interval = 5000;
const long sensor_read_interval = 5000; // Ambil data setiap 5 detik

// =================================================================
// --- FUNGSI-FUNGSI INTI (Tidak perlu diubah) ---
// =================================================================

// Menyimpan data ke file yang ditentukan
void saveData(float suhu, const char* filename) {
  File file = SPIFFS.open(filename, "a");
  if (!file) {
    Serial.printf("Gagal membuka file %s\n", filename);
    return;
  }
  unsigned long timestamp = time(NULL);
  file.print(String(timestamp) + "," + String(suhu) + "\n");
  file.close();
  Serial.printf("Data disimpan ke %s: ", filename);
  Serial.println(String(timestamp) + "," + String(suhu));
}

// Mengirim data tersimpan dari file yang ditentukan ke topik yang ditentukan
void sendStoredData(const char* filename, const char* topic) {
  if (!SPIFFS.exists(filename) || !client.connected()) return;

  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.printf("Gagal membuka file data %s\n", filename);
    return;
  }
  Serial.printf("Mengirim data tersimpan dari %s ke topik %s...\n", filename, topic);
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 0) {
      if (client.publish(topic, line.c_str())) {
        Serial.print("Terkirim: ");
        Serial.println(line);
      } else {
        Serial.println("Gagal kirim, coba lagi nanti.");
        file.close();
        return; // Hentikan jika satu pengiriman gagal
      }
      delay(200); // Beri jeda antar pesan
    }
  }
  file.close();
  SPIFFS.remove(filename); // Hapus file jika semua data berhasil dikirim
  Serial.printf("Semua data dari %s telah dikirim.\n", filename);
}

// Fungsi untuk membaca, mencetak, dan mengirim data sensor
void readAndPublishSensor(MAX6675& sensor, const char* topic, const char* dataFile, const char* sensorName) {
    float suhu = sensor.readCelsius();
    if (isnan(suhu)) {
      Serial.printf("Error: Gagal membaca %s!\n", sensorName);
      return;
    }

    Serial.printf("%s: %.2f *C\n", sensorName, suhu);

    String payload = String(time(NULL)) + "," + String(suhu);

    if (client.connected()) {
      if (client.publish(topic, payload.c_str())) {
        Serial.printf("Data %s terkirim ke MQTT: %s\n", sensorName, payload.c_str());
      } else {
        Serial.printf("Gagal kirim %s, menyimpan...\n", sensorName);
        saveData(suhu, dataFile);
      }
    } else {
      Serial.printf("MQTT Offline. Menyimpan data %s...\n", sensorName);
      saveData(suhu, dataFile);
    }
}

void manageWifi() {
  if (WiFi.status() != WL_CONNECTED && millis() - last_wifi_check_millis > wifi_reconnect_interval) {
    last_wifi_check_millis = millis();
    Serial.println("Mencoba hubungkan ulang WiFi...");
    WiFi.reconnect();
  }
}

void manageMqtt() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    if (millis() - last_mqtt_check_millis > mqtt_reconnect_interval) {
      last_mqtt_check_millis = millis();
      Serial.print("Mencoba koneksi MQTT...");
      String clientId = "ESP32-Dryer3-" + String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) {
        Serial.println(" BERHASIL!");
        // Setelah berhasil terhubung, kirim data yang tersimpan
        sendStoredData(data_file, topic);
      } else {
        Serial.print(" GAGAL, rc=");
        Serial.print(client.state());
        Serial.println(" Coba lagi...");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nMemulai ESP32 Dryer 3...");
  
  if (!SPIFFS.begin(true)) {
    Serial.println("Gagal SPIFFS!");
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");
  Serial.print("Alamat IP ESP32: ");
  Serial.println(WiFi.localIP());

  configTime(7 * 3600, 0, "pool.ntp.org"); // WIB = GMT+7
  
  client.setServer(mqtt_server, mqtt_port);
  Serial.println("Setup selesai.");
}

void loop() {
  manageWifi();
  manageMqtt();

  if (client.connected()) {
    client.loop();
  }
  
  if (millis() - last_sensor_read_millis > sensor_read_interval) {
    last_sensor_read_millis = millis();
    
    // Baca dan kirim data dari sensor
    readAndPublishSensor(thermocouple, topic, data_file, "Suhu Dryer 3");
  }
}
