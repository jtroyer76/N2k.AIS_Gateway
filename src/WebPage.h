// The status and console page (SPEC 6.2): one self-contained HTML file, no external resources
// (the boat may have no internet). Status every 2 s, console every 1 s.
#pragma once

#include <pgmspace.h>

static const char kWebPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AIS Gateway</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1c1e21;--dim:#65676b;--line:#dadde1;--ok:#1e8e3e;--warn:#c77700;--bad:#d93025;--con:#0f1115;--conf:#d7dae0}
@media (prefers-color-scheme:dark){:root{--bg:#101214;--card:#1b1e22;--fg:#e4e6eb;--dim:#9aa0a6;--line:#2f3338;--ok:#3fb950;--warn:#e3a008;--bad:#f85149;--con:#0a0b0d}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;border-bottom:1px solid var(--line);background:var(--card)}
header h1{font-size:17px;margin:0;flex:1}
header small{color:var(--dim)}
main{padding:16px;max-width:1100px;margin:0 auto}
.tiles{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:10px}
.tile{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px 12px}
.tile h2{font-size:12px;font-weight:600;text-transform:uppercase;letter-spacing:.04em;color:var(--dim);margin:0 0 4px}
.tile .v{font-size:20px;font-weight:600}
.tile .s{color:var(--dim);font-size:13px;overflow-wrap:anywhere}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
section{margin-top:16px}
.bar{display:flex;align-items:center;gap:8px;margin-bottom:6px}
.bar h3{margin:0;font-size:15px;flex:1}
button{font:inherit;padding:5px 12px;border-radius:6px;border:1px solid var(--line);background:var(--card);color:var(--fg);cursor:pointer}
button.danger{border-color:var(--bad);color:var(--bad)}
#con,#raw{background:var(--con);color:var(--conf);font:12px/1.45 ui-monospace,Consolas,Menlo,monospace;border-radius:8px;padding:8px 10px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;margin:0}
#con{height:50vh;min-height:240px}
#con .W{color:#e3a008}#con .E{color:#f85149}#con .m{color:#8ab4f8}
details{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:8px 12px}
summary{cursor:pointer;font-weight:600}
#raw{margin-top:8px;max-height:40vh}
#err{display:none;padding:8px 16px;background:var(--bad);color:#fff}
</style></head><body>
<header><h1>AIS Gateway</h1><small id="ver"></small><a href="/setup" style="color:inherit">Settings</a><button class="danger" id="rst">Restart</button></header>
<div id="err">Can't reach the gateway. Retrying&hellip;</div>
<main>
<div class="tiles">
<div class="tile"><h2>Last AIS</h2><div class="v" id="t-ais">&ndash;</div><div class="s" id="t-ais2"></div></div>
<div class="tile"><h2>NMEA 2000</h2><div class="v" id="t-n2k">&ndash;</div><div class="s" id="t-n2k2"></div></div>
<div class="tile"><h2>CAN</h2><div class="v" id="t-can">&ndash;</div><div class="s" id="t-can2"></div></div>
<div class="tile"><h2>WiFi</h2><div class="v" id="t-wifi">&ndash;</div><div class="s" id="t-wifi2"></div></div>
<div class="tile"><h2>Uptime</h2><div class="v" id="t-up">&ndash;</div><div class="s" id="t-up2"></div></div>
<div class="tile"><h2>Memory</h2><div class="v" id="t-mem">&ndash;</div><div class="s" id="t-mem2"></div></div>
</div>
<section>
<div class="bar"><h3>Console</h3><label><input type="checkbox" id="auto" checked> Auto-scroll</label><button id="pause">Pause</button></div>
<pre id="con"></pre>
</section>
<section><details id="rawbox"><summary>Last 20 AIS sentences</summary><pre id="raw">none yet</pre></details></section>
</main>
<script>
const $=id=>document.getElementById(id);
const MAXLINES=1000;
let since=0,paused=false,logTimer=null,lastUp=0;
function set(id,text,cls){const e=$(id);e.textContent=text;if(cls!==undefined)e.className='v '+cls;}
function dur(s){s=Math.floor(s);const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
 return d?d+'d '+h+'h':h?h+'h '+m+'m':m?m+'m '+(s%60)+'s':s+'s';}
function kb(b){return Math.round(b/1024)+' KB';}
async function status(){
 try{
  const r=await fetch('/api/status',{cache:'no-store'});const s=await r.json();$('err').style.display='none';
  // Uptime went backwards: the gateway restarted, so its log numbering starts again.
  if(s.uptime_ms<lastUp&&since){since=0;addLine('m','--- gateway restarted ---');}
  lastUp=s.uptime_ms;
  $('ver').textContent='v'+s.version+' · '+s.partition;
  const a=s.ais,t=a.since_last_s;
  if(t===null)set('t-ais','never','bad');else set('t-ais',dur(t)+' ago',t<60?'ok':t<300?'warn':'bad');
  $('t-ais2').textContent=a.ais+' AIS, '+a.decoded+' decoded, '+a.errors+' errors · '+a.baud+' baud';
  const n=s.n2k;set('t-n2k','addr '+n.address,n.failed?'warn':'ok');
  $('t-n2k2').textContent=n.sent+' sent, '+n.failed+' failed, '+n.rx+' rx';
  const c=s.can;set('t-can',c.state,c.state==='running'?(c.rec>=96||c.tec>=96?'warn':'ok'):'bad');
  $('t-can2').textContent='TEC '+c.tec+', REC '+c.rec+' · bus-off '+c.bus_off+', recovered '+c.recoveries+
   (c.restart_failures?', restart failures '+c.restart_failures:'')+' · tx failed '+c.tx_failed+', rx missed '+c.rx_missed+', overrun '+c.rx_overrun;
  const w=s.wifi;
  if(w.connected){set('t-wifi',w.ssid,w.rssi>-75?'ok':'warn');$('t-wifi2').textContent=w.ip+' · RSSI '+w.rssi+(w.hotspot?' · hotspot on':'');}
  else if(w.hotspot){set('t-wifi','hotspot','warn');$('t-wifi2').textContent=w.hotspot_ip+' · '+w.hotspot_clients+' client(s)';}
  else{set('t-wifi','not connected','bad');$('t-wifi2').textContent='';}
  set('t-up',dur(s.uptime_ms/1000),'');$('t-up2').textContent='reset: '+s.reset;
  set('t-mem',kb(s.heap.free),'');$('t-mem2').textContent='min '+kb(s.heap.min);
  $('raw').textContent=s.raw.length?s.raw.join('\n'):'none yet';
 }catch(e){$('err').style.display='block';}
}
function addLine(cls,text){
 const con=$('con'),atEnd=con.scrollTop+con.clientHeight>=con.scrollHeight-4;
 const d=document.createElement('div');d.className=cls;d.textContent=text;con.appendChild(d);
 while(con.childNodes.length>MAXLINES)con.removeChild(con.firstChild);
 if($('auto').checked&&atEnd)con.scrollTop=con.scrollHeight;
}
async function log(){
 logTimer=null;if(paused)return;
 let again=1000;
 try{
  const r=await fetch('/api/log?since='+since,{cache:'no-store'});const j=await r.json();
  if(j.reset&&since)addLine('m','--- gateway restarted ---');
  if(j.lost)addLine('m','--- '+j.lost+' line(s) not shown (buffer wrapped) ---');
  for(const l of j.lines)addLine(l[2],'['+(l[1]/1000).toFixed(3).padStart(10)+'] '+l[2]+' '+l[3]);
  since=j.next;if(j.more)again=50;
 }catch(e){again=3000;}
 if(!paused)logTimer=setTimeout(log,again);
}
$('pause').onclick=()=>{paused=!paused;$('pause').textContent=paused?'Resume':'Pause';if(!paused&&!logTimer)log();};
$('rst').onclick=async()=>{
 if(!confirm('Restart the gateway? AIS bridging stops for a few seconds.'))return;
 try{await fetch('/api/restart',{method:'POST'});}catch(e){}
 addLine('m','--- restart requested ---');
};
status();setInterval(status,2000);log();
</script></body></html>)HTML";
