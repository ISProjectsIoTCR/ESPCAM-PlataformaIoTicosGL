#include <Arduino.h>
#include "Splitter.h"
#include "WiFi.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
//#include "DHTesp.h"  //Libreria para el sensor de temperatura
#include "esp_camera.h"
//#include "base64.h"
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
//#include <SocketIOclient.h>

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
//PINS-OUTPUTS
#define led 4




//CONEXION CON IoTPROJECTS
String dId = "2976";
String webhook_pass = "mb8C6aiDgo";
String webhook_endpoint = "http://192.168.0.12:3001/api/getdevicecredentials";
const char *mqtt_server = "192.168.0.12";
const char* wsserver = "192.168.0.12";
int wsport = 3004;

//CONFIGURACION DE WiFi
const char* hostname = "ESP32CAM";
const char* ssid = "Neuro";
const char* password = "Neuroaula17$";

const long sendDBInterval = 300000;

//CONFIGURACION DE SENSORES Y ACTUADORES
struct Config                               //PINS-INPUTS (json construct setup)
{
  float sensor_1;
  float sensor_2;
  //float temperature;
  int ledstate;
};
Config config;
//int pinDHT = 15;


//Functions definitions
void sendToDashboard(const Config & config);
bool get_mqtt_credentials();
void check_mqtt_connection();
bool reconnect();
void process_sensors();
void process_actuators();
void connect_to_IoTCRv2();
void send_data_to_broker();
void callback(char *topic, byte *payload, unsigned int length);
void process_incoming_msg(String topic, String incoming);
void print_stats();
void clear();
//DHTesp dht; //Instanciamos el DHT

//Global Vars
WiFiClient espclient;
PubSubClient client(espclient);
Splitter splitter;
DynamicJsonDocument mqtt_data_doc(2048);
WebSocketsClient webSocket;

long lastReconnectAttemp = 0;
long varsLastSend[20];
String last_received_msg = "";
String last_received_topic = "";
long lastStats = 0;
long lastsendToDB = 0;
unsigned long messageTimestamp = 0;



//_________________________________SET-UP_______________________________________
void setup()
{

  Serial.begin(115200);
  pinMode(led, OUTPUT);
  // dht.setup(pinDHT, DHTesp::DHT22);
  clear();


  // Connect to Wi-Fi

  int counter = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    counter++;

    if (counter > 10)
    {
      Serial.print("  ⤵");
      Serial.print("\n\n         Ups WiFi Connection Failed :( ");
      Serial.println(" -> Restarting..." );
      delay(2000);
      ESP.restart();
    }
  }
  Serial.print("  ⤵" );
  //Printing local ip
  Serial.println( "\n\n         WiFi Connection -> SUCCESS :)" );
  Serial.print("\n         Local IP -> ");
  Serial.print(WiFi.localIP());
  delay(5000);

  setupCamera();
  // server address, port and URL
  webSocket.begin(wsserver, wsport, "/esp32camera/" + dId);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);

  get_mqtt_credentials();
  client.setCallback(callback);
}


//__________________________________LOOP________________________________________
void loop()
{
  check_mqtt_connection();
  client.loop();
  process_sensors();
  sendToDashboard(config);
  sendtows();
  print_stats();

}


//________________________________SENSORES ⤵____________________________________
void process_sensors()
{
  //  TempAndHumidity data = dht.getTempAndHumidity();

  //get temperature simulation
  config.sensor_1 = random(1, 100);
  //get humidity simulation
  config.sensor_2 = random(1, 50);

  //get led status
  mqtt_data_doc["variables"][6]["last"]["value"] = (HIGH == digitalRead(led));

}

//________________________________SENT DATA WS CAMERA ⤵____________________________________

void sendtows() {
  webSocket.loop();
  uint64_t now = millis();

  if (now - messageTimestamp > 30) {
    messageTimestamp = now;

    camera_fb_t * fb = NULL;

    // Take Picture with Camera
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    webSocket.sendBIN(fb->buf, fb->len);
    Serial.println("Image sent");
    esp_camera_fb_return(fb);
  }
}


