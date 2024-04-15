#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <elapsedMillis.h>
#include <WiFi.h>
#include "secrets.h"
#include "ThingSpeak.h"
#include <BlynkSimpleEsp32.h>

//Blynk Library
#define BLYNK_TEMPLATE_ID "TMPL6-qMxfxcM"
#define BLYNK_TEMPLATE_NAME "test"
#define BLYNK_AUTH_TOKEN "PVQxyBwaW3gc8BxYJNjgZX_FtsGtDOU8"
int mod = 0;


// Configuration lcd
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Component pins
const int waterLevelSensorPin = 2;  // default 2 // ESP32
//int flow1 = 36;
//int flow2 = 13;
const int relayV1Pin = 25;  // ESP32
const int relayV2Pin = 27;  // ESP32
const int relayPoPin = 16;  // ESP32
const int relayMoPin = 4;   // ESP32


// Confiration DS18B20 Sensor
OneWire ds(15);  // ESP32
float celsius;

// Kebutuhan PPM
int kebutuhanPPM[] = {
  0, 20, 40, 60, 80, 100, 120, 140, 160, 180, 200,
  220, 240, 260, 280, 300, 320, 340, 360, 380, 400, 420
};

// Variable flow sensors
//volatile long pulse1;
//float volume1;
//unsigned long lastPulseTime1 = 0;       // Waktu terakhir pulse bertambah
//const unsigned long resetTime1 = 2000;  // Waktu dalam milidetik (2 detik)

//volatile long pulse2;
//float volume2;
//unsigned long lastPulseTime2 = 0;       // Waktu terakhir pulse bertambah
//const unsigned long resetTime2 = 2000;  // Waktu dalam milidetik (2 detik)



#define TdsSensorPin 33  // ESP32
#define VREF 3.3
#define SCOUNT 30

int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
int waterLevelSensorValue;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25;

elapsedMillis LCDMillis;
unsigned long LCDInter = 2000;

elapsedMillis ReadSensorMillis;
unsigned long RDSensorInter = 1000;


// TDS Sensor
// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

//------------thingspeak and Blynk----- //
char auth[] = BLYNK_AUTH_TOKEN; // BLYNK token
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password
//BlynkTimer timer; // Set Time 
//WidgetLED led1(V0); // Datastream V0
WidgetLCD lcd1(V1); // Data Stream V1
int keyIndex = 0;           // your network key Index number (needed only for WEP)
WiFiClient client;

unsigned long myChannelNumber = SECRET_CH_ID;
const char* myWriteAPIKey = SECRET_WRITE_APIKEY;

// Initialize our values
int number1 = 0;
int number2 = random(0, 100);
int number3 = random(0, 100);
int number4 = random(0, 100);
String myStatus = "";

elapsedMillis sendMillis;
unsigned long sendInter = 10000;

void sendiot() {

  // Connect or reconnect to WiFi
  // set the fields with the values
  ThingSpeak.setField(1, tdsValue);
  ThingSpeak.setField(2, celsius);
   ThingSpeak.setField(3, mod);
  // ThingSpeak.setField(4, );

  // if(number1 > number2){
  //   myStatus = String("field1 is greater than field2");
  // }
  // else if(number1 < number2){
  //   myStatus = String("field1 is less than field2");
  // }
  // else{
  //   myStatus = String("field1 equals field2");
  // }

  // set the status
  ThingSpeak.setStatus(myStatus);

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200) {
    Serial.println("Channel update successful.");
  } else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }

  // change the values
  number1++;
  if (number1 > 99) {
    number1 = 0;
  }
  number2 = random(0, 100);
  number3 = random(0, 100);
  number4 = random(0, 100);
}


void intWifi() {

  while (!Serial) {
    ;  // wait for serial port to connect. Needed for Leonardo native USB port only
  }

  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);  // Initialize ThingSpeak

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, pass);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);
    }
    Serial.println("\nConnected.");
  }
}




void tdsSensor() {
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) {
      analogBufferIndex = 0;
    }
  }

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];

      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4096.0;

      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
      float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
      //temperature compensation
      float compensationVoltage = averageVoltage / compensationCoefficient;

      //convert voltage value to tds value
      tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;

      //Serial.print("voltage:");
      //Serial.print(averageVoltage,2);
      //Serial.print("V   ");
      // Serial.print("TDS Value:");
      // Serial.print(tdsValue, 0);
      // Serial.println(" ppm");
      // lcd.setCursor(0, 1);
      // lcd.print("PPM : " + String(tdsValue));

      //Blynk.virtualWrite(V1, tdsValue);
    }
  }
}

