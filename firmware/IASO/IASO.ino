// IASO - monitor de signos vitales de astronauta. ESP32 + Nextion NX3224T024 + kit KY-xxx -> MQTT.
// Librerias (Library Manager): PubSubClient, OneWire, DallasTemperature, DHT sensor library.
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include "config.h"

WiFiClient net;
PubSubClient mqtt(net);
OneWire ow(PIN_DS18);
DallasTemperature ds(&ow);
DHT dht(PIN_DHT, DHT11);

// ---- pulso (KY-039): deteccion de picos + IBI para BPM y HRV (RMSSD) ----
#define NIBI 20
float ibi[NIBI]; int nIbi = 0, iIbi = 0;
float base = 0, sig = 0, env = 0;
uint32_t lastBeat = 0, lastSample = 0;
bool above = false;
int nMin = 4095, nMax = 0;  // ruido: pico a pico del KY-038 en la ventana de publicacion

void heartTick() {
  if (micros() - lastSample < 10000) return;  // 100 Hz
  lastSample = micros();
  int n = analogRead(PIN_NOISE); nMin = min(nMin, n); nMax = max(nMax, n);
  float raw = analogRead(PIN_HEART) * HR_SIGN;
  base += 0.01f * (raw - base);
  sig += 0.2f * ((raw - base) - sig);
  env = max(fabsf(sig), env * 0.995f);
  float thr = env * 0.5f;
  uint32_t now = millis();
  if (env > 20 && !above && sig > thr) {
    above = true;
    float d = now - lastBeat;
    if (lastBeat && d > 300 && d < 1500) { ibi[iIbi] = d; iIbi = (iIbi + 1) % NIBI; if (nIbi < NIBI) nIbi++; }
    lastBeat = now;
  } else if (sig < thr * 0.5f) above = false;
}

float bpm() {  // 0 = sin dedo / sin datos
  if (nIbi < 5 || millis() - lastBeat > 3000) return 0;
  float s = 0; for (int i = 0; i < nIbi; i++) s += ibi[i];
  return 60000.0f * nIbi / s;
}
float rmssd() {
  if (nIbi < 5 || millis() - lastBeat > 3000) return 0;
  // el buffer es circular: recorrer en orden cronologico
  float s = 0; int start = (nIbi == NIBI) ? iIbi : 0;
  for (int i = 1; i < nIbi; i++) {
    float a = ibi[(start + i - 1) % NIBI], b = ibi[(start + i) % NIBI];
    s += (b - a) * (b - a);
  }
  return sqrtf(s / (nIbi - 1));
}

// ---- movimiento y radiacion ----
volatile uint32_t vibCount = 0, radCount = 0;
void IRAM_ATTR onVib() { vibCount++; }
void IRAM_ATTR onRad() { radCount++; }
uint32_t lastMove = 0;

// ---- Nextion: actualiza componentes de texto por nombre (ver README) ----
void nex(const char* obj, const char* txt) {
  Serial2.printf("%s.txt=\"%s\"", obj, txt);
  Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
}

void wifiMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) { heartTick(); delay(10); }
    if (WiFi.status() != WL_CONNECTED) return;
  }
  if (!mqtt.connected()) {
    String t = String("iaso/") + ASTRO_ID + "/status";
    mqtt.connect(ASTRO_ID, nullptr, nullptr, t.c_str(), 0, true, "offline");
    if (mqtt.connected()) mqtt.publish(t.c_str(), "online", true);
  }
}

float baseHr = 0, baseHrv = 0; int hrN = 0;  // perfil basal individual (media movil lenta)
uint32_t lastPub = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, NEX_RX, NEX_TX);
  pinMode(PIN_VIB, INPUT_PULLUP); pinMode(PIN_RAD, INPUT); pinMode(PIN_BUZZ, OUTPUT);
  attachInterrupt(PIN_VIB, onVib, FALLING);
  attachInterrupt(PIN_RAD, onRad, FALLING);
  ds.begin(); dht.begin();
  ds.setWaitForConversion(false);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  analogSetAttenuation(ADC_11db);
  lastMove = millis(); lastPub = millis();
}

void loop() {
  heartTick();
  if (millis() - lastPub < PUBLISH_MS) return;
  uint32_t dt = millis() - lastPub; lastPub = millis();

  noInterrupts(); uint32_t vib = vibCount, rad = radCount; vibCount = radCount = 0; interrupts();
  if (vib) lastMove = millis();
  float cpm = rad * 60000.0f / dt;
  uint32_t still = millis() - lastMove;
  const char* act = (still > REST_MS) ? "rest" : "active";
  uint32_t restMin = still > REST_MS ? still / 60000 : 0;  // modulo de sueno: duracion aprox. del descanso actual

  float hr = bpm(), hrv = rmssd();
  float tb = ds.getTempCByIndex(0); ds.requestTemperatures();  // lectura de la conversion anterior
  float tc = dht.readTemperature(), hum = dht.readHumidity();
  int noise = nMax - nMin, light = analogRead(PIN_LIGHT);
  nMin = 4095; nMax = 0;

  // alertas: rangos fijos + desviacion respecto al basal propio
  String al = "";
  if (hr > 0) {
    if (hr > 120 || hr < 45) al += "HR ";
    if (hrN >= 12 && fabsf(hr - baseHr) > 0.25f * baseHr) al += "HR_BASAL ";
    // modulo de estres: caida de HRV >40% bajo el basal (sin GSR en el kit)
    if (hrN >= 12 && hrv > 0 && hrv < 0.6f * baseHrv) al += "ESTRES ";
    baseHr = hrN ? baseHr + 0.02f * (hr - baseHr) : hr;
    baseHrv = hrN ? baseHrv + 0.02f * (hrv - baseHrv) : hrv;
    if (hrN < 100) hrN++;
  }
  if (tb > -100 && (tb >= 38 || tb <= 35)) al += "TEMP ";
  if (!isnan(tc) && tc > 30) al += "CABINA ";
  if (cpm > RAD_CPM_MAX) al += "RAD ";
  digitalWrite(PIN_BUZZ, al.length() ? HIGH : LOW);

  char js[384];
  snprintf(js, sizeof js,
    "{\"hr\":%.0f,\"hrv\":%.0f,\"temp\":%.1f,\"cab_t\":%.1f,\"cab_h\":%.0f,\"move\":%u,\"act\":\"%s\",\"rest_min\":%lu,"
    "\"rad_cpm\":%.1f,\"noise\":%d,\"light\":%d,\"alerts\":\"%s\",\"up\":%lu}",
    hr, hrv, tb, isnan(tc) ? 0 : tc, isnan(hum) ? 0 : hum, vib, act, restMin, cpm, noise, light, al.c_str(), millis() / 1000);
  Serial.println(js);

  wifiMqtt();
  if (mqtt.connected()) mqtt.publish((String("iaso/") + ASTRO_ID + "/telemetry").c_str(), js);
  mqtt.loop();

  char b[32];
  snprintf(b, sizeof b, "%.0f", hr);        nex("t_hr", b);
  snprintf(b, sizeof b, "%.0f", hrv);       nex("t_hrv", b);
  snprintf(b, sizeof b, "%.1f", tb);        nex("t_temp", b);
  snprintf(b, sizeof b, "%.1f", cpm);       nex("t_rad", b);
  snprintf(b, sizeof b, "%.0fC %.0f%%", tc, hum); nex("t_cab", b);
  nex("t_act", act);
  nex("t_alert", al.length() ? al.c_str() : "OK");
}
