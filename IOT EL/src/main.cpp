#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"
#include <ArduinoJson.h>

// Use Arduino String explicitly to avoid MB_String conflict
using String = ::String;

#define DHTTYPE DHT11
#define DHTPIN 18
#define DS18B20 5
#define REPORTING_PERIOD_MS 1000

float temperature, humidity, BPM, SpO2, bodytemperature;
bool isLoggedIn = false;

// Function prototypes
void handle_OnConnect();
void handle_NotFound();

String SendHTML(float temperature, float humidity, float BPM, float SpO2, float bodytemperature);
String SendLoginPage(String errorMsg = "");

// Wi-Fi and Firebase credentials
const char* ssid = "notbhuvi";
const char* password = "00000000";

DHT dht(DHTPIN, DHTTYPE);
PulseOximeter pox;
uint32_t tsLastReport = 0;
OneWire oneWire(DS18B20);
DallasTemperature sensors(&oneWire);
WebServer server(80);

void handle_Data() {
  JsonDocument doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["heart_rate"] = BPM;
  doc["spo2"] = SpO2;
  doc["body_temperature"] = bodytemperature;

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

void onBeatDetected() {
  Serial.println("Beat!");
}

String SendLoginPage(String errorMsg) {
  String html;
  html.reserve(1024); // Reserve memory to avoid fragmentation
  html.concat("<!DOCTYPE html><html lang='en'><head>");
  html.concat("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  html.concat("<title>Health Monitor Login</title>");
  html.concat("<style>");
  html.concat("body { font-family: Arial, sans-serif; background-color: #000; color: white; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }");
  html.concat(".login-container { background: rgba(255,255,255,0.1); backdrop-filter: blur(10px); padding: 2rem; border-radius: 15px; box-shadow: 0 8px 32px rgba(0,0,0,0.2); max-width: 350px; width: 90%; }");
  html.concat("h2 { text-align: center; margin-bottom: 1.5rem; }");
  html.concat("form { display: flex; flex-direction: column; }");
  html.concat("input { margin-bottom: 1rem; padding: 0.7rem; border: none; border-radius: 5px; font-size: 1rem; }");
  html.concat("button { background: #00ff88; border: none; padding: 0.7rem; border-radius: 5px; cursor: pointer; font-weight: bold; font-size: 1rem; }");
  html.concat("button:hover { background: #00cc6a; }");
  html.concat(".error { color: #ff4c4c; text-align: center; margin-bottom: 1rem; font-weight: bold; }");
  html.concat("</style></head><body>");
  html.concat("<div class='login-container'>");
  html.concat("<h2>ESP32 Health Monitor</h2>");
  if (errorMsg.length() > 0) {
    String errorDiv;
    errorDiv.concat("<div class='error'>");
    errorDiv.concat(errorMsg);
    errorDiv.concat("</div>");
    html.concat(errorDiv);
  }
  html.concat("<form action='/login' method='GET'>");
  html.concat("<input type='text' name='username' placeholder='Username' required>");
  html.concat("<input type='password' name='password' placeholder='Password' required>");
  html.concat("<input type='text' name='patient_id' placeholder='Patient ID' required>");
  html.concat("<button type='submit'>Login</button>");
  html.concat("</form></div></body></html>");
  return html;
}

void setup() {
  Serial.begin(115200);
  pinMode(19, OUTPUT);
  delay(100);
  Serial.println(F("DHTxx test!"));
  dht.begin();
    sensors.begin();
  Serial.println("Connecting to ");
  Serial.println(ssid);

  server.on("/data", handle_Data);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: "); Serial.println(WiFi.localIP());


  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.on("/login", []() {
    if (server.method() == HTTP_GET && server.hasArg("username") && server.hasArg("password") && server.hasArg("patient_id")) {
      String user = server.arg("username");
      String pass = server.arg("password");
      String pid = server.arg("patient_id");
      if (user == "drbehera" && pass == "Bsquare@2004" && pid == "CD013") {
        isLoggedIn = true;
        server.sendHeader("Location", "/");
        server.send(302);
      } else {
        server.send(200, "text/html", SendLoginPage("Invalid credentials! Please try again."));
      }
    } else {
      server.send(200, "text/html", SendLoginPage());
    }
  });

  server.begin();
  Serial.println("HTTP server started");

  Serial.print("Initializing pulse oximeter..");
  if (!pox.begin()) {
    Serial.println("FAILED");
    for (;;);
  } else {
    Serial.println("SUCCESS");
    pox.setOnBeatDetectedCallback(onBeatDetected);
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
}

// ... (other includes and code remain unchanged)

void loop() {
  server.handleClient();
  pox.update();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
  }

  BPM = pox.getHeartRate();
  SpO2 = pox.getSpO2();

  sensors.requestTemperatures();
  bodytemperature = sensors.getTempCByIndex(0);

  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    Serial.print("Room Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");
    Serial.print("Room Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
    Serial.print("BPM: ");
    Serial.print(BPM);
    Serial.println("BPM");
    Serial.print("SpO2: ");
    Serial.print(SpO2);
    Serial.println("%");
    Serial.print("Body Temperature: ");
    Serial.print(bodytemperature);
    Serial.println("°C");
    Serial.println("*\n");
    tsLastReport = millis();
  }

}

// ... (rest of the code remains unchanged)
void handle_OnConnect() {
  if (!isLoggedIn) {
    server.sendHeader("Location", "/login");
    server.send(302);
    return;
  }
  server.send(200, "text/html", SendHTML(temperature, humidity, BPM, SpO2, bodytemperature));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float temperature, float humidity, float BPM, float SpO2, float bodytemperature) {
  String html;
  html.reserve(4096); // Reserve memory to avoid fragmentation
  html.concat("<!DOCTYPE html>");
  html.concat("<html lang='en'>");
  html.concat("<head>");
  html.concat("<meta charset='UTF-8'>");
  html.concat("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  html.concat("<title>Smart Health Monitoring Dashboard</title>");
  html.concat("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css'>");
  html.concat("<style>");
  html.concat("* { margin: 0; padding: 0; box-sizing: border-box; }");
  html.concat("body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #000; min-height: 100vh; color: #333; overflow-x: hidden; }");
  html.concat(".particles { position: fixed; top: 0; left: 0; width: 100%; height: 100%; pointer-events: none; z-index: 1; }");
  html.concat(".particle { position: absolute; background: rgba(255,255,255,0.1); border-radius: 50%; animation: float 6s ease-in-out infinite; }");
  html.concat("@keyframes float { 0%, 100% { transform: translateY(0px) rotate(0deg); } 50% { transform: translateY(-20px) rotate(180deg); } }");
  html.concat(".header { position: relative; z-index: 10; text-align: center; padding: 2rem 1rem; background: rgba(255,255,255,0.1); backdrop-filter: blur(10px); border-bottom: 1px solid rgba(255,255,255,0.2); }");
  html.concat(".header h1 { font-size: 3rem; font-weight: 700; color: white; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); margin-bottom: 0.5rem; animation: glow 2s ease-in-out infinite alternate; }");
  html.concat("@keyframes glow { from { text-shadow: 2px 2px 4px rgba(0,0,0,0.3); } to { text-shadow: 2px 2px 20px rgba(255,255,255,0.5); } }");
  html.concat(".header p { font-size: 1.2rem; color: rgba(255,255,255,0.9); }");
  html.concat(".status-indicator { display: inline-block; width: 12px; height: 12px; background: #00ff88; border-radius: 50%; margin-left: 8px; animation: pulse 2s infinite; }");
  html.concat("@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }");
  html.concat(".container { position: relative; z-index: 10; max-width: 1400px; margin: 2rem auto; padding: 0 1rem; }");
  html.concat(".sensors-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 2rem; margin-top: 2rem; }");
  html.concat(".sensor-card { background: rgba(255,255,255,0.15); backdrop-filter: blur(15px); border-radius: 20px; padding: 2rem; border: 1px solid rgba(255,255,255,0.2); transition: all 0.3s ease; position: relative; overflow: hidden; box-shadow: 0 8px 32px rgba(0,0,0,0.1); }");
  html.concat(".sensor-card:hover { transform: translateY(-5px); box-shadow: 0 12px 40px rgba(0,0,0,0.2); }");
  html.concat(".sensor-card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 3px; background: linear-gradient(90deg, #ff6b6b, #4ecdc4, #45b7d1, #96ceb4, #feca57); }");
  html.concat(".sensor-icon { font-size: 3rem; margin-bottom: 1rem; display: block; animation: bounce 2s infinite; }");
  html.concat("@keyframes bounce { 0%, 20%, 50%, 80%, 100% { transform: translateY(0); } 40% { transform: translateY(-10px); } 60% { transform: translateY(-5px); } }");
  html.concat(".sensor-title { font-size: 1.1rem; font-weight: 600; color: rgba(255,255,255,0.9); margin-bottom: 0.5rem; text-transform: uppercase; letter-spacing: 1px; }");
  html.concat(".sensor-value { font-size: 2.5rem; font-weight: 700; color: white; margin-bottom: 0.5rem; }");
  html.concat(".sensor-unit { font-size: 1rem; color: rgba(255,255,255,0.7); font-weight: 500; }");
  html.concat(".sensor-status { margin-top: 1rem; padding: 0.5rem 1rem; border-radius: 25px; font-size: 0.9rem; font-weight: 500; text-align: center; }");
  html.concat(".status-normal { background: rgba(0,255,136,0.2); color: #00ff88; border: 1px solid rgba(0,255,136,0.3); }");
  html.concat(".status-warning { background: rgba(255,193,7,0.2); color: #ffc107; border: 1px solid rgba(255,193,7,0.3); }");
  html.concat(".status-critical { background: rgba(220,53,69,0.2); color: #dc3545; border: 1px solid rgba(220,53,69,0.3); }");
  html.concat(".chart-container { margin-top: 2rem; background: rgba(255,255,255,0.1); backdrop-filter: blur(10px); border-radius: 15px; padding: 1.5rem; border: 1px solid rgba(255,255,255,0.2); }");
  html.concat(".chart-title { color: white; font-size: 1.3rem; font-weight: 600; margin-bottom: 1rem; text-align: center; }");
  html.concat(".footer { text-align: center; margin-top: 3rem; padding: 2rem; color: rgba(255,255,255,0.7); border-top: 1px solid rgba(255,255,255,0.1); }");
  html.concat(".last-updated { font-size: 0.9rem; margin-top: 0.5rem; }");
  html.concat("@media (max-width: 768px) { .header h1 { font-size: 2rem; } .sensors-grid { grid-template-columns: 1fr; gap: 1rem; } .sensor-card { padding: 1.5rem; } .sensor-value { font-size: 2rem; } .container { margin: 1rem auto; padding: 0 0.5rem; } }");
  html.concat("</style>");
  html.concat("</head>");
  html.concat("<body>");
  html.concat("<div class='particles'>");
  for (int i = 0; i < 6; i++) {
    String particle;
    particle.concat("<div class='particle' style='width: ");
    particle.concat(String(10 + i*5));
    particle.concat("px; height: ");
    particle.concat(String(10 + i*5));
    particle.concat("px; left: ");
    particle.concat(String(i*15 + 10));
    particle.concat("%; top: ");
    particle.concat(String(i*12 + 20));
    particle.concat("%; animation-delay: ");
    particle.concat(String(i*0.5));
    particle.concat("s;'></div>");
    html.concat(particle);
  }
  html.concat("</div>");
  html.concat("<div class='header'>");
  html.concat("<h1><i class='fas fa-heartbeat'></i> Health Monitor</h1>");
  html.concat("<p>Real-time Patient Monitoring Dashboard<span class='status-indicator'></span> Live</p>");
  html.concat("</div>");
  html.concat("<div class='container'>");
  html.concat("<div class='sensors-grid'>");
  html.concat("<div class='sensor-card'>");
  html.concat("<i class='fas fa-thermometer-half sensor-icon' style='color: #4ecdc4;'></i>");
  html.concat("<div class='sensor-title'>Room Temperature</div>");
  String tempValue;
  tempValue.concat("<div class='sensor-value'>");
  tempValue.concat(String((int)temperature));
  tempValue.concat("<span class='sensor-unit'>°C</span></div>");
  html.concat(tempValue);
  String tempStatus = (temperature >= 20 && temperature <= 25) ? "status-normal'>Optimal" : 
                     (temperature > 25) ? "status-warning'>Warm" : "status-critical'>Cold";
  String tempStatusDiv;
  tempStatusDiv.concat("<div class='sensor-status ");
  tempStatusDiv.concat(tempStatus);
  tempStatusDiv.concat("</div>");
  html.concat(tempStatusDiv);
  html.concat("</div>");
  html.concat("<div class='sensor-card'>");
  html.concat("<i class='fas fa-tint sensor-icon' style='color: #45b7d1;'></i>");
  html.concat("<div class='sensor-title'>Room Humidity</div>");
  String humidValue;
  humidValue.concat("<div class='sensor-value'>");
  humidValue.concat(String((int)humidity));
  humidValue.concat("<span class='sensor-unit'>%</span></div>");
  html.concat(humidValue);
  String humidStatus = (humidity >= 40 && humidity <= 60) ? "status-normal'>Optimal" : 
                      (humidity > 60) ? "status-warning'>High" : "status-critical'>Low";
  String humidStatusDiv;
  humidStatusDiv.concat("<div class='sensor-status ");
  humidStatusDiv.concat(humidStatus);
  humidStatusDiv.concat("</div>");
  html.concat(humidStatusDiv);
  html.concat("</div>");
  html.concat("<div class='sensor-card'>");
  html.concat("<i class='fas fa-heartbeat sensor-icon' style='color: #ff6b6b;'></i>");
  html.concat("<div class='sensor-title'>Heart Rate</div>");
  String bpmValue;
  bpmValue.concat("<div class='sensor-value'>");
  bpmValue.concat(String((int)BPM));
  bpmValue.concat("<span class='sensor-unit'>BPM</span></div>");
  html.concat(bpmValue);
  String bpmStatus = (BPM >= 60 && BPM <= 100) ? "status-normal'>Normal" : 
                    (BPM > 100) ? "status-warning'>Elevated" : "status-critical'>Low";
  String bpmStatusDiv;
  bpmStatusDiv.concat("<div class='sensor-status ");
  bpmStatusDiv.concat(bpmStatus);
  bpmStatusDiv.concat("</div>");
  html.concat(bpmStatusDiv);
  html.concat("</div>");
  html.concat("<div class='sensor-card'>");
  html.concat("<i class='fas fa-lungs sensor-icon' style='color: #96ceb4;'></i>");
  html.concat("<div class='sensor-title'>Blood Oxygen</div>");
  String spo2Value;
  spo2Value.concat("<div class='sensor-value'>");
  spo2Value.concat(String((int)SpO2));
  spo2Value.concat("<span class='sensor-unit'>%</span></div>");
  html.concat(spo2Value);
  String spo2Status = (SpO2 >= 95) ? "status-normal'>Normal" : 
                     (SpO2 >= 90) ? "status-warning'>Low" : "status-critical'>Critical";
  String spo2StatusDiv;
  spo2StatusDiv.concat("<div class='sensor-status ");
  spo2StatusDiv.concat(spo2Status);
  spo2StatusDiv.concat("</div>");
  html.concat(spo2StatusDiv);
  html.concat("</div>");
  html.concat("<div class='sensor-card'>");
  html.concat("<i class='fas fa-thermometer-full sensor-icon' style='color: #feca57;'></i>");
  html.concat("<div class='sensor-title'>Body Temperature</div>");
  String bodyTempValue;
  bodyTempValue.concat("<div class='sensor-value'>");
  bodyTempValue.concat(String(bodytemperature, 1));
  bodyTempValue.concat("<span class='sensor-unit'>°C</span></div>");
  html.concat(bodyTempValue);
  String bodyTempStatus = (bodytemperature >= 36.1 && bodytemperature <= 37.2) ? "status-normal'>Normal" : 
                         (bodytemperature > 37.2) ? "status-warning'>Fever" : "status-critical'>Low";
  String bodyTempStatusDiv;
  bodyTempStatusDiv.concat("<div class='sensor-status ");
  bodyTempStatusDiv.concat(bodyTempStatus);
  bodyTempStatusDiv.concat("</div>");
  html.concat(bodyTempStatusDiv);
  html.concat("</div>");
  html.concat("</div>");
  html.concat("</div>");
  html.concat("<div class='footer'>");
  html.concat("<p><i class='fas fa-wifi'></i> Connected to ESP32 Health Monitor</p>");
  html.concat("<div class='last-updated'>Last Updated: <span id='timestamp'></span></div>");
  html.concat("</div>");
  html.concat("<script>");
  html.concat("function updateTimestamp() {");
  html.concat("  const now = new Date();");
  html.concat("  document.getElementById('timestamp').textContent = now.toLocaleTimeString();");
  html.concat("}");
  html.concat("updateTimestamp();");
  html.concat("setInterval(function() {");
  html.concat("  var xhttp = new XMLHttpRequest();");
  html.concat("  xhttp.onreadystatechange = function() {");
  html.concat("    if (this.readyState == 4 && this.status == 200) {");
  html.concat("      document.open(); document.write(this.responseText); document.close();");
  html.concat("    }");
  html.concat("  };");
  html.concat("  xhttp.open('GET', '/', true);");
  html.concat("  xhttp.send();");
  html.concat("}, 2000);");
  html.concat("</script>");
  html.concat("</body>");
  html.concat("</html>");
  return html;
}