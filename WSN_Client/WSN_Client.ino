#include <ESP8266HTTPClient.h>

#include <pt.h>
#include "ESP8266WiFi.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <elapsedMillis.h>

#include "DHT.h"        // including the library of DHT11 temperature and humidity sensor
#define DHTTYPE DHT11   // DHT 11

#define dht_dpin 5
DHT dht(dht_dpin, DHTTYPE); 

int addr = 0;
int address = 0;
float value;
int firstTimeWrite = 0;

static struct pt pt1, pt2;

elapsedMillis timeElapsed;
unsigned int interval = 13000;
////////////////////////////////////////////

#include "AESLib.h"

AESLib aesLib;

String plaintext = "HELLO WORLD!";

char cleartext[400];
//char ciphertext[400];

// AES Encryption Key
byte aes_key[] = { 0x30, 0x30, 0x30, 0x30, 0x30 };

// General initialization vector (you must use your own IV's in production for full security!!!)
byte aes_iv[N_BLOCK] = { 0, 0, 0, 0, 0 };

// Generate IV (once)
void aes_init() {
  Serial.println("gen_iv()");
  aesLib.gen_iv(aes_iv);
  // workaround for incorrect B64 functionality on first run...
  Serial.println("encrypt()");
  Serial.println(encrypt(strdup(plaintext.c_str()), aes_iv));
}

String encrypt(char * msg, byte iv[]) {  
  int msgLen = strlen(msg);
//  Serial.print("msglen = "); Serial.println(msgLen);
  char encrypted[4 * msgLen]; // AHA! needs to be large, 2x is not enough
  aesLib.encrypt64(msg, encrypted, aes_key, iv);
  Serial.print("Encrypted data = "); Serial.println(encrypted);
  return String(encrypted);
}

String decrypt(char * msg, byte iv[]) {
  unsigned long ms = micros();
  int msgLen = strlen(msg);
  char decrypted[msgLen]; // half may be enough
  aesLib.decrypt64(msg, decrypted, aes_key, iv);
  return String(decrypted);
}

///////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  PT_INIT(&pt1);  // initialise the two
  PT_INIT(&pt2);  
  Serial.println("Setup done");
//  aes_init(); 
}

static int protothread1(struct pt *pt, int interval) {
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
  timestamp = millis(); // take a new timestamp
  writeSensorDataToEEPROM();
  
  PT_END(pt);
}
/* exactly the same as the protothread1 function */
static int protothread2(struct pt *pt, int interval) {
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
  timestamp = millis();
  scanWiFiNetworks();
  
  PT_END(pt);
}



void loop() {
  protothread1(&pt1, 1500); // write sensor data to eeprom
  protothread2(&pt2, 10000); // scan for wifi access point
}



void scanWiFiNetworks(){
  Serial.println("scan start");
  
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      timeElapsed = 0;
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      if(WiFi.SSID(i)=="ESPap"){
        WiFi.begin("ESPap", "thereisnospoon");
 
        while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.println("Connecting..");
          Serial.print("tileElapsed: ");
          Serial.println(timeElapsed);
          if(timeElapsed > interval){
            break;  
          }
        }
               
        if (WiFi.status() == WL_CONNECTED) {
          createJSONObject();  
          WiFi.disconnect(); 
        }
        break;
      }else{
        WiFi.disconnect();  
      }
      delay(10);
      
    }
  }
  Serial.println("");
}

void writeSensorDataToEEPROM(){
//  Serial.println(WiFi.status());
  if(WiFi.status() != WL_CONNECTED){
    float val = getSensorData();
  
    Serial.print(addr);
    Serial.print("\t");
    Serial.println(val);
    EEPROM.put(addr, val);
  
//    addr = addr + 1;
    addr += sizeof(float);
    if (addr == 448) {
      addr = 0;
      EEPROM.commit();
      WiFi.disconnect();
    } 
    firstTimeWrite = 0;
  }
}

float getSensorData(){ 
  dht.begin();
  float t =  dht.readTemperature();  
  return t;
}

void createJSONObject(){
  StaticJsonBuffer<460> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();
 
  JSONencoder["sensorType"] = "Temperature";
  int batchN = 1;
  JSONencoder["batch"] = batchN;
  JsonArray& values = JSONencoder.createNestedArray("values");

  for(int i=0;i<=addr; i += sizeof(float)){
    if (addr == 448) {
      break;
    }
    EEPROM.get(i,value); 

    values.add(value);
    if(JSONencoder["values"].size()==16){
//       JSONencoder.printTo(Serial);
//       Serial.println(); 
       String jsonStr;
       JSONencoder.printTo(jsonStr);
//       doEncrypt(jsonStr);
//       sendDataToBaseStation(doEncrypt(jsonStr));
       sendDataToBaseStation(jsonStr);
       delay(2000);
       JSONbuffer.clear();
       JsonObject& JSONencoder = JSONbuffer.createObject();
 
       JSONencoder["sensorType"] = "Temperature";
       batchN++;
       JSONencoder["batch"] = batchN;
//      JsonArray& addresses = JSONencoder.createNestedArray("addresses");
       JsonArray& values = JSONencoder.createNestedArray("values");
    }else if(i==addr){
//      JSONencoder.printTo(Serial);
//      Serial.println(); 
      String jsonStr;
      JSONencoder.printTo(jsonStr);
      Serial.print("free heap: "); Serial.println(ESP.getFreeHeap());
//      doEncrypt(jsonStr);
//      sendDataToBaseStation(doEncrypt(jsonStr));
      sendDataToBaseStation(jsonStr);
      delay(2000);
      sendDataToBaseStation("end");
    }
  }
  
  int arraySize = JSONencoder["values"].size();   //get size of JSON Array
  Serial.print("\nSize of values array: ");
  Serial.println(EEPROM.length());
  delay(20000);
}


void sendDataToBaseStation(String jsonStr){
  Serial.print("Sending data: ");Serial.println(jsonStr);
  HTTPClient http;  //Object of class HTTPClient
  http.begin("http://192.168.4.1/sensor");
  http.addHeader("Content-Type", "text/plain");
  int httpCode = http.POST(jsonStr);   //Send the request
  String payload = http.getString();                  //Get the response payload  
//  Serial.println(httpCode);   //Print HTTP return code  
  http.end();   //Close connection
}


////////////////////////////////////
String doEncrypt(String data){
  sprintf(cleartext, "%s", data.c_str()); 
  byte enc_iv_enc[N_BLOCK] = { 0, 0, 0, 0, 0}; // iv_block gets written to, provide own fresh copy...
  String encrypted = encrypt(cleartext, enc_iv_enc);
  sprintf(cleartext, "%s", encrypted.c_str());
//  Serial.print("Ciphertext: ");
//  Serial.println(encrypted);  

  byte enc_iv_dec[N_BLOCK] = { 0, 0, 0, 0, 0};
  String decrypted = decrypt(cleartext, enc_iv_dec);
  Serial.print("Decrypted data: ");
  Serial.println(decrypted);  
    
  return encrypted;
}