// DS (Temperature) Sensor
void dsSensor(void) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

  if (!ds.search(addr)) {
    ds.reset_search();
    delay(250);
    return;
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return;
  }
  Serial.println();

  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Not Found");
      delay(2000);
      lcd.clear();
      Serial.println("Device is not a DS18x20 family device.");
      return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);  // start conversion, with parasite power on at the end
  delay(1000);
  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);  // Read Scratchpad

  for (i = 0; i < 9; i++) {
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3;  // 9 bit resolution default
    if (data[7] == 0x10) {
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw & ~7;       // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3;  // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1;  // 11 bit res, 375 ms
  }
  celsius = (float)raw / 16.0;
  // Serial.print("Temperature = ");
  // Serial.print(celsius);
  // Serial.print(" Celsius ");
  // lcd.setCursor(0, 0);
  // lcd.print("SUHU: " + String(celsius) + " C");
}
//void volFlow1() {
  //volume1 = 2.66 * pulse1;
  // Serial.println("ML air = " + String(volume1) + "ML");
  // Serial.println("pulse  = " + String(pulse1));
  //if (millis() - lastPulseTime1 >= resetTime1) {
    //pulse1 = 0;  // Reset nilai pulse menjadi 0
    //Serial.println("pulse direset");
  //}
//}
//void volFlow2() {
  //volume2 = 2.66 * pulse2;
  // Serial.println("ML air = " + String(volume2) + "ML");
  // Serial.println("pulse  = " + String(pulse2));

  // Cek apakah pulse tidak bertambah dalam 2 detik terakhir
  //if (millis() - lastPulseTime2 >= resetTime2) {
    //pulse2 = 0;  // Reset nilai pulse menjadi 0
    //Serial.println("pulse direset");
  //}
//}

//void increase1() {
  //pulse1++;
  //lastPulseTime1 = millis();  // Perbarui waktu terakhir pulse bertambah
  // digitalWrite(relayV1Pin, LOW);
  // digitalWrite(relayV2Pin, LOW);

  //if (pulse1 == 19) {
    //digitalWrite(relayV1Pin, HIGH);
    //digitalWrite(relayV2Pin, HIGH);
  //}

  // Cek apakah pulse tidak bertambah dalam 2 detik terakhir

  // if (pulse1 == 225){
  //   pulse1 = 0;


  //   //19    = 50   ML
  //   //225   = 600  ML
  //   //1879  = 5000 ML
  // }
//}

//void increase2() {
  //pulse2++;
  //lastPulseTime2 = millis();  // Perbarui waktu terakhir pulse bertambah
  // digitalWrite(relayPoPin, LOW);
  //if (pulse2 >= 1879) {
    //digitalWrite(relayPoPin, HIGH);
    //Serial.print("MATI");
    ////mode = 0;
  //}


  // if (pulse2 == 1879){
  //   pulse2 = 0;


  //   //19    = 19   ML
  //   //225   = 600  ML
  //   //1879  = 5000 ML
  // }
//}



void setup() {
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass); // begin the Blynk
  //timer.setInterval(1000L, LiquidLevelCheck);
  intWifi();

  pinMode(TdsSensorPin, INPUT);
  pinMode(waterLevelSensorPin, INPUT_PULLUP);
  //pinMode(flow1, INPUT);
  //pinMode(flow2, INPUT);

  pinMode(relayV1Pin, OUTPUT);
  pinMode(relayV2Pin, OUTPUT);
  pinMode(relayPoPin, OUTPUT);
  pinMode(relayMoPin, OUTPUT);


  // Relay Low

  digitalWrite(relayV1Pin, HIGH);
  digitalWrite(relayV2Pin, HIGH);
  digitalWrite(relayPoPin, HIGH);
  digitalWrite(relayMoPin, HIGH);

  //attachInterrupt(digitalPinToInterrupt(flow1), increase1, RISING);
  //attachInterrupt(digitalPinToInterrupt(flow2), increase2, RISING);



  lcd.begin();
  lcd.backlight();
}

void print1() {

  lcd.setCursor(0, 0);
  lcd.print("mode :");
  lcd.setCursor(6,0);
  lcd.print(mod);


  lcd.setCursor(0, 1);
  lcd.print("Tem: " + String(celsius) + " C");
  Blynk.virtualWrite(V4, celsius);  
  lcd.setCursor(0, 2);
  lcd.print("PPM : " + String(tdsValue));
  Blynk.virtualWrite(V3, tdsValue);
}

// void print2() {

//   lcd.setCursor(0, 0);
//   lcd.print("Air: Kosong|MOD NUT");

//   lcd.setCursor(0, 1);
//   lcd.print("Air " + String(volume1) + "ML");

//   lcd.setCursor(0, 2);
//   lcd.print("nutrisi" + String(volume2) + " mL/s");

//   lcd.setCursor(0, 3);
//   lcd.print("PPM : " + String(tdsValue));
// }

