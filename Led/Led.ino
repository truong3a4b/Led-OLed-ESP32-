#include <Wire.h>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <WebServer.h>

#define BTN_BLE 26
#define BTN_WIFI 25

String TYPE = "LIGHT";
const int ledPin = 5;
bool checkLed = false;
bool ledNhapNhay = false;

//Dung cho ket noi bluetooth
bool bleOn = false;
BluetoothSerial SerialBT;
String receivedData = "";
bool checkBluetooth = false;//Kiem tra trang thai bluetooh
bool isConnected = false;
bool listenWifi = false;
WebServer server(80);

//Dung cho ket noi Wifi
const char *apSSID = "ESP32_AP";
const char *apPassword = "12345678";
bool APOn = false;
unsigned long startBroadcastTime = 0;
bool broadcast = true;
WiFiUDP udp;

void handleLedOn(){
  digitalWrite(ledPin,HIGH);
  checkLed = true;
  server.send(200,"text/plain","LED is ON");
}
void handleLedOff(){
  digitalWrite(ledPin, LOW);
  checkLed = false;
  server.send(200,"text/plain","LED is OFF");
}
void handleCheck(){
  if(checkLed){
     server.send(200,"text/plain","ON");
  }else{
     server.send(200,"text/plain","OFF");
  }
}

void handleRoot() {
  String html = "<form action=\"/connect\" method=\"POST\">"
                "SSID: <input name=\"ssid\"><br>"
                "Password: <input name=\"password\"><br>"
                "<input type=\"submit\">"
                "</form>";
  server.send(200, "text/html", html);
}

void handleConnect() {
  
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  Serial.print("ssid,pass: ");
  Serial.print(ssid);
  Serial.println(password);
  server.send(200, "text/plain", "Connecting to " + ssid);
  delay(1000);
  WiFi.softAPdisconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  int maxTries = 10; // 5 giây (10 x 500ms)
  while (WiFi.status() != WL_CONNECTED && maxTries-- > 0) {
    delay(1000);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Kết nối thành công!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    ledNhapNhay = false;
    checkLed = true;
    digitalWrite(ledPin,HIGH);
    udp.begin(4210);
    udp.beginPacket("255.255.255.255", 4210);
    startBroadcastTime = millis();
  } else{
    Serial.println("Kết nối khong thành công!");
  }
}

void handleSuccess(){
  String res = server.arg("res");
  if(res=="OK"){
    broadcast = false;
    String cont = WiFi.macAddress() + "###"+TYPE;
    Serial.println(cont); 
    server.send(200, "text/plain", cont);
  }
 
}
void setup() {
  Serial.begin(115200);
  pinMode(BTN_BLE, INPUT_PULLUP);
  pinMode(BTN_WIFI, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin,LOW);
  
  server.on("/", handleRoot);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/success",HTTP_POST,handleSuccess);
  server.on("/led/on", handleLedOn);
  server.on("/led/off", handleLedOff);
  server.on("/check",handleCheck);
  
}

void loop() {

  server.handleClient();

  static bool lastBtnBLE = HIGH;
  static bool lastBtnWifi = HIGH;
  int btnBLE = digitalRead(BTN_BLE);
  int btnWifi = digitalRead(BTN_WIFI);

  if(ledNhapNhay){
    digitalWrite(ledPin,HIGH);
    delay(500);
    digitalWrite(ledPin,LOW);
  }

  //Ket noi Wifi qua bluetooth khi nhan nut
  if (btnBLE == LOW && lastBtnBLE == HIGH) {  // Nút được nhấn (do nối xuống GND)
    bleOn = !bleOn;
    if(bleOn){
      SerialBT.begin("[LIGHT] NXT123");
      Serial.println("Bluetooth Started! Ready to pair...");
      checkBluetooth = true;
      ledNhapNhay = true;
    }else{
      SerialBT.end();
      checkBluetooth = false;
    }
  }
  lastBtnBLE = btnBLE;

  if(checkBluetooth){
    
    if(SerialBT.available()){
      char c = SerialBT.read();
      if(c == 0x01){
        listenWifi = true;
        Serial.println("Dang nghe wifi");
        return;
      }
    if(listenWifi){
      if(c != '\n'){
        receivedData += c;
        Serial.print(".");
      }else{
        listenWifi = false;
        receivedData.trim();
        int sep = receivedData.indexOf(';');
        if (sep > 0) {
          String ssid = receivedData.substring(0, sep);
          String password = receivedData.substring(sep + 1);

          Serial.println("");
          Serial.println("Đang kết nối Wi-Fi...");
          WiFi.begin(ssid.c_str(), password.c_str());

          int timeout = 0;
          while (WiFi.status() != WL_CONNECTED && timeout < 20) {
            delay(500);
            Serial.print(".");
            timeout++;
          }

          if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            Serial.println();
            Serial.println("Kết nối thành công!");
            digitalWrite(ledPin,HIGH);
            checkLed = true;
            ledNhapNhay = false;
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
            SerialBT.write(0x02);

            String cont = WiFi.localIP().toString() + "###"+TYPE;
            SerialBT.print(cont);
            SerialBT.print("\n");
            server.begin();
            checkBluetooth = false;
          } else {
            Serial.println();
            Serial.println("Kết nối thất bại.");
            SerialBT.write(0x03);
            
          }
        } else {
          Serial.println("Định dạng sai. Dùng: SSID;PASSWORD");
        }

        receivedData = ""; //
      }
    }
  }
  }

  //Ket noi wifi qua wifi khi nhan nut
  if(btnWifi == LOW && lastBtnWifi == HIGH){
    APOn = !APOn;
    if(APOn){
      
      bool result = WiFi.softAP(apSSID, apPassword);

      if (result) {
        Serial.println("AP đã tạo thành công");
        Serial.print("IP AP: ");
        Serial.println(WiFi.softAPIP());
        server.begin();
        ledNhapNhay = true;
      } else {
        Serial.println("Tạo AP thất bại");
      }
    
    }else{
        WiFi.softAPdisconnect(true);  
    }
    
  }
  lastBtnWifi = btnWifi;
  if (broadcast && WiFi.status() == WL_CONNECTED) {
    if (millis() - startBroadcastTime < 15000) {  // gửi trong 15 giây
      String ip = WiFi.localIP().toString();
      udp.beginPacket("255.255.255.255", 4210);
      udp.write((const uint8_t*)ip.c_str(), ip.length());
      udp.endPacket();
      Serial.println("Đã gửi UDP IP: " + ip);
      delay(4000); // gửi mỗi giây
    } else {
      broadcast = false;
    }
  }
  delay(100);  // Delay tránh đọc liên tục quá nhanh
}