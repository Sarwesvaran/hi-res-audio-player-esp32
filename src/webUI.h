#pragma once
#include <Arduino.h>

const char WEB_HTML[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>SARVS Hi-Fi OS</title>
<style>
:root{--bg:#07070f;--bg2:#0d0d1a;--glass:rgba(255,255,255,.04);--gborder:rgba(255,255,255,.08);--ac:#8b5cf6;--ac2:#3b82f6;--gr:linear-gradient(135deg,#8b5cf6,#3b82f6);--tx:#f1f5f9;--mu:#64748b;--rd:#ef4444;}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
html{scroll-behavior:smooth}
body{background:var(--bg);color:var(--tx);font-family:-apple-system,sans-serif;min-height:100vh;overflow-x:hidden;background-image:radial-gradient(ellipse at 20% 0%,rgba(139,92,246,.08) 0%,transparent 60%),radial-gradient(ellipse at 80% 100%,rgba(59,130,246,.06) 0%,transparent 60%)}
.hdr{position:sticky;top:0;z-index:100;background:rgba(7,7,15,.85);backdrop-filter:blur(24px);border-bottom:1px solid var(--gborder);display:flex;align-items:center;justify-content:space-between;padding:12px 18px}
.logo{font-size:22px;font-weight:900;letter-spacing:5px;background:var(--gr);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.logo-sub{font-size:8px;color:var(--mu);letter-spacing:5px;margin-top:2px;text-transform:uppercase}
.tabs{display:flex;background:rgba(0,0,0,.4);border-bottom:1px solid var(--gborder);overflow-x:auto;-webkit-overflow-scrolling:touch;scrollbar-width:none}
.tb{flex:1;min-width:72px;padding:13px 10px;background:none;border:none;border-bottom:2px solid transparent;color:var(--mu);font-size:11px;font-weight:700;letter-spacing:.8px;cursor:pointer;transition:.2s;white-space:nowrap}
.tb.on{color:var(--tx);border-bottom-color:var(--ac)}
.content{padding:14px;padding-bottom:28px;max-width:540px;margin:0 auto}
.pane{display:none}.pane.on{display:block;animation:fadeIn .25s}
@keyframes fadeIn{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
.card{background:var(--glass);border:1px solid var(--gborder);border-radius:16px;padding:18px;margin-bottom:14px;backdrop-filter:blur(12px)}
.ct{font-size:9px;font-weight:800;letter-spacing:2.5px;color:var(--mu);text-transform:uppercase;margin-bottom:14px}
.sgrid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.sb{background:rgba(255,255,255,.03);border:1px solid var(--gborder);border-radius:11px;padding:13px 6px;display:flex;flex-direction:column;align-items:center;gap:5px;cursor:pointer;transition:all .2s;font-size:9px;font-weight:800;color:var(--mu);letter-spacing:.5px;user-select:none}
.sb.on{background:linear-gradient(135deg,rgba(139,92,246,.18),rgba(59,130,246,.12));border-color:rgba(139,92,246,.5);color:#c4b5fd}
.si{font-size:21px;line-height:1}
.nprow{display:flex;align-items:center;gap:14px;margin-bottom:16px}
.art{width:60px;height:60px;border-radius:14px;flex-shrink:0;background:var(--gr);display:flex;align-items:center;justify-content:center;font-size:24px;position:relative;overflow:hidden}
.npi{flex:1;min-width:0}
.npt{font-size:16px;font-weight:800;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.nps{font-size:11px;color:var(--mu);margin-top:4px}
.transport{display:flex;align-items:center;justify-content:center;gap:20px}
.tbtn{background:none;border:none;color:var(--mu);cursor:pointer;font-size:20px;padding:9px;transition:all .15s}
.tplay{width:52px;height:52px;background:var(--gr);border:none;border-radius:50%;color:#fff;font-size:20px;cursor:pointer}
.sl{-webkit-appearance:none;width:100%;height:4px;background:rgba(255,255,255,.1);border-radius:3px;outline:none;cursor:pointer;margin:10px 0}
.sl::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:#fff;cursor:pointer}
.inp{width:100%;padding:11px 14px;border-radius:10px;background:rgba(255,255,255,.06);border:1px solid var(--gborder);color:var(--tx);font-size:14px;outline:none;margin-bottom:10px}
.btn{display:block;width:100%;padding:12px 18px;border-radius:10px;border:none;font-weight:800;font-size:12px;cursor:pointer;margin-bottom:9px;text-align:center}
.btn-p{background:var(--gr);color:#fff}
.btn-g{background:rgba(255,255,255,.07);color:var(--tx);border:1px solid var(--gborder)}
#toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);padding:9px 20px;border-radius:22px;font-size:12px;font-weight:700;opacity:0;transition:.3s;z-index:999;background:rgba(16,185,129,.95);color:#fff;pointer-events:none}
#toast.on{opacity:1}
</style>
</head>
<body>

<div class="hdr"><div class="logo-wrap"><div class="logo">SARVS</div><div class="logo-sub">Hi-Fi DAP Engine v7.7</div></div></div>

<div class="tabs">
  <button class="tb on" onclick="tab('player',this)">▶ PLAYER</button>
  <button class="tb" onclick="tab('dsp',this)">⚡ DSP</button>
  <button class="tb" onclick="tab('radio',this)">📻 RADIO & FM</button>
  <button class="tb" onclick="tab('sys',this)">⚙ SYSTEM</button>
</div>

<div class="content">

<div id="pane-player" class="pane on">
  <div class="card">
    <div class="ct">Audio Source</div>
    <div class="sgrid">
      <div class="sb" id="s0" onclick="cmd('src','0')"><span class="si">💾</span>SD CARD</div>
      <div class="sb" id="s1" onclick="cmd('src','1')"><span class="si">🔵</span>BLUETOOTH</div>
      <div class="sb" id="s2" onclick="cmd('src','2')"><span class="si">📡</span>NET RADIO</div>
      <div class="sb" id="s3" onclick="cmd('src','3')"><span class="si">📻</span>FM TUNER</div>
      <div class="sb" id="s4" onclick="cmd('src','4')"><span class="si">🔌</span>AUX IN</div>
    </div>
  </div>

  <div class="card">
    <div class="nprow">
      <div class="art">🎵</div>
      <div class="npi">
        <div class="npt" id="npt">Loading...</div>
        <div class="nps" id="nps">—</div>
      </div>
    </div>
    <div class="transport">
      <button class="tbtn" onclick="cmd('cmd','prev')">⏮</button>
      <button class="tplay" id="bplay" onclick="cmd('cmd','play')">⏸</button>
      <button class="tbtn" onclick="cmd('cmd','next')">⏭</button>
    </div>
    
    <div style="margin-top:20px">
      <div class="ct" style="display:flex;justify-content:space-between"><span>Master Volume</span><span id="volV">40</span></div>
      <input type="range" id="volSl" min="0" max="100" class="sl" onchange="cmd('vol',this.value)" oninput="document.getElementById('volV').innerText=this.value">
    </div>
  </div>
</div>

<div id="pane-dsp" class="pane">
  <div class="card">
    <div class="ct">Tone Controls (-15 to +15)</div>
    <div style="display:flex;justify-content:space-between;font-size:11px"><span>BASS</span><span id="bV">0</span></div>
    <input type="range" id="bSl" min="-15" max="15" class="sl" onchange="cmd('bass',this.value)" oninput="document.getElementById('bV').innerText=this.value">
    
    <div style="display:flex;justify-content:space-between;font-size:11px;margin-top:10px"><span>MID</span><span id="mV">0</span></div>
    <input type="range" id="mSl" min="-15" max="15" class="sl" onchange="cmd('mid',this.value)" oninput="document.getElementById('mV').innerText=this.value">
    
    <div style="display:flex;justify-content:space-between;font-size:11px;margin-top:10px"><span>TREBLE</span><span id="tV">0</span></div>
    <input type="range" id="tSl" min="-15" max="15" class="sl" onchange="cmd('treb',this.value)" oninput="document.getElementById('tV').innerText=this.value">
  </div>

  <div class="card">
    <div class="ct">DSP Parameters</div>
    <span style="font-size:11px">Bass Q-Factor</span>
    <select class="inp" id="bqSel" onchange="cmd('bq',this.value)">
      <option value="0">0.5</option><option value="1">1.0</option><option value="2">1.5</option><option value="3">2.0</option>
    </select>
    
    <span style="font-size:11px">Subwoofer LPF</span>
    <select class="inp" id="sfSel" onchange="cmd('subf',this.value)">
      <option value="0">FLAT</option><option value="1">55 Hz</option><option value="2">85 Hz</option><option value="3">120 Hz</option><option value="4">160 Hz</option>
    </select>
  </div>

  <div class="card">
    <div class="ct">Subwoofer Output</div>
    <div style="display:flex;justify-content:space-between;font-size:11px"><span>SUB VOL (0-100)</span><span id="sV">50</span></div>
    <input type="range" id="sSl" min="0" max="100" class="sl" onchange="cmd('sub',this.value)" oninput="document.getElementById('sV').innerText=this.value">
  </div>
</div>

<div id="pane-radio" class="pane">
  <div class="card">
    <div class="ct">Net Radio Presets</div>
    <select class="inp" id="rPre" onchange="rEditLoad()">
      <option value="0">Preset 1</option><option value="1">Preset 2</option>
      <option value="2">Preset 3</option><option value="3">Preset 4</option>
      <option value="4">Preset 5</option>
    </select>
    <input type="text" id="rName" class="inp" placeholder="Station Name">
    <input type="url" id="rUrl" class="inp" placeholder="Stream URL">
    <button class="btn btn-p" onclick="rSave()">💾 Save Radio Preset</button>
  </div>
  
  <div class="card">
    <div class="ct">FM Tuner Presets</div>
    <div style="display:flex;gap:10px;margin-bottom:10px">
      <input type="number" step="0.1" id="fm1" class="inp" placeholder="CH 1" style="margin:0">
      <input type="number" step="0.1" id="fm2" class="inp" placeholder="CH 2" style="margin:0">
      <input type="number" step="0.1" id="fm3" class="inp" placeholder="CH 3" style="margin:0">
    </div>
    <button class="btn btn-g" onclick="fmSave()">📻 Save FM Presets</button>
  </div>
</div>

<div id="pane-sys" class="pane">
  <div class="card">
    <div class="ct">WiFi Network Setup</div>
    <div style="font-size:12px;margin-bottom:15px;color:var(--ac2)">Current IP: <span id="sIP" style="font-weight:bold;color:var(--tx)">—</span></div>
    
    <button class="btn btn-g" id="btnScan" onclick="wScan()">🔍 Scan Available Networks</button>
    
    <input list="wList" id="wSSID" class="inp" placeholder="WiFi SSID (Select or Type)">
    <datalist id="wList"></datalist>
    
    <input type="password" id="wPWD" class="inp" placeholder="WiFi Password">
    <button class="btn btn-p" onclick="wConnect()">🔗 Save & Reboot</button>
  </div>

  <div class="card">
    <div class="ct">System Control</div>
    <button class="btn btn-g" onclick="sys('ntp')">🕐 Sync NTP Clock</button>
    <button class="btn btn-g" style="color:var(--rd)" onclick="if(confirm('Reboot DAP?'))sys('reboot')">⚡ Reboot System</button>
  </div>
</div>

</div>

<div id="toast"></div>

<script>
var D={}; var rN=['','','','','']; var rU=['','','','','']; var fP=[98.3, 104.1, 106.4];
function tab(n,b){
  document.querySelectorAll('.pane').forEach(p=>p.classList.remove('on'));
  document.querySelectorAll('.tb').forEach(x=>x.classList.remove('on'));
  document.getElementById('pane-'+n).classList.add('on'); b.classList.add('on');
}
function toast(m){var t=document.getElementById('toast');t.innerText=m;t.classList.add('on');setTimeout(()=>t.classList.remove('on'),2000);}

function cmd(p,v){
  if(p==='src' && v==='1') {
    if(!confirm('Switching to Bluetooth will disable the WiFi antenna and this Web UI until you switch to a different source. Continue?')) return;
  }
  fetch('/api/set?'+p+'='+encodeURIComponent(v)).then(()=>poll());
}
function sys(c){fetch('/api/sys?cmd='+c).then(()=>toast('Command Sent'));}

function poll(){
  fetch('/api/get').then(r=>r.json()).then(d=>{
    D=d; rN=d.rNames||rN; rU=d.rUrls||rU; fP=d.fmP||fP;
    for(var i=0;i<5;i++) document.getElementById('s'+i).classList.toggle('on',i===d.src);
    document.getElementById('npt').innerText=d.title||'SARVS HiFi';
    document.getElementById('nps').innerText=d.artist||'—';
    document.getElementById('bplay').innerText=d.play?'⏸':'▶';
    document.getElementById('sIP').innerText=d.ip||'Not Connected';
    
    if(!document.getElementById('volSl').matches(':active')){document.getElementById('volSl').value=d.vol; document.getElementById('volV').innerText=d.vol;}
    if(!document.getElementById('bSl').matches(':active')){document.getElementById('bSl').value=d.bass; document.getElementById('bV').innerText=d.bass;}
    if(!document.getElementById('mSl').matches(':active')){document.getElementById('mSl').value=d.mid; document.getElementById('mV').innerText=d.mid;}
    if(!document.getElementById('tSl').matches(':active')){document.getElementById('tSl').value=d.treb; document.getElementById('tV').innerText=d.treb;}
    if(!document.getElementById('sSl').matches(':active')){document.getElementById('sSl').value=d.sub; document.getElementById('sV').innerText=d.sub;}
    
    if(!document.getElementById('bqSel').matches(':focus')) document.getElementById('bqSel').value=d.bq;
    if(!document.getElementById('sfSel').matches(':focus')) document.getElementById('sfSel').value=d.sf;
    
    // Each FM input now has its own independent focus check so they don't overwrite each other!
    if(!document.getElementById('fm1').matches(':focus')) document.getElementById('fm1').value = fP[0];
    if(!document.getElementById('fm2').matches(':focus')) document.getElementById('fm2').value = fP[1];
    if(!document.getElementById('fm3').matches(':focus')) document.getElementById('fm3').value = fP[2];
    
  }).catch(()=>{});
}

function wScan(){
  var b=document.getElementById('btnScan'); b.innerText='Scanning...'; b.disabled=true;
  fetch('/api/wifiscan').then(r=>r.json()).then(d=>{
    var l=document.getElementById('wList'); l.innerHTML='';
    d.forEach(w=>{ l.innerHTML+='<option value="'+w.ssid+'">'; });
    b.innerText='🔍 Scan Available Networks'; b.disabled=false; toast('Scan complete');
  }).catch(()=>{b.innerText='🔍 Scan Available Networks'; b.disabled=false; toast('Error scanning');});
}

function rEditLoad(){
  var i=document.getElementById('rPre').value;
  document.getElementById('rName').value=rN[i]||''; document.getElementById('rUrl').value=rU[i]||'';
}
function rSave(){
  var i=document.getElementById('rPre').value, n=document.getElementById('rName').value.trim(), u=document.getElementById('rUrl').value.trim();
  if(!n||!u){toast('Enter name & URL');return;}
  fetch('/api/radio?idx='+i+'&name='+encodeURIComponent(n)+'&url='+encodeURIComponent(u)).then(()=>{rN[i]=n; rU[i]=u; toast('Saved Preset '+(parseInt(i)+1));});
}
function fmSave(){
  var f1=document.getElementById('fm1').value, f2=document.getElementById('fm2').value, f3=document.getElementById('fm3').value;
  fetch('/api/fm?f1='+f1+'&f2='+f2+'&f3='+f3).then(()=>toast('FM Presets Saved'));
}
function wConnect(){
  var s=document.getElementById('wSSID').value.trim(), p=document.getElementById('wPWD').value.trim();
  if(!s){toast('SSID required');return;}
  toast('Saving & Rebooting...');
  fetch('/api/wifi?ssid='+encodeURIComponent(s)+'&pwd='+encodeURIComponent(p)).then(()=>setTimeout(()=>location.reload(),4000));
}

setInterval(poll,1500); window.onload=()=>{poll(); setTimeout(rEditLoad,500);};
</script>
</body>
</html>
)HTMLEOF";