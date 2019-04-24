/*
ESP8266/ESP32 publish the RSSI as the WiFi signal strength to ThingSpeak channel.
This example is for explaining how to use the AutoConnect library.

In order to execute this example, the ThingSpeak account is needed. Sing up
for New User Account and create a New Channel via My Channels.
For details, please refer to the project page.
https://hieromon.github.io/AutoConnect/examples/index.html#used-with-mqtt-as-a-client-application

This example is based on the environment as of March 20, 2018.
Copyright (c) 2018 Hieromon Ikasamo.
This software is released under the MIT License.
https://opensource.org/licenses/MIT
*/

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define GET_CHIPID()  (ESP.getChipId())
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#define GET_CHIPID()  ((uint16_t)(ESP.getEfuseMac()>>32))
#endif
#include <FS.h>
#include <PubSubClient.h>
#include <AutoConnect.h>
#include "DHT.h"

#define PARAM_FILE      "/param.json"
#define AUX_SETTING_URI "/mqtt_setting"
#define AUX_SAVE_URI    "/mqtt_save"
#define AUX_CLEAR_URI   "/mqtt_clear"

#define DHTPIN 15 
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Adjusting WebServer class with between ESP8266 and ESP32.
#if defined(ARDUINO_ARCH_ESP8266)
typedef ESP8266WebServer  WiFiWebServer;
#elif defined(ARDUINO_ARCH_ESP32)
typedef WebServer WiFiWebServer;
#endif

// This example shows a sketch that realizes the equivalent operation
// of mqttRSSI without using JSON.
// By comparing this example with the example using JSON, mqttRSSI or
// mqttRSSI_FS, you will better understand AutoConnect's custom Web page
// facility.

// Declare AutoConnectElements for the page asf /mqtt_setting
ACText(header, "<h2>MQTT Server settings</h2>", "text-align:center;color:#2f4f4f;padding:10px;");
ACText(caption, "Configure Mqtt", "font-family:serif;color:#4682b4;");
ACInput(mqttserver, "", "Server", "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$", "MQTT broker server");
ACInput(userid, "anyone", "User ID", "^[0-9][a-z][A-Z]{10}$");
ACInput(channelid, "", "Channel ID", "^[0-9]{6}$");
ACInput(userkey, "", "User Key");
ACInput(apikey, "", "API Key");
ACElement(newline, "<hr>");
ACCheckbox(uniqueid, "unique", "Use APID unique");
ACRadio(period, { "15 sec.", "30 sec.", "60 sec.", "180 sec." }, "Update period", AC_Vertical, 1);
ACSubmit(save, "Start", AUX_SAVE_URI);
ACSubmit(discard, "Discard", "/");

// Declare the custom Web page as /mqtt_setting and contains the AutoConnectElements
AutoConnectAux mqtt_setting(AUX_SETTING_URI, "MQTT Setting", true, {
  header,
  caption,
  mqttserver,
  userid,
  channelid,
  userkey,
  apikey,
  newline,
  uniqueid,
  newline,
  period,
  newline,
  save,
  discard
});

// Declare AutoConnectElements for the page as /mqtt_save
ACText(caption2, "<h4>Parameters available as:</h4>", "text-align:center;color:#2f4f4f;padding:10px;");
ACText(parameters);
ACSubmit(clear, "Clear channel", AUX_CLEAR_URI);

// Declare the custom Web page as /mqtt_save and contains the AutoConnectElements
AutoConnectAux mqtt_save(AUX_SAVE_URI, "MQTT Setting", false, {
  caption2,
  parameters,
  clear
});

AutoConnect  portal;
AutoConnectConfig config;
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned int  updateInterval = 0;
unsigned long lastPub = 0;

#define MQTT_Device_ID  "TEMPRATURE"

bool mqttConnect() {
  static const char alphanum[] = "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.
  char    clientId[9];

  uint8_t retry = 3;
  while (!mqttClient.connected()) {
    if (mqttserver.value.length() <= 0)
      break;

    mqttClient.setServer(mqttserver.value.c_str(), 1883);
    Serial.println(String("Attempting MQTT broker:") + mqttserver.value);

    for (uint8_t i = 0; i < 8; i++) {
      clientId[i] = alphanum[random(62)];
    }
    clientId[8] = '\0';

    if (mqttClient.connect(clientId, MQTT_Device_ID, userkey.value.c_str())) {
      
      Serial.println("Established:" + String(clientId));
      return true;
    }
    else {
      Serial.println("Connection failed:" + String(mqttClient.state()));
      if (!--retry)
        break;
      delay(3000);
    }
  }
  return false;
}

int getStrength(uint8_t points) {
  uint8_t sc = points;
  long    rssi = 0;

  while (sc--) {
    rssi += WiFi.RSSI();
    delay(20);
  }
  return points ? static_cast<int>(rssi / points) : 0;
}

// Retreive the value of each element entered by '/mqtt_setting'.
String saveParams(AutoConnectAux& aux, PageArgument& args) {
  // The 'where()' function returns the AutoConnectAux that caused
  // the transition to this page.
  mqttserver.value.trim();
  userid.value.trim();
  channelid.value.trim();
  userkey.value.trim();
  apikey.value.trim();
  updateInterval = period.value().substring(0, 2).toInt() * 1000;

  // Echo back saved parameters to AutoConnectAux page.
  String echo = "Server: " + mqttserver.value + "<br>";
  echo += "User ID: " + userid.value + "<br>";
  echo += "Channel ID: " + channelid.value + "<br>";
  echo += "User Key: " + userkey.value + "<br>";
  echo += "API Key: " + apikey.value + "<br>";
  echo += "Update period: " + String(updateInterval / 1000) + " sec.<br>";
  echo += "Use APID unique: " + String(uniqueid.checked == true ? "true" : "false") + "<br>";
  parameters.value = echo;

  return String("");
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  SPIFFS.begin();

  if (uniqueid.checked) {
    config.apid = String("ESP") + "-" + String(GET_CHIPID(), HEX);
    Serial.println("apid set to " + config.apid);
  }

  config.bootUri = AC_ONBOOTURI_HOME;
  config.homeUri = "/";
  config.autoReconnect = true;
  portal.config(config);

  portal.join({ mqtt_setting, mqtt_save });
  portal.on(AUX_SAVE_URI, saveParams);

  Serial.print("WiFi ");
  if (portal.begin()) {
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
  }
  else {
    Serial.println("connection failed:" + String(WiFi.status()));
    while (1) {
      delay(100);
      yield();
    }
  }
}

void readSensor(){
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  creatJSON(t,h);
}

void creatJSON(float t, float h){
    String data = String("{\"TEMPRATURE\":\"") + t + String("\",\"HUMIDITY\":\"") + h + String("\",\"USERID\":\"") + userid.value + String("\",\"DEVICEID\":\"") + String(GET_CHIPID(), HEX) + String("\"}");
    mqttPublish(data);
}

void mqttPublish(String msg) {
  String path = String("/impetus/rtb/") + userid.value + String("/") + channelid.value + MQTT_Device_ID + apikey.value;
  mqttClient.publish(path.c_str(), msg.c_str());
}

void loop() {
  if (updateInterval > 0) {
    if (millis() - lastPub > updateInterval) {
      if (!mqttClient.connected()) {
        mqttConnect();
      }
      readSensor();
      mqttClient.loop();
      lastPub = millis();
    }
  }
  portal.handleClient();
}
