#pragma once
// Red propia del ESP32: conecta el celular y abre http://192.168.4.1
#define AP_SSID     "IASO-astro01"
#define AP_PASS     "iaso1234"     // minimo 8 caracteres

// WiFi con internet (2.4 GHz) para MQTT. Dejalo "" para usar solo la red propia.
#define WIFI_SSID   ""
#define WIFI_PASS   ""

// Broker Mosquitto publico y gratis (sin cuenta). Cualquiera puede leer los topics:
// cambia ASTRO_ID por algo unico y no uses datos reales de personas.
#define MQTT_HOST   "test.mosquitto.org"
#define MQTT_PORT   1883
#define ASTRO_ID    "iaso-astro01-x7k2"

// Pines ESP32-S3. Analogicos solo en ADC1 (GPIO1-10): el ADC2 no funciona con WiFi activo.
// Evitar: 0/3/45/46 (arranque), 19/20 (USB), 26-37 (flash/PSRAM), 43/44 (USB-serie), 38/48 (LED RGB de la placa)
#define PIN_HEART   1    // KY-039 pulso (analogico)
#define PIN_LIGHT   4    // KY-018 fotorresistor (analogico)
#define PIN_DS18    5    // KY-001 DS18B20 temperatura corporal (necesita pull-up 4.7k a 3V3)
#define PIN_DHT     6    // KY-015 DHT11 temp/humedad de cabina
#define PIN_VIB     7    // KY-002 vibracion -> movimiento
#define PIN_RAD     15   // pulso de contador Geiger (NO viene en el kit)
#define PIN_BUZZ    21   // KY-012 buzzer activo (alertas)
#define NEX_RX      16   // ESP32 RX2 <- Nextion TX
#define NEX_TX      17   // ESP32 TX2 -> Nextion RX

#define HR_SIGN       1      // pon -1 si tu KY-039 da la senal invertida
#define PUBLISH_MS    5000
#define REST_MS       600000UL  // sin movimiento 10 min => "rest"
#define RAD_CPM_MAX   100       // ajustar al dosimetro real
