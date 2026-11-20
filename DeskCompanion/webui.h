#pragma once
// Pagina web servida por el ESP32 (control de expresiones + mensajeria)
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Desk Buddy</title>
<style>
:root{--bg:#0f1115;--card:#181b22;--line:#262a33;--txt:#e9ecf1;--mut:#8b93a3;--acc:#7cc4ff;--ok:#6ee7a8}
*{box-sizing:border-box}body{margin:0;font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--txt)}
main{max-width:640px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;gap:14px;margin:6px 0 16px}
#face{font-size:46px;width:72px;height:72px;display:grid;place-items:center;background:#000;border-radius:18px;border:1px solid var(--line)}
h1{font-size:20px;margin:0}.mut{color:var(--mut);font-size:13px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:14px;margin-bottom:14px}
.card h2{font-size:14px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:0 0 10px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(88px,1fr));gap:8px}
.ex{background:#11141a;border:1px solid var(--line);border-radius:12px;padding:8px 4px;color:var(--txt);cursor:pointer;font-size:12px}
.ex b{display:block;font-size:26px;font-weight:400}.ex.on{border-color:var(--acc);background:#132131}
.ex.auto{width:100%;margin-bottom:10px;font-size:14px;padding:10px}.ex.auto b{font-size:22px}
input,textarea,select{width:100%;background:#0d0f13;border:1px solid var(--line);border-radius:10px;color:var(--txt);padding:10px;font:inherit}
textarea{resize:vertical;min-height:76px}
.row{display:flex;gap:8px;align-items:center;margin-top:8px}.row>*{flex:1}
button.main{background:var(--acc);color:#04121f;border:0;border-radius:10px;padding:11px 16px;font-weight:600;cursor:pointer}
button.sec{background:#222731;color:var(--txt);border:1px solid var(--line);border-radius:10px;padding:9px 12px;cursor:pointer}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.chip{background:#11141a;border:1px solid var(--line);border-radius:99px;padding:5px 10px;font-size:13px;cursor:pointer;color:var(--txt)}
.msg{border-top:1px solid var(--line);padding:9px 0}.msg:first-child{border:0}
.msg .h{display:flex;justify-content:space-between;font-size:12px;color:var(--mut)}.seen{color:var(--ok)}
.msg p{margin:3px 0 0;white-space:pre-wrap;word-break:break-word}
.sw{display:flex;align-items:center;gap:8px;color:var(--txt);font-size:15px;margin:2px 0 4px}.sw input{width:auto;transform:scale(1.3)}
hr{border:0;border-top:1px solid var(--line);margin:14px 0}
details summary{cursor:pointer;color:var(--mut)}label{display:block;font-size:13px;color:var(--mut);margin-top:10px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:#222731;border:1px solid var(--line);padding:9px 14px;border-radius:10px;opacity:0;transition:.25s;pointer-events:none}
#toast.show{opacity:1}code{background:#0d0f13;padding:2px 5px;border-radius:6px;font-size:12px}
.net{display:flex;justify-content:space-between;padding:7px 4px;border-bottom:1px solid var(--line);cursor:pointer}
</style></head><body><main>
<header><div id="face">🙂</div><div><h1 id="nm">Desk Buddy</h1><div class="mut" id="st">conectando…</div></div></header>

<section class="card"><h2>Expresión</h2>
<button class="ex auto" id="auto"><b>🕘</b>Automático por hora<br><span class="mut">más alegre a la mañana, cansado a las 18</span></button>
<div class="grid" id="grid"></div></section>

<section class="card"><h2>Mandar mensaje</h2>
<input id="from" maxlength="16" placeholder="Tu nombre">
<textarea id="text" maxlength="280" placeholder="Escribí tu mensaje…" style="margin-top:8px"></textarea>
<div class="chips" id="chips"></div>
<div class="row"><select id="mexpr"><option value="">Sin cambiar la cara</option></select>
<button class="main" id="send" style="flex:0 0 auto">Enviar ✉️</button></div>
<div class="mut" id="cnt" style="margin-top:6px">0/280</div></section>

<section class="card"><h2>Extras</h2>
<label class="sw"><input type="checkbox" id="clock"> 🕒 Mostrar el reloj</label>
<div class="row"><div><label>cada (segundos)</label><input type="number" id="clockEvery" min="5" max="3600"></div>
<div><label>durante (segundos)</label><input type="number" id="clockFor" min="1" max="60"></div></div>
<hr>
<label class="sw"><input type="checkbox" id="buddy"> 💬 Modo compañero</label>
<div class="mut">Cada tanto hace una cara y te dice algo (“¡Hola Kharlita!!”).</div>
<div class="row"><div><label>Nombre</label><input id="owner" maxlength="16"></div>
<div><label>cada (minutos, aprox.)</label><input type="number" id="buddyEvery" min="1" max="240"></div></div>
<hr>
<label class="sw"><input type="checkbox" id="weather"> 🌦 Mostrar el clima</label>
<div class="mut" id="wxnow">Sin datos del clima todavía.</div>
<div class="row"><div><label>cada (segundos)</label><input type="number" id="weatherEvery" min="5" max="3600"></div>
<div><label>durante (segundos)</label><input type="number" id="weatherFor" min="1" max="60"></div></div>
<label>Ciudad</label>
<div class="row" style="margin-top:4px"><input id="city" placeholder="Ej: Rosario" style="flex:2"><button class="sec" id="findCity" style="flex:1">Buscar</button></div>
<div class="mut" id="cityRes"></div>
<label class="sw" style="margin-top:10px"><input type="checkbox" id="weatherReact"> ☂️ Reaccionar al clima</label>
<div class="mut">Si va a llover, hace frío o mucho calor, pone caras y te lo dice (“Va a llover, Kharlita. ¡Llevá paraguas!”).</div>
<div class="row"><button class="sec" id="testW">Ver el clima ahora</button></div>
<hr>
<label class="sw"><input type="checkbox" id="night"> 🌙 Apagar la pantalla de noche</label>
<div class="row"><div><label>desde</label><input type="time" id="nightFrom"></div>
<div><label>hasta</label><input type="time" id="nightTo"></div></div>
<label class="sw" style="font-size:14px"><input type="checkbox" id="nightMsgs"> Prender igual si llega un mensaje</label>
<div class="mut">De noche, tocando el sensor se prende 15 s. Los mensajes que lleguen se muestran a la mañana.</div>
<div class="row"><button class="main" id="saveX">Guardar</button><button class="sec" id="testB">Probar ahora</button></div>
</section>

<section class="card"><h2>Mensajes</h2><div class="mut" style="margin-bottom:6px">🔒 El contenido es privado: solo se ve en la pantalla del buddy.</div><div id="msgs" class="mut">Sin mensajes todavía.</div></section>

<section class="card"><details><summary>Ajustes</summary>
<label>Nombre del companion</label><input id="sname" maxlength="20">
<label>Brillo <span id="bv"></span></label><input type="range" id="bright" min="1" max="255">
<label><input type="checkbox" id="flip" style="width:auto"> Rotar pantalla 180°</label>
<div class="row"><button class="sec" id="save">Guardar ajustes</button><button class="sec" id="clear">Borrar mensajes</button></div>
<label>WiFi</label><div id="nets"></div>
<div class="row"><button class="sec" id="scan">Buscar redes</button></div>
<input id="ssid" placeholder="Nombre de red (SSID)" style="margin-top:8px">
<input id="pass" type="password" placeholder="Contraseña" style="margin-top:8px">
<div class="row"><button class="main" id="wifi">Conectar y reiniciar</button><button class="sec" id="forget">Olvidar WiFi</button></div>
<div class="row"><button class="sec" onclick="location.href='/ota'">Actualizar firmware (OTA)</button></div>
<p class="mut">Versión <span id="ver"></span> · API: <code>/api/msg?from=Ana&amp;text=Hola</code> · <code>/api/expr?id=happy</code> · <code>/api/state</code></p>
</details></section>
</main><div id="toast"></div>
<script>
const PAGE_VER=")HTML" FW_VERSION R"HTML(";
const $=s=>document.querySelector(s);let S=null,built=false;
const toast=t=>{const e=$('#toast');e.textContent=t;e.classList.add('show');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('show'),1800)};
async function api(p,d){const o=d?{method:'POST',body:new URLSearchParams(d)}:{};const r=await fetch(p,o);return r.json()}
const esc=s=>s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function ago(s){if(s<60)return'hace '+s+'s';if(s<3600)return'hace '+Math.floor(s/60)+' min';return'hace '+Math.floor(s/3600)+' h'}
function build(){const g=$('#grid'),m=$('#mexpr');
 S.exprs.forEach(x=>{const b=document.createElement('button');b.className='ex';b.dataset.id=x.id;b.innerHTML='<b>'+x.emoji+'</b>'+esc(x.name);
  b.onclick=async()=>{await api('/api/expr',{id:x.id});toast(x.emoji+' '+x.name+' (fija)');load()};g.appendChild(b);
  const o=document.createElement('option');o.value=x.id;o.textContent=x.emoji+' '+x.name;m.appendChild(o)});
 $('#ver').textContent=S.version;
 $('#clock').checked=S.clock;$('#clockEvery').value=S.clockEvery;$('#clockFor').value=S.clockFor;
 $('#weather').checked=S.weather;$('#weatherEvery').value=S.weatherEvery;$('#weatherFor').value=S.weatherFor;
 $('#weatherReact').checked=S.weatherReact;$('#city').value=S.city;
 $('#night').checked=S.night;$('#nightFrom').value=S.nightFrom;$('#nightTo').value=S.nightTo;$('#nightMsgs').checked=S.nightMsgs;
 $('#buddy').checked=S.buddy;$('#buddyEvery').value=S.buddyEvery;$('#owner').value=S.owner;$('#sname').value=S.name;$('#bright').value=S.bright;$('#bv').textContent=S.bright;$('#flip').checked=S.flip;built=true}
function render(){if(!built)build();
 const cur=S.exprs.find(x=>x.id==S.expr)||S.exprs[0];$('#face').textContent=cur.emoji;$('#nm').textContent=S.name;document.title=S.name;
 $('#st').textContent=S.time+(S.wx.ok?' · '+Math.round(S.wx.temp)+'°':'')+(S.screenOff?' · 🌙 pantalla apagada':'')+' · '+(S.online?'WiFi '+S.ssid+' · '+S.ip:(S.ap?'Modo AP · '+S.ssid:'buscando '+S.ssid))+(S.unread?' · '+S.unread+' sin leer':'');
 $('#auto').classList.toggle('on',S.auto);
 const w=S.wx;$('#wxnow').textContent=w.ok?('Ahora en '+S.city+': '+Math.round(w.temp)+'° '+w.desc+' · '+w.min+'°/'+w.max+'°'+(w.rain>=20?' · lluvia '+w.rain+'%':'')):
  (w.error?'Clima: error ('+w.error+')':'Sin datos del clima todavía (necesita WiFi con internet).');
 document.querySelectorAll('.ex').forEach(b=>b.classList.toggle('on',b.dataset.id==S.expr));
 const L=$('#msgs');if(!S.msgs.length){L.innerHTML='Sin mensajes todavía.';return}
 L.innerHTML=S.msgs.slice().reverse().map(m=>'<div class="msg"><div class="h"><span>'+esc(m.from)+' · '+ago(m.ago)+'</span><span class="'+(m.read?'seen':'')+'">'+(m.read?'✓✓ visto':'✓ entregado')+'</span></div><p>✉️ Mensaje de '+esc(m.from)+'</p></div>').join('')}
async function load(){try{S=await api('/api/state');
 if(S.version!==PAGE_VER&&!sessionStorage.getItem('rl'+S.version)){sessionStorage.setItem('rl'+S.version,1);location.reload();return}
 render()}catch(e){$('#st').textContent='sin conexión…'}}
['¡Hola! 👋','¿Café? ☕','Te quiero ❤️','¡Buen trabajo! 👍','¿Almorzamos?','Vení un toque','Ya llego 🏃'].forEach(t=>{const c=document.createElement('span');c.className='chip';c.textContent=t;c.onclick=()=>{$('#text').value=t;upd()};$('#chips').appendChild(c)});
const upd=()=>$('#cnt').textContent=$('#text').value.length+'/280';$('#text').oninput=upd;
$('#from').value=localStorage.getItem('db_from')||'';
$('#send').onclick=async()=>{const text=$('#text').value.trim();if(!text)return toast('Escribí algo');
 const from=$('#from').value.trim()||'Anónimo';localStorage.setItem('db_from',from);
 const r=await api('/api/msg',{from,text,expr:$('#mexpr').value});if(r.ok){$('#text').value='';upd();toast('Enviado ✉️');load()}else toast(r.error||'Error')};
$('#auto').onclick=async()=>{await api('/api/auto',{on:S.auto?0:1});toast(S.auto?'Cara fija':'🕘 Automático por hora');load()};
$('#saveX').onclick=async()=>{await api('/api/settings',{clock:$('#clock').checked?1:0,clockEvery:$('#clockEvery').value,clockFor:$('#clockFor').value,
 weather:$('#weather').checked?1:0,weatherEvery:$('#weatherEvery').value,weatherFor:$('#weatherFor').value,weatherReact:$('#weatherReact').checked?1:0,
 ...(geo?{lat:geo.lat,lon:geo.lon,city:geo.city}:{}),
 night:$('#night').checked?1:0,nightFrom:$('#nightFrom').value,nightTo:$('#nightTo').value,nightMsgs:$('#nightMsgs').checked?1:0,
 buddy:$('#buddy').checked?1:0,buddyEvery:$('#buddyEvery').value,owner:$('#owner').value});toast('Guardado');geo=null;$('#cityRes').textContent='';load()};
let geo=null;
$('#findCity').onclick=async()=>{const q=$('#city').value.trim();if(!q)return;$('#cityRes').textContent='Buscando…';
 try{const r=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=1&language=es&name='+encodeURIComponent(q))).json();
  const c=r.results&&r.results[0];if(!c){$('#cityRes').textContent='No la encontré';return}
  geo={lat:c.latitude.toFixed(4),lon:c.longitude.toFixed(4),city:c.name};
  $('#cityRes').textContent='📍 '+[c.name,c.admin1,c.country].filter(Boolean).join(', ')+' — tocá Guardar'}
 catch(e){$('#cityRes').textContent='Sin internet en este celular para buscar'}};
$('#testW').onclick=async()=>{const r=await api('/api/weather',{show:1});toast(r.ok?'🌦 Mirá la pantalla':(r.error||'Error'))};
$('#testB').onclick=async()=>{await api('/api/buddy',{});toast('💬 Mirá la pantalla')};
$('#bright').oninput=e=>$('#bv').textContent=e.target.value;
$('#save').onclick=async()=>{await api('/api/settings',{name:$('#sname').value,bright:$('#bright').value,flip:$('#flip').checked?1:0});toast('Guardado');load()};
$('#clear').onclick=async()=>{if(confirm('¿Borrar todos los mensajes?')){await api('/api/clear',{});load()}};
$('#scan').onclick=async()=>{$('#nets').textContent='Buscando…';const n=await api('/api/scan');
 $('#nets').innerHTML=n.map(x=>'<div class="net" data-s="'+esc(x.ssid)+'"><span>'+esc(x.ssid)+'</span><span class="mut">'+(x.lock?'🔒 ':'')+x.rssi+' dBm</span></div>').join('')||'Nada encontrado';
 document.querySelectorAll('.net').forEach(d=>d.onclick=()=>{$('#ssid').value=d.dataset.s;$('#pass').focus()})};
$('#wifi').onclick=async()=>{if(!$('#ssid').value)return toast('Elegí una red');await api('/api/wifi',{ssid:$('#ssid').value,pass:$('#pass').value});
 toast('Reiniciando… buscalo en http://deskbuddy.local')};
$('#forget').onclick=async()=>{if(confirm('¿Olvidar la red WiFi?')){await api('/api/forget',{});toast('Reiniciando en modo AP')}};
load();setInterval(load,3000);
</script></body></html>)HTML";

// Pagina de actualizacion por WiFi (protegida con OTA_USER / OTA_PASS)
static const char OTA_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Actualizar firmware</title>
<style>body{margin:0;font:15px/1.45 system-ui,sans-serif;background:#0f1115;color:#e9ecf1}main{max-width:520px;margin:0 auto;padding:20px}
.card{background:#181b22;border:1px solid #262a33;border-radius:16px;padding:18px}h1{font-size:20px}.mut{color:#8b93a3;font-size:13px}
input[type=file]{width:100%;margin:12px 0;color:#e9ecf1}button{background:#7cc4ff;color:#04121f;border:0;border-radius:10px;padding:11px 16px;font-weight:600;width:100%;cursor:pointer}
button:disabled{opacity:.5}.bar{height:12px;background:#0d0f13;border-radius:8px;overflow:hidden;margin-top:14px}.bar i{display:block;height:100%;width:0;background:#6ee7a8;transition:width .2s}
a{color:#7cc4ff}</style></head><body><main>
<h1>Actualizar firmware</h1><div class="card">
<div class="mut">Versión actual: <b id="v">…</b></div>
<input type="file" id="f" accept=".bin">
<button id="go">Subir y actualizar</button>
<div class="bar"><i id="p"></i></div><p id="s" class="mut">Usá el archivo <b>ota.bin</b> (no el factory).</p>
</div><p><a href="/">← Volver</a></p></main>
<script>
const $=s=>document.querySelector(s);
fetch('/api/state').then(r=>r.json()).then(j=>$('#v').textContent=j.version);
$('#go').onclick=async()=>{const f=$('#f').files[0];if(!f)return $('#s').textContent='Elegí un archivo .bin';
 const head=new Uint8Array(await f.slice(0,4).arrayBuffer());
 if(head[0]!==0xE9)return $('#s').textContent='Ese archivo no es un firmware de ESP32.';
 const pt=new Uint8Array(await f.slice(0x8000,0x8002).arrayBuffer());
 if(/factory/i.test(f.name)||(pt[0]===0xAA&&pt[1]===0x50))return $('#s').textContent='Ese es el factory.bin (solo para USB). Usá ota.bin.';
 $('#go').disabled=true;const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest();
 x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded*100/e.total);$('#p').style.width=p+'%';$('#s').textContent='Subiendo… '+p+'%'}};
 x.onload=()=>{let ok=false,err='';try{const j=JSON.parse(x.responseText);ok=j.ok;err=j.error||''}catch(e){}
  if(ok){$('#s').textContent='¡Listo! Reiniciando… volvé en unos segundos.';setTimeout(()=>location.href='/',9000)}
  else{$('#s').textContent='Error: '+(err||x.status);$('#go').disabled=false}};
 x.onerror=()=>{$('#s').textContent='Se cortó la conexión.';$('#go').disabled=false};
 x.open('POST','/ota');x.send(fd)};
</script></body></html>)HTML";
