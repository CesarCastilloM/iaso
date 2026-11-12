# IASO — monitor de signos vitales de astronauta (prototipo)

ESP32 + pantalla Nextion NX3224T024 + sensores del kit KY-xxx → MQTT (Mosquitto público) → dashboard web.
Basado en `Hackathon.pdf` (módulos cardiovascular, sueño, estrés, radiación, alertas con perfil basal).

## Estructura
- `firmware/IASO/` — sketch Arduino (`config.h`: WiFi, broker, pines)
- `dashboard/index.html` — visor MQTT por WebSocket (abrir directo, o subir a GitHub Pages gratis)

## Cableado (ESP32, 3V3 salvo indicación)
| Sensor | Módulo | Pin ESP32 | Uso |
|---|---|---|---|
| Pulso | KY-039 | GPIO34 | BPM + HRV (RMSSD) |
| Temp. corporal | KY-001 (DS18B20) | GPIO4 (+pull-up 4.7k) | temperatura de piel |
| Temp/humedad | KY-015 (DHT11) | GPIO5 | cabina |
| Vibración | KY-002 | GPIO27 | movimiento → activo/rest |
| Micrófono | KY-038 (AO) | GPIO32 | ruido de cabina |
| Fotorresistor | KY-018 | GPIO35 | luz (ciclo sueño) |
| Buzzer activo | KY-012 | GPIO25 | alarma |
| Nextion | NX3224T024 | RX2=16, TX2=17 (5V para VCC, GND común) | UI local |
| Radiación | contador Geiger externo | GPIO33 | CPM |

## Pantalla Nextion
El firmware solo manda texto a componentes **con estos nombres exactos**. Crea en Nextion Editor
(320x240, horizontal, baud 9600) un Text por cada uno: `t_hr` `t_hrv` `t_temp` `t_rad` `t_cab` `t_act` `t_alert`
(+ etiquetas fijas). Compila y copia el `.tft` a una microSD FAT32 para flashear la pantalla.
El `.HMI` no se puede generar desde código.

## Módulos del Hackathon → implementación
| Módulo (PDF) | Qué hace este prototipo |
|---|---|
| Cardiovascular | BPM + HRV (KY-039), alerta por rango fijo y por desviación >25 % del basal propio |
| Sueño | `act` active/rest por vibración (KY-002), `rest_min` = duración del descanso actual, `move` = movimientos por muestra, `light` para contexto de ciclo |
| Estrés | alerta `ESTRES` si la HRV cae >40 % bajo su basal (sin GSR en el kit) |
| Radiación | conteo de pulsos Geiger en GPIO33 → CPM |
| Óseo / inmunológico / perfil basal | carga manual en el dashboard → `iaso/<ASTRO_ID>/lab` (retained) |
| Registro temporal / .csv | el dashboard guarda el historial de la sesión y lo descarga en .csv para el análisis en Python |

## MQTT
Topics: `iaso/<ASTRO_ID>/telemetry` (JSON cada 5 s), `iaso/<ASTRO_ID>/status` (LWT online/offline) y
`iaso/<ASTRO_ID>/lab` (resultados de laboratorio capturados a mano, retained).
Prueba rápida: `mosquitto_sub -h test.mosquitto.org -t "iaso/<ASTRO_ID>/#" -v`

## Lo que el kit NO cubre (del PDF del hackathon)
- **SpO₂, ECG, presión arterial, GSR/electrodérmica**: no hay sensor en el kit (el KY-039 da pulso, no SpO₂). Añadir MAX30102, AD8232, sensor GSR.
- **Radiación**: ningún KY detecta radiación ionizante (el Hall KY-003/024 es magnético). Hace falta un tubo Geiger; el firmware ya cuenta pulsos en GPIO33.
- **Acelerómetro**: el kit no trae uno; el KY-002 (vibración) solo detecta "hubo movimiento", no aceleración ni patrones finos. Añadir MPU6050 para sueño/actividad reales.
- **Análisis multimodal, modelos predictivos y recomendaciones** (Python, datasets NASA): van en el servidor, fuera de este nodo. Los datos de laboratorio ya se capturan en el dashboard.
- La Nextion NX3224T024 es un TFT resistivo, no OLED.

## Límites
- El broker público no tiene autenticación ni privacidad: ASTRO_ID único y solo datos de prueba. Para algo serio usa un Mosquitto propio con usuario/TLS.
- KY-039/DHT11/DS18B20 son sensores didácticos, no médicos: tendencia, no diagnóstico.
