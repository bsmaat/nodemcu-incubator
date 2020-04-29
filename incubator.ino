#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"

// Uncomment one of the lines below for whatever DHT sensor type you're using!
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321


/*Put your SSID & Password*/
const char* ssid = "PLUSNET-7CHX";  // Enter SSID here
const char* password = "d9e82bd692";  //Enter Password here

ESP8266WebServer server(80);
SSD1306Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h

// DHT Sensor
uint8_t DHTPin = D3; 
int DHTVccPin = D6;

int LightPin = D5;

// Initialize DHT sensor.

DHT dht(DHTPin, DHTTYPE);                

float Temperature;
float Humidity;

int _failedMeasures = 0;
int _consecutiveFailedMeasures = 0;

int _maxFailedMeasures = 20;
int _failedMeasuresToRestart = 10;
float _lowestTemp = 1000, _highestTemp = 0;
float _lowestHumidity = 1000, _highestHumidity = 0;

int period = 10000;
unsigned long last_time = 0;
const float targetTemp = 37.4; // 38.8-1.2;

bool _stateOn = true;

String errorMessage;

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(DHTVccPin, OUTPUT);
  pinMode(DHTPin, INPUT);
  pinMode(LightPin, OUTPUT);

  // turn on DHT22
  digitalWrite(DHTVccPin, HIGH);
  
  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  delay(100);
  introScreen();
  delay(1000);
  
  dht.begin();              

  Serial.println("Connecting to ");
  Serial.println(ssid);

  //connect to your local wi-fi network
  WiFi.begin(ssid, password);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");

}

void updateLight() {
  if (!_stateOn) {
    lightOff();
    return;
  }
  
  if (Temperature < targetTemp) {
    lightOn();
  }
  else {
    lightOff();
  }
}

void lightOn() {
  Serial.println("Light on");
  digitalWrite(LightPin, HIGH);
}

void lightOff() {
  Serial.println("Light off");
  digitalWrite(LightPin, LOW);
}

void loop() {
  
  server.handleClient();

  if(millis() > last_time + period){
      last_time = millis();
      Serial.println("Updating readings...");
      if (!updateReadings()) {
        _failedMeasures++;
        _consecutiveFailedMeasures++;
      }
      else {
        _consecutiveFailedMeasures = 0;
      }

      if (_consecutiveFailedMeasures > _failedMeasuresToRestart) {
        // try restart
        DHT_restart();
      }

      if (_consecutiveFailedMeasures > _maxFailedMeasures) {
        lightOff();
        errorMessage = "Too many retries";
        _stateOn == false;
      }

      Serial.println("Updating light...");
      updateLight();

      displayTempHumid();
  }
 
}

// Physically power recycle the DHT using a relay/GPIO
void DHT_restart() {
  digitalWrite(DHTVccPin, LOW);
  digitalWrite(DHTPin, LOW);
  delay(1000);
  digitalWrite(DHTVccPin, HIGH);
  digitalWrite(DHTPin, HIGH);
  delay(1000);
}

bool updateReadings() {
  float temp;
  float humidity;

  bool success = false;
  
  temp = dht.readTemperature(); // Gets the values of the temperature
  humidity = dht.readHumidity(); // Gets the values of the humidity 

  for (int i = 0; i < 5; i++) {
    if (isnan(temp) || isnan(humidity)) {
      // error, need to redo
      delay(2000);
      temp = dht.readTemperature(); // Gets the values of the temperature
      humidity = dht.readHumidity(); // Gets the values of the humidity 
    }
    else {
      success = true;
      break;
    }
  }

  if (success) {
    Temperature = temp;
    Humidity = humidity;
    Serial.print("Temperature = ");
    Serial.print(temp);
    Serial.print(" *C ");
    Serial.print("    Humidity = ");
    Serial.print(humidity);
    Serial.println(" % ");

    if (temp < _lowestTemp) {
      _lowestTemp = temp;
    }

    if (temp > _highestTemp) {
      _highestTemp = temp;
    }

    if (humidity < _lowestHumidity) {
      _lowestHumidity = humidity;
    }

    if (humidity > _highestHumidity) {
      _highestHumidity = humidity;
    }

    // just in case we go too high a temperature
    if (temp > targetTemp + 0.5) {
      _stateOn = false;
      errorMessage = "Exceeded temp";
    }
  }
  else {
    Serial.println("Unsuccessful reading");
  }

  return success;
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float Temperaturestat,float Humiditystat){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>ESP8266 Weather Report</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<div id=\"webpage\">\n";
  ptr +="<h1>ESP8266 NodeMCU Weather Report</h1>\n";
  
  ptr +="<p>Temperature: ";
  ptr +=Temperaturestat;
  ptr +="C</p>";
  ptr +="<p>Humidity: ";
  ptr +=Humiditystat;
  ptr +="%</p>";
  
  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

void handle_OnConnect() {
  updateReadings();
  server.send(200, "text/html", SendHTML(Temperature,Humidity)); 
}

void introScreen() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Incubator");
  display.display();
}

void displayTempHumid(){
  
  display.clear();

  display.setFont(ArialMT_Plain_16);
  
  if (!_stateOn) {
    // show error
    display.drawString(0, 0, errorMessage); 
    display.display();
    return;
  }
  
  display.drawString(0, 0, "Humidity: " + String(Humidity) + "%\t"); 
  display.drawString(0, 16, "Temp: " + String(Temperature) + "C"); 
  // display.drawStringMaxWidth(0, 32, 128, "Low: " + String(_lowestTemp) + "C, " + String(_lowestHumidity) + "%\t");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 32, "Low: " + String(_lowestTemp) + "C, " + String(_lowestHumidity) + "%\t");
  display.drawString(0, 42, "High: " + String(_highestTemp) + "C, " + String(_highestHumidity) + "%\t"); 
  // display.setFont(ArialMT_Plain_8);
  String s = "Fails: " + String(_failedMeasures);
  if (_stateOn) {
    s = s + " (On)";
  }
  else {
    s = s + " (Off)";
  }
  
  display.drawString(0, 52, s);
  display.display();
}
