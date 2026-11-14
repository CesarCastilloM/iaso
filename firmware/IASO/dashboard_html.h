#pragma once
// GENERADO desde dashboard/index.html con dashboard/gen_header.py. No editar a mano.
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IASO Telemetría</title>
<style>
:root{--bg:#0b1020;--card:#151c33;--fg:#e8ecf8;--mut:#8a94b8;--ok:#3ddc97;--bad:#ff5d5d;--warn:#ffb020}
@media(prefers-color-scheme:light){:root{--bg:#f3f5fb;--card:#fff;--fg:#14182b;--mut:#5b6486;--ok:#0a9e66;--bad:#d32f2f;--warn:#b26a00}}
body{margin:0;padding:16px;background:var(--bg);color:var(--fg);font:16px system-ui,sans-serif}
h1{font-size:20px;margin:0 0 4px}#st{color:var(--mut);margin-bottom:16px}
#g{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}
.c{background:var(--card);border-radius:12px;padding:12px}.c small{color:var(--mut)}
.v{font-size:28px;font-weight:600}#al{grid-column:1/-1}.bad{color:var(--bad)}.ok{color:var(--ok)}.warn{color:var(--warn)}
h2{font-size:16px;margin:20px 0 8px}button{padding:8px 14px;border-radius:8px;border:0;background:var(--ok);color:#000;font:inherit;cursor:pointer}
#cmp{border-collapse:collapse;width:100%;font-size:14px}#cmp th{text-align:left;color:var(--mut);font-weight:400}
#cmp td,#cmp th{padding:4px 6px}#cmp input{width:90px;padding:6px;border-radius:6px;border:1px solid var(--mut);background:var(--card);color:var(--fg);font:inherit}
#lab{overflow-x:auto}
#perfil{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px;align-items:end}
#perfil label{display:flex;flex-direction:column;font-size:13px;color:var(--mut);gap:4px}
#perfil input,#perfil select{padding:6px;border-radius:6px;border:1px solid var(--mut);background:var(--card);color:var(--fg);font:inherit}
</style></head><body>
<h1>IASO · signos vitales</h1><div id="st">conectando…</div>
<h2>Tiempo real (sensores del nodo)</h2>
<div id="g"></div>
<p><button id="csv">Descargar historial .csv</button> <small id="nrows"></small></p>
<h2>Perfil del astronauta (se captura una vez, antes de la misión)</h2>
<form id="perfil">
<label>Sexo<select name="sexo"><option value="">–</option><option value="M">Masculino</option><option value="F">Femenino</option></select></label>
<label>Fecha de nacimiento<input name="nacimiento" type="date"></label>
<label>Altura (cm)<input name="altura" type="number" step="any" min="0"></label>
<label>Peso (kg)<input name="peso" type="number" step="any" min="0"></label>
<label>Meta de ejercicio (min/día)<input name="meta_ejercicio" type="number" step="any" min="0"></label>
<div><button>Guardar perfil</button> <small id="perfst"></small></div>
</form>
<p><small id="perfinfo"></small></p>
<h2>Carga manual vs. rango de referencia y basal individual (SpO₂, presión, laboratorio, óseo, inmunológico)</h2>
<p><small>Captura el basal (preflight) una vez y los valores actuales cuando se midan. Cada dato se evalúa contra el rango de referencia (según sexo del perfil) y contra el basal propio. Ámbar = fuera del rango normal o del basal; rojo = crítico.</small></p>
<form id="lab"><table id="cmp"><thead><tr><th>Parámetro</th><th>Rango</th><th>Actual</th><th>Basal</th><th>Δ %</th><th>Estado</th></tr></thead><tbody></tbody></table>
<p><button value="lab">Guardar actual</button> <button value="basal">Guardar basal</button> <small id="labst"></small></p></form>
<script>
// Dos modos: servido por el ESP32 en su red propia (http://192.168.4.1, sin internet) -> lee /data del ESP32.
// Abierto desde otro lugar (archivo, GitHub Pages) -> MQTT. Uso: index.html#iaso-astro01-x7k2 (ASTRO_ID de config.h)
const local=location.hostname==="192.168.4.1";
const id=location.hash.slice(1)||"iaso-astro01-x7k2";
const F=[["hr","Pulso","bpm"],["hrv","HRV (RMSSD)","ms"],["temp","Temp. corporal","°C"],["rad_cpm","Radiación","CPM"],
["cab_t","Cabina","°C"],["cab_h","Humedad","%"],["act","Actividad",""],["rest_min","Descanso","min"],["move","Movimientos","/muestra"],["light","Luz","raw"]];
// Datos que NO se pueden medir con los sensores del nodo (Hackathon.pdf): se capturan a mano.
// Incluye signos vitales sin sensor en el kit, perfil basal, modulos oseo e inmunologico.
const L=[["spo2","Saturación O₂ (SpO₂)","%"],["pa_sis","Presión sistólica","mmHg"],["pa_dia","Presión diastólica","mmHg"],
["gsr","Conductancia piel (GSR)","µS"],["glucosa","Glucosa","mg/dL"],["calcio","Calcio","mg/dL"],["creatinina","Creatinina","mg/dL"],["sodio","Sodio","mEq/L"],
["potasio","Potasio","mEq/L"],["bun","BUN","mg/dL"],["dmo","Densidad ósea (T-score)",""],["leucocitos","Leucocitos","10³/µL"],
["linfocitos","Linfocitos","%"],["hemoglobina","Hemoglobina","g/dL"],["plaquetas","Plaquetas","10³/µL"],
["pcr","PCR","mg/L"],["il6","IL-6","pg/mL"],["ejercicio","Ejercicio cumplido","min/día"]];
g.innerHTML=F.map(([k,n,u])=>`<div class="c"><small>${n}</small><div class="v" id="${k}">–</div><small>${u}</small></div>`).join("")+
 '<div class="c" id="al"><small>Alertas</small><div class="v ok" id="a">–</div></div>';
const TOL={spo2:3,pa_sis:10,pa_dia:10};  // tolerancia (% sobre el basal); resto: 15 %
// Rangos de referencia de adulto: [normal_min, normal_max, critico_bajo (v<), critico_alto (v>=)]; null = sin limite.
// Son valores generales: validarlos con el area medica de la mision antes de usarlos.
const R={spo2:[95,100,92,null],pa_sis:[90,120,90,140],pa_dia:[60,80,60,90],glucosa:[70,99,70,126],calcio:[8.6,10.2,8,10.5],
 sodio:[135,145,130,150],potasio:[3.5,5,3,5.5],bun:[7,20,null,25],dmo:[-1,null,-2.45,null],  // -2.45: que T-score -2.5 (osteoporosis) cuente como critico
 leucocitos:[4.5,11,3.5,12],linfocitos:[20,40,15,45],plaquetas:[150,450,100,500],pcr:[null,5,null,10],il6:[null,7,null,15]};
// gsr: sin rango universal, solo basal. Dependen del sexo del perfil: hemoglobina y creatinina. ejercicio: meta del perfil.
let P={};  // perfil del astronauta
function rng(k){const s=P.sexo;
 if(k==="hemoglobina")return s==="F"?[12,15.5,11,18]:s==="M"?[13.5,17.5,11,18]:[12,17.5,11,18];
 if(k==="creatinina")return s==="F"?[0.6,1.1,null,1.5]:s==="M"?[0.7,1.3,null,1.5]:[0.6,1.3,null,1.5];
 if(k==="ejercicio"){const m=+P.meta_ejercicio;return m>0?[m,null,0.8*m,null]:null}
 return R[k]||null}
const fr=r=>!r?"solo basal":r[0]!=null&&r[1]!=null?`${r[0]}–${r[1]}`:r[0]!=null?`≥ ${+r[0].toFixed(1)}`:`≤ ${r[1]}`;
cmp.tBodies[0].innerHTML=L.map(([k,n,u])=>`<tr><td>${n} <small>${u}</small></td><td id="r_${k}"><small>–</small></td><td><input name="a_${k}" type="number" step="any"></td>`+
 `<td><input name="b_${k}" type="number" step="any"></td><td id="d_${k}">–</td><td id="e_${k}">–</td></tr>`).join("");
function refresh(){L.forEach(([k])=>{const a=lab.elements["a_"+k].value,b=lab.elements["b_"+k].value,d=document.getElementById("d_"+k),e=document.getElementById("e_"+k),r=rng(k);
 document.getElementById("r_"+k).innerHTML="<small>"+fr(r)+"</small>";
 d.textContent="–";e.textContent="–";e.className="";if(a==="")return;
 const v=+a;let lvl=0,why=[];  // lvl: 0 OK, 1 ambar, 2 critico
 if(r){if((r[2]!=null&&v<r[2])||(r[3]!=null&&v>=r[3])){lvl=2;why.push("Crítico")}
  else if((r[0]!=null&&v<r[0])||(r[1]!=null&&v>r[1])){lvl=1;why.push("Fuera de rango")}}
 if(b!==""&&+b!==0){const p=(v-+b)/+b*100;d.textContent=(p>0?"+":"")+p.toFixed(1)+" %";
  if(Math.abs(p)>(TOL[k]??15)){lvl=Math.max(lvl,1);why.push("Δ basal")}}
 else if(!r){return}
 e.textContent=lvl?why.join(" · "):"OK";e.className=["ok","warn","bad"][lvl]})}
lab.addEventListener("input",refresh);
// perfil: sexo, nacimiento, altura, peso y meta de ejercicio. Define los rangos de referencia; edad e IMC son solo informativos.
function showPerfil(d){P=d;["sexo","nacimiento","altura","peso","meta_ejercicio"].forEach(k=>perfil.elements[k].value=d[k]??"");perfInfo();refresh();
 if(d.fecha)perfst.textContent="guardado: "+d.fecha}
function perfInfo(){const o=[];
 if(P.nacimiento){const n=new Date(P.nacimiento),t=new Date();let e=t.getFullYear()-n.getFullYear();
  if(t.getMonth()<n.getMonth()||(t.getMonth()===n.getMonth()&&t.getDate()<n.getDate()))e--;if(e>=0)o.push("Edad: "+e+" años")}
 if(+P.peso>0&&+P.altura>0){const m=+P.peso/((+P.altura/100)**2);
  o.push("IMC: "+m.toFixed(1)+" ("+(m<18.5?"bajo peso":m<25?"normal":m<30?"sobrepeso":"obesidad")+")")}
 if(!P.sexo)o.push("sin sexo en el perfil: se usan rangos generales");
 perfinfo.textContent=o.join(" · ")}
perfil.addEventListener("input",()=>{P={...P,...Object.fromEntries(new FormData(perfil))};perfInfo();refresh()});
const hist=[];let lastUp,saveManual;  // shortcut: historial solo en memoria (se pierde al recargar); el persistente va en el servidor Python
function show(d){
 if(d.up===undefined||d.up===lastUp)return;lastUp=d.up;
 hist.push({fecha:new Date().toISOString(),...d});nrows.textContent=hist.length+" muestras";
 F.forEach(([k])=>document.getElementById(k).textContent=d[k]??"–");
 a.textContent=d.alerts||"OK";a.className="v "+(d.alerts?"bad":"ok");
}
// kind: "lab" = valores actuales (columna a_), "basal" = referencia individual (columna b_)
function showManual(kind,d){const p=kind==="lab"?"a_":"b_";
 L.forEach(([k])=>lab.elements[p+k].value=d[k]??"");refresh();
 if(d.fecha)labst.textContent="último "+(kind==="lab"?"actual":"basal")+": "+d.fecha}
if(local){
 const poll=()=>fetch("/data").then(r=>r.json()).then(d=>{st.textContent="nodo ESP32 · red local";show(d)})
  .catch(()=>st.textContent="sin conexión con el ESP32");
 poll();setInterval(poll,2500);
 fetch("/perfil").then(r=>r.json()).then(showPerfil).catch(()=>{});
 ["lab","basal"].forEach(kind=>fetch("/"+kind).then(r=>r.json()).then(d=>showManual(kind,d)).catch(()=>{}));
 saveManual=(kind,d)=>fetch("/"+kind,{method:"POST",body:JSON.stringify(d)}).then(r=>{if(!r.ok)throw 0});
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
   t.endsWith("/lab")?showManual("lab",d):t.endsWith("/basal")?showManual("basal",d):t.endsWith("/perfil")?showPerfil(d):show(d);
  });
  // retained: el ultimo resultado queda en el broker para el servidor Python y otros visores
  saveManual=(kind,d)=>c.publishAsync(`iaso/${id}/${kind}`,JSON.stringify(d),{retain:true});
 };
 document.head.append(s);
}
lab.onsubmit=e=>{e.preventDefault();if(!saveManual){labst.textContent="sin conexión";return}
 const kind=e.submitter?.value||"lab",p=kind==="lab"?"a_":"b_",d={fecha:new Date().toISOString()};
 L.forEach(([k])=>{const v=lab.elements[p+k].value;if(v!=="")d[k]=+v});
 saveManual(kind,d).then(()=>labst.textContent="guardado "+(kind==="lab"?"actual":"basal")+" "+d.fecha,()=>labst.textContent="error al guardar")};
perfil.onsubmit=e=>{e.preventDefault();if(!saveManual){perfst.textContent="sin conexión";return}
 const d={fecha:new Date().toISOString()};
 for(const[k,v]of new FormData(perfil))if(v!=="")d[k]=["sexo","nacimiento"].includes(k)?v:+v;
 saveManual("perfil",d).then(()=>{P=d;perfst.textContent="guardado "+d.fecha;perfInfo();refresh()},()=>perfst.textContent="error al guardar")};
csv.onclick=()=>{if(!hist.length)return;const k=Object.keys(hist[hist.length-1]);
 const s=[k.join(","),...hist.map(r=>k.map(x=>JSON.stringify(r[x]??"")).join(","))].join("\n");
 const u=URL.createObjectURL(new Blob([s],{type:"text/csv"}));
 Object.assign(document.createElement("a"),{href:u,download:`iaso_${id}.csv`}).click();setTimeout(()=>URL.revokeObjectURL(u),1000)};
</script></body></html>
)rawliteral";
