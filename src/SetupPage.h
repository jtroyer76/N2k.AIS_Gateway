// The setup page (SPEC 7.5): WiFi network, AIS baud and device password. One self-contained
// HTML file, no external resources. Served at /setup, and at / while no network is saved.
#pragma once

#include <pgmspace.h>

static const char kSetupPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AIS Gateway setup</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1c1e21;--dim:#65676b;--line:#dadde1;--ok:#1e8e3e;--bad:#d93025;--acc:#1a73e8}
@media (prefers-color-scheme:dark){:root{--bg:#101214;--card:#1b1e22;--fg:#e4e6eb;--dim:#9aa0a6;--line:#2f3338;--ok:#3fb950;--bad:#f85149;--acc:#8ab4f8}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;border-bottom:1px solid var(--line);background:var(--card)}
header h1{font-size:17px;margin:0;flex:1}
header a{color:var(--acc)}
main{padding:16px;max-width:520px;margin:0 auto}
.card{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:12px 14px;margin-bottom:12px}
label{display:block;font-weight:600;margin:10px 0 4px}
.hint{color:var(--dim);font-size:13px;font-weight:400}
input,select{width:100%;font:inherit;padding:8px 10px;border-radius:6px;border:1px solid var(--line);background:var(--bg);color:var(--fg)}
button{font:inherit;padding:8px 14px;border-radius:6px;border:1px solid var(--line);background:var(--card);color:var(--fg);cursor:pointer}
button.primary{background:var(--acc);border-color:var(--acc);color:#fff;width:100%;margin-top:14px}
#nets{list-style:none;margin:6px 0 0;padding:0;max-height:220px;overflow:auto}
#nets li{display:flex;justify-content:space-between;padding:8px 10px;border:1px solid var(--line);border-radius:6px;margin-bottom:4px;cursor:pointer}
#nets li.sel{border-color:var(--acc)}
#nets li span{color:var(--dim);font-size:13px}
.pw{font:16px ui-monospace,Consolas,Menlo,monospace;letter-spacing:.05em}
.msg{display:none;padding:10px 12px;border-radius:6px;margin-top:12px}
.msg.ok{display:block;background:var(--ok);color:#fff}.msg.bad{display:block;background:var(--bad);color:#fff}
.row{display:flex;gap:8px;align-items:center}.row h2{flex:1;font-size:15px;margin:0}
</style></head><body>
<header><h1>AIS Gateway setup</h1><a href="/" id="home">Status</a></header>
<main>
<form id="f" autocomplete="off">
<div class="card">
<div class="row"><h2>WiFi network</h2><button type="button" id="scan">Scan</button></div>
<ul id="nets"><li><span>Looking for networks&hellip;</span></li></ul>
<label for="ssid">Network name <span class="hint">(case matters)</span></label>
<input id="ssid" name="ssid" maxlength="32" required>
<label for="wifi_pass">WiFi password <span class="hint" id="wphint">(blank for an open network)</span></label>
<input id="wifi_pass" name="wifi_pass" type="password" maxlength="63">
</div>
<div class="card">
<label for="ais_baud">AIS input baud</label>
<select id="ais_baud" name="ais_baud"><option>38400</option><option>4800</option></select>
<div class="hint">38400 for AIS receivers and transponders; 4800 only if yours is set to it.</div>
</div>
<div class="card">
<label>Device password</label>
<div class="hint" id="pwinfo">Protects updates, these settings and the gateway's own hotspot.</div>
<div class="pw" id="pw"></div>
<label for="new_pass">New device password <span class="hint">(optional, 8&ndash;63 characters)</span></label>
<input id="new_pass" name="new_pass" type="text" minlength="8" maxlength="63" autocapitalize="off">
<div id="cur"><label for="password">Current device password</label>
<input id="password" name="password" type="password"></div>
</div>
<button class="primary" type="submit">Save and restart</button>
<div id="msg" class="msg"></div>
</form>
<div class="card" style="margin-top:12px">
<div class="row"><h2>Firmware update</h2></div>
<div class="hint">Running <span id="fwver"></span>. Choose a release's <code>-update.bin</code>. AIS bridging pauses during the upload. If the new firmware can't reach the NMEA 2000 bus within 5 minutes, the gateway goes back to this version by itself.</div>
<label for="fwfile">Firmware file</label><input type="file" id="fwfile" accept=".bin">
<div id="fwpw"><label for="fwpass">Device password</label><input id="fwpass" type="password"></div>
<button type="button" id="fwgo" style="margin-top:10px">Upload and restart</button>
<div id="fwmsg" class="msg"></div>
</div>
</main>
<script>
const $=id=>document.getElementById(id);
let info={};
function msg(text,ok,id='msg'){const m=$(id);m.textContent=text;m.className='msg '+(ok?'ok':'bad');}
function bars(r){return r>-60?'strong':r>-75?'good':'weak';}
async function nets(){
 try{
  const j=await (await fetch('/api/scan',{cache:'no-store'})).json(),ul=$('nets');
  ul.innerHTML='';
  if(!j.networks.length){const li=document.createElement('li');li.innerHTML='<span>'+(j.scanning?'Scanning&hellip;':'No networks found')+'</span>';ul.appendChild(li);}
  for(const n of j.networks){
   const li=document.createElement('li'),a=document.createElement('b'),s=document.createElement('span');
   a.textContent=n[0];s.textContent=bars(n[1])+(n[2]?', open':'');li.append(a,s);
   if(n[0]===$('ssid').value)li.className='sel';
   li.onclick=()=>{$('ssid').value=n[0];for(const x of ul.children)x.className='';li.className='sel';$('wifi_pass').focus();};
   ul.appendChild(li);
  }
  if(j.scanning)setTimeout(nets,2000);
 }catch(e){setTimeout(nets,3000);}
}
async function load(){
 info=await (await fetch('/api/setup',{cache:'no-store'})).json();
 $('ssid').value=info.ssid;$('ais_baud').value=String(info.ais_baud);
 if(info.ssid)$('wphint').textContent='(blank keeps the saved one for this network)';
 if(info.setup)$('home').style.display='none';
 // Through a hotspot the password is shown and not asked for (SPEC 7.2).
 if(info.via_hotspot){$('pw').textContent=info.device_pass;$('cur').style.display='none';}
 else{$('pw').textContent='';$('password').required=true;}
 $('scan').style.display=info.via_hotspot?'none':'';
 $('fwver').textContent='v'+info.version;
 if(info.via_hotspot){$('fwpass').value=info.device_pass;$('fwpw').style.display='none';}
 nets();
}
$('scan').onclick=async()=>{try{await fetch('/api/scan',{method:'POST'});}catch(e){}setTimeout(nets,500);};
$('f').onsubmit=async e=>{
 e.preventDefault();
 try{
  const r=await fetch('/api/settings',{method:'POST',body:new URLSearchParams(new FormData($('f')))});
  const j=await r.json();
  if(!r.ok){msg(j.error||'Not saved',false);return;}
  const np=$('new_pass').value;
  msg('Saved. The gateway is restarting and will join "'+$('ssid').value+'". Join that network too, then open http://'+info.hostname+'.local/ .'+
   (np?' Your device password is now: '+np:''),true);
 }catch(err){msg("Can't reach the gateway.",false);}
};
$('fwgo').onclick=async()=>{
 const f=$('fwfile').files[0];
 if(!f){msg('Choose a firmware file first.',false,'fwmsg');return;}
 if(!confirm('Upload '+f.name+' and restart? AIS bridging pauses during the upload.'))return;
 msg('Uploading…',true,'fwmsg');
 try{
  const r=await fetch('/ota',{method:'POST',headers:{'X-OTA-Password':$('fwpass').value},body:f});
  const j=await r.json();
  if(!r.ok){msg(j.error||'Upload failed',false,'fwmsg');return;}
  msg('Uploaded. The gateway is restarting with the new firmware.',true,'fwmsg');
 }catch(e){msg("Upload failed: can't reach the gateway.",false,'fwmsg');}
};
load().catch(()=>msg("Can't reach the gateway.",false));
</script></body></html>)HTML";
