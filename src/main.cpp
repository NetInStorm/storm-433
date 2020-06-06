#include <Arduino.h> // I mean... I don't to write everything from scratch ;(
#include <cppQueue.h>

/* Radio Stuff */
#include <RH_NRF905.h>
#include <RHMesh.h>

/* Wi-Fi and WebUI Stuff */
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"

/* TODO: Load from FS */
const char *ssid = "InStorm-20";
const char *password = "demo_password";

AsyncWebServer server(80); // Setting up API Web-server

RH_NRF905 nrf905(5, 4, 15); // Initializing NRF905 Radio Driver
RHMesh mesh(nrf905, 20); // ADDR fix

IPAddress local_IP(192,168,1,1); // 192.168.1.x, cuz 10.x.x.x would be later used in modern versions in routing.
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

uint8_t kl_buf[RH_MESH_MAX_MESSAGE_LEN];
Queue	q(sizeof(kl_buf), 10, LIFO);	// Instantiate queue

void setup() {
  delay(1000); // Pre-start delay
  Serial.begin(115200);
  Serial.println();
  SPIFFS.begin(); // probably need to replace it with LittleFS, but esp8266 support for it isn't YET rly ready... so... nvm.

  String nrf_state = String("PRE_INIT");
 
  /* Setting up AP */
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed!");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP is: ");
  Serial.println(myIP);

  /* Setting up HTTP server */
  
  server.on("/random", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("[Wi-Fi] Request on /random");
    request->send(200, "text/plain", String(random(1000))); // Just testing programmability of a server. DELETE later. TODO
  });

  server.on("/api/v1/send", HTTP_POST, [](AsyncWebServerRequest *request){
    uint8_t addr;
    u_char *buf;
    String data;
    Serial.println("[Wi-Fi] Request on /api/v1/send");

    if (request->hasParam("addr", true) && request->hasParam("body", true)) {
        Serial.println("[Wi-Fi/RadioLogic] Params are present");
        addr = request->getParam("addr", true)->value().toInt();
        Serial.print("[Wi-Fi/RadioLogic] Sending message to ");
        Serial.println(addr);
        data = request->getParam("body", true)->value().c_str();
        Serial.print("[Wi-Fi/RadioLogic] Message is ");
        Serial.println(data);
        strcpy((char*) buf, data.c_str()); 
        Serial.println("[Wi-Fi/RadioLogic] Message encoded and ready to send");
        if (mesh.sendtoWait(buf, sizeof(buf), addr) == RH_ROUTER_ERROR_NONE) {
          Serial.println("[Wi-Fi/RadioLogic] Sending failed");
          request->send(500, "text/plain", "fail");
        } else {
          Serial.println("[Wi-Fi/RadioLogic] Sending success");
          request->send(200, "text/plain", "OK");
        }
    } else {
       request->send(500, "text/plain", "fail");
    };
  });

  server.on("/api/v1/poll", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("[Wi-Fi] Request on /api/v1/poll");
    uint8_t l_buf[RH_MESH_MAX_MESSAGE_LEN];
    if (q.pop(&l_buf)) {
      std::string s(l_buf, l_buf + sizeof(l_buf));
      request->send(200, "text/plain", s.c_str());
    } else {
      request->send(500, "text/plain", String("no_new_msg"));
    }
  });


  server.on("/api/v1/name", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("[Wi-Fi] Request on /api/v1/name");
    request->send(200, "text/plain", String(ssid));
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html"); // Serving main Web-UI

  server.begin();
  Serial.println("Server started.");

  /* Setting up NRF905 Logic */
  Serial.print("Setting nrf905 ... ");
  if (nrf905.init()) {
    Serial.println("Ready");
    nrf_state = "RAD-INIT";
  } else {
    Serial.println("Failed!\n");
    Serial.println("Resetting now!");
  }

  Serial.print("Setting up mesh ... ");
  if (mesh.init()) {
    Serial.println("Ready");
    nrf_state = "MESH-INIT";
  } else {
    Serial.println("Failed!\n");
    Serial.println("Resetting now!");
  }

  nrf905.setRF(RH_NRF905::TransmitPower10dBm); // Setting up power to max, cuz: "why not?"
}

void loop() {
  uint8_t l_buf[RH_MESH_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(l_buf);
  uint8_t from;

  if (mesh.recvfromAck(l_buf, &len, &from))
  {
    Serial.print("[RadioLogic] Message from ");
    Serial.println(from);
    q.push(l_buf);
  }
}
