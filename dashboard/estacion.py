"""IASO estacion: corre en la laptop conectada a la red del ESP32 (IASO-astro01, sin internet).
Lee http://192.168.4.1/data cada 2 s, guarda cada muestra en datos/iaso_<fecha>.csv, calcula un estudio
de los datos y sirve el dashboard en http://localhost:8080. Solo libreria estandar de Python.

Uso:  python estacion.py                      (ESP32 en 192.168.4.1, dashboard en :8080)
      python estacion.py --esp http://IP --puerto 8080 --datos carpeta
      python estacion.py --test               (autoprueba del analisis)
"""
import argparse, csv, json, statistics, threading, time, urllib.parse, urllib.request
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

AQUI = Path(__file__).parent
CAMPOS = ["fecha", "hr", "hrv", "temp", "cab_t", "cab_h", "move", "act", "rest_min",
          "rad_cpm", "noise", "light", "alerts", "up"]
NUM = [c for c in CAMPOS if c not in ("fecha", "act", "alerts")]
# clave: (nombre, unidad, rango normal o None). Rangos = los mismos umbrales que usa el firmware.
METRICAS = {
    "hr": ("Pulso", "bpm", (45, 120)),
    "hrv": ("HRV (RMSSD)", "ms", None),
    "temp": ("Temp. corporal", "°C", (35, 38)),
    "cab_t": ("Temp. cabina", "°C", (18, 30)),
    "cab_h": ("Humedad cabina", "%", (30, 70)),
    "rad_cpm": ("Radiación", "CPM", (0, 100)),
    "noise": ("Ruido", "p-p", None),
    "light": ("Luz", "raw", None),
}
HUECO_S = 15  # sin muestras por mas de esto = hueco (el ESP32 publica cada 5 s)

filas, lock = [], threading.Lock()
estado = {"conectado": False, "error": "esperando primera lectura", "esp": ""}


def valido(k, r):
    """El firmware manda 0 / -127 cuando no hay dedo o sensor: no son lecturas reales."""
    if k in ("hr", "hrv"): return r[k] > 0
    if k == "temp": return r[k] > -100
    if k in ("cab_t", "cab_h"): return r["cab_h"] > 0  # DHT11 caido => ambos en 0
    return True


def fila(d, fecha):
    r = {k: float(d.get(k) or 0) for k in NUM}
    r.update(fecha=fecha, act=str(d.get("act", "")), alerts=str(d.get("alerts", "")).strip())
    r["t"] = datetime.fromisoformat(fecha).timestamp()
    return r


def pendiente_h(pts):
    """Tendencia por hora (regresion lineal minimos cuadrados)."""
    if len(pts) < 3: return 0.0
    t0 = pts[0][0]; xs = [(t - t0) / 3600 for t, _ in pts]; ys = [v for _, v in pts]
    mx, my = statistics.fmean(xs), statistics.fmean(ys)
    den = sum((x - mx) ** 2 for x in xs)
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / den if den else 0.0


