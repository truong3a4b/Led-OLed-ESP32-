#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <WebServer.h>

#define BTN_BLE 26
#define BTN_WIFI 25
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1  // Reset không dùng chân riêng trên ESP32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String TYPE = "TV";
bool oledOn = true;
int channel = 1;

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

void showOled(int x,int y,String content){
  display.clearDisplay();
  display.setTextSize(1);               // Cỡ chữ
  display.setTextColor(SSD1306_WHITE);  // Màu chữ
  display.setCursor(x, y);              // Vị trí bắt đầu
  display.println(content);        // Nội dung
  display.display();
}

void handlePower(){
  oledOn = !oledOn;
  if (oledOn) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    String content = "VTV "+String(channel);
    showOled(6, 16,content);
    server.send(200,"text/plain","Ok");
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    server.send(200,"text/plain","Ok");
  }
}

void handleChUp(){
  if(oledOn){
    channel = (channel+1)%10;
    String content = "VTV "+String(channel);
    showOled(6, 16,content);
  }
  server.send(200,"text/plain","Ok");
}
void handleChDown(){
  if(oledOn){
    channel = (channel-1)%10;
    String content = "VTV "+String(channel);
    showOled(6, 16,content);
  }
  server.send(200,"text/plain","Ok");
}
void handleCheck(){
  Serial.println("HEkk");
  if(oledOn){
     server.send(200,"text/plain","ON");
  }else{
     server.send(200,"text/plain","OFF");
  }
}

void handleConnect() {
  showOled(0, 16, "Connecting to WIFI...");
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
    showOled(0, 16, "Connected to Wifi");
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
  // Khởi động màn hình OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Không tìm thấy màn hình OLED"));
    while (true)
      ;  // Dừng chương trình
  }
  showOled(0,0,"");
  server.on("/tv/power",handlePower);
  server.on("/tv/chup",handleChUp);
  server.on("/tv/chdown",handleChDown);
  server.on("/check",handleCheck);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/success",HTTP_POST,handleSuccess);
  
}

void loop() {

  server.handleClient();

  static bool lastBtnBLE = HIGH;
  static bool lastBtnWifi = HIGH;
  int btnBLE = digitalRead(BTN_BLE);
  int btnWifi = digitalRead(BTN_WIFI);

  //Ket noi Wifi qua bluetooth khi nhan nut
  if (btnBLE == LOW && lastBtnBLE == HIGH) {  // Nút được nhấn (do nối xuống GND)
    if(oledOn){
      bleOn = !bleOn;
      if(bleOn){
        SerialBT.begin("[TV] MX370");
        Serial.println("Bluetooth Started! Ready to pair...");
        checkBluetooth = true;

        display.clearDisplay();                // Xóa màn hình
        display.setCursor(0, 0);              // Vị trí bắt đầu
        display.println("Bluetooh Started");
        display.setCursor(0, 16);              // Vị trí bắt đầu
        display.println("Ready to pair...");
        display.display();
      }else{
        SerialBT.end();
        checkBluetooth = false;
        String content = "VTV "+String(channel);
        showOled(0, 0,content);
      }
    }else{
      showOled(0,0,"");
    }
  
  }
  lastBtnBLE = btnBLE;

  if(checkBluetooth){
    if(SerialBT.hasClient()){
      display.clearDisplay();                // Xóa màn hình
      display.setCursor(0, 0);              // Vị trí bắt đầu
      display.println("Bluetooh Started");
      display.setCursor(0, 16);              // Vị trí bắt đầu
      display.println("Paired");
      display.display();
    }
    if(SerialBT.available()){
      char c = SerialBT.read();
      if(c == 0x01){
        listenWifi = true;
        Serial.println("Dang nghe wifi");
        display.setCursor(0, 32);              // Vị trí bắt đầu
        display.println("Connecting to Wifi");
        display.display();
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
            showOled(0,16,"Connnected to WIFI");
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
            showOled(0,0,"Connected Failed");
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
    if(oledOn){
      APOn = !APOn;
      if(APOn){
       
        bool result = WiFi.softAP(apSSID, apPassword);

        if (result) {
          Serial.println("AP đã tạo thành công");
          Serial.print("🔗 IP AP: ");
          Serial.println(WiFi.softAPIP());
          server.begin();
        } else {
          Serial.println("Tạo AP thất bại");
        }
        
        
        showOled(0, 0, "AP mode started");
        
      }else{
          WiFi.softAPdisconnect(true);  
      }
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