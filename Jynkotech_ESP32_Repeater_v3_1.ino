/*
 Jynkotech ESP32 Wi-Fi Repeater v3.1
 ESP32 Dev Module (classic ESP32), Arduino-ESP32 3.3.x
 Bundled libraries only: WiFi, WebServer, Preferences.
 Login default: admin / ChangeMe123! (change immediately)
 Default setup AP: Jynkotech_Repeater / ChangeMe123!
 Admin: http://192.168.50.1
 NAT routed IPv4, not a transparent layer-2 repeater.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

WebServer server(80);
Preferences prefs;
const char* NS = "jynko-wifi";
const char* DEFAULT_AP = "Jynkotech_Repeater";
const char* DEFAULT_PASS = "ChangeMe123!";
const IPAddress AP_MASK(255,255,255,0);

String upstreamSSID, upstreamPass, apSSID, apPass, adminPass, dnsText;
// Eight independent saved upstream networks. Lower priority number = preferred.
constexpr uint8_t MAX_NETWORKS = 8;
struct SavedNetwork { String ssid, pass; uint8_t priority; };
SavedNetwork networks[MAX_NETWORKS];
uint8_t networkCount = 0;
int8_t currentNetwork = -1;
int8_t pendingNetwork = -1;
uint8_t attemptsThisRound = 0;
bool tried[MAX_NETWORKS] = {false};
unsigned long connectStarted = 0, nextConnectAt = 0, connectedAt = 0;
constexpr unsigned long CONNECT_TIMEOUT_MS = 16000;
constexpr unsigned long SCAN_RETRY_MS = 45000;
constexpr unsigned long LINK_STABLE_MS = 10000;
constexpr unsigned long FAILBACK_DELAY_MS = 120000;
unsigned long nextPreferredCheck = 0;

uint8_t apChannel = 1, apClients = 4, txLevel = 2;
bool hiddenAP = false, powerSave = false, natOk = false;
// NAT is installed after the upstream receives its DHCP lease.
// Rebuild the NAT state when switching the upstream network.
bool lastRouterOnline = false;
bool subnetConflict = false;
String diagnosis = "Not tested yet";
String dnsDiagnosis = "Not tested yet";
String tcpDiagnosis = "Not tested yet";
unsigned long lastDiagnosticAt = 0;

IPAddress apIP(192,168,50,1), dnsIP(1,1,1,1);
unsigned long nextRestart = 0, lastPrint = 0;

bool validText(const String &v, size_t low, size_t high) { return v.length() >= low && v.length() <= high; }
String esc(const String &s) {
  String r; r.reserve(s.length() + 20);
  for (unsigned int i=0;i<s.length();i++) {
    switch(s[i]) {case '&':r+="&amp;";break;case '<':r+="&lt;";break;
      case '>':r+="&gt;";break;case '"':r+="&quot;";break;
      case '\'':r+="&#39;";break;default:r+=s[i];}
  }
  return r;
}
String attr(const String &v) { return esc(v); }
String check(bool v) {return v ? " checked" : "";}
String selectOpt(int v, int current, const String &label) {
  return "<option value='"+String(v)+"'"+(v==current?" selected":"")+">"+label+"</option>";
}
bool auth() {
  if(server.authenticate("admin",adminPass.c_str()))return true;
  server.requestAuthentication(BASIC_AUTH,"Jynkotech Repeater"); return false;
}
void sendHtml(int code, const String &s) {
  server.sendHeader("Cache-Control","no-store, max-age=0");
  server.sendHeader("X-Content-Type-Options","nosniff");
  server.sendHeader("X-Frame-Options","DENY");
  server.send(code,"text/html; charset=UTF-8",s);
}
String beginPage(const String &title, const String &active) {
  String h=F("<!doctype html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
   "<title>Jynkotech Repeater</title><style>"
   "*{box-sizing:border-box}body{margin:0;background:#f3f5f8;color:#20262e;font:15px system-ui,Arial}"
   "header{background:#24282f;color:white;padding:19px 16px}header .wrap{max-width:980px;margin:auto}"
   "header strong{font-size:22px;letter-spacing:.4px}header b{color:#ff902e}main{max-width:980px;margin:18px auto;padding:0 14px}"
   "nav{display:flex;flex-wrap:wrap;gap:8px;margin:16px 0}nav a{color:#313943;text-decoration:none;padding:10px 13px;background:white;border-radius:10px}"
   "nav a.active{background:#f47c20;color:white}.card{background:#fff;border-radius:13px;padding:21px;margin:14px 0;box-shadow:0 3px 14px #1726380c}"
   "h1{font-size:22px;margin:0 0 12px}h2{font-size:17px;margin:4px 0 10px}label{display:block;margin:15px 0 6px;font-weight:650}"
   "input,select{display:block;width:100%;border:1px solid #cbd3dd;border-radius:9px;padding:11px;font-size:15px;background:#fff}"
   "input[type=checkbox]{display:inline;width:auto;margin-right:9px}button,.btn{display:inline-block;background:#ee791b;color:white;border:0;"
   "border-radius:9px;padding:11px 16px;font-weight:700;cursor:pointer;text-decoration:none;margin:10px 7px 0 0;font-size:14px}"
   ".secondary{background:#455566}.danger{background:#bd3636}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}"
   ".stat{background:#f5f7fa;padding:14px;border-radius:9px;overflow-wrap:anywhere}.stat small{display:block;color:#687582;margin-bottom:5px}"
   ".note{color:#64717e}.warning{padding:12px 14px;background:#fff1d9;border-radius:9px}.error{color:#b92b2b}"
   "footer{color:#788390;margin:24px 0;font-size:13px;text-align:center;line-height:1.9}footer a{color:#b85711;text-decoration:none;font-weight:650}form.inline{display:inline}.right{float:right}"
   "@media(max-width:580px){.grid{grid-template-columns:1fr}.card{padding:16px}nav a{flex:1;text-align:center}}"
   "</style></head><body><header><div class='wrap'><strong><b>JYNKO</b>TECH</strong><div>ESP32 Wi-Fi Repeater &bull; v3.1</div><div class='brand-contact'>Industrial Automation &bull; Embedded Systems &bull; IoT</div></div></header><main><nav>");
  const char* paths[]={"/","/internet","/repeater","/network","/diagnostics","/system"};
  const char* labels[]={"Status","Saved Networks","Repeater Wi-Fi","Network","Diagnostics","System"};
  for(int i=0;i<6;i++)h+="<a href='"+String(paths[i])+"'"+(active==paths[i]?" class='active'":"")+">"+labels[i]+"</a>";
  h+="</nav><section class='card'><h1>"+esc(title)+"</h1>";
  return h;
}
String finishPage(){return F("</section><footer><strong>JYNKOTECH</strong> &bull; Industrial Automation &amp; Embedded Systems<br><a href='https://www.jynkotech.com/' target='_blank' rel='noopener noreferrer'>www.jynkotech.com</a> &bull; <a href='mailto:jynkotech@gmail.com'>jynkotech@gmail.com</a><br>Local administration: 192.168.50.1 &bull; 2.4 GHz &bull; IPv4 NAT<br>&copy; 2026 Jynkotech &bull; MIT License</footer></main></body></html>");}
String errorPage(const String &message){return beginPage("Invalid setting","")+"<p class='error'>"+esc(message)+"</p><a class='btn secondary' href='javascript:history.back()'>Go back</a>"+finishPage();}
void refreshLater(){nextRestart=millis()+2200;}
void statusPage() {
  if(!auth())return;
  bool c=WiFi.status()==WL_CONNECTED;
  String h=beginPage("Device status","/");
  h+="<div class='grid'>";
  h+="<div class='stat'><small>Upstream router</small><strong>"+(c?String("Connected"):String("Disconnected"))+"</strong></div>";
  h+="<div class='stat'><small>NAT routing</small><strong>"+(natOk?String("Enabled"):String("Failed"))+"</strong></div>";
  h+="<div class='stat'><small>Active router</small><strong>"+esc(c?WiFi.SSID():String("None"))+"</strong></div>";
  h+="<div class='stat'><small>Saved routers</small><strong>"+String(networkCount)+" / "+String(MAX_NETWORKS)+"</strong></div>";
  h+="<div class='stat'><small>Router IP</small><strong>"+(c?WiFi.localIP().toString():String("—"))+"</strong></div>";
  h+="<div class='stat'><small>Upstream gateway</small><strong>"+(c?WiFi.gatewayIP().toString():String("—"))+"</strong></div>";
  h+="<div class='stat'><small>Subnet overlap</small><strong>"+(subnetConflict?String("CONFLICT: change repeater IP"):String("No overlap detected"))+"</strong></div>";
  h+="<div class='stat'><small>Wi-Fi signal</small><strong>"+(c?String(WiFi.RSSI())+" dBm ("+(WiFi.RSSI()>=-65?"good":WiFi.RSSI()>=-75?"fair":"weak")+")":String("—"))+"</strong></div>";
  h+="<div class='stat'><small>Router channel</small><strong>"+(c?String(WiFi.channel()):String("—"))+"</strong></div>";
  h+="<div class='stat'><small>Repeater Wi-Fi</small><strong>"+esc(apSSID)+"</strong></div>";
  h+="<div class='stat'><small>Connected devices</small><strong>"+String(WiFi.softAPgetStationNum())+" / "+String(apClients)+"</strong></div>";
  h+="<div class='stat'><small>Gateway</small><strong>"+WiFi.softAPIP().toString()+"</strong></div>";
  h+="<div class='stat'><small>Configured DNS</small><strong>"+dnsIP.toString()+"</strong></div>";
  h+="<div class='stat'><small>Transmit power profile</small><strong>"+(txLevel==2?String("Maximum"):txLevel==1?String("Balanced"):String("Low"))+"</strong></div>";
  h+="<div class='stat'><small>Uptime</small><strong>"+String(millis()/1000)+" seconds</strong></div></div>";
  h+="<p class='note'>NAT is activated after the router connection and refreshed during failover. Router connection and NAT enabled still do not prove internet access on a client.</p>";
  h+="<a class='btn' href='/internet'>Manage saved routers</a><a class='btn secondary' href='/diagnostics'>Run internet diagnostics</a><a class='btn secondary' href='/'>Refresh</a>"+finishPage();
  sendHtml(200,h);
}
// HTML registration form deliberately never discloses stored Wi-Fi passwords.
void internetPage(){
  if(!auth())return;
  String h=beginPage("Saved internet networks","/internet");
  h+=F("<p class='note'>Save up to 8 different Wi-Fi routers. The ESP32 tries them in priority order (1 first). When one fails, it tries the next automatically. Existing connected clients remain on your repeater SSID while the upstream changes.</p>");
  h+="<p><strong>Currently connected:</strong> "+esc(WiFi.status()==WL_CONNECTED?WiFi.SSID():String("None"))+" &nbsp; <strong>Saved:</strong> "+String(networkCount)+"</p>";
  h+=F("<div class='grid'>");
  for(uint8_t i=0;i<networkCount;i++){
    h+="<div class='stat'><small>Priority "+String(networks[i].priority)+" / "+(WiFi.status()==WL_CONNECTED && currentNetwork==i?String("CONNECTED"):String("Saved"))+"</small>";
    h+="<strong>"+esc(networks[i].ssid)+"</strong><div>";
    h+="<form method='POST' class='inline' action='/network-up'><input type='hidden' name='index' value='"+String(i)+"'><button class='secondary' "+String(i==0?"disabled":"")+">Move up</button></form>";
    h+="<form method='POST' class='inline' action='/network-down'><input type='hidden' name='index' value='"+String(i)+"'><button class='secondary' "+String(i==networkCount-1?"disabled":"")+">Move down</button></form>";
    h+="<form method='POST' class='inline' action='/network-delete' onsubmit=\"return confirm('Forget this Wi-Fi network?')\"><input type='hidden' name='index' value='"+String(i)+"'><button class='danger'>Forget</button></form>";
    h+="</div></div>";
  }
  h+="</div>";
  if(networkCount<MAX_NETWORKS){
    h+=F("<h2>Add another Wi-Fi</h2><form method='POST' action='/network-add'><label>Nearby Wi-Fi</label><select name='selected'><option value=''>Select or type SSID below</option>");
    // Manual scanning on page load, so routine failover never needs scan.
    int n=WiFi.scanNetworks(false,true);
    if(n>0){for(int i=0;i<n && i<40;i++){
      String ssid=WiFi.SSID(i);
      if(ssid.isEmpty())continue;
      h+="<option value='"+attr(ssid)+"'>"+esc(ssid)+" ("+String(WiFi.RSSI(i))+" dBm)</option>";
    }}
    WiFi.scanDelete();
    h+=F("</select><label>SSID (manual / hidden Wi-Fi)</label><input name='ssid' maxlength='32' placeholder='Optional if selected above'>"
         "<label>Wi-Fi password</label><input type='password' name='password' maxlength='63' autocomplete='new-password' placeholder='8–63 characters, or blank for open Wi-Fi'>"
         "<button>Save Wi-Fi and connect</button></form>");
  }
  if(networkCount){
    h+=F("<h2>Change password for a saved Wi-Fi</h2><form method='POST' action='/network-password'><label>Saved Wi-Fi</label><select name='index'>");
    for(uint8_t i=0;i<networkCount;i++)h+="<option value='"+String(i)+"'>"+esc(networks[i].ssid)+"</option>";
    h+=F("</select><label>New password</label><input type='password' name='password' maxlength='63' autocomplete='new-password' placeholder='Blank sets open Wi-Fi'>"
         "<button>Update password</button></form>");
  }
  h+=F("<p class='warning'>Failover is automatic after a Wi-Fi disconnect or timeout. The repeater cannot combine multiple internet connections. Changing upstream networks may temporarily interrupt browsing and NAT sessions.</p>");
  h+=finishPage();sendHtml(200,h);
}

// Lightweight diagnostics through the STA interface. These probes do not establish
// whether the PC's forwarded packets work; the client-side tests remain definitive.
void diagnosticsPage(){
  if(!auth())return;
  String h=beginPage("Internet diagnostics","/diagnostics");
  bool linked=WiFi.status()==WL_CONNECTED;
  h+="<div class='grid'>";
  h+="<div class='stat'><small>Upstream Wi-Fi</small><strong>"+String(linked?"Connected":"Disconnected")+"</strong></div>";
  h+="<div class='stat'><small>NAT activation</small><strong>"+String(natOk?"Enabled":"Not enabled")+"</strong></div>";
  h+="<div class='stat'><small>Upstream gateway</small><strong>"+(linked?WiFi.gatewayIP().toString():String("—"))+"</strong></div>";
  h+="<div class='stat'><small>Upstream subnet mask</small><strong>"+(linked?WiFi.subnetMask().toString():String("—"))+"</strong></div>";
  h+="<div class='stat'><small>ESP32 DNS lookup</small><strong>"+esc(dnsDiagnosis)+"</strong></div>";
  h+="<div class='stat'><small>ESP32 TCP internet</small><strong>"+esc(tcpDiagnosis)+"</strong></div>";
  h+="</div><p><strong>Result:</strong> "+esc(diagnosis)+"</p>";
  if(subnetConflict)h+="<p class='warning'><strong>Address conflict.</strong> Your main router and repeater share the same subnet. Open Network, and choose a different repeater gateway such as 192.168.60.1, then reconnect your PC.</p>";
  h+="<form method='POST' action='/diagnostics-run'><button>Run DNS and internet tests</button></form>";
  h+="<p class='note'>DNS tests check the ESP32 itself. Even a passing result does not prove the PC is routed through NAT. On the PC, run: <code>ipconfig /all</code>, <code>ping 1.1.1.1</code>, and <code>nslookup google.com</code>.</p>";
  h+="<p class='note'>Expected PC IPv4: repeater subnet, gateway "+apIP.toString()+", DNS "+dnsIP.toString()+". A numeric-IP ping can fail when ICMP is blocked; test opening a website too.</p>";
  h+=finishPage();sendHtml(200,h);
}
void runDiagnostics(){
  if(!auth())return;
  dnsDiagnosis="Not tested";tcpDiagnosis="Not tested";
  if(WiFi.status()!=WL_CONNECTED){
    diagnosis="Upstream Wi-Fi is disconnected.";
  }else if(subnetConflict){
    diagnosis="Subnet overlap detected. Change repeater gateway subnet first.";
  }else{
    IPAddress resolved;
    int found=WiFi.hostByName("example.com",resolved);
    dnsDiagnosis=found==1 ? "OK: "+resolved.toString() : "FAILED";
    WiFiClient c;
    c.setTimeout(1800);
    // This tests outbound connectivity from the ESP32 itself, not client NAT.
    bool worked=c.connect(IPAddress(1,1,1,1),80,1800);
    tcpDiagnosis=worked?"Connected to 1.1.1.1:80":"Failed to connect to 1.1.1.1:80";
    c.stop();
    if(worked&&found==1)diagnosis="ESP32 has outbound TCP and DNS. If PC still has no internet, inspect client DHCP / NAT forwarding.";
    else if(worked)diagnosis="ESP32 has outbound TCP but DNS lookup failed. Check upstream and DNS.";
    else diagnosis="ESP32 outbound TCP test failed. Inspect upstream internet/router; a network firewall could also block this probe.";
  }
  lastDiagnosticAt=millis();
  server.sendHeader("Location","/diagnostics",true);server.send(303,"text/plain","");
}
// Uses the actual upstream subnet mask, not an assumed /24.
bool hasSubnetConflict(){
  if(WiFi.status()!=WL_CONNECTED)return false;
  IPAddress up=WiFi.localIP(), mask=WiFi.subnetMask();
  for(int i=0;i<4;i++)if((apIP[i]&mask[i])!=(up[i]&mask[i]))return false;
  return true;
}
void refreshNat(){
  if(natOk){
    if(!WiFi.AP.enableNAPT(false))Serial.println("WARN: NAT disable failed");
    natOk=false;
  }
  subnetConflict=hasSubnetConflict();
  if(WiFi.status()!=WL_CONNECTED)return;
  if(subnetConflict){
    Serial.println("ERROR: upstream and repeater subnets overlap. Change AP gateway in Network settings.");
    diagnosis="Address conflict: change repeater subnet";
    return;
  }
  natOk=WiFi.AP.enableNAPT(true);
  Serial.printf("NAT after STA DHCP: %s | Upstream GW: %s | AP GW: %s\n",
                natOk?"ENABLED":"FAILED",WiFi.gatewayIP().toString().c_str(),apIP.toString().c_str());
  if(!natOk)diagnosis="NAT API failed. Confirm ESP32 board core supports NAPT.";
}
void repeaterPage(){
  if(!auth())return;
  String h=beginPage("Repeater Wi-Fi settings","/repeater");
  h+="<form method='POST' action='/save-repeater'><label>Repeater network name (SSID)</label><input required name='ssid' maxlength='32' value='"+attr(apSSID)+"'>";
  h+=F("<label>Repeater password (WPA2)</label><input type='password' name='password' minlength='8' maxlength='63' autocomplete='new-password' placeholder='Leave blank to keep current password'>"
   "<label><input type='checkbox' name='hidden' value='1'");
  h+=check(hiddenAP)+F(">Hide repeater network name</label><label>Preferred AP channel (when not connected upstream)</label><select name='channel'>");
  for(int i=1;i<=11;i++)h+=selectOpt(i,apChannel,"Channel "+String(i));
  h+=F("</select><p class='note'>When connected upstream, the single ESP32 radio must operate on the router's channel, overriding this preference.</p>"
   "<label>Maximum connected devices</label><select name='clients'>");
  for(int i=1;i<=8;i++)h+=selectOpt(i,apClients,String(i)+" devices");
  h+=F("</select><button>Save repeater and restart</button></form>");
  h+=finishPage();sendHtml(200,h);
}
void networkPage(){
  if(!auth())return;
  String h=beginPage("Network and reception","/network");
  h+=F("<form method='POST' action='/save-network'><label>Repeater gateway IP</label>");
  h+="<input name='ip' required value='"+apIP.toString()+"' placeholder='192.168.50.1'>";
  h+=F("<p class='note'>Gateway must be a private IPv4 host address in a /24 subnet. Avoid overlap with your upstream router subnet. Changing it changes the portal URL.</p>"
   "<label>DNS advertised to connected devices</label>");
  h+="<input name='dns' required value='"+dnsIP.toString()+"'>";
  h+=F("<label>Transmit power</label><select name='power'>");
  h+=selectOpt(2,txLevel,"Maximum (19.5 dBm request)");
  h+=selectOpt(1,txLevel,"Balanced (15 dBm)");
  h+=selectOpt(0,txLevel,"Low (8.5 dBm)");
  h+=F("</select><label><input type='checkbox' name='sleep' value='1'");
  h+=check(powerSave)+F(">Enable modem power saving (may reduce responsiveness)</label>"
   "<p class='warning'><strong>Far signal reception:</strong> Maximum TX power helps ESP32 transmissions; it does not raise receiver sensitivity. Disabling modem sleep and placing the antenna away from metal can improve stability. Standard routers do not support ESP32 proprietary LR mode.</p>"
   "<button>Save network and restart</button></form>");
  h+=finishPage();sendHtml(200,h);
}
void systemPage(){
  if(!auth())return;
  String h=beginPage("System settings","/system");
  h+=F("<h2>Administrator password</h2><p class='note'>Username stays <strong>admin</strong>. Set a separate password from the repeater Wi-Fi password.</p>"
   "<form method='POST' action='/save-admin'><label>New admin password</label>"
   "<input type='password' name='newpass' minlength='8' maxlength='63' required autocomplete='new-password'>"
   "<label>Confirm password</label><input type='password' name='confirm' minlength='8' maxlength='63' required>"
   "<button>Update admin password</button></form></section><section class='card'><h2>Restart device</h2>"
   "<p class='note'>Reboot the ESP32 repeater without erasing settings.</p>"
   "<form method='POST' action='/restart' onsubmit=\"return confirm('Restart ESP32 repeater?')\"><button class='secondary'>Restart repeater</button></form>"
   "</section><section class='card'><h2>Factory reset</h2>"
   "<p class='warning'>Erases saved router credentials, repeater network and admin password. The initial setup network will return.</p>"
   "<form method='POST' action='/factory-reset' onsubmit=\"return confirm('Erase ALL repeater settings?')\">"
   "<label>Type RESET to confirm</label><input name='confirm' required placeholder='RESET' autocomplete='off'>"
   "<button class='danger'>Factory reset ESP32</button></form>"
   "<p class='note'>These buttons do not restart or reset your original internet router.</p>");
  h+=finishPage();sendHtml(200,h);
}
bool openPrefs(){return prefs.begin(NS,false);}
void resultPage(const String &heading,const String &msg,bool reboot){
  sendHtml(200,beginPage(heading,"")+"<p>"+esc(msg)+"</p>"+(reboot?"<p>Reconnect to the updated Wi-Fi and open the gateway address in your browser.</p>":"")+finishPage());
  if(reboot)refreshLater();
}
bool validRouterCredentials(const String &ssid, const String &pass){
  // WPA2-PSK accepts 8–63 ASCII characters; an empty password is open Wi-Fi.
  return validText(ssid,1,32) && (pass.isEmpty() || validText(pass,8,63));
}
bool persistNetworkList(){
  if(!openPrefs())return false;
  // Temporary count=0 prevents using a partially-written list after a power cut.
  bool ok=prefs.putUChar("netcount",0)>0;
  for(uint8_t i=0;i<MAX_NETWORKS;i++){
    String a="n"+String(i)+"s",b="n"+String(i)+"p";
    if(i<networkCount){
      ok &=prefs.putString(a.c_str(),networks[i].ssid)==networks[i].ssid.length();
      ok &=prefs.putString(b.c_str(),networks[i].pass)==networks[i].pass.length();
    }else{ prefs.remove(a.c_str());prefs.remove(b.c_str()); }
  }
  if(ok)ok=prefs.putUChar("netcount",networkCount)>0;
  prefs.end();return ok;
}
// When settings change, restart to avoid stale indices or half-open connections.
void networkAdd(){
  if(!auth())return;
  if(networkCount>=MAX_NETWORKS){sendHtml(400,errorPage("Eight Wi-Fi networks already saved."));return;}
  String ssid=server.arg("ssid");if(ssid.isEmpty())ssid=server.arg("selected");
  String pass=server.arg("password");
  if(!validRouterCredentials(ssid,pass)){sendHtml(400,errorPage("SSID must be 1–32 bytes. Password must be blank (open) or 8–63 characters."));return;}
  for(uint8_t i=0;i<networkCount;i++)if(networks[i].ssid==ssid){
    sendHtml(400,errorPage("Wi-Fi already saved. Use Change password instead."));return;
  }
  networks[networkCount]={ssid,pass,(uint8_t)(networkCount+1)};
  ++networkCount;
  if(!persistNetworkList()){sendHtml(500,errorPage("Could not save networks to flash."));return;}
  resultPage("Wi-Fi saved","New router added. Restarting to connect automatically.",true);
}
int selectedIndex(){
  String x=server.arg("index");
  if(x.isEmpty())return -1;
  for(char c:x)if(c<'0'||c>'9')return -1;
  int i=x.toInt();return i>=0 && i<networkCount?i:-1;
}
void networkDelete(){
  if(!auth())return;int i=selectedIndex();
  if(i<0){sendHtml(400,errorPage("Invalid network."));return;}
  for(uint8_t j=i;j+1<networkCount;j++)networks[j]=networks[j+1];
  --networkCount;
  for(uint8_t j=0;j<networkCount;j++)networks[j].priority=j+1;
  if(!persistNetworkList()){sendHtml(500,errorPage("Could not update saved Wi-Fi list."));return;}
  resultPage("Network forgotten","Removed network; ESP32 will restart and select another saved network.",true);
}
void networkMove(bool up){
  if(!auth())return;int i=selectedIndex();
  int other=up?i-1:i+1;
  if(i<0 || other<0 || other>=networkCount){sendHtml(400,errorPage("Cannot move this network."));return;}
  SavedNetwork tmp=networks[i];networks[i]=networks[other];networks[other]=tmp;
  for(uint8_t j=0;j<networkCount;j++)networks[j].priority=j+1;
  if(!persistNetworkList()){sendHtml(500,errorPage("Could not reorder networks."));return;}
  resultPage("Priority changed","Saved Wi-Fi priority updated. Restarting.",true);
}
void networkPassword(){
  if(!auth())return;int i=selectedIndex();String pass=server.arg("password");
  if(i<0 || !(pass.isEmpty()||validText(pass,8,63))){sendHtml(400,errorPage("Invalid Wi-Fi or password. Use blank for open Wi-Fi or 8–63 characters."));return;}
  networks[i].pass=pass;
  if(!persistNetworkList()){sendHtml(500,errorPage("Could not save changed password."));return;}
  resultPage("Password changed","Saved password updated. Restarting.",true);
}
void saveRepeater(){
  if(!auth())return;
  String s=server.arg("ssid"),p=server.arg("password");
  int ch=server.arg("channel").toInt(),clients=server.arg("clients").toInt();
  if(!validText(s,1,32)||(!p.isEmpty()&&!validText(p,8,63))||ch<1||ch>11||clients<1||clients>8){sendHtml(400,errorPage("Check SSID, password (8–63), channel (1–11), and clients (1–8)."));return;}
  if(p.isEmpty())p=apPass;
  if(!openPrefs()){sendHtml(500,errorPage("Cannot open flash storage."));return;}
  bool ok=prefs.putString("apssid",s)==s.length();
  ok &=prefs.putString("appass",p)==p.length();
  ok &=prefs.putUChar("channel",ch)>0;
  ok &=prefs.putUChar("clients",clients)>0;
  ok &=prefs.putBool("hidden",server.hasArg("hidden"))>0;
  prefs.end();
  if(!ok){sendHtml(500,errorPage("Could not save all repeater settings."));return;}
  resultPage("Saved","Repeater Wi-Fi settings saved. Restarting.",true);
}
void saveNetwork(){
  if(!auth())return;
  String a=server.arg("ip"),d=server.arg("dns");IPAddress ip,dns;
  int power=server.arg("power").toInt();
  if(!ip.fromString(a)||!dns.fromString(d)||power<0||power>2||ip[0]==0||ip[0]==127||ip[0]>=224||ip[3]<1||ip[3]>253 || dns==(uint32_t)0){
    sendHtml(400,errorPage("Enter valid gateway and DNS IPv4 addresses and transmit power."));return;
  }
  // Reserve .1-.19 for infrastructure; DHCP starts at .20 within this /24.
  if(ip[3]>=20){sendHtml(400,errorPage("For this version, gateway IP must end with .1 through .19; DHCP clients begin at .20."));return;}
  if(!openPrefs()){sendHtml(500,errorPage("Cannot open flash storage."));return;}
  bool ok=prefs.putString("apip",a)==a.length();
  ok &=prefs.putString("dns",d)==d.length();
  ok &=prefs.putUChar("power",power)>0;
  ok &=prefs.putBool("sleep",server.hasArg("sleep"))>0;
  prefs.end();
  if(!ok){sendHtml(500,errorPage("Could not save all network settings."));return;}
  resultPage("Saved","Network settings saved. Restarting. Use your new gateway address to open the portal.",true);
}
void saveAdmin(){
  if(!auth())return;
  String p=server.arg("newpass");
  if(!validText(p,8,63)||p!=server.arg("confirm")){sendHtml(400,errorPage("Admin passwords must match and contain 8–63 characters."));return;}
  if(!openPrefs()){sendHtml(500,errorPage("Cannot open flash storage."));return;}
  bool ok=prefs.putString("adminpass",p)==p.length();prefs.end();
  if(!ok){sendHtml(500,errorPage("Failed to save admin password."));return;}
  resultPage("Password saved","Admin password updated. Restarting device to clear previous login session.",true);
}
void doRestart(){if(!auth())return;resultPage("Restart","Restarting ESP32 repeater.",true);}
void doFactoryReset(){
  if(!auth())return;
  if(server.arg("confirm")!="RESET"){sendHtml(400,errorPage("Type RESET exactly to confirm."));return;}
  if(!openPrefs()){sendHtml(500,errorPage("Flash storage unavailable."));return;}
  bool ok=prefs.clear();prefs.end();
  if(!ok){sendHtml(500,errorPage("Factory reset failed."));return;}
  resultPage("Factory reset","All repeater settings erased. Restarting with factory defaults.",true);
}
void loadSettings(){
  upstreamSSID="";upstreamPass="";apSSID=DEFAULT_AP;apPass=DEFAULT_PASS;adminPass=DEFAULT_PASS;
  if(prefs.begin(NS,true)){
    upstreamSSID=prefs.getString("router","");upstreamPass=prefs.getString("rpass","");
    networkCount=prefs.getUChar("netcount",0);
    if(networkCount>MAX_NETWORKS)networkCount=0;
    for(uint8_t i=0;i<networkCount;i++){
      networks[i].ssid=prefs.getString(("n"+String(i)+"s").c_str(),"");
      networks[i].pass=prefs.getString(("n"+String(i)+"p").c_str(),"");
      networks[i].priority=i+1;
    }
    apSSID=prefs.getString("apssid",DEFAULT_AP);apPass=prefs.getString("appass",DEFAULT_PASS);
    adminPass=prefs.getString("adminpass",DEFAULT_PASS);
    apChannel=prefs.getUChar("channel",1);apClients=prefs.getUChar("clients",4);
    hiddenAP=prefs.getBool("hidden",false);powerSave=prefs.getBool("sleep",false);
    txLevel=prefs.getUChar("power",2);
    String a=prefs.getString("apip","192.168.50.1"),d=prefs.getString("dns","1.1.1.1");
    IPAddress parsed;if(parsed.fromString(a))apIP=parsed;
    if(parsed.fromString(d))dnsIP=parsed;
    prefs.end();
  }
  if(!validText(apSSID,1,32))apSSID=DEFAULT_AP;
  if(!validText(apPass,8,63))apPass=DEFAULT_PASS;
  if(!validText(adminPass,8,63))adminPass=DEFAULT_PASS;
  if(apChannel<1||apChannel>11)apChannel=1;
  if(apClients<1||apClients>8)apClients=4;
  if(txLevel>2)txLevel=2;
  // On first boot of v3, import v2's single saved router.
  if(networkCount==0 && validRouterCredentials(upstreamSSID,upstreamPass)){
    networks[0]={upstreamSSID,upstreamPass,1};networkCount=1;
    if(persistNetworkList()){
      Serial.println("Migrated old router to saved Wi-Fi list");
      if(openPrefs()){
        prefs.remove("router");prefs.remove("rpass");prefs.end();
      }
    }
  }
  // Reject invalid or incomplete flash entries.
  uint8_t valid=0;
  for(uint8_t i=0;i<networkCount;i++){
    if(validRouterCredentials(networks[i].ssid,networks[i].pass)){
      networks[valid]=networks[i];networks[valid].priority=valid+1;valid++;
    }
  }
  networkCount=valid;
}
void resetCycle(){
  for(uint8_t i=0;i<MAX_NETWORKS;i++)tried[i]=false;
  attemptsThisRound=0;
}
void startCandidate(uint8_t i){
  Serial.printf("Connecting to saved Wi-Fi #%u: %s\n",i+1,networks[i].ssid.c_str());
  WiFi.disconnect(false,false); // Keep STA radio enabled and keep flash settings.
  WiFi.begin(networks[i].ssid.c_str(),networks[i].pass.c_str());
  pendingNetwork=i;
  connectStarted=millis();
  tried[i]=true;attemptsThisRound++;
}
void chooseNext(){
  if(networkCount==0)return;
  for(uint8_t i=0;i<networkCount;i++){
    if(!tried[i]){startCandidate(i);return;}
  }
  // Failed all saved networks: retry entire cycle periodically.
  resetCycle(); pendingNetwork=-1;
  nextConnectAt=millis()+SCAN_RETRY_MS;
  Serial.println("All saved routers failed. Waiting before retrying list.");
}
void processUpstream(){
  unsigned long now=millis();
  bool online=WiFi.status()==WL_CONNECTED;
  if(online){
    if(pendingNetwork>=0){
      currentNetwork=pendingNetwork;pendingNetwork=-1;
      connectedAt=now;
      resetCycle();
      nextPreferredCheck=now+FAILBACK_DELAY_MS;
      refreshNat();
      Serial.printf("Connected to: %s | IP: %s | RSSI: %d dBm\n",WiFi.SSID().c_str(),WiFi.localIP().toString().c_str(),WiFi.RSSI());
    }
    // Avoid interrupting good links with scans. Preferred-network failback
    // happens after a disconnect; priority applies at boot and each retry cycle.
    return;
  }
  if(currentNetwork>=0){
    Serial.println("Wi-Fi lost; switching to another saved network");
    currentNetwork=-1;
    refreshNat();
    pendingNetwork=-1;
    resetCycle();
    nextConnectAt=now;
  }
  if(pendingNetwork>=0){
    if((uint32_t)(now-connectStarted)<CONNECT_TIMEOUT_MS)return;
    Serial.printf("Wi-Fi timeout: %s\n",networks[pendingNetwork].ssid.c_str());
    pendingNetwork=-1;
    nextConnectAt=now;
  }
  if((int32_t)(now-nextConnectAt)>=0)chooseNext();
}
void setup(){
  Serial.begin(115200);delay(300);
  loadSettings();Serial.println("\nJynkotech Wi-Fi Repeater v3.1");
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(powerSave);
  WiFi.setAutoReconnect(false); // Failover state machine selects router explicitly.
  // Requested maximum may be limited by ESP32 silicon, calibration, country regulations.
  wifi_power_t power=txLevel==2?WIFI_POWER_19_5dBm:(txLevel==1?WIFI_POWER_15dBm:WIFI_POWER_8_5dBm);
  if(!WiFi.setTxPower(power))Serial.println("Warning: setting TX power failed");
  IPAddress start(apIP[0],apIP[1],apIP[2],20);
  if(!WiFi.softAPConfig(apIP,apIP,AP_MASK,start,dnsIP))Serial.println("ERROR: AP address/DNS config failed");
  if(!WiFi.softAP(apSSID.c_str(),apPass.c_str(),apChannel,hiddenAP?1:0,apClients))Serial.println("ERROR: softAP failed");
  // NAPT is enabled only after STA gets an IP/gateway via DHCP.
  Serial.printf("Portal: http://%s | NAT: waiting for upstream\n",WiFi.softAPIP().toString().c_str());
  server.on("/",HTTP_GET,statusPage);
  server.on("/internet",HTTP_GET,internetPage);
  server.on("/repeater",HTTP_GET,repeaterPage);
  server.on("/network",HTTP_GET,networkPage);
  server.on("/system",HTTP_GET,systemPage);
  server.on("/diagnostics",HTTP_GET,diagnosticsPage);
  server.on("/diagnostics-run",HTTP_POST,runDiagnostics);
  server.on("/network-add",HTTP_POST,networkAdd);
  server.on("/network-password",HTTP_POST,networkPassword);
  server.on("/network-delete",HTTP_POST,networkDelete);
  server.on("/network-up",HTTP_POST,[](){networkMove(true);});
  server.on("/network-down",HTTP_POST,[](){networkMove(false);});
  server.on("/save-repeater",HTTP_POST,saveRepeater);
  server.on("/save-network",HTTP_POST,saveNetwork);
  server.on("/save-admin",HTTP_POST,saveAdmin);
  
  server.on("/restart",HTTP_POST,doRestart);
  server.on("/factory-reset",HTTP_POST,doFactoryReset);
  server.onNotFound([](){if(!auth())return;server.sendHeader("Location","/",true);server.send(302,"text/plain","");});
  server.begin();resetCycle();nextConnectAt=millis();
}
void loop(){
  server.handleClient();
  unsigned long now=millis();
  if(nextRestart && (int32_t)(now-nextRestart)>=0){delay(50);ESP.restart();}
  processUpstream();
  if((uint32_t)(now-lastPrint)>12000){
    lastPrint=now;
    Serial.printf("Router: %s | RSSI: %d dBm | clients: %u | NAT: %s\n",
     WiFi.status()==WL_CONNECTED?"connected":"offline",WiFi.status()==WL_CONNECTED?WiFi.RSSI():0,
     WiFi.softAPgetStationNum(),natOk?"on":"FAILED");
  }
  delay(2);
}
