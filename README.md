# IASO — monitor de signos vitales de astronauta (prototipo)

ESP32 + pantalla Nextion NX3224T024 + sensores del kit KY-xxx → MQTT (Mosquitto público) → dashboard web.
Basado en `Hackathon.pdf` (módulos cardiovascular, sueño, estrés, radiación, alertas con perfil basal).

## Estructura
- `firmware/IASO/` — sketch Arduino (`config.h`: red propia, WiFi, broker, pines)
- `dashboard/index.html` — dashboard. Lo sirve el propio ESP32 y también funciona por MQTT (archivo local o GitHub Pages)
- `dashboard/gen_header.py` — copia `index.html` a `firmware/IASO/dashboard_html.h`. **Córrelo cada vez que cambies el dashboard** y vuelve a subir el sketch

## Ver el dashboard en el celular (sin internet)
1. El ESP32 crea su propia red WiFi: **`IASO-astro01`**, contraseña **`iaso1234`** (se cambian en `config.h`).
2. Conecta el celular a esa red. Si Android avisa "sin internet", elige **mantener la conexión**.
3. Abre **http://192.168.4.1** en el navegador (con `http`, no `https`).

La página se actualiza cada 2.5 s. Los resultados de laboratorio quedan guardados en la flash del ESP32 aunque se reinicie.
Si además pones tu WiFi con internet en `WIFI_SSID`, también publica por MQTT. La red propia sigue activa, pero al conectarse
cambia al canal de tu router, y el celular puede desconectarse un momento.

## Estación de análisis en la laptop (sin internet)
`dashboard/estacion.py` recibe los datos del ESP32, los guarda y los analiza. Solo usa la librería estándar de Python.
1. Conecta la laptop a la red **`IASO-astro01`**.
2. Corre `python dashboard/estacion.py` y abre **http://localhost:8080**.

- Lee `http://192.168.4.1/data` cada 2 s y guarda cada muestra en `dashboard/datos/iaso_<fecha>.csv`.
  Al reiniciarse, vuelve a cargar todo lo guardado.
- Gráficas por métrica con el rango normal sombreado, y periodos de 15 min, 1 h, 6 h o todo.
- **Estudio** del periodo: promedio, mínimo, máximo, desviación, tendencia por hora, % del tiempo en rango normal,
  episodios de alerta por tipo, % de descanso, estrés (HRV más de 40 % bajo su valor típico), cortes de conexión
  y hallazgos en texto. Se exporta en .csv.
- `--esp http://IP` si el ESP32 tiene otra dirección. `--test` corre la autoprueba del análisis.

## Cableado (ESP32-S3, sensores a 3V3, GND común)
| Sensor | Módulo | Pin ESP32-S3 | Uso |
|---|---|---|---|
| Pulso | KY-039 | GPIO1 | BPM + HRV (RMSSD) |
| Fotorresistor | KY-018 | GPIO4 | luz (ciclo sueño) |
| Temp. corporal | KY-001 (DS18B20) | GPIO5 (+pull-up 4.7k si el módulo no la trae) | temperatura de piel |
| Temp/humedad | KY-015 (DHT11) | GPIO6 | cabina |
| Vibración | KY-002 | GPIO7 | movimiento → activo/rest |
| Radiación | contador Geiger externo | GPIO15 | CPM |
| Buzzer activo | KY-012 | GPIO21 | alarma |
| Nextion | NX3224T024 | TX→GPIO16, RX→GPIO17, +5V→5V, GND | UI local |

Compilar con la placa **ESP32S3 Dev Module** (`esp32:esp32:esp32s3`). Si la placa tiene dos USB, usa el marcado **COM/UART**.

## Pantalla Nextion (táctil)
Proyecto en `firmware/IASO/nextion/IASO.HMI` (Nextion Editor, NX3224T024, horizontal, fondo negro). Tiene 3 páginas,
y se navega con la barra de abajo:
- **Vitales**: pulso, HRV, temperatura corporal y alertas.
- **Ambiente**: radiación, cabina, actividad y luz.
- **Sistema**: tiempo encendido, IP, MQTT, botón **Silenciar** (manda `M` al ESP32 y apaga el buzzer hasta que
  terminen las alertas) y barra de **brillo**.

