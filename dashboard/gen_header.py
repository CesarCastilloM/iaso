# Copia dashboard/index.html dentro del firmware (dashboard_html.h). Correr tras cada cambio: python dashboard/gen_header.py
from pathlib import Path

root = Path(__file__).resolve().parent.parent
s = (root / "dashboard/index.html").read_text(encoding="utf-8")
assert ")rawliteral" not in s
(root / "firmware/IASO/dashboard_html.h").write_text(
    "#pragma once\n// GENERADO desde dashboard/index.html con dashboard/gen_header.py. No editar a mano.\n"
    f'const char DASHBOARD_HTML[] PROGMEM = R"rawliteral({s})rawliteral";\n',
    encoding="utf-8", newline="\n")
