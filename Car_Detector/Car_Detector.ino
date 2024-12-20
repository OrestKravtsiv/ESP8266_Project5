// Курсова робота: Визначення часу проходження машинки за допомогою датчика Холла та ESP8266
#define PI 3.1415926535897932384626433832795

#include <FS.h> 

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiManager.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>   



template <int order>
class LowPassFilter
{
  private:
    double a[order+1];
    double b[order+1];
    double omega0;
    double dt;
    bool adapt;
    double tn1 = 0;
    double x[order+1]; // Raw values
    double y[order+1]; // Filtered values
  public:  
    LowPassFilter(double f0, double fs, bool adaptive){
      // f0: cutoff frequency (Hz)
      // fs: sample frequency (Hz)
      // adaptive: boolean flag, if set to 1, the code will automatically set the sample frequency based on the time history.
      
      omega0 = 2*PI*f0;
      dt = 1.0/fs;
      adapt = adaptive;
      tn1 = -dt;
      for(int k = 0; k < order+1; k++){
        x[k] = 0;
        y[k] = 0;        
      }
      setCoef();
    }

    void PolynomialMultiply(double A[], int nA, double B[], int nB){ // array1,length of array1, array2(always size of 3, except if order is odd)
        double result[nB+nA-1]; 

        for(int i = 0; i<nB+nA-1; i++)
          result[i] = 0.0;

        for(int i = 0; i<nA; i++)
          for(int j = 0; j<nB; j++)
            result[i+j]+=A[i]*B[j];
        
        for(int i = 0; i<nB+nA-1; i++)
          A[i] = result[i];
      return;
    }
    
    void setCoef(){
      if(adapt){
        double t = micros()/1.0e6;
        dt = t - tn1;
        tn1 = t;
      }
      
      double alpha = omega0*dt;
      double alphaSq = alpha*alpha;
      
      a[0] = 1;
      int length_a = 1;
      if(order%2 == 1){
        a[0]=alpha+2;
        a[1]=alpha-2;
        length_a = 2;
      }
      for(int k = 1; k<= (order - (order%2))/2; k++){
        double coef = 4.0*alpha*cos((2.0*k+order-1.0)/(2.0*order)*PI);
        double element_wo_coef = 4+alphaSq;
        double Bn[3] = {element_wo_coef - coef,-8.0 + 2.0*alphaSq,element_wo_coef+coef};
        PolynomialMultiply(a,length_a,Bn,3);
        length_a+=2;
      }
      double a0 = a[0];

      b[0] = alpha/a0;
      b[1] = alpha/a0;
      double temp[2] = {alpha,alpha};
      for(int i = 2; i<=order; i++){
        PolynomialMultiply(b,i,temp,2);
      }

      for(int i = 0; i<=order; i++){
        a[i]/=-a0;
      }

    }

    double filt(double xn){
      // Provide me with the current raw value: x
      // I will give you the current filtered value: y
      y[0] = 0.0;
      x[0] = xn;
      // Compute the filtered values
      for(int k = 0; k < order; k++){
        y[0] += a[k+1]*y[k+1] + b[k]*x[k];
      }
      y[0] += b[order]*x[order];
      for(int k = order; k > 0; k--){
        y[k] = y[k-1];
        x[k] = x[k-1];
      }
  
      // Return the filtered value    
      return y[0];
    }
};





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

const char* serverUrl = "0.0.0.0:0";


// Підключення необхідних пінів
int Hall_pin = A0; 
int Thresold_Pin = D5;

// Змінні для фіксації часу
double prev_val = -1024,prev_prev_val = -1024;
// unsigned long first_time = 0, last_time = 0;
unsigned long time_array[2] = {0,0};
double val_array[2] = {0,0};
double car_size = 13.5;
// Фільтрація шумів
const int Eps = 5;
const int noiseThreshold = 3;

double Normalization = 0;

int bootTime;

int delay_time = 1;

