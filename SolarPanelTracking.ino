#define BLYNK_TEMPLATE_ID "TMPL41XhEJx_t"
#define BLYNK_TEMPLATE_NAME "SolarPaneltracking"
#define BLYNK_AUTH_TOKEN "w2lZnC03R0Gwri_4p8fj0UzCUxHK5xYY"

#include <ESP32Servo.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#define SERVO_HORIZONTAL_PIN 13
#define SERVO_VERTICAL_PIN 12
#define LDR_TOP_PIN 32
#define LDR_BOTTOM_PIN 33
#define LDR_LEFT_PIN 34
#define LDR_RIGHT_PIN 35
#define NIGHT_AND_WINTER_THRESHOLD 250

Preferences preferences;

Servo verticalServo;
Servo horizontalServo;
BlynkTimer timer;
WebServer server(80);

int posVer = 90;
int posHor = 90;
unsigned long t = 0;
String ssid_input = "";
String password_input = "";

bool isWebServerEnabled = false;
int isAutoMode = 0;

const char* html_form = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>WiFi Settings</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
  </head>
  <body>
    <center>
      <h2>Connect to WiFi</h2>
      <form action="/connect" method="POST">
        <p>SSID:<br><input type="text" name="ssid" size="30"><br><br></p>
        <p>Password:<br><input type="password" name="password" size="30"><br><br></p>
        <input type="submit" value="Connect">
      </form>
    </center>
  </body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", html_form);
}

void handleConnect() {
  ssid_input = server.arg("ssid");
  password_input = server.arg("password");

  if (ssid_input.length() > 0 && password_input.length() > 0) {
    preferences.begin("wifi_data", false);
    preferences.putString("ssid", ssid_input);
    preferences.putString("password", password_input);
    preferences.end();
  server.send(200, "text/html", "<h2>Saving Wi-Fi credentials...</h2>");
  } else {
      server.send(400, "text/html", "<h2>SSID or password is empty!</h2>");
    }
}

void connectToWiFi() {
  preferences.begin("wifi_data", true);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  preferences.end();

  if (ssid.length() > 0 && password.length() > 0) {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid.c_str(), password.c_str());

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      WiFi.softAPdisconnect(true);
      Serial.println("\nConnected!");

      Blynk.config(BLYNK_AUTH_TOKEN);
      Blynk.connect();

      isWebServerEnabled = false;
      return;
    }
    Serial.println("\nConnection failed.");
  }

  WiFi.softAP("ESP32_Config", "12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("\nWi-Fi config portal active. IP: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/connect", HTTP_POST, handleConnect);
  server.begin();

  isWebServerEnabled = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  WiFi.mode(WIFI_AP_STA);
  connectToWiFi();

  sendWiFiInfo();
  timer.setInterval(2000L, runTracking);

  verticalServo.attach(SERVO_VERTICAL_PIN);
  horizontalServo.attach(SERVO_HORIZONTAL_PIN); 
}

bool nightAndWinterControl(int avgLight) {
  if (avgLight < NIGHT_AND_WINTER_THRESHOLD) {
    Blynk.virtualWrite(V10, "Night Mode");
    if (t == 0) {
      verticalServo.detach();
      horizontalServo.detach();
      Serial.println("Night mode. Waiting 2 hour...");
      t = millis();
    } 
    if (millis() - t < 120 * 60 * 1000) {
      return false;
    }
  } else if(t != 0){
    verticalServo.attach(SERVO_VERTICAL_PIN);
    horizontalServo.attach(SERVO_HORIZONTAL_PIN);
    Serial.println("Exit from night mode.");

    posVer = 90;
    posHor = 100;
    verticalServo.write(posVer);
    horizontalServo.write(posHor);
    delay(600);
    horizontalServo.write(90);

    t = 0;
  }
  return true;
}

int calculateVerticalStep(int valueTop, int valueBottom){
  int delta = valueTop - valueBottom;
  int step = 0;

  if(abs(delta) < 100){
    return 0;
  } else if(abs(delta) > 1000){
      step = (delta > 0) ? 15 : -15;
  } else if(abs(delta) > 200){
      step = (delta > 0) ? 10 : -10;
  }
  return step;
}

int calculateHorizontalStep(int valueLeft, int valueRight) {
  int delta = valueLeft - valueRight;

  if (abs(delta) < 100) {
    return 90;
  } else if (delta < 0) {
    return 80;
  } else {
    return 100;
  }
}

void runTracking(){
  int valTop = analogRead(LDR_TOP_PIN);
  int valBottom = analogRead(LDR_BOTTOM_PIN);
  int valLeft = analogRead(LDR_LEFT_PIN);
  int valRight = analogRead(LDR_RIGHT_PIN);
  int avgLight = (valTop + valBottom + valLeft + valRight) / 4;
  Blynk.virtualWrite(V4, avgLight);
  
  if (!isAutoMode == 0) return;

  if (!nightAndWinterControl(avgLight)) {
    return;
  }

  Blynk.virtualWrite(V10, "Day Mode");
  int stepVer = calculateVerticalStep(valTop, valBottom);
  posVer = constrain(posVer + stepVer, 0, 179);
  if (stepVer != 0) {
  verticalServo.write(posVer);
  }

  int stepHor = calculateHorizontalStep(valLeft, valRight);
  horizontalServo.write(stepHor);
  
  if(stepHor != 90){
    delay(500);
    horizontalServo.write(90);
  }
}

void sendWiFiInfo() {
  String ssid = WiFi.SSID();
  String ip = WiFi.localIP().toString();
  Blynk.virtualWrite(V5, ssid);
  Blynk.virtualWrite(V6, ip);
}

BLYNK_WRITE(V9) {
  isAutoMode = param.asInt();  // Перемикач Авто/Ручне
  if (isAutoMode == 0) {
    verticalServo.write(90);
    horizontalServo.write(80);
    delay(500);
    horizontalServo.write(90);
  }
}

BLYNK_WRITE(V8) {  // Горизонтальне керування
  if (isAutoMode == 1) {
    posHor = param.asInt();
    horizontalServo.write(posHor);
  } else {
    Blynk.virtualWrite(V8, posHor); 
  }
}

BLYNK_WRITE(V7) {  // Вертикальне керування
  if (isAutoMode == 1) {
    posVer = param.asInt();
    verticalServo.write(posVer);
  } else {
    Blynk.virtualWrite(V7, posVer);
  }
}

void loop() {
  if (isWebServerEnabled) {
    server.handleClient(); 
  }
  Blynk.run();
  timer.run();
}