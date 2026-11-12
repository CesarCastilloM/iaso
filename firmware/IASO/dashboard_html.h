#pragma once
// GENERADO desde dashboard/index.html con dashboard/gen_header.py. No editar a mano.
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IASO Telemetría</title>
<style>
:root{--bg:#0b1020;--card:#151c33;--fg:#e8ecf8;--mut:#8a94b8;--ok:#3ddc97;--bad:#ff5d5d}
@media(prefers-color-scheme:light){:root{--bg:#f3f5fb;--card:#fff;--fg:#14182b;--mut:#5b6486;--ok:#0a9e66;--bad:#d32f2f}}
body{margin:0;padding:16px;background:var(--bg);color:var(--fg);font:16px system-ui,sans-serif}
h1{font-size:20px;margin:0 0 4px}#st{color:var(--mut);margin-bottom:16px}
#g{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}
.c{background:var(--card);border-radius:12px;padding:12px}.c small{color:var(--mut)}
.v{font-size:28px;font-weight:600}#al{grid-column:1/-1}.bad{color:var(--bad)}.ok{color:var(--ok)}
h2{font-size:16px;margin:20px 0 8px}button{padding:8px 14px;border-radius:8px;border:0;background:var(--ok);color:#000;font:inherit;cursor:pointer}
#lab{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px}
#lab label{display:flex;flex-direction:column;gap:4px;color:var(--mut);font-size:14px}
#lab input{padding:6px;border-radius:6px;border:1px solid var(--mut);background:var(--card);color:var(--fg);font:inherit}
</style></head><body>
<h1>IASO · signos vitales</h1><div id="st">conectando…</div>
<div id="g"></div>
<p><button id="csv">Descargar historial .csv</button> <small id="nrows"></small></p>
<h2>Resultados de laboratorio (carga manual)</h2>
<form id="lab"></form>
<p><button form="lab">Guardar</button> <small id="labst"></small></p>
<script>
// Dos modos: servido por el ESP32 en su red propia (http://192.168.4.1, sin internet) -> lee /data del ESP32.
// Abierto desde otro lugar (archivo, GitHub Pages) -> MQTT. Uso: index.html#iaso-astro01-x7k2 (ASTRO_ID de config.h)
const local=location.hostname==="192.168.4.1";
const id=location.hash.slice(1)||"iaso-astro01-x7k2";
const F=[["hr","Pulso","bpm"],["hrv","HRV (RMSSD)","ms"],["temp","Temp. corporal","°C"],["rad_cpm","Radiación","CPM"],
["cab_t","Cabina","°C"],["cab_h","Humedad","%"],["act","Actividad",""],["rest_min","Descanso","min"],["noise","Ruido","p-p"],["light","Luz","raw"]];
// perfil basal + modulos oseo e inmunologico (Hackathon.pdf): datos de laboratorio, no de sensores
const L=[["glucosa","Glucosa","mg/dL"],["calcio","Calcio","mg/dL"],["creatinina","Creatinina","mg/dL"],["sodio","Sodio","mEq/L"],
["potasio","Potasio","mEq/L"],["bun","BUN","mg/dL"],["dmo","Densidad ósea (T-score)",""],["leucocitos","Leucocitos","10³/µL"],
["linfocitos","Linfocitos","%"],["pcr","PCR","mg/L"],["ejercicio","Ejercicio cumplido","min/día"]];
g.innerHTML=F.map(([k,n,u])=>`<div class="c"><small>${n}</small><div class="v" id="${k}">–</div><small>${u}</small></div>`).join("")+
 '<div class="c" id="al"><small>Alertas</small><div class="v ok" id="a">–</div></div>';
lab.innerHTML=L.map(([k,n,u])=>`<label>${n} ${u&&"("+u+")"}<input name="${k}" type="number" step="any"></label>`).join("");
const hist=[];let lastUp,saveLab;  // shortcut: historial solo en memoria (se pierde al recargar); el persistente va en el servidor Python
function show(d){
 if(d.up===undefined||d.up===lastUp)return;lastUp=d.up;
 hist.push({fecha:new Date().toISOString(),...d});nrows.textContent=hist.length+" muestras";
 F.forEach(([k])=>document.getElementById(k).textContent=d[k]??"–");
 a.textContent=d.alerts||"OK";a.className="v "+(d.alerts?"bad":"ok");
}
function showLab(d){L.forEach(([k])=>lab.elements[k].value=d[k]??"");if(d.fecha)labst.textContent="último: "+d.fecha}
if(local){
 const poll=()=>fetch("/data").then(r=>r.json()).then(d=>{st.textContent="nodo ESP32 · red local";show(d)})
  .catch(()=>st.textContent="sin conexión con el ESP32");
 poll();setInterval(poll,2500);
 fetch("/lab").then(r=>r.json()).then(showLab).catch(()=>{});
 saveLab=d=>fetch("/lab",{method:"POST",body:JSON.stringify(d)}).then(r=>{if(!r.ok)throw 0});
}else{
 const s=document.createElement("script");s.src="https://unpkg.com/mqtt@5.3.4/dist/mqtt.min.js";
 s.onerror=()=>st.textContent="sin internet: conéctate a la red del ESP32 y abre http://192.168.4.1";
 s.onload=()=>{
  const c=mqtt.connect("wss://test.mosquitto.org:8081");
  c.on("connect",()=>{st.textContent="broker OK · "+id;c.subscribe(`iaso/${id}/#`)});
  c.on("error",e=>st.textContent="error: "+e.message);
  c.on("message",(t,m)=>{
   if(t.endsWith("/status")){st.textContent="nodo "+m;return}
   let d;try{d=JSON.parse(m)}catch{return}
   t.endsWith("/lab")?showLab(d):show(d);
  });
  // retained: el ultimo resultado queda en el broker para el servidor Python y otros visores
  saveLab=d=>c.publishAsync(`iaso/${id}/lab`,JSON.stringify(d),{retain:true});
 };
 document.head.append(s);
}
lab.onsubmit=e=>{e.preventDefault();if(!saveLab){labst.textContent="sin conexión";return}
 const d={fecha:new Date().toISOString()};
 L.forEach(([k])=>{const v=lab.elements[k].value;if(v!=="")d[k]=+v});
 saveLab(d).then(()=>labst.textContent="guardado "+d.fecha,()=>labst.textContent="error al guardar")};
csv.onclick=()=>{if(!hist.length)return;const k=Object.keys(hist[hist.length-1]);
 const s=[k.join(","),...hist.map(r=>k.map(x=>JSON.stringify(r[x]??"")).join(","))].join("\n");
 const u=URL.createObjectURL(new Blob([s],{type:"text/csv"}));
 Object.assign(document.createElement("a"),{href:u,download:`iaso_${id}.csv`}).click();setTimeout(()=>URL.revokeObjectURL(u),1000)};
</script></body></html>
)rawliteral";
