#pragma once
// WiFi (2.4 GHz)
#define WIFI_SSID   "TU_WIFI"
#define WIFI_PASS   "TU_PASSWORD"

// Broker Mosquitto publico y gratis (sin cuenta). Cualquiera puede leer los topics:
// cambia ASTRO_ID por algo unico y no uses datos reales de personas.
#define MQTT_HOST   "test.mosquitto.org"
#define MQTT_PORT   1883
#define ASTRO_ID    "iaso-astro01-x7k2"

// Pines ESP32 (ADC1 solamente: el ADC2 no funciona con WiFi activo)
#define PIN_HEART   34   // KY-039 pulso (analogico)
#define PIN_NOISE   32   // KY-038 microfono (analogico)
#define PIN_LIGHT   35   // KY-018 fotorresistor (analogico)
#define PIN_DS18    4    // KY-001 DS18B20 temperatura corporal (necesita pull-up 4.7k a 3V3)
#define PIN_DHT     5    // KY-015 DHT11 temp/humedad de cabina
#define PIN_VIB     27   // KY-002 vibracion -> movimiento
#define PIN_RAD     33   // pulso de contador Geiger (NO viene en el kit)
#define PIN_BUZZ    25   // KY-012 buzzer activo (alertas)
#define NEX_RX      16   // ESP32 RX2 <- Nextion TX
#define NEX_TX      17   // ESP32 TX2 -> Nextion RX

#define HR_SIGN       1      // pon -1 si tu KY-039 da la senal invertida
#define PUBLISH_MS    5000
#define REST_MS       600000UL  // sin movimiento 10 min => "rest"
#define RAD_CPM_MAX   100       // ajustar al dosimetro real
