#include <WiFi.h>
#include <HTTPClient.h>
#include <max6675.h>

// Konfigurasi PIN Sensor MAX6675
int ktcSO = 15;  // MISO
int ktcCS = 2;   // CS
int ktcCLK = 4;  // SCK

MAX6675 thermocouple(ktcCLK, ktcCS, ktcSO);

// Konfigurasi WiFi
const char* ssid = "monitoring ";
const char* password = "wijaya21";

// Ganti dengan URL API Flask kamu di Fly.io
const char* serverUrl = "https://your-app.fly.dev/data";  

void setup() {
  Serial.begin(115200);
  Serial.println("Memulai ESP32 MAX6675 HTTP...");

  // Hubungkan WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi terhubung");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Tunggu sensor siap
  Serial.println("Menunggu MAX6675 siap...");
  delay(3000);
}

void loop() {
  // Baca suhu dari MAX6675
  float suhu = thermocouple.readCelsius();

  if (isnan(suhu) || suhu == 0.0) {
    Serial.println("Error: Tidak bisa membaca sensor MAX6675!");
    delay(1000);
    return;
  }

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

  // Kirim data via HTTP POST
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"suhu\": " + String(suhu, 2) + "}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Response Code: ");
      Serial.println(httpResponseCode);
      Serial.print("Response: ");
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi tidak terhubung!");
  }

  delay(5000); // kirim data tiap 5 detik
}
