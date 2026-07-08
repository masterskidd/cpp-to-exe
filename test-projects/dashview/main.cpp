#include "webview/webview.h"
#include "system_info.h"
#include <sstream>
#include <thread>
#include <chrono>

static SysInfo g_info;

static std::string json_escape(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else o += c;
  }
  return o;
}

static std::string get_info_json() {
  std::ostringstream j;
  g_info = gather_sysinfo();
  j << "{";
  j << "\"cpuName\":\"" << json_escape(g_info.cpu_name) << "\",";
  j << "\"cpuArch\":\"" << json_escape(g_info.cpu_arch) << "\",";
  j << "\"cpuCores\":" << g_info.cpu_cores << ",";
  j << "\"cpuLogical\":" << g_info.cpu_logical << ",";
  j << "\"memTotal\":" << g_info.mem.ullTotalPhys << ",";
  j << "\"memAvail\":" << g_info.mem.ullAvailPhys << ",";
  j << "\"memPercent\":" << (int)g_info.mem.dwMemoryLoad << ",";

  j << "\"drives\":[";
  for (size_t i = 0; i < g_info.drives.size(); i++) {
    if (i) j << ",";
    j << "{\"path\":\"" << json_escape(g_info.drives[i])
      << "\",\"total\":" << g_info.drives_total[i]
      << ",\"free\":" << g_info.drives_free[i] << "}";
  }
  j << "],";

  j << "\"osName\":\"" << json_escape(g_info.os_name) << "\",";
  j << "\"osVersion\":\"" << json_escape(g_info.os_version) << "\",";
  j << "\"computerName\":\"" << json_escape(g_info.computer_name) << "\",";
  j << "\"userName\":\"" << json_escape(g_info.user_name) << "\"";
  j << "}";
  return j.str();
}

static std::string get_cpu_usage_json() {
  double pct = cpu_usage(g_info);
  std::ostringstream j;
  j << "{\"percent\":" << pct << "}";
  return j.str();
}

