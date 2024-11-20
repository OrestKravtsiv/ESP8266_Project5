// Курсова робота: Визначення часу проходження машинки за допомогою датчика Холла та ESP8266

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiManager.h>

WiFiClient wifiClient;
WiFiManager  wifiManager;

HTTPClient http;

char ssid[] = "TP-LINK_F9970D";
char pass[] = "49401318";
// const char* serverUrl = "http://192.168.0.102:3000/data";


// Підключення необхідних пінів
int Hall_pin = A0; 
int Thresold_Pin = D5;

// Змінні для фіксації часу
unsigned long startTime = 0, endTime = 0;
int max_val = -1024, min_val = 1024;
unsigned long max_time = 0, min_time = 0;

// Гістерезис для фільтрації шумів
const int Eps = 3;
const int noiseThreshold = 3;

int bootTime;
void setup() {
  WiFi.mode(WIFI_STA);
  wifiManager.resetSettings();
  pinMode(Thresold_Pin, INPUT);
  Serial.begin(9600);

  bool resWiFI, resHTTP;
  resWIFI = wifiManager.autoConnect("AutoConnectAP");
  if(!resWIFI){
    Serial.println("Failed to connect");
  } else{
    Serial.println("Connected to WiFi");
    resHTTP = http.begin(wifiClient,serverUrl);// Встановлюємо URL сервера
    if(!resHTTP){
      Serial.println("Failed to connect to server");
    } else{
      Serial.println("Connected to server");
      bootTime = millis();
    }
  }
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
    Serial.print(0.135/(deltaTime/1000.0),4);
    Serial.println(" м/с");
    };
    // Скидання змінних для нового вимірювання
    max_val = -1024;
    min_val = 1024;
    max_time = 0;
    min_time = 0;
  }

  delay(1); // Невелика затримка для стабільності вимірювання
}
