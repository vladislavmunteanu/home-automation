#include <Adafruit_Sensor.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/************ WIFI and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
#define wifi_ssid "***"
#define wifi_password "*******"
#define mqtt_server "hvdlocal.duckdns.org"
#define mqtt_user "homeassistant"
#define mqtt_password "******"
#define mqtt_port 1883

/************* MQTT TOPICS (change these topics as you wish)  **************************/
#define sensor_topic "home/bedroom"

/**************************** FOR OTA **************************************************/
#define SENSORNAME "bedroom"
#define OTApassword "*******" // change this to whatever password you want to use when you upload OTA
int OTAport = 8266;

/**************************** PIN DEFINITIONS ********************************************/
#define LEDPIN   D5
#define DHTPIN    D4
#define DHTTYPE   DHT22
#define PIRPIN   D7

/**************************** SENSOR DEFINITIONS *******************************************/
float diffTEMP = 0.2;
float tempValue;

float diffHUM = 1;
float humValue;

int pirState = LOW;
String motionStatus = "Standby";

char message_buff[100];

int calibrationTime = 0;

const int BUFFER_SIZE = 300;

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);



/********************************** START SETUP*****************************************/
void setup() {

  Serial.begin(115200);

  pinMode(PIRPIN, INPUT);
  pinMode(DHTPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);

  Serial.begin(115200);
  delay(10);

  ArduinoOTA.setPort(OTAport);

  ArduinoOTA.setHostname(SENSORNAME);

  ArduinoOTA.setPassword((const char *)OTApassword);

  Serial.print("calibrating sensor ");
  for (int i = 0; i < calibrationTime; i++) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("Starting Node named " + String(SENSORNAME));

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);

  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    blink(20, 100);
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IPess: ");
  Serial.println(WiFi.localIP());
  reconnect();
  blink(5, 1000);
}

/********************************** START SETUP WIFI*****************************************/
void setup_wifi() {
  blink(5, 200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/********************************** START SEND STATE*****************************************/
void sendState() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["humidity"] = (String)humValue;
  root["temperature"] = (String)tempValue;
  root["motion"] = (String)motionStatus;
  
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  Serial.println(buffer);
  client.publish(sensor_topic, buffer, true);
}

/********************************** START RECONNECT*****************************************/
void reconnect() {
  blink(5, 300);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(SENSORNAME, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      sendState();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/********************************** START CHECK SENSOR **********************************/
bool checkBoundSensor(float newValue, float prevValue, float maxDiff) {
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}

/********************************** START MAIN LOOP***************************************/
void loop() {
 
  ArduinoOTA.handle();

  if (!client.connected()) {
    software_Reset();
  }
  client.loop();

  float newTempValue = dht.readTemperature(true); //to use celsius remove the true text inside the parentheses
  float newHumValue = dht.readHumidity();


  long sign = digitalRead(PIRPIN);

  if (sign == HIGH) {
    analogWrite (LEDPIN, 255);
    if (pirState == LOW) {
      motionStatus = "Motion Detected";
      sendState();
      pirState = HIGH;
    }
  }
  else {
    analogWrite (LEDPIN, 1);
    if (pirState == HIGH) {
      motionStatus = "Standby";
      sendState();
      pirState = LOW;
    }

  }

  delay(100);

  if (checkBoundSensor(newTempValue, tempValue, diffTEMP)) {
    tempValue = newTempValue;
    sendState();
  }

  if (checkBoundSensor(newHumValue, humValue, diffHUM)) {
    humValue = newHumValue;
    sendState();
  }

}

/****reset***/
void software_Reset(){
  blink(4, 500);
  Serial.print("resetting");
  ESP.reset();
}

/*** LED BLINK ***/
void blink(int times, int rate) {

  analogWrite (LEDPIN, 1);
  delay(100);

  for (int i = 0; i < times; i++) {
    analogWrite (LEDPIN, 255);
    delay(rate);
    analogWrite (LEDPIN, 1);
    delay(rate);
  }

}