void loop() {
  Blynk.run(); // Running Blynk
  //timer.run();
  int waterLevelSensorValue = digitalRead(waterLevelSensorPin);
  print1() ;
  //print2() ;
  if (ReadSensorMillis >= RDSensorInter) {
    tdsSensor();
    dsSensor();
    ReadSensorMillis = 0;
  }

  if (tdsValue <= 100 && waterLevelSensorValue == HIGH && celsius <= 29) {
    mod = 1;
    Serial.println("mode 1 tds LOW");
    Serial.println("ralayV2V1 menyala");
    lcd.setCursor(0, 3);
    lcd.print("TDS LOW V2V1 ON"); 
    lcd1.print(0,0, "KONDISI");
    lcd1.print(0,1,"TDS LOW V2V1 ON" ); 
    delay(300);
    print1();
  }
  if (waterLevelSensorValue == LOW && tdsValue >= 100 && celsius <= 29) {
    mod = 2;
    Serial.println("mode 2 waterlvl LOW");
    Serial.println("relay pompa menyaka");
    lcd.setCursor(0, 3);
    lcd.print("AIR KOSONG POM ON");
    lcd1.print(0,0, "KONDISI");
    lcd1.print(0,1,"AIR KOSONG POM ON" ); 
    delay(300);
  }
  if (waterLevelSensorValue == LOW && tdsValue <= 100 && celsius <= 29) {
    mod = 3;
    Serial.println("mode 3 tds,water lvl LOW ");
    Serial.println("semua relay menyala");
    lcd.setCursor(0, 3);
    lcd.print("TDS LOW AIR kosong ");
    lcd1.print(0,0, "KONDISI");
    lcd1.print(0,1,"TDS LOW AIR kosong " ); 
    delay(300);
  } 
  if (waterLevelSensorValue == HIGH && tdsValue >= 100 && celsius <= 29) {
    mod = 4;
    Serial.println("mode 4 semua normal ");
    Serial.println("semua relay mati");
    lcd.setCursor(0, 3);
    lcd.print("semua normal"); 
    lcd1.print(0,0, "KONDISI");
    lcd1.print(0,1,"semua normal" ); 
    delay(300);
  }

  else{
    
  }

  if (sendMillis >= sendInter) {
    sendiot();
    sendMillis = 0;
  }

  //delay (15000);
  Serial.print("TDS Value:");
  Serial.print(tdsValue, 0);
  Serial.println(" ppm");
  Serial.println();
  delay(2000);
  Serial.print("Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius ");
  Serial.println();
  delay(2000);
  //Serial.println("ML air 1 = " + String(volume1) + "ML");
  //Serial.println("pulse  = " + String(pulse1));
  //Serial.println();
  // delay(2000);
  //Serial.println("ML air 2 = " + String(volume2) + "ML");
  //Serial.println("pulse  = " + String(pulse2));
  //Serial.println();
  //delay(1000);

  switch (mod) {
    case 1:
      lcd.clear();
      lcd1.clear();
      digitalWrite(relayV1Pin, LOW);
      digitalWrite(relayV2Pin, LOW);
      digitalWrite(relayPoPin, HIGH);
      digitalWrite(relayMoPin, HIGH);
      //volFlow1();
      delay(5000);
      //lcd.clear();
      // digitalWrite(relayV1Pin, HIGH);
      // digitalWrite(relayV2Pin, HIGH);
      // digitalWrite(relayPoPin, HIGH);
      // digitalWrite(relayMoPin, HIGH);
      break;
    case 2:
      lcd.clear();
      lcd1.clear();
      digitalWrite(relayPoPin, LOW);
      digitalWrite(relayMoPin, LOW);
      digitalWrite(relayV1Pin, HIGH);
      digitalWrite(relayV2Pin, HIGH);
      //volFlow2();
      delay(5000);
      //lcd.clear();

      // digitalWrite(relayV1Pin, HIGH);
      // digitalWrite(relayV2Pin, HIGH);
      // digitalWrite(relayPoPin, HIGH);
      // digitalWrite(relayMoPin, HIGH);
      
      break;
    case 3:

      lcd.clear();
      lcd1.clear();
      digitalWrite(relayV1Pin, LOW);
      digitalWrite(relayV2Pin, LOW);
      digitalWrite(relayPoPin, LOW);
      digitalWrite(relayMoPin, LOW);
      //volFlow1();
      //volFlow2();
      delay(5000);
      //lcd.clear();
      // digitalWrite(relayV1Pin, HIGH);
      // digitalWrite(relayV2Pin, HIGH);
      // digitalWrite(relayPoPin, HIGH);
      // digitalWrite(relayMoPin, HIGH);
      break;

    case 4:
      lcd.clear();
      lcd1.clear();
      digitalWrite(relayV1Pin, HIGH);
      digitalWrite(relayV2Pin, HIGH);
      digitalWrite(relayPoPin, HIGH);
      digitalWrite(relayMoPin, HIGH);
      //lcd.clear();
      break;

    default:

      break;
  }
}
