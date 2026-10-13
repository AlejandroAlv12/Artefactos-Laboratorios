#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// ======================================================
// CONFIGURACIÓN WIFI
// ======================================================
const char* WIFI_SSID = "Alejandro's S23 Ultra";
const char* WIFI_PASSWORD = "brrrrrrr";

// ======================================================
// CONFIGURACIÓN MQTT
// ======================================================
const char* MQTT_SERVER = "10.175.96.178"; 
const int MQTT_PORT = 1883;

// Topics utilizados por el dashboard React
const char* TOPIC_TEMPERATURA = "clima/esp32/temperatura";
const char* TOPIC_HUMEDAD = "clima/esp32/humedad";
const char* TOPIC_LUZ = "clima/esp32/luz";

// ======================================================
// CONFIGURACIÓN DE SENSORES
// ======================================================
#define DHTPIN 18
#define LDR_PIN 34

DHTesp dht;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const unsigned long INTERVALO_ENVIO = 2000;
unsigned long ultimoEnvio = 0;
int contadorLecturas = 0;

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a Mosquitto...");
    String clientId = "ESP32-";
    clientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" conectado");
    } else {
      Serial.print(" error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 2 segundos...");
      delay(2000);
    }
  }
}

// ======================================================
// LEER Y PUBLICAR SENSORES
// ======================================================
void publicarSensores() {
  // 1. Obtener lectura de iluminación (convertida a porcentaje 0 - 100%)
  int valorLdr = analogRead(LDR_PIN);
  int porcentajeLuz = map(valorLdr, 0, 4095, 0, 100);
  char strLuz[8];
  snprintf(strLuz, sizeof(strLuz), "%d", porcentajeLuz);

  // Publicar luz independientemente de si el DHT falla o no
  mqttClient.publish(TOPIC_LUZ, strLuz, true);

  // 2. Obtener temperatura y humedad con DHTesp
  TempAndHumidity datos = dht.getTempAndHumidity();

  // Validar lectura del sensor DHT
  if (dht.getStatus() != DHTesp::ERROR_NONE) {
    Serial.print("Error al leer el sensor DHT. Causa: ");
    Serial.println(dht.getStatusString());
    Serial.print("Luz detectada actualmente: "); Serial.print(strLuz); Serial.println(" %");
    return;
  }

  // 3. Preparar los valores de temperatura y humedad para MQTT
  char strTemp[8];
  char strHum[8];
  dtostrf(datos.temperature, 1, 1, strTemp);
  dtostrf(datos.humidity, 1, 0, strHum);

  // 4. Publicar en los topics MQTT
  mqttClient.publish(TOPIC_TEMPERATURA, strTemp, true);
  mqttClient.publish(TOPIC_HUMEDAD, strHum, true);

  // 5. Imprimir en el Monitor Serial (ideal para las capturas del proyecto)
  contadorLecturas++;
  Serial.println("----------------------------------------");
  Serial.print("Medicion #"); Serial.println(contadorLecturas);
  Serial.print("Temperatura: "); Serial.print(strTemp); Serial.println(" °C");
  Serial.print("Humedad:     "); Serial.print(strHum); Serial.println(" %");
  Serial.print("Luz:         "); Serial.print(strLuz); Serial.println(" %");
  Serial.println("----------------------------------------");
}

void setup() {
  Serial.begin(115200);

  // Inicializar pin de luz
  pinMode(LDR_PIN, INPUT);

  // Inicializar sensor DHT11 con la librería optimizada
  dht.setup(DHTPIN, DHTesp::DHT11);
  delay(1000);

  conectarWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();

  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = millis();
    publicarSensores();
  }
}
