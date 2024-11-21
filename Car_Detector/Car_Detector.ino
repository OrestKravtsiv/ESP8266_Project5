// Курсова робота: Визначення часу проходження машинки за допомогою датчика Холла та ESP8266

#include <FS.h> 
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>   

WiFiClient wifiClient;
HTTPClient http;

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String outputState = "off";

// Assign output variables to GPIO pins
char output[21] = "256.256.256.256:9999";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


char ssid[] = "TP-LINK_F9970D";
char pass[] = "49401318";
const char* serverUrl = "0.0.0.0:0";


// Підключення необхідних пінів
int Hall_pin = A0; 
int Thresold_Pin = D5;

// Змінні для фіксації часу
unsigned long startTime = 0, endTime = 0;
int max_val = -1024, min_val = 1024  ;
unsigned long max_time = 0, min_time = 0;
double car_size = 13.5;
// Гістерезис для фільтрації шумів
const int Eps = 3;
const int noiseThreshold = 3;

int bootTime;
void setup() {

  // WiFi.mode(WIFI_STA);
  Serial.begin(9600);



  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(output, json["output"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  WiFiManagerParameter custom_output("output", "output", output, 25);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_output);









  pinMode(Thresold_Pin, INPUT);





  wifiManager.resetSettings();


  bool resWIFI, resHTTP;
  resWIFI = wifiManager.autoConnect("AutoConnectAP");


  if(!resWIFI){
    Serial.println("Failed to connect");
  } else{
    Serial.println("Connected to WiFi");

    strcpy(output, custom_output.getValue());

    //save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["output"] = output;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
    }

    serverUrl = output;

    server.begin();


    resHTTP = http.begin(wifiClient,String("http://") + serverUrl);// Встановлюємо URL сервера
    if(!resHTTP){
      Serial.println("Failed to connect to server");
    } else{
      Serial.println("Connected to server");
      bootTime = millis();
    }
  }
      // Serial.println(wifiClient.SSID());
}
int Normalization;

void loop() {

  
  // Зчитування аналогового значення з датчика
  int analogVal = analogRead(Hall_pin);
  int analogValNorma;
  if(millis()<100+bootTime){
    Normalization = analogVal;
    Serial.print("Normalization:");
    Serial.println(Normalization);
  }
    analogValNorma = analogVal - Normalization; // Центрування значення
    // Serial.println(analogValNorma);
  
  // Перевірка на максимуми та мінімуми
  if (analogValNorma > max_val && analogValNorma > Eps) {
    max_val = analogValNorma;
    max_time = millis();
  } 
  if (analogValNorma < min_val && analogValNorma < -Eps) {
    min_val = analogValNorma;
    min_time = millis();
  }

  // Фіксація проходження піків
  if (labs(analogValNorma) < noiseThreshold && max_time > 0 && min_time > 0) {
    // Визначення часу між піками
    unsigned long deltaTime = labs(max_time - min_time);

    // Виведення результату
    Serial.print("Час між піками: ");
    Serial.print(deltaTime);
    Serial.println(" мс");


    if(deltaTime!=0){
      Serial.print("Швидкість машинки: ");
      double speed = (car_size/deltaTime)*36;
      Serial.print(speed,4);
      Serial.println(" км/год");

      if (WiFi.status() != WL_CONNECTED){
        Serial.println("ROUTER DISCONNECTED !!!");
        WiFi.reconnect();
        WiFiManager wifiManager;
        wifiManager.autoConnect("AutoConnectAP");
        Serial.println("Connected to WiFi");
      }
      
      String timeToParse = getTimeRequest();
      int delimeterIndex = timeToParse.indexOf(":\"");
      String realTime = timeToParse.substring(delimeterIndex+2, timeToParse.length()-2);
      realTime.replace("T"," ");
      postDataRequest(deltaTime, speed, realTime);
      getTimeRequest();
    };
    // Скидання змінних для нового вимірювання
    max_val = -1024;
    min_val = 1024;
    max_time = 0;
    min_time = 0;
  }

  delay(1); // Невелика з атримка для стабільності вимірювання
}


void postDataRequest(unsigned long deltaTime, double speed, String realTime){
  
  http.begin(wifiClient,String("http://") + serverUrl + String("/data"));
  Serial.println(String("http://") + serverUrl + String("/data"));



  String jsonToServer = "{\"crossPeriod\": " + String(deltaTime) + ", \"offTime\": \"" + String(realTime) + "\" , \"speed\": " + String(speed, 2) + "}";
  http.addHeader("Content-Type", "application/json");
  Serial.println(jsonToServer);
  int httpResponseCode = http.POST(jsonToServer);
  if (httpResponseCode > 0) {
      Serial.print("Data sent successfully: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Failed to send data: ");
      Serial.println(httpResponseCode);
    }
}


String getTimeRequest(){
  http.begin(wifiClient,String("http://") + serverUrl + String("/time"));
  int httpResponseCode = http.GET();
  if (httpResponseCode) {
    String payload = http.getString();
    int delimeterIndex = payload.indexOf(":\"");
    String realTime = payload.substring(delimeterIndex, payload.length()-2);
    return realTime;
  }
  return "";
}

