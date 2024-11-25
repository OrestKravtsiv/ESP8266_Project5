// Курсова робота: Визначення часу проходження машинки за допомогою датчика Холла та ESP8266

#include <FS.h> 

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiManager.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>   

WiFiClient wifiClient;
HTTPClient http;

// Задання порта сервера
WiFiServer server(80);

// Створення змінної для Header запита
String header;

// Задання стану виведення налаштувань
String outputState = "off";

// Задання базоового значення для додаткового параметра
char output[21] = "256.256.256.256:9999";

//прапореуь для зберігання інформації
bool shouldSaveConfig = false;

//Коллбек який сповіщує що потрібео зберегти
void saveConfigCallback () {
  // Serial.println("Should save config");
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
// Фільтрація шумів
const int Eps = 3;
const int noiseThreshold = 3;

int Normalization;

int bootTime;
void setup() {

  // WiFi.mode(WIFI_STA);
  Serial.begin(9600);


  if (SPIFFS.begin()) {
    // Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //Якщо все добре і файл створився
      // Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        // Serial.println("opened config file");
        size_t size = configFile.size();
        // визначаємо комірку для розміщення бафера інформації.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        // json.printTo(Serial);
        if (json.success()) {
          // Serial.println("\nparsed json");
          strcpy(output, json["output"]);
        } else {
          // Serial.println("failed to load json config");
        }
      }
    }
  } else {
    // Serial.println("failed to mount FS");
  }

  WiFiManagerParameter custom_output("output", "output", output, 25);


  WiFiManager wifiManager;

  //передаємо дозві на збереження
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //додавання всіх додаткових параметрів
  wifiManager.addParameter(&custom_output);

  pinMode(Thresold_Pin, INPUT);

  // wifiManager.resetSettings();

  bool resWIFI, resHTTP;
  resWIFI = wifiManager.autoConnect("AutoConnectAP");

  if(!resWIFI){
    // Serial.println("Failed to connect");
  } else{
    // Serial.println("Connected to WiFi");

    strcpy(output, custom_output.getValue());

    //зберігання додаткових параметрів
    if (shouldSaveConfig) {
      // Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["output"] = output;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        // Serial.println("failed to open config file for writing");
      }

      // json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
    }

    serverUrl = output;

    server.begin();


    resHTTP = http.begin(wifiClient,String("http://") + serverUrl);// Встановлюємо URL сервера
    if(!resHTTP){
      // Serial.println("Failed to connect to server");
    } else{
      // Serial.println("Connected to server");
    }
  }
      // Serial.println(wifiClient.SSID());
  for(int i = 0; i<10; i++){
    Normalization = analogRead(Hall_pin);
    // Serial.print( String(i) + "Normalization:");
    // Serial.println(Normalization);
  }
  bootTime = millis();
}


void loop() {
  
  // Зчитування аналогового значення з датчика
  int analogVal = analogRead(Hall_pin);
  int analogValNorma= analogVal - Normalization;// Центрування значення
  Serial.println(analogValNorma);
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
    // Serial.print("Час між піками: ");
    // Serial.print(deltaTime);
    // Serial.println(" мс");


    if(deltaTime!=0){
      // Serial.print("Швидкість машинки: ");
      double speed = (car_size/deltaTime)*36;
      // Serial.print(speed,4);
      // Serial.println(" км/год");

      if (WiFi.status() != WL_CONNECTED){
        // Serial.println("ROUTER DISCONNECTED !!!");
        WiFi.reconnect();
        WiFiManager wifiManager;
        wifiManager.autoConnect("AutoConnectAP");
        // Serial.println("Connected to WiFi");
      }
      
      String timeToParse = getTimeRequest();
      int delimeterIndex = timeToParse.indexOf(":\"");
      String realTime = timeToParse.substring(delimeterIndex+2, timeToParse.length()-2);
      realTime.replace("T"," ");
      postDataRequest(deltaTime, speed, realTime);
      // delay(100);
      // for(int i = 0; i<10; i++){
      //   Normalization = analogRead(Hall_pin);
      //   // Serial.print( String(i) + "Normalization:");
      //   // Serial.println(Normalization);
      // }
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
  // Serial.println(String("http://") + serverUrl + String("/data"));

  String jsonToServer = "{\"crossPeriod\": " + String(deltaTime) + ", \"offTime\": \"" + String(realTime) + "\" , \"speed\": " + String(speed, 2) + "}";
  http.addHeader("Content-Type", "application/json");
  // Serial.println(jsonToServer);
  int httpResponseCode = http.POST(jsonToServer);
  if (httpResponseCode > 0) {
      // Serial.print("Data sent successfully: ");
      // Serial.println(httpResponseCode);
    } else {
      // Serial.print("Failed to send data: ");
      // Serial.println(httpResponseCode);
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

