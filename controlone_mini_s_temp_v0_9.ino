#include <FS.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <WiFiManager.h>

#include <ArduinoJson.h>

#include "DHT.h"

const char* ssid = "BANET";
const char* password = "4271051500";
//const char* host = "OTA-LEDS";

#define DHTPIN 2  //Cambiar por el pin al que este el sensor
#define DHTTYPE DHT22
char static_ip[16] = "192.168.0.19";
char static_gw[16] = "192.168.0.1";
char static_sn[16] = "255.255.255.0";

//char controlone_ip[16] = "192.168.0.20";
char server_ip[16] = "192.168.0.20";
char controlone_ip[16] = "None";

char min_temp[2];
char max_temp[2];

char min_temp_cmd[1000];
char max_temp_cmd[1000];

char custom_sensor_name[50];
char sensor_name[50];

//char ssid[50]= "ControlOne_Mini_Config_BAT" ;

String ssidStr = "";
char ssid_test[100];

const char* host = "CO-MINI";

uint8_t controlFlag = 0;

bool shouldSaveConfig = false;

//WiFiServer server(4998);
int report_port = 5004;
//int report_port = 4997;
int controlone_port = 4998;

char data[1000];
int ind = 0;

//pins MGMT
int resetButton = 12;
int connLed = 13;

IPAddress controlone, report_server;

float min_temp_f;
float max_temp_f;

float total_t = 0.0;
float total_h = 0.0;
int read_count = 0;


//Setting the timing parameter(third) to 11 to ensure
//that the sensor speaks at a proper speed with the ESP
DHT dht(DHTPIN, DHTTYPE, 11);

/**************************************************************************
   Funciones Auxiliares
 **************************************************************************/
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    //  if (i < 5)
    //  result += '_';
  }
  return result;
}

String getMACStr()
{
  String clientMac = "";
  unsigned char mac[6];
  WiFi.macAddress(mac);
  clientMac += macToStr(mac);
  String tempStr = "ControlOne_S_" + clientMac;
  return tempStr;
}
/**************************************************************************
   Fin de Funciones
 **************************************************************************/