def estudio(rs):
    """Estadistica por metrica + alertas, actividad, estres, calidad de datos y hallazgos en texto."""
    out = {"muestras": len(rs), "metricas": {}, "hallazgos": []}
    H = out["hallazgos"]
    if not rs:
        H.append("Aún no hay datos: conecta la laptop a la red IASO-astro01 y espera unos segundos.")
        return out
    dur = (rs[-1]["t"] - rs[0]["t"]) / 60
    huecos = sum(1 for a, b in zip(rs, rs[1:]) if b["t"] - a["t"] > HUECO_S)
    out.update(desde=rs[0]["fecha"], hasta=rs[-1]["fecha"], duracion_min=round(dur, 1), huecos=huecos)

    for k, (nombre, u, rango) in METRICAS.items():
        pts = [(r["t"], r[k]) for r in rs if valido(k, r)]
        m = {"nombre": nombre, "unidad": u, "n": len(pts), "cobertura": round(100 * len(pts) / len(rs))}
        if pts:
            vs = [v for _, v in pts]
            m.update(media=round(statistics.fmean(vs), 1), min=min(vs), max=max(vs),
                     desv=round(statistics.pstdev(vs), 1), ultimo=vs[-1], tendencia_h=round(pendiente_h(pts), 2))
            if rango:
                m["rango"] = rango
                m["en_rango"] = round(100 * sum(rango[0] <= v <= rango[1] for v in vs) / len(vs))
        out["metricas"][k] = m

    # alertas: tiempo con alerta, episodios (pasa de sin alerta a con alerta) y tipos
    con = [bool(r["alerts"]) for r in rs]
    tipos = {}
    for r in rs:
        for a in r["alerts"].split(): tipos[a] = tipos.get(a, 0) + 1
    out["alertas"] = {"pct_tiempo": round(100 * sum(con) / len(rs)),
                      "episodios": sum(1 for i, c in enumerate(con) if c and (i == 0 or not con[i - 1])),
                      "tipos": dict(sorted(tipos.items(), key=lambda x: -x[1]))}
    descanso = [r for r in rs if r["act"] == "rest"]
    out["actividad"] = {"pct_descanso": round(100 * len(descanso) / len(rs)),
                        "descanso_max_min": int(max((r["rest_min"] for r in rs), default=0)),
                        "movimientos": int(sum(r["move"] for r in rs))}
    hrv = [r["hrv"] for r in rs if valido("hrv", r)]
    if len(hrv) >= 10:
        base = statistics.median(hrv)
        out["estres"] = {"hrv_basal": round(base), "pct_hrv_baja": round(100 * sum(v < 0.6 * base for v in hrv) / len(hrv))}

    # ---- hallazgos en lenguaje simple ----
    M = out["metricas"]
    H.append(f"{len(rs)} muestras en {dur:.0f} min" + (f", con {huecos} corte{'s' * (huecos > 1)} de conexión." if huecos else ", sin cortes."))
    for k in ("hr", "temp"):
        m = M[k]
        if not m["n"]:
            H.append(f"{m['nombre']}: sin lecturas ({'coloca el dedo en el KY-039' if k == 'hr' else 'revisa el DS18B20 y su pull-up 4.7k'}).")
            continue
        s = f"{m['nombre']}: promedio {m['media']} {m['unidad']} ({m['min']:g}–{m['max']:g}), {m['en_rango']}% del tiempo en rango normal"
        if m["cobertura"] < 80: s += f"; solo {m['cobertura']}% de las muestras tienen lectura"
        H.append(s + ".")
    largo = dur >= 10  # con pocos minutos la pendiente por hora exagera cualquier subida
    if largo and M["hr"]["n"] >= 10 and abs(M["hr"]["tendencia_h"]) >= 5:
        H.append(f"El pulso va {'al alza' if M['hr']['tendencia_h'] > 0 else 'a la baja'} ({M['hr']['tendencia_h']:+} bpm/h).")
    if largo and M["cab_t"]["n"] >= 10 and abs(M["cab_t"]["tendencia_h"]) >= 1:
        H.append(f"La cabina se está {'calentando' if M['cab_t']['tendencia_h'] > 0 else 'enfriando'} ({M['cab_t']['tendencia_h']:+} °C/h).")
    if not M["cab_t"]["n"]: H.append("Cabina: el DHT11 no está respondiendo.")
    if M["rad_cpm"].get("max", 0) > 100: H.append(f"Radiación: picos de hasta {M['rad_cpm']['max']:g} CPM (límite 100).")
    if "estres" in out and out["estres"]["pct_hrv_baja"] >= 20:
        H.append(f"Posible estrés: {out['estres']['pct_hrv_baja']}% del tiempo la HRV estuvo >40% bajo su valor típico ({out['estres']['hrv_basal']} ms).")
    al = out["alertas"]
    if al["episodios"]:
        H.append(f"{al['episodios']} episodio{'s' * (al['episodios'] > 1)} de alerta ({al['pct_tiempo']}% del tiempo); la más frecuente: {next(iter(al['tipos']))}.")
    else:
        H.append("Sin alertas en el periodo.")
    ac = out["actividad"]
    H.append(f"En descanso {ac['pct_descanso']}% del tiempo" + (f"; descanso más largo {ac['descanso_max_min']} min." if ac["descanso_max_min"] else "."))
    return out


def recortar(rs, minutos):
    if not minutos or not rs: return rs
    lim = rs[-1]["t"] - minutos * 60
    return [r for r in rs if r["t"] >= lim]


def cargar(carpeta):
    for f in sorted(carpeta.glob("iaso_*.csv")):
        with open(f, newline="", encoding="utf-8") as fh:
            for d in csv.DictReader(fh):
                try: filas.append(fila(d, d["fecha"]))
                except (KeyError, ValueError): pass  # fila corrupta (corte de luz a media escritura)


def leer_esp(esp, carpeta):
    ultimo_up = None
    while True:
        try:
            with urllib.request.urlopen(esp + "/data", timeout=3) as resp: d = json.load(resp)
            estado.update(conectado=True, error="")
            if "up" in d and d["up"] != ultimo_up:
                ultimo_up = d["up"]
                ahora = datetime.now().isoformat(timespec="seconds")
                r = fila(d, ahora)
                arch = carpeta / f"iaso_{ahora[:10]}.csv"
                nuevo = not arch.exists()
                with open(arch, "a", newline="", encoding="utf-8") as fh:
                    w = csv.DictWriter(fh, CAMPOS, extrasaction="ignore")
                    if nuevo: w.writeheader()
                    w.writerow(r)
                with lock: filas.append(r)
        except Exception as e:
            estado.update(conectado=False, error=f"{type(e).__name__}: {e}")
        time.sleep(2)