//________________________PUBLICAR EN IoTPROJECTS ⤵_____________________________
void sendToDashboard(const Config & config)
{
  if (!(millis() - lastsendToDB > sendDBInterval))
  {
    //*********************CADA POSICIÓN ES UN WIDGET QUE CREASTE*******************


    mqtt_data_doc["variables"][0]["last"]["value"] = config.sensor_1;
    //posición 1 del template
    mqtt_data_doc["variables"][2]["last"]["value"] = config.sensor_1;

    //posición 3 del template
    mqtt_data_doc["variables"][4]["last"]["value"] = config.sensor_2;


    //******************************************************************************
    send_data_to_broker();
  }
  else
  {
    Serial.println("ENVIANDO A BASE DE DATOS");
    send_data_to_DB();
    lastsendToDB = millis();
  }
}


//________________________________ACTUADORES ⤵__________________________________
void process_actuators()
{
  if (mqtt_data_doc["variables"][1]["last"]["value"] == true)
    //posición 4 del template
  {
    digitalWrite(led, HIGH);
    //mqtt_data_doc["variables"][4]["last"]["value"] = "";
    varsLastSend[6] = 0;                               //posición 6 del template
  }
  else if (mqtt_data_doc["variables"][1]["last"]["value"] == false)
    //posición 4 del template
  {
    digitalWrite(led, LOW);
    //mqtt_data_doc["variables"][5]["last"]["value"] = "";
    varsLastSend[6] = 0;                               //posición 6 del template
  }
}


//________________________________OBTENER_CREDENCIALES ⤵________________________
bool get_mqtt_credentials()
{


  Serial.print( "\n\n\nGetting MQTT Credentials from WebHook ⤵");
  delay(1000);

  String toSend = "dId=" + dId + "&password=" + webhook_pass;

  HTTPClient http;
  http.begin(webhook_endpoint);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int response_code = http.POST(toSend);

  if (response_code < 0)
  {
    Serial.print("\n\n         Error Sending Post Request :( " );
    http.end();
    return false;
  }

  if (response_code != 200)
  {
    Serial.print("\n\n         Error in response :(   e-> " + response_code);
    http.end();
    return false;
  }

  if (response_code == 200)
  {
    String responseBody = http.getString();

    Serial.print( "\n\n         Mqtt Credentials Obtained Successfully :) " );

    deserializeJson(mqtt_data_doc, responseBody);
    http.end();
    delay(1000);
  }

  return true;

}

//____________________________CHECK_MQTT_CONECTION ⤵____________________________
void check_mqtt_connection()
{

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print( "\n\n         Ups WiFi Connection Failed :( ");
    Serial.println(" -> Restarting...");
    delay(15000);
    ESP.restart();
  }

  if (!client.connected())
  {

    long now = millis();

    if (now - lastReconnectAttemp > 5000)
    {
      lastReconnectAttemp = millis();
      if (reconnect())
      {
        lastReconnectAttemp = 0;
      }
    }
  }
}

//________________________________SEND_TO_BROKER ⤵______________________________
void send_data_to_broker()
{

  long now = millis();

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
  {

    if (mqtt_data_doc["variables"][i]["variableType"] == "output")
    {
      continue;
    }

    int freq = mqtt_data_doc["variables"][i]["variableSendFreq"];

    if (now - varsLastSend[i] > freq * 1000)
    {
      varsLastSend[i] = millis();
      mqtt_data_doc["variables"][i]["last"]["save"] = 0;

      String str_root_topic = mqtt_data_doc["topic"];
      String str_variable = mqtt_data_doc["variables"][i]["variable"];
      String topic = str_root_topic + str_variable + "/sdata";

      String toSend = "";

      serializeJson(mqtt_data_doc["variables"][i]["last"], toSend);

      client.publish(topic.c_str(), toSend.c_str());


      //STATS
      long counter = mqtt_data_doc["variables"][i]["counter"];
      counter++;
      mqtt_data_doc["variables"][i]["counter"] = counter;

    }
  }
}

//______________________________________________________________________________
void send_data_to_DB()
{
  long now = millis();

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
  {

    if (mqtt_data_doc["variables"][i]["variableType"] == "output")
    {
      continue;
    }

    mqtt_data_doc["variables"][i]["last"]["save"] = 1;

    String str_root_topic = mqtt_data_doc["topic"];
    String str_variable = mqtt_data_doc["variables"][i]["variable"];
    String topic = str_root_topic + str_variable + "/sdata";

    String toSend = "";

    serializeJson(mqtt_data_doc["variables"][i]["last"], toSend);
    client.publish(topic.c_str(), toSend.c_str());
    Serial.print(" Mqtt ENVIADO:) " );

    //STATS
    long counter = mqtt_data_doc["variables"][i]["counter"];
    counter++;
    mqtt_data_doc["variables"][i]["counter"] = counter;
  }
}