void setup() {
  ssidStr = getMACStr();
  ssidStr.toCharArray(ssid_test, 100);

  Serial.begin(38400);
  //set management pins
  pinMode(resetButton, INPUT);
  pinMode(connLed, OUTPUT);

  Serial.println("Starting...");

  if (SPIFFS.begin())
  {
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile)
    {
      size_t size = configFile.size();

      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      json.prettyPrintTo(Serial);
      if (json.success())
      {

        json.prettyPrintTo(Serial);
        
        strcpy(static_ip, json["ip"]);
        strcpy(static_gw, json["gateway"]);
        strcpy(static_sn, json["subnet"]);

        if (json["server_ip"] && json["controlone_ip"] && json["min_temp"] && json["max_temp"]) 
        {
          strcpy(server_ip, json["server_ip"]);
          strcpy(controlone_ip, json["controlone_ip"]);
          strcpy(min_temp, json["min_temp"]);
          strcpy(max_temp, json["max_temp"]);
          strcpy(min_temp_cmd, json["min_temp_cmd"]);
          strcpy(max_temp_cmd, json["max_temp_cmd"]);
        }
        if(json["sensor_name"])
        {
          strcpy(sensor_name, json["sensor_name"]); 
        }
      }
    }
  }

  


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter sensor_name_param("sensor_name", "Nombre del Sensor", custom_sensor_name, 50);
  WiFiManagerParameter server_ip_param("server_ip", "IP del servidor", server_ip, 16);
  WiFiManagerParameter controlone_ip_param("controlone_ip", "IP del ControlOne, None para no usar", controlone_ip, 16);
  WiFiManagerParameter min_temp_param("min_temp", "Temperatura minima", min_temp, 2);
  WiFiManagerParameter min_temp_cmd_param("min_temp_cmd", "Comando IR para la temperatura minima", min_temp_cmd, 1000);
  WiFiManagerParameter max_temp_param("max_temp", "Temperatura maxima", max_temp, 2);
  WiFiManagerParameter max_temp_cmd_param("max_temp_cmd", "Comando IR para la temperatura maxima", max_temp_cmd, 1000);
  
  
  WiFiManager wifiManager;

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //   WiFi.mode(WIFI_STA);
  //
  //   WiFi.begin(ssid, password);
  //
  //   while (WiFi.waitForConnectResult() != WL_CONNECTED){
  //     WiFi.begin(ssid, password);
  //     Serial.println("Retrying connection...");
  //  }


  IPAddress _ip, _gw, _sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn); 
 
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

  //adding all the parameters
  wifiManager.addParameter(&sensor_name_param);
  wifiManager.addParameter(&server_ip_param);
  wifiManager.addParameter(&controlone_ip_param);
  wifiManager.addParameter(&min_temp_param);
  wifiManager.addParameter(&min_temp_cmd_param);
  wifiManager.addParameter(&max_temp_param);
  wifiManager.addParameter(&max_temp_cmd_param);

  //reset settings - for testing
  if (digitalRead(resetButton) == LOW)
  {
    wifiManager.resetSettings();
  }
  //  wifiManager.resetSettings();

  wifiManager.setMinimumSignalQuality();

  if (!wifiManager.autoConnect(ssid_test)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  /* 
   *  Testing to see if here it can grab the params
   */
   if (shouldSaveConfig)
   {
     strcpy(sensor_name, sensor_name_param.getValue());
     strcpy(server_ip, server_ip_param.getValue());
     strcpy(controlone_ip,controlone_ip_param.getValue());
     strcpy(min_temp,min_temp_param.getValue());
     strcpy(min_temp_cmd,min_temp_cmd_param.getValue());
     strcpy(max_temp,max_temp_param.getValue());
     strcpy(max_temp_cmd,max_temp_cmd_param.getValue());
    
     DynamicJsonBuffer jsonBuffer;
     JsonObject& json = jsonBuffer.createObject();

     json["ip"] = WiFi.localIP().toString();
     json["gateway"] = WiFi.gatewayIP().toString();
     json["subnet"] = WiFi.subnetMask().toString();
    
     json["sensor_name"] = sensor_name;
     json["server_ip"] = server_ip;
     json["controlone_ip"] = controlone_ip;
     json["min_temp"] = min_temp;
     json["max_temp"] = max_temp;
     json["min_temp_cmd"] = min_temp_cmd;
     json["max_temp_cmd"] = max_temp_cmd;

     File configFile = SPIFFS.open("/config.json", "w");

     json.prettyPrintTo(Serial);
     json.printTo(configFile);
     configFile.close();
   }   
   Serial.print("Server address: ");
   Serial.println(server_ip);

   Serial.print("Sensor Name: ");
   Serial.println(sensor_name);

   report_server.fromString(server_ip);
  if (strstr(controlone_ip, "None") == NULL)
  {
    controlone.fromString(controlone_ip);
    controlFlag = 1;
  }

  int min_temp_int = (min_temp[0] - '0') * 10 + (min_temp[1] - '0');
  int max_temp_int = (max_temp[0] - '0') * 10 + (max_temp[1] - '0');

  min_temp_f = float(min_temp_int);
  max_temp_f = float(max_temp_int);

  Serial.print("temp min: ");
  Serial.println(min_temp_f);
  Serial.print("temp max: ");
  Serial.println(max_temp_f);
  
  digitalWrite(connLed, HIGH);

  ArduinoOTA.setHostname(host);
  ArduinoOTA.onStart([]() { // switch off all the PWMs during upgrade
    Serial.println("Starting firmware update...");
  });

  ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
    Serial.println("Firmware update complete, rebooting...");
    ESP.restart();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  /* setup the OTA server */
  ArduinoOTA.begin();

  Serial.println("Ready");

  dht.begin();
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
}

void loop() {
  ArduinoOTA.handle();
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  StaticJsonBuffer<100> dataJSONBuffer;
  char dataBuffer[100];
  JsonObject& data = dataJSONBuffer.createObject();
  
  WiFiClient report_client;
  WiFiClient controlone_client;
  
  data["id"] = sensor_name;
  data["temp"] = t;
  data["hum"] = h;
  
  data.prettyPrintTo(dataBuffer,sizeof(dataBuffer));
  
  if(!report_client.connect(report_server,report_port))
  {
    Serial.println("connection failed");
  }
  else
  {
    Serial.println(dataBuffer);
    report_client.print(dataBuffer);
  }
  delay(10);
  if(controlFlag == 1)
  {
    if (t > max_temp_f)
    {
      if(!controlone_client.connect(controlone, controlone_port))
      {
        controlone_client.print(max_temp_cmd);
      }
    }
    if(t < min_temp_f)
    {
      if(!controlone_client.connect(controlone, controlone_port))
      {
        controlone_client.print(min_temp_cmd);
      }
    }
  }
 
  delay(5000);
}
