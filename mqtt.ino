#include <WiFi.h>
#include <PubSubClient.h>
#include <max6675.h>
#include <FS.h>       // Pustaka untuk File System
#include "SPIFFS.h"   // Pustaka untuk memori flash ESP32

// Konfigurasi PIN Sensor
int ktcSO = 15;
int ktcCS = 2;
int ktcCLK = 4;
MAX6675 thermocouple(ktcCLK, ktcCS, ktcSO);

// Setup WiFi
const char* ssid = "SSID";
const char* password = "PASS";

// Setup MQTT
const char* mqtt_server = "BROKER";
const int mqtt_port = PORT;
const char* topic = "TOPIC";

WiFiClient espClient;
PubSubClient client(espClient);

// --- BARU: Variabel untuk Store and Forward ---
const char* data_file = "/data_suhu.txt"; // Nama file untuk menyimpan data
unsigned long last_wifi_check = 0;
const long wifi_check_interval = 30000; // Coba hubungkan ulang WiFi setiap 30 detik

// --- FUNGSI BARU: Menyimpan data ke SPIFFS ---
void saveData(float suhu) {
  File file = SPIFFS.open(data_file, "a"); // Buka file dalam mode 'append' (menambahkan)
  if (!file) {
    Serial.println("Gagal membuka file untuk menyimpan data");
    return;
  }
  // Simpan data dengan format: timestamp,suhu
  // (timestamp berguna agar server tahu kapan data ini diambil)
  unsigned long timestamp = time(NULL);
  file.print(String(timestamp) + "," + String(suhu) + "\n");
  file.close();
  Serial.print("Data disimpan ke memori: ");
  Serial.println(String(timestamp) + "," + String(suhu));
}

// --- FUNGSI BARU: Mengirim data yang tersimpan dari SPIFFS ---
void sendStoredData() {
  if (!SPIFFS.exists(data_file)) {
    return; // Tidak ada file untuk dikirim, keluar
  }

  File file = SPIFFS.open(data_file, "r"); // Buka file dalam mode 'read' (membaca)
  if (!file) {
    Serial.println("Gagal membuka file data yang tersimpan");
    return;
  }

  Serial.println("Mulai mengirim data yang tersimpan...");

  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 0) {
      // Di sini kita bisa mem-parsing timestamp dan suhu jika perlu
      // Untuk sekarang, kita kirim saja seluruh baris
      if (client.publish(topic, line.c_str())) {
        Serial.print("Data tersimpan terkirim: ");
        Serial.println(line);
      } else {
        Serial.println("Gagal mengirim data tersimpan, akan dicoba lagi nanti.");
        file.close();
        return; // Hentikan jika pengiriman gagal agar tidak kehilangan data
      }
      delay(200); // Beri jeda agar tidak membanjiri server
    }
  }
  file.close();

  // Setelah semua data berhasil terkirim, hapus file
  SPIFFS.remove(data_file);
  Serial.println("Semua data tersimpan telah dikirim dan file dihapus.");
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Menghubungkan ke WiFi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) { // Coba selama 10 detik
    delay(500);
    Serial.print(".");
    retries++;
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi terhubung");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    // Sinkronisasi waktu dari internet (diperlukan untuk timestamp)
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Menunggu sinkronisasi waktu...");
    while (time(NULL) < 1000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWaktu sudah sinkron.");
  } else {
    Serial.println("\nGagal terhubung ke WiFi.");
  }
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Terhubung!");
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
  
  // --- BARU: Inisialisasi SPIFFS ---
  if (!SPIFFS.begin(true)) {
    Serial.println("Gagal menginisialisasi SPIFFS!");
    return;
  }
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("Menunggu MAX6675 siap...");
  delay(1000);
}

// --- MODIFIKASI TOTAL: Logika loop utama ---
void loop() {
  float suhu = thermocouple.readCelsius();
  if (isnan(suhu) || suhu == 0.0) {
    Serial.println("Error: Tidak bisa membaca sensor MAX6675!");
    delay(1000);
    return;
  }

  // --- KONDISI 1: JIKA TERHUBUNG WIFI ---
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect_mqtt();
    }
    client.loop();

    // Prioritas utama: kirim data yang tersimpan jika ada
    sendStoredData();

    // Kirim data saat ini secara langsung
    char msg[50];
    dtostrf(suhu, 6, 2, msg);
    
    if (client.publish(topic, msg)) {
      Serial.print("Data langsung terkirim ke MQTT: ");
      Serial.println(msg);
    } else {
      Serial.println("Gagal kirim data langsung, akan disimpan.");
      saveData(suhu); // Jika gagal kirim saat terkoneksi, simpan saja
    }

  // --- KONDISI 2: JIKA TIDAK TERHUBUNG WIFI ---
  } else {
    Serial.println("Koneksi WiFi terputus. Menyimpan data...");
    saveData(suhu); // Simpan data ke memori

    // Coba hubungkan ulang WiFi secara non-blocking
    unsigned long current_millis = millis();
    if (current_millis - last_wifi_check >= wifi_check_interval) {
      last_wifi_check = current_millis;
      Serial.println("Mencoba menghubungkan ulang WiFi...");
      WiFi.reconnect();
    }
  }
  
  delay(30000); // Ambil data setiap 30 detik
}
