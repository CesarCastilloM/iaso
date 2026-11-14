// Puente USB <-> Nextion para cargar el .tft sin microSD (Nextion Editor -> Upload, COM del ESP32, 9600 baud).
// Despues de cargar, vuelve a subir firmware/IASO.
#define NEX_RX 17  // igual que firmware/IASO/config.h
#define NEX_TX 18
#ifndef BAUD
#define BAUD 115200  // debe coincidir con la velocidad actual de la pantalla (9600 de fabrica)
#endif

void setup() {
  Serial.setRxBufferSize(4096);
  Serial2.setRxBufferSize(4096);
  Serial.begin(BAUD);
  Serial2.begin(BAUD, SERIAL_8N1, NEX_RX, NEX_TX);
}

void loop() {
  uint8_t b[256]; size_t n;
  if ((n = Serial.available())) Serial2.write(b, Serial.readBytes(b, min(n, sizeof b)));
  if ((n = Serial2.available())) Serial.write(b, Serial2.readBytes(b, min(n, sizeof b)));
}