static std::string html() {
  return R"raw(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>DashView</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#0f0f1a;color:#e0e0e0;padding:20px;min-height:100vh}
h1{font-size:24px;margin-bottom:4px;background:linear-gradient(135deg,#667eea,#764ba2);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.sub{color:#888;font-size:13px;margin-bottom:20px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px;margin-bottom:20px}
.card{background:#1a1a2e;border-radius:12px;padding:20px;border:1px solid #2a2a4a}
.card h2{font-size:14px;color:#888;text-transform:uppercase;letter-spacing:1px;margin-bottom:12px}
.card .val{font-size:28px;font-weight:600}
.card .subval{font-size:13px;color:#888;margin-top:4px}
.chart-card{background:#1a1a2e;border-radius:12px;padding:20px;border:1px solid #2a2a4a;margin-bottom:20px}
.chart-card h2{font-size:14px;color:#888;text-transform:uppercase;letter-spacing:1px;margin-bottom:12px}
canvas{width:100%!important;height:200px!important;border-radius:8px}
.drive-item{margin-bottom:10px}
.drive-item:last-child{margin-bottom:0}
.drive-header{display:flex;justify-content:space-between;margin-bottom:4px;font-size:13px}
.drive-bar{height:8px;background:#2a2a4a;border-radius:4px;overflow:hidden}
.drive-fill{height:100%;border-radius:4px;transition:width .5s;background:linear-gradient(90deg,#667eea,#764ba2)}
.badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:11px;background:#2a2a4a;margin-left:8px}
</style>
</head>
<body>
<h1>DashView</h1>
<p class="sub" id="subtitle">Loading system information...</p>
<div class="grid" id="infoGrid"></div>
<div class="chart-card"><h2>CPU Usage</h2><canvas id="cpuChart"></canvas></div>
<script>
const MAX_POINTS=60;const cpuData=[];
const ctx=document.getElementById('cpuChart').getContext('2d');
function resizeCanvas(){const r=ctx.canvas.getBoundingClientRect();ctx.canvas.width=r.width*devicePixelRatio;ctx.canvas.height=r.height*devicePixelRatio;ctx.scale(devicePixelRatio,devicePixelRatio)}
resizeCanvas();window.addEventListener('resize',resizeCanvas);
function drawChart(){
const w=ctx.canvas.width/devicePixelRatio,h=ctx.canvas.height/devicePixelRatio;
ctx.clearRect(0,0,w,h);
if(cpuData.length<2)return;
const pad=10,chartW=w-pad*2,chartH=h-pad*2;
const max=Math.max(...cpuData,1),min=0;
ctx.beginPath();ctx.strokeStyle='#667eea';ctx.lineWidth=2;ctx.lineJoin='round';
for(let i=0;i<cpuData.length;i++){
const x=pad+(i/(MAX_POINTS-1))*chartW;
const y=pad+chartH-((cpuData[i]-min)/(max-min))*chartH;
i===0?ctx.moveTo(x,y):ctx.lineTo(x,y)
}
ctx.stroke();
ctx.beginPath();ctx.strokeStyle='#2a2a4a';ctx.lineWidth=1;ctx.setLineDash([4,4]);
for(let p=0;p<=100;p+=25){const y=pad+chartH-(p/100)*chartH;ctx.moveTo(pad,y);ctx.lineTo(pad+chartW,y)}
ctx.stroke();ctx.setLineDash([]);
}
function fmt(s){if(!s)return'0 B';const u=['B','KB','MB','GB','TB'];let i=0;let n=s;while(n>=1024&&i<4){n/=1024;i++}return n.toFixed(1)+' '+u[i]}
function pct(a,b){return a>0?((b/a)*100).toFixed(1):'0.0'}
function updateInfo(){
fetch('sysinfo').then(r=>r.json()).then(d=>{
document.getElementById('subtitle').textContent=d.computerName+' \\\\ '+d.userName;
let h='';
h+='<div class=card><h2>CPU</h2><div class=val>'+d.cpuName+'</div><div class=subval>'+d.cpuArch+' &middot; '+d.cpuCores+' cores / '+d.cpuLogical+' logical</div></div>';
h+='<div class=card><h2>Memory</h2><div class=val>'+fmt(d.memTotal)+'</div><div class=subval>'+fmt(d.memAvail)+' free &middot; '+d.memPercent+'% used</div></div>';
h+='<div class=card><h2>Operating System</h2><div class=val style=font-size:18px>'+d.osName+'</div><div class=subval>'+d.computerName+'</div></div>';
h+='<div class=card><h2>Drives</h2>';
for(const dr of d.drives){const up=pct(dr.total,dr.total-dr.free);h+='<div class=drive-item><div class=drive-header><span>'+dr.path+'</span><span>'+fmt(dr.total)+' &middot; '+up+'% used</span></div><div class=drive-bar><div class=drive-fill style=width:'+up+'%></div></div></div>'}
h+='</div>';
document.getElementById('infoGrid').innerHTML=h
}).catch(()=>{})
}
function updateCpu(){
fetch('cpu').then(r=>r.json()).then(d=>{
cpuData.push(d.percent);if(cpuData.length>MAX_POINTS)cpuData.shift();
drawChart()
}).catch(()=>{})
}
updateInfo();updateCpu();
setInterval(updateInfo,2000);
setInterval(updateCpu,1000);
function bind(name,fn){const o=name;window[o]=function(){const id=Date.now()+Math.random();window._cb=window._cb||{};window._cb[id]=fn;window.external.invoke(JSON.stringify({id:id,name:o}))}}
</script>
</body>
</html>
)raw";
}

struct BindContext {
  webview::webview* w;
};

static void on_sysinfo(const std::string& id, const std::string& req, void* arg) {
  auto ctx = (BindContext*)arg;
  std::string result = get_info_json();
  ctx->w->resolve(id, 0, result);
}

static void on_cpu(const std::string& id, const std::string& req, void* arg) {
  auto ctx = (BindContext*)arg;
  std::string result = get_cpu_usage_json();
  ctx->w->resolve(id, 0, result);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  g_info = gather_sysinfo();
  cpu_usage(g_info);

  webview::webview w(false, nullptr);
  w.set_title("DashView - System Dashboard");
  w.set_size(800, 650, WEBVIEW_HINT_NONE);

  BindContext ctx{&w};
  w.bind("sysinfo", on_sysinfo, &ctx);
  w.bind("cpu", on_cpu, &ctx);

  w.set_html(html());
  w.run();
  return 0;
}