class Web(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def enviar(self, cuerpo, tipo="application/json", extra=None):
        b = cuerpo if isinstance(cuerpo, bytes) else json.dumps(cuerpo, ensure_ascii=False).encode()
        self.send_response(200)
        self.send_header("Content-Type", tipo + "; charset=utf-8")
        for k, v in (extra or {}).items(): self.send_header(k, v)
        self.end_headers(); self.wfile.write(b)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        minutos = int(q.get("min", ["0"])[0] or 0)
        with lock: rs = recortar(list(filas), minutos)
        if u.path == "/":
            self.enviar((AQUI / "estacion.html").read_bytes(), "text/html")
        elif u.path == "/api/estado":
            self.enviar({**estado, "ultimo": rs[-1] if rs else None})
        elif u.path == "/api/serie":
            paso = max(1, len(rs) // 600)  # shortcut: muestreo simple a <=600 puntos; si se pierden picos, usar min/max por bloque
            self.enviar([{k: r[k] for k in ("t", *METRICAS, "alerts")} for r in rs[::paso]])
        elif u.path == "/api/estudio":
            self.enviar(estudio(rs))
        elif u.path == "/api/csv":
            out = ",".join(CAMPOS) + "\n" + "".join(",".join(str(r[c]) for c in CAMPOS) + "\n" for r in rs)
            self.enviar(out.encode(), "text/csv", {"Content-Disposition": "attachment; filename=iaso_estudio.csv"})
        else:
            self.send_error(404)


def _test():
    t0 = datetime(2026, 1, 1, 12, 0, 0).timestamp()
    rs = []
    for i in range(60):  # 5 min a 5 s (+ hueco final = ~7 min); pulso sube 60->89, alerta de 10 muestras a la mitad
        d = {"hr": 60 + i * 0.5, "hrv": 50 if i < 50 else 20, "temp": 36.5, "cab_t": 24, "cab_h": 40,
             "act": "active", "alerts": "TEMP " if 20 <= i < 30 else "", "up": i * 5}
        rs.append(fila(d, datetime.fromtimestamp(t0 + i * 5).isoformat()))
    rs.append(fila({"hr": 0, "temp": -127, "act": "rest", "rest_min": 12, "up": 400},
                   datetime.fromtimestamp(t0 + 400).isoformat()))  # sin dedo/sensor + hueco de 105 s
    e = estudio(rs)
    hr = e["metricas"]["hr"]
    assert hr["n"] == 60 and hr["min"] == 60 and hr["max"] == 89.5, hr
    assert hr["en_rango"] == 100 and hr["tendencia_h"] > 300, hr  # +0.5 bpm cada 5 s = +360 bpm/h
    assert e["metricas"]["temp"]["n"] == 60, "temp -127 no debe contar"
    assert e["huecos"] == 1, e["huecos"]
    assert e["alertas"]["episodios"] == 1 and e["alertas"]["tipos"] == {"TEMP": 10}, e["alertas"]
    assert e["actividad"]["descanso_max_min"] == 12
    assert e["estres"]["hrv_basal"] == 50 and e["estres"]["pct_hrv_baja"] == 17, e["estres"]
    assert estudio([])["muestras"] == 0
    assert len(recortar(rs, 1)) == 1  # el ultimo minuto solo contiene la muestra final
    print("autoprueba OK")
    for h in e["hallazgos"]: print(" -", h)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--esp", default="http://192.168.4.1")
    ap.add_argument("--puerto", type=int, default=8080)
    ap.add_argument("--datos", default=str(AQUI / "datos"))
    ap.add_argument("--test", action="store_true")
    a = ap.parse_args()
    if a.test:
        _test(); raise SystemExit
    carpeta = Path(a.datos); carpeta.mkdir(parents=True, exist_ok=True)
    cargar(carpeta)
    estado["esp"] = a.esp
    threading.Thread(target=leer_esp, args=(a.esp.rstrip("/"), carpeta), daemon=True).start()
    print(f"IASO estacion: leyendo {a.esp}/data, guardando en {carpeta}")
    print(f"Abre http://localhost:{a.puerto} (Ctrl+C para salir)")
    ThreadingHTTPServer(("127.0.0.1", a.puerto), Web).serve_forever()
