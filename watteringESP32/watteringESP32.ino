#include <WiFi.h>
#include <PubSubClient.h>
#include "time.h"
#include <stdio.h>
#include <stdlib.h>

//------- WIFI -------
const char* ssid     = "ZIZU's Wifi";
const char* password = "606525491zizuneta!";

//------- MQTT -------
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* topic = "ferran/wateringSystem/esp32";
const char* infoTopic = "ferran/wateringSystem/info";
const char* subscribeTopic = "ferran/wateringSystem/server";


//------- TIME -------
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;            // horari hivern
const long  gmtOffset_sec_estiu = 3600;   // horari estiu
const int   daylightOffset_sec = 3600;    // UTC+1
uint32_t cada_500ms = 0;
uint32_t cada_hora = 0;

char timeMonth[10];
char timeDay[10];
char timeHour[10];
char timeMinute[10];
char timeSecond[10];

//------- MODS -------
int pumpRunTime = 10;  //in seconds
char turnOnTime[10] = "19";
bool activateMoistureSensor = false;
bool isWatered = false;
int moistureLowThreshold = 500;
int moistureHighThreshold = 460;

/* ------------- Definicio Funcions -----------*/
void obtenirLocalTime(bool imprimir_info);
void printAllConfig();

WiFiClient espClient;
PubSubClient client(espClient);
String command;

int pump_pin = 5;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("Connected to the WiFi network");
 
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);           // for receiving messages MQTT
  while (!client.connected()) {
    mqttConnect();
  }

// Ens conectem al servidor NTP i obtenim la data, configurant el RTC amb aquesta
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  obtenirLocalTime(true);
  if ((strcmp(timeMonth,"April") == 0) || (strcmp(timeMonth,"May") == 0) || (strcmp(timeMonth,"June") == 0) || (strcmp(timeMonth,"July") == 0) || (strcmp(timeMonth,"August") == 0) || (strcmp(timeMonth,"September") == 0) || (strcmp(timeMonth,"October") == 0))
  {
    configTime(gmtOffset_sec_estiu, daylightOffset_sec, ntpServer);
    obtenirLocalTime(true);
  }
  
  Serial.println("Type 1 OR 0");
  pinMode(pump_pin, OUTPUT);
  cada_500ms = millis();
  cada_hora = millis();
}

void mqttConnect(){
  Serial.println("Connecting to MQTT...");
  if(client.connect("ESP32Client")) {
    Serial.println("connected");
  }
  else{
    Serial.print("failed with state ");
    Serial.print(client.state());
    delay(2000);
  }
  if (client.subscribe(subscribeTopic, 1)){
    Serial.println("Subscribed to MQTT topic");
  }
}
 
void sendData(String payload){
  while (!client.connected()) {
    mqttConnect();
  }
  Serial.print("sendData: ");
  Serial.println();

  char datatosend[1000];
  payload.toCharArray(datatosend, 1000);
  client.publish(infoTopic, datatosend);
  Serial.println(datatosend);
  delay(1000);
}
 
 
int value = 0;

