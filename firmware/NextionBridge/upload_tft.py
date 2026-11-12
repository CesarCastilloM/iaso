"""Carga un .tft a la Nextion a traves del puente NextionBridge (ESP32 por USB).
Uso: python upload_tft.py COM3 IASO.tft [baud]   (baud = el BAUD del puente)"""
import os, sys, time, serial

EOC = b"\xff\xff\xff"
port, path = sys.argv[1], sys.argv[2]
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200
size = os.path.getsize(path)
s = serial.Serial(baudrate=baud, timeout=0.5)
s.port, s.dtr, s.rts = port, False, False  # no reiniciar el ESP32 al abrir el puerto
s.open()
time.sleep(1.5)
s.reset_input_buffer()  # descarta un posible log de arranque del ESP32

if "--raw" not in sys.argv:  # --raw: la pantalla ya muestra "writing data..." y solo espera los bytes
    s.write(EOC + b"connect" + EOC)
    time.sleep(1)
    r = s.read(256)
    print("connect:", r)
    assert b"comok" in r, "la pantalla no respondio (desconecta y conecta el USB y reintenta)"
    s.write(b"whmi-wri %d,%d,0" % (size, baud) + EOC)
    s.timeout = 5
    assert s.read(1) == b"\x05", "la pantalla no acepto la carga"

s.timeout = 10
with open(path, "rb") as f:
    sent = 0
    while chunk := f.read(4096):
        s.write(chunk)
        sent += len(chunk)
        assert s.read(1) == b"\x05", f"sin confirmacion en {sent} bytes"
        print(f"\r{sent * 100 // size}%", end="", flush=True)
print("\nlisto")