//________________________________RECONNECT ⤵___________________________________
bool reconnect()
{

  if (!get_mqtt_credentials())
  {
    Serial.println("\n\n      Error getting mqtt credentials :( \n\n RESTARTING IN 10 SECONDS");

    delay(10000);
    ESP.restart();
  }

  //Setting up Mqtt Server
  client.setServer(mqtt_server, 1883);

  Serial.print("\n\n\nTrying MQTT Connection ⤵");

  String str_client_id = "device_" + dId + "_" + random(1, 9999);
  const char *username = mqtt_data_doc["username"];
  const char *password = mqtt_data_doc["password"];
  String str_topic = mqtt_data_doc["topic"];

  if (client.connect(str_client_id.c_str(), username, password))
  {
    Serial.print( "\n\n         Mqtt Client Connected :) ");
    delay(2000);

    client.subscribe((str_topic + "+/actdata").c_str());
    return true;
  }
  else
  {
    Serial.print( "\n\n         Mqtt Client Connection Failed :( " );
  }

}


//________________________________SENSORES ⤵____________________________________
//TEMPLATE ⤵
void process_incoming_msg(String topic, String incoming) {

  last_received_topic = topic;
  last_received_msg = incoming;

  String variable = splitter.split(topic, '/', 2);

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++ ) {

    if (mqtt_data_doc["variables"][i]["variable"] == variable) {

      DynamicJsonDocument doc(256);
      deserializeJson(doc, incoming);
      mqtt_data_doc["variables"][i]["last"] = doc;

      long counter = mqtt_data_doc["variables"][i]["counter"];
      counter++;
      mqtt_data_doc["variables"][i]["counter"] = counter;
    }
  }

  process_actuators();

}

//________________________________CALLBACK ⤵____________________________________
void callback(char *topic, byte *payload, unsigned int length)
{

  String incoming = "";

  for (int i = 0; i < length; i++)
  {
    incoming += (char)payload[i];
  }

  incoming.trim();

  process_incoming_msg(String(topic), incoming);
}


//________________________________WEBSOCKET EVENTS ⤵________________________________

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED: {
        Serial.printf("[WSc] Connected to url: %s\n", payload);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
    case WStype_PING:
      // pong will be send automatically
      Serial.printf("[WSc] get ping\n");
      break;
    case WStype_PONG:
      // answer to a ping we send
      Serial.printf("[WSc] get pong\n");
      break;
  }

}


//________________________________SETUP CAMERAL ⤵________________________________

void setupCamera()
{

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
  config.jpeg_quality = 8;//10
  config.fb_count = 1;

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }


}

//_________________________________PRINTS ⤵_____________________________________
void print_stats()
{
  long now = millis();

  if (now - lastStats > 2000)
  {
    lastStats = millis();
    clear();
    Serial.print("\n");
    Serial.print( "\n╔══════════════════════════╗" );
    Serial.print( "\n║       SYSTEM STATS       ║" );
    Serial.print( "\n╚══════════════════════════╝" );
    Serial.print("\n\n");
    Serial.print("\n\n");

    Serial.print( "# \t Name \t\t Var \t\t Type  \t\t Count  \t\t Last V \n\n");
    for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
    {
      String variableFullName = mqtt_data_doc["variables"][i]["variableFullName"];
      String variable = mqtt_data_doc["variables"][i]["variable"];
      String variableType = mqtt_data_doc["variables"][i]["variableType"];
      String lastMsg = mqtt_data_doc["variables"][i]["last"];
      long counter = mqtt_data_doc["variables"][i]["counter"];

      Serial.println(String(i) + " \t " + variableFullName.substring(0, 5) + " \t\t " + variable.substring(0, 10) + " \t " + variableType.substring(0, 5) + " \t\t " + String(counter).substring(0, 10) + " \t\t " + lastMsg);
    }
    Serial.print( "\n\n Last Incomming Msg -> " + last_received_msg);
  }
}
//________________________________CLEAR_SERIAL ⤵________________________________
void clear()
{
  Serial.write(27);    // ESC command
  Serial.print("[2J"); // clear screen command
  Serial.write(27);
  Serial.print("[H"); // cursor to home command

}
