// IASO - monitor de signos vitales de astronauta. ESP32 + Nextion NX3224T024 + kit KY-xxx -> MQTT.
// Librerias (Library Manager): PubSubClient, OneWire, DallasTemperature, DHT sensor library.
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"
#include "dashboard_html.h"

WebServer web(80);
Preferences prefs;
char js[384] = "{}";   // ultima telemetria, servida en /data
String labJson;        // ultimos datos manuales (laboratorio, SpO2, presion...), guardados en flash
String basalJson;      // basal individual de esos mismos datos, guardado en flash
String perfilJson;     // perfil del astronauta (sexo, nacimiento, altura, peso, meta de ejercicio), guardado en flash

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

void heartTick() {
  if (micros() - lastSample < 10000) return;  // 100 Hz
  lastSample = micros();
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

// ---- Nextion: pantalla tactil disenada en nextion/IASO.HMI (cargar con NextionBridge/upload_tft.py) ----
// page0 Vitales: t7 pulso, t8 HRV, t9 temp, t14 alertas | page1 Ambiente: t10 rad, t11 cabina, t12 actividad, t13 luz
// page2 Sistema: t7 encendido, t8 red, t9 MQTT, b3 silenciar (envia 'M'), h0 brillo (lo maneja la pantalla)
// los valores son vscope global, por eso se pueden escribir aunque su pagina no este visible
void nexCmd(const char* c) {
  Serial2.print(c); Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
}
void nexTxt(const char* obj, const char* val) {
  char c[80]; snprintf(c, sizeof c, "%s.txt=\"%s\"", obj, val); nexCmd(c);
}
bool muted = false;  // alarma silenciada desde la pantalla; se rearma sola cuando ya no hay alertas
void nexInput() {
  while (Serial2.available())
    if (Serial2.read() == 'M') {
      muted = !muted;
      if (muted) digitalWrite(PIN_BUZZ, LOW);
      nexTxt("page2.b3", muted ? "Silenciado" : "Silenciar");
    }
}

// guarda un JSON manual (lab, basal o perfil) en flash y lo publica retained si hay MQTT
void saveManual(const char* name, String& store) {
  String b = web.arg("plain");
  if (b.length() > 600 || !b.startsWith("{")) { web.send(400); return; }
  store = b; prefs.putString(name, b);
  if (mqtt.connected()) mqtt.publish((String("iaso/") + ASTRO_ID + "/" + name).c_str(), b.c_str(), true);
  web.send(204);
}

void wifiMqtt() {
  // el ESP32 reconecta solo; no repetir WiFi.begin() porque cada intento corta la red propia
  if (WiFi.status() != WL_CONNECTED) return;
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
  Serial2.setTxBufferSize(1024);  // a 9600 baud la pantalla tarda ~0.7 s: sin buffer bloquearia el muestreo del pulso
  Serial2.begin(9600, SERIAL_8N1, NEX_RX, NEX_TX);
  nexCmd("");  // limpia cualquier basura en el buffer de la pantalla
  pinMode(PIN_VIB, INPUT_PULLUP); pinMode(PIN_RAD, INPUT); pinMode(PIN_BUZZ, OUTPUT);
  attachInterrupt(PIN_VIB, onVib, FALLING);
  attachInterrupt(PIN_RAD, onRad, FALLING);
  ds.begin(); dht.begin();
  ds.setWaitForConversion(false);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024);  // el default (256 B) corta la telemetria con alertas y los datos de laboratorio
  analogSetAttenuation(ADC_11db);

  WiFi.mode(WIFI_AP_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // menos pico de corriente (evita brownout con USB debil); alcanza ~10 m
  WiFi.softAP(AP_SSID, AP_PASS);
  if (strlen(WIFI_SSID)) WiFi.begin(WIFI_SSID, WIFI_PASS);
  prefs.begin("iaso");
  labJson = prefs.getString("lab", "{}");
  basalJson = prefs.getString("basal", "{}");
  perfilJson = prefs.getString("perfil", "{}");
  web.on("/", [] { web.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML); });
  web.on("/data", [] { web.send(200, "application/json", js); });
  // /lab = valores actuales, /basal = referencia individual; el dashboard compara uno contra otro
  web.on("/lab", HTTP_GET, [] { web.send(200, "application/json", labJson); });
  web.on("/lab", HTTP_POST, [] { saveManual("lab", labJson); });
  web.on("/basal", HTTP_GET, [] { web.send(200, "application/json", basalJson); });
  web.on("/basal", HTTP_POST, [] { saveManual("basal", basalJson); });
  web.on("/perfil", HTTP_GET, [] { web.send(200, "application/json", perfilJson); });
  web.on("/perfil", HTTP_POST, [] { saveManual("perfil", perfilJson); });
  web.begin();
  Serial.printf("Red %s -> http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  lastMove = millis(); lastPub = millis();
}

void loop() {
  heartTick();
  nexInput();
  web.handleClient();
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
  int light = analogRead(PIN_LIGHT);

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
  if (!al.length() && muted) { muted = false; nexTxt("page2.b3", "Silenciar"); }
  digitalWrite(PIN_BUZZ, al.length() && !muted ? HIGH : LOW);

  snprintf(js, sizeof js,
    "{\"hr\":%.0f,\"hrv\":%.0f,\"temp\":%.1f,\"cab_t\":%.1f,\"cab_h\":%.0f,\"move\":%u,\"act\":\"%s\",\"rest_min\":%lu,"
    "\"rad_cpm\":%.1f,\"light\":%d,\"alerts\":\"%s\",\"up\":%lu}",
    hr, hrv, tb, isnan(tc) ? 0 : tc, isnan(hum) ? 0 : hum, vib, act, restMin, cpm, light, al.c_str(), millis() / 1000);
  Serial.println(js);

  wifiMqtt();
  if (mqtt.connected()) mqtt.publish((String("iaso/") + ASTRO_ID + "/telemetry").c_str(), js);
  mqtt.loop();

  char b[32];
  snprintf(b, sizeof b, "%.0f bpm", hr);    nexTxt("page0.t7", hr > 0 ? b : "sin dedo");
  snprintf(b, sizeof b, "%.0f ms", hrv);    nexTxt("page0.t8", b);
  if (tb > -100) snprintf(b, sizeof b, "%.1f C", tb); else strcpy(b, "sin sensor");
  nexTxt("page0.t9", b);
  nexCmd(al.length() ? "page0.t14.pco=63488" : "page0.t14.pco=2016");  // rojo / verde
  al.trim(); nexTxt("page0.t14", al.length() ? al.c_str() : "Sin alertas");
  snprintf(b, sizeof b, "%.1f CPM", cpm);   nexTxt("page1.t10", b);
  if (isnan(tc)) strcpy(b, "sin sensor"); else snprintf(b, sizeof b, "%.0fC %.0f%%", tc, hum);
  nexTxt("page1.t11", b);
  if (still > REST_MS) snprintf(b, sizeof b, "rest %lu min", restMin); else strcpy(b, act);
  nexTxt("page1.t12", b);
  snprintf(b, sizeof b, "%d", light); nexTxt("page1.t13", b);
  uint32_t up = millis() / 60000;
  snprintf(b, sizeof b, "%luh %02lum", up / 60, up % 60); nexTxt("page2.t7", b);
  nexTxt("page2.t8", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str()
                                                   : WiFi.softAPIP().toString().c_str());
  nexTxt("page2.t9", !strlen(WIFI_SSID) ? "sin WiFi" : mqtt.connected() ? "conectado" : "desconectado");
}