void loop(){
  if(Serial.available()){ //SERIAL DEBUGING
      command = Serial.readStringUntil('\n');
      Serial.println("Command: " + command);

      if(command == "1"){
        Serial.println("turn on");
        digitalWrite(pump_pin, HIGH);
        sendData("PUMP IS ON");
      }
      else if(command == "0"){
        Serial.println("turn off");
        digitalWrite(pump_pin, LOW);
        obtenirLocalTime(true);
      }
      else{
        Serial.println("wrong command");
      }
  }
  if(!client.loop()){
      mqttConnect();
  }
  if ((millis() - cada_500ms) > 500)
  {
    cada_500ms = millis();
    cada_hora = millis();
    obtenirLocalTime(false);

    // Si és la hora configurada
    if ((strcmp(timeHour,turnOnTime) == 0) && !isWatered) //&& (TIMER_ON == true)) //TODO THIS TIMER_ON IS NOT IMPLEMENTED RLLY
    {
      turnOnPump(pumpRunTime);
      isWatered = true;
      sendData("Pump was automatically activated.");
    }

    if (isWatered && (millis() - cada_hora) > 3601000){
      isWatered = false;
    }
    
    // Si son les 02:00:00 del 1 de Novembre
    if ((strcmp(timeHour,"02") == 0) && (strcmp(timeMinute,"00") == 0) && (strcmp(timeSecond,"00") == 0) && (strcmp(timeDay,"01") == 0) && (strcmp(timeMonth,"November") == 0))
    {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }

    // Si son les 02:00:00 del 1 de Abril
    if ((strcmp(timeHour,"02") == 0) && (strcmp(timeMinute,"00") == 0) && (strcmp(timeSecond,"00") == 0) && (strcmp(timeDay,"01") == 0) && (strcmp(timeMonth,"April") == 0))
    {
        configTime(gmtOffset_sec_estiu, daylightOffset_sec, ntpServer);
    }
  }
  protegir_temporitzadors_millis();
  
}

/* Look for message received */
void callback(char* topic, byte* payload, unsigned int length)
{

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  //Convert char to string  
  
  char *message = ((char*)payload);
  char option = (String((char*)payload))[0];
  message=message+1;
  int configValue = atoi(message);
  char tempString[10];
  char finalTempString[100];
  
  Serial.print("configValue: ");
  Serial.println(configValue);
  Serial.println();
  
  switch (option){
    case '0':
      printAllConfig();
      break;
    case '1':
      Serial.println("Pump time on");
      pumpRunTime = configValue%100;
      sprintf(tempString,"%ld", configValue%100);
      strcpy(finalTempString, "Pump will activate for ");
      strcat(finalTempString, tempString);
      sendData(finalTempString);
      break;
    case '2':
      Serial.println("When to turn on");
      sprintf(turnOnTime,"%ld", configValue%100);
      isWatered = false;
      strcpy(finalTempString, "Pump will now activate at ");
      strcat(finalTempString, turnOnTime);
      sendData(finalTempString);
      break;
    case '3':
      Serial.println("ActivateMoistureSensor");
      activateMoistureSensor = configValue != 0;
      break;
    case '4':
      Serial.println("moistureLowThreshold");
      moistureLowThreshold = configValue;
      break;
    case '5':
      Serial.println("moistureLowThreshold");
      moistureHighThreshold = configValue;
      break;
    case '6':
      turnOnPump(5);
      sendData("6 recieved: pump has been activated");
      break;
    default:
      Serial.println("unknown");//unknown command
  }

}

void turnOnPump(int _pumpRunTime)
{
  Serial.println("turnON");
  digitalWrite(pump_pin, HIGH);
  delay(_pumpRunTime*1000);
  digitalWrite(pump_pin, LOW);
  Serial.println("turnOFF");
}

void obtenirLocalTime(bool imprimir_info)
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  if (imprimir_info == true)
  {
     Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }
  strftime(timeMonth,10, "%B", &timeinfo);
  strftime(timeDay,10, "%d", &timeinfo);
  strftime(timeHour,10, "%H", &timeinfo);
  strftime(timeMinute,10, "%M", &timeinfo);
  strftime(timeSecond,10, "%S", &timeinfo);
}

void protegir_temporitzadors_millis()
{
  if (cada_500ms > millis()) cada_500ms = millis();
  if (cada_hora > millis()) cada_hora = millis();
}

void printAllConfig(){
  Serial.println("Current configuration is:");
  Serial.printf("  (1)Pump will be on for %d seconds\n", pumpRunTime);
  Serial.printf("  (2)Pump will turn on at %s:00 \n", turnOnTime);
  Serial.printf("  (3)(4)(5)Moisture sensor is %d, from %d to %d \n", activateMoistureSensor, moistureLowThreshold, moistureHighThreshold);
}