Los valores son `vscope global`, así que el ESP32 los escribe con `pageN.tX.txt` aunque esa página no esté visible.

**Cargar el `.tft` sin microSD**, usando el ESP32 como puente USB↔pantalla:
1. Sube `firmware/NextionBridge` compilado a 9600:
   `arduino-cli compile --upload -p COM3 --fqbn esp32:esp32:esp32s3 --build-property "compiler.cpp.extra_flags=-DBAUD=9600" firmware/NextionBridge`
2. Carga la pantalla: `python firmware/NextionBridge/upload_tft.py COM3 firmware/IASO/nextion/IASO.tft 9600`
3. Vuelve a subir `firmware/IASO`.

Si una carga se interrumpe, la pantalla se queda en "writing data…": desconéctale la corriente unos 10 s y repite.

## Módulos del Hackathon → implementación
| Módulo (PDF) | Qué hace este prototipo |
|---|---|
| Cardiovascular | BPM + HRV (KY-039), alerta por rango fijo y por desviación >25 % del basal propio |
| Sueño | `act` active/rest por vibración (KY-002), `rest_min` = duración del descanso actual, `move` = movimientos por muestra, `light` para contexto de ciclo |
| Estrés | alerta `ESTRES` si la HRV cae >40 % bajo su basal (sin GSR en el kit) |
| Radiación | conteo de pulsos Geiger en GPIO15 → CPM |
| Óseo / inmunológico / perfil basal | carga manual en el dashboard → `iaso/<ASTRO_ID>/lab` (retained) |
| SpO₂, presión arterial, GSR | sin sensor en el kit: también carga manual en el dashboard (mismo topic `lab`) |
| Registro temporal / .csv | el dashboard guarda el historial de la sesión y lo descarga en .csv para el análisis en Python |

Cada dato manual se compara contra el **basal individual**: el dashboard tiene dos columnas (Actual y Basal) y calcula Δ %.
Marca "Fuera de rango" si excede la tolerancia (SpO₂ 3 %, presión 10 %, resto 15 %; editable en `TOL` de `index.html`).
El ESP32 guarda ambos en su flash (`/lab` y `/basal`) y, si hay MQTT, los publica retained en `iaso/<ASTRO_ID>/lab` y `iaso/<ASTRO_ID>/basal`.

El dashboard separa dos secciones: **Tiempo real** (solo lo que miden los sensores) y **Carga manual** (todo lo que requiere laboratorio o equipo que el kit no tiene).

## MQTT
Topics: `iaso/<ASTRO_ID>/telemetry` (JSON cada 5 s), `iaso/<ASTRO_ID>/status` (LWT online/offline) y
`iaso/<ASTRO_ID>/lab` (datos manuales actuales: laboratorio, SpO₂, presión, retained) y
`iaso/<ASTRO_ID>/basal` (basal individual de esos mismos datos, retained).
Prueba rápida: `mosquitto_sub -h test.mosquitto.org -t "iaso/<ASTRO_ID>/#" -v`

## Lo que el kit NO cubre (del PDF del hackathon)
- **SpO₂, ECG, presión arterial, GSR/electrodérmica**: no hay sensor en el kit (el KY-039 da pulso, no SpO₂). Añadir MAX30102, AD8232, sensor GSR.
- **Radiación**: ningún KY detecta radiación ionizante (el Hall KY-003/024 es magnético). Hace falta un tubo Geiger; el firmware ya cuenta pulsos en GPIO15.
- **Acelerómetro**: el kit no trae uno; el KY-002 (vibración) solo detecta "hubo movimiento", no aceleración ni patrones finos. Añadir MPU6050 para sueño/actividad reales.
- **Modelos predictivos y recomendaciones** (datasets NASA): no están hechos. `estacion.py` hace estadística descriptiva
  y reglas por umbral, no predicción. Los datos de laboratorio se capturan en el dashboard del ESP32, pero la estación
  todavía no los analiza.
- La Nextion NX3224T024 es un TFT resistivo, no OLED.

## Límites
- El broker público no tiene autenticación ni privacidad: ASTRO_ID único y solo datos de prueba. Para algo serio usa un Mosquitto propio con usuario/TLS.
- KY-039/DHT11/DS18B20 son sensores didácticos, no médicos: tendencia, no diagnóstico.
