
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include "AESLib.h"

AESLib aesLib;
char cleartext[400];

const char *ssid = "ESPap";
const char *password = "thereisnospoon";
byte aes_key[] = { 0x30, 0x30, 0x30, 0x30, 0x30 };
ESP8266WebServer server(80);
/* Just a little test message.  Go to http://192.168.4.1 in a web browser
   connected to this access point to see it.
*/
void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!");
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void){
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
  
  server.on("/", handleRoot);
  server.on("/sensor",getData);
  
  server.on("/inline", [](){
    server.send(200, "text/plain", "Temperatuer Senosr Data : ");
  });

  

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void){
  server.handleClient();
}

void getData(){
  Serial.println("HTTP POST method called.");
  String data = server.arg("plain");
  Serial.println(data);

  byte enc_iv_dec[N_BLOCK] = { 0, 0, 0, 0, 0};
  sprintf(cleartext, "%s", data.c_str());
  String decrypted = decrypt(cleartext, enc_iv_dec);
  Serial.print("Decrypted text: ");
  Serial.println(decrypted);
}

String decrypt(char * msg, byte iv[]) {
  unsigned long ms = micros();
  int msgLen = strlen(msg);
  char decrypted[msgLen]; // half may be enough
  aesLib.decrypt64(msg, decrypted, aes_key, iv);
  return String(decrypted);
}