LowPassFilter<3> lp3_0_5(5,1000.0/delay_time,false);
LowPassFilter<3> lp3_0_5_Norm(5,1000.0/delay_time,false);

// double Normalize(double value_to_normalize ){}

void setup() {
  // WiFi.mode(WIFI_STA);
  Serial.begin(115200);

  //SPIFFS.format();

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
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  WiFiManagerParameter custom_output("output", "output", output, 25);

  WiFiManager wifiManager;

  //передаємо дозві на збереження
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //додавання всіх додаткових параметрів
  wifiManager.addParameter(&custom_output);

  pinMode(Thresold_Pin, OUTPUT);

  // wifiManager.resetSettings();

  bool resWIFI, resHTTP;
  resWIFI = wifiManager.autoConnect("AutoConnectAP");

  if(!resWIFI){
    Serial.println("Failed to connect");
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
        Serial.println("failed to open config file for writing");
      }

      // json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
    }

    serverUrl = output;
    server.begin();
    resHTTP = http.begin(wifiClient,String("http://") + serverUrl);// Встановлюємо URL сервера
    if(!resHTTP){
      Serial.println("Failed to connect to server");
    } else{
      Serial.println("Connected to server");
    }
  }
      // Serial.println(wifiClient.SSID());
  // for(int i = 0; i<100; i++){
  //   // delay(3000);
  //   Normalization = analogRead(Hall_pin) + 60;
  //   Serial.print("Normalization:");
  //   Serial.println(Normalization);
  // }
  bootTime = millis();
}

int Norm = 0;
bool peak_flag = false; //counter for two peaks
double NormMax = 0;
double analogValFiltered,analogValNormaFiltered3_0_5,analogValNorma; 
int analogVal;
void loop() {
  analogVal = analogRead(Hall_pin);
  
  if (Norm < 2000){
    Normalization = lp3_0_5_Norm.filt(analogVal);//
    // Serial.print(Normalization);
    // Serial.print(" ");
    Norm++;
    if(Norm > 1100 && Normalization>NormMax){NormMax = Normalization;}
  }
  // Зчитування аналогового значення з датчика
  analogValFiltered = lp3_0_5.filt(analogVal);// Центрування значення lp3_0_5.filt(analogVal)
  
  analogValNormaFiltered3_0_5 = analogValFiltered- NormMax;
  analogValNorma = analogVal- NormMax;
  Serial.println(analogValNormaFiltered3_0_5);

  // Serial.print(" ");
  // Serial.println(analogValNorma);
  // Перевірка на 2 максимуми

    if (val_array[peak_flag] < prev_val && prev_prev_val < prev_val && analogValNormaFiltered3_0_5 < prev_val && analogValNormaFiltered3_0_5 > Eps) {
      time_array[peak_flag] = millis() - delay_time;
      val_array[peak_flag] = prev_val;
      // peak_flag = !peak_flag;
      // digitalWrite(Thresold_Pin,LOW);
      

    }
    else if(time_array[peak_flag] != 0 && analogValNormaFiltered3_0_5 < Eps){
      peak_flag = !peak_flag;
    } 
    // else{
      
    // }
    prev_prev_val = prev_val;

    prev_val = analogValNormaFiltered3_0_5;





  // Фіксація проходження піків
  if (analogValNormaFiltered3_0_5 < noiseThreshold && time_array[0] != 0 && time_array[1] != 0) {
    // Визначення часу між піками
    unsigned long deltaTime = labs(time_array[1] - time_array[0]);

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



      delay(100);
      for(int i = 0; i<10; i++){
        Normalization = analogRead(Hall_pin);
        Serial.print( String(i) + "Normalization:");
        Serial.println(Normalization);
      }
    };
    // Скидання змінних для нового вимірювання
    prev_val = 0;
    prev_prev_val = 0;
    time_array[0] = 0;
    time_array[1] = 0;
  }

  delay(delay_time); // Невелика з атримка для стабільності вимірювання
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

