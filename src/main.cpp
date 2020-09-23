#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <HardwareSerial.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>





/*==========================================================================================================*
* =============================================CONSTANTS====================================================*
*==========================================================================================================*/

#define SERIAL_2_ON 
// #define SERIAL_USB  //uncomment to find out IP address, then recomment.

#ifdef SERIAL_2_ON
  #define RXPIN 16
  #define TXPIN 17
#endif

#ifdef SERIAL_USB
  #define RX0 3
  #define TX0 1
#endif


#define MAX_SIZE 255
#define BAUD_SPEED 9600
const int dns_port = 53;
const int http_port =  80;
const int ws_port = 1337;
const char term = '\n'; /*, comm_init = '#'; */ //  <--- Uncomment for mandatory first character in each message from Arduino 

const char *ssid = "RAFL_AP";
const char *pass =  "raflnet123";


WebSocketsServer webSocket = WebSocketsServer(ws_port);
AsyncWebServer server(http_port);

bool last_LED = false, new_data = false;
char rc;
char msg_buf[MAX_SIZE], last_buf[MAX_SIZE], *aux;
uint8_t show_data_in = 1, start_send=0, i=0, n_bytes = 0, check = 1, c_data = 0;





/*==========================================================================================================*
*==============================================FUNCTIONS====================================================*
*==========================================================================================================*/

void check_arduino(){
    //  Uncomment below for mandatory first character in each message from Arduino 
    /*
      while(Serial2.available() > 0 && rc !=comm_init) 
      rc = Serial2.read();
    */
  while(Serial2.available() > 0 && new_data == false) //reading
  {
    rc = Serial2.read();
    if(rc != term)
    {
      msg_buf[n_bytes] = rc;
      n_bytes++;
      if(n_bytes > 255) {check = 0; return;}
    }
    else 
    {
      msg_buf[n_bytes] = '\0';
      n_bytes = 0;
      new_data = true;
    }
  }
}
void print_data(){
  if(!check) {Serial.println("BUFFER FULL"); check = 1;}
  if(new_data) {
    new_data = false; // avaiable for reading again
    check = 1;
  }
}
uint8_t send_data(char* str){
  if(Serial2.availableForWrite() > strlen(str)){
    Serial2.write(str);
    delay(50);
    return 1;
  }
  return 0;
}
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lengh){
  switch (type)
  {
    case WStype_DISCONNECTED:
    break;
    case WStype_CONNECTED:
    {
      IPAddress ip = webSocket.remoteIP(num);   
    } break;
    case WStype_TEXT: //SENDS MESSAGE IF IT IS DIFFERENT
    {
      if(!strcmp((char *)payload, "<#SC>") && start_send && !c_data ){
        if(!strcmp(last_buf, msg_buf))
        {
          webSocket.sendTXT(num, "No changes");
          strcpy(last_buf, msg_buf);
        }
        else //send message to website
        {
          webSocket.sendTXT(num, msg_buf);
          strcpy(last_buf, msg_buf);
        }
      } 
      else if(!strcmp((char *)payload, "<#SS>"))  {
        start_send = 1;
        webSocket.sendTXT(num, "Connection established with ESP32");
      }
      else if(!strcmp((char * )payload, "<#CM>") && !c_data){
        c_data = 1;
        break;
      }
      if(c_data){ // message by client (websit->esp->arduino)
        c_data = !c_data; 
        for(int i = 0 ; i< strlen(last_buf); i++) last_buf[i] = '\0'; //clear last_buf
        uint8_t c = send_data((char * )payload);
        if(!c){
          webSocket.sendTXT(num, "Size of incoming message exceeded");
        }
      }
    } break;

    default:
    break;
  }
}
void onIndexRequest(AsyncWebServerRequest *request){
  IPAddress ip = request->client()->remoteIP(); //client IP
  request->send(SPIFFS, "/index.html", "text/html");
}
#ifdef CSS
void onCSSRequest(AsyncWebServerRequest *request)
{
  IPAddress ip = request->client()->remoteIP(); //client IP
  Serial.println("[" + ip.toString() + "] HTTP request of " + request->url());
  request->send(SPIFFS, "/style.css", "text/css");
}
#endif
void onPageNotFound(AsyncWebServerRequest *request){
  IPAddress ip = request->client()->remoteIP(); //client IP
  request->send(404, "text/plain", "Not found"); 
}

void setup() {

  Serial2.begin(BAUD_SPEED, SERIAL_8N1, RXPIN, TXPIN);

  

  if(!SPIFFS.begin())
  {
    while(1);
  }

  WiFi.softAP(ssid, pass);
 
  #ifdef SERIAL_USB
    Serial.begin(9600, SERIAL_8N1, RX0, TX0);
    Serial.println("Acess Point Working!");
    Serial.print("MY IP: ");
    Serial.println(WiFi.softAPIP());
    while(1);
  #endif
  server.on("/", HTTP_GET, onIndexRequest); //Send index.html


  #ifdef CSS
  server.on("/", HTTP_GET, onCSSRequest); //IF css file exists
  #endif

  server.onNotFound(onPageNotFound);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  check_arduino();   
  print_data(); 
  webSocket.loop();
}
