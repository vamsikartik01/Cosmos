#include <stdio.h>
#include <ESP8266WebServer.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50

const char* wifi_ssid = "<WIFI_SID>";
const char* wifi_passwd = "<WIFI_PASSWORD>";
const char* hub_reference_id = "<UNIQUE_HUB_ID>";
WebSocketsClient webSocket;
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 5000;
const char* pingText = "pinging";
bool pingStatus = false;
unsigned long currTime;

DynamicJsonDocument textJson(1024);
DynamicJsonDocument textJsonPing(1024);


int init_wifi() {
  int retries = 0;
  Serial.println("Connecting to WiFi ...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_passwd);
  while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY)) {
    retries++;
    delay(WIFI_RETRY_DELAY);
    Serial.print(".");
  }
  return WiFi.status();
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      digitalWrite(2, LOW);
      pingStatus = false;
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      char buffer[1024];
      serializeJson(textJson, buffer);
      Serial.print("Connected text");
      Serial.println((char*)buffer);
      webSocket.sendTXT(buffer);
      digitalWrite(2, HIGH);
      pingStatus = true;
      break;
    case WStype_PONG: // Receive pong response
      Serial.println("Received PONG from server");
      break;
    case WStype_TEXT:
      Serial.println("Received message: " + String((char*)payload));
      DynamicJsonDocument jsonDoc(1024); 
      DeserializationError error = deserializeJson(jsonDoc, String((char*)payload));
      if (error) {
        Serial.print("Parsing failed: ");
        Serial.println(error.c_str());
        return;
      }
      JsonObject jsonObject = jsonDoc.as<JsonObject>();
      for (JsonPair kvp : jsonObject) {
        const char* key = kvp.key().c_str();
        const char* value = kvp.value().as<const char*>();
        jsonDoc[key] = value;
      }
      if(strcmp(jsonDoc["ping"], pingText) == 0)  {
        Serial.print("ping");
        pingStatus = true;
      }
      if(strcmp(jsonDoc["hub_port"], "1") == 0 && strcmp(jsonDoc["value"], "1") == 0)  {
        Serial.print("turning on");
        digitalWrite(4, LOW);
      }
      if(strcmp(jsonDoc["hub_port"], "1") == 0 && strcmp(jsonDoc["value"], "0") == 0)  {
        Serial.print("turning off");
        digitalWrite(4, HIGH);
      }
      if(strcmp(jsonDoc["hub_port"], "2") == 0 && strcmp(jsonDoc["value"], "1") == 0)  {
        Serial.print("turning on");
        digitalWrite(5, LOW);
      }
      if(strcmp(jsonDoc["hub_port"], "2") == 0 && strcmp(jsonDoc["value"], "0") == 0)  {
        Serial.print("turning off");
        digitalWrite(5, HIGH);
      }
      break;
  }
}

void setup(void) {
  Serial.begin(115200);
  Serial.println("MCU Program Started...");

  textJson["ref_id"] = hub_reference_id;
  textJsonPing["ping"] = pingText;

  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  pinMode(2, OUTPUT); //Built in led for status
  digitalWrite(2, LOW);
    
  if (init_wifi() == WL_CONNECTED) {
    Serial.println("WIFI Connetted");
    Serial.print(wifi_ssid);
    Serial.print("--- IP: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.print("Error connecting to: ");
    Serial.println(wifi_ssid);
  }

  webSocket.begin("<HOST_IP>", 2000, "/wsdevice");
  //webSocket.begin("192.168.0.105", 2000, "/wsdevice");
  webSocket.onEvent(webSocketEvent);

}

void loop(void) {
  webSocket.loop();

  currTime = millis();
  if (currTime - lastPingTime >= pingInterval) {
    if (pingStatus) {
      char buffer[1024];
      serializeJson(textJsonPing, buffer);
      Serial.println((char*)buffer);
      webSocket.sendTXT(buffer);
      pingStatus = false;
    } else {
      webSocket.disconnect();
      //webSocket.begin("192.168.0.105", 2000, "/wsdevice");
      webSocket.begin("<HOST_IP>", 2000, "/wsdevice");
      webSocket.onEvent(webSocketEvent);
      pingStatus = true;
    }
    lastPingTime = currTime;
  }

}
