#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// WiFi
const char* ssid = "wifi name";
const char* password = "password";
// ThingSpeak
WiFiClient client;
unsigned long channel = 3300823;
const char* apiKey = "DASTW2Q44HGZON6U";
// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // try 0x3F if needed
// DHT
#define DHTPIN D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
// Pins
#define SOIL_ANALOG A0
#define RELAY D0
#define FLOW_SENSOR D3
#define SOIL_DIGITAL D5
#define RAIN_SENSOR D6
#define TRIG D7
#define ECHO D8
#define BUZZER 3
#define RED_LED 1
// Soil calibration (CHANGE if needed after testing)
int dryValue = 900;
int wetValue = 350;

volatile int pulse = 0;
float flow = 0;
float totalWater = 0;

int displayIndex = 0;

// Interrupt
void ICACHE_RAM_ATTR countPulse() {
  pulse++;
}

// Ultrasonic
long getLevel() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH);
  return duration * 0.034 / 2;
}

// LCD Display
void displayLCD(float temp, float hum, int soil, int level, float flow, int motor, int rain) {
  lcd.clear();
  switch(displayIndex) {
    //  TEMPERATURE + HUMIDITY
    case 0:
      lcd.setCursor(0,0);
      lcd.print("Temp:");
      lcd.print(temp);
      lcd.setCursor(0,1);
      lcd.print("Hum:");
      lcd.print(hum);
      break;
    //  SOIL WITH CONDITION
    case 1:
      lcd.setCursor(0,0);
      lcd.print("Soil Moisture:");
      lcd.print(soil);
      lcd.print("%");
      lcd.setCursor(0,1);
      if (soil <= 30) {
        lcd.print("Dry - Motor ON");
      }
      else if (soil <= 60) {
        lcd.print("Medium - ON");
      }
      else {
        lcd.print("Wet - Motor OFF");
      }
      break;
    // WATER LEVEL
    case 2:
      lcd.setCursor(0,0);
      lcd.print("Water Level:");
      lcd.setCursor(0,1);
      lcd.print(level);
      lcd.print("%");
      break;
    //  FLOW
    case 3:
      lcd.setCursor(0,0);
      lcd.print("Consmued water:");
      lcd.setCursor(0,1);
      lcd.print(flow);
      lcd.print(" L/m");
      break;
    //  MOTOR
    case 4:
      lcd.setCursor(0,0);
      lcd.print("Motor Status:");
      lcd.setCursor(0,1);
      if(motor) lcd.print("ON");
      else lcd.print("OFF");
      break;
    // RAIN (UPDATED LOGIC DISPLAY)
    case 5:
      lcd.setCursor(0,0);
      lcd.print("Rain fall:");
      lcd.setCursor(0,1);
      if (rain == LOW) {
        lcd.print("NO Rain-Motor ON");
      } else {
        lcd.print("YES Rainfall ");
      }
      break;
    //  TOTAL WATER
    case 6:
      lcd.setCursor(0,0);
      lcd.print("Used Water:");
      lcd.setCursor(0,1);
      lcd.print(totalWater);
      lcd.print(" L");
      break;
  }
  displayIndex++;
  if(displayIndex > 6) displayIndex = 0;
}
void setup() {
  Serial.begin(9600);
  pinMode(RELAY, OUTPUT);
  pinMode(FLOW_SENSOR, INPUT);
  pinMode(SOIL_DIGITAL, INPUT);
  pinMode(RAIN_SENSOR, INPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR), countPulse, RISING);
  dht.begin();
  lcd.init();
  lcd.backlight();
  // Title
  lcd.setCursor(0,0);
  lcd.print("IoT SMT Irr");
  lcd.setCursor(0,1);
  lcd.print("Env Mot System");
  delay(3000);
  lcd.clear();
  lcd.print("WiFi Connecting");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  lcd.clear();
  lcd.print("WiFi Connected");
  delay(2000);
  lcd.clear();
  ThingSpeak.begin(client);
}
void loop()
{
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  //  SOIL (AVERAGED + CALIBRATED)
  int sum = 0;
  for(int i = 0; i < 10; i++) {
    sum += analogRead(SOIL_ANALOG);
    delay(10);
  }
  int soilRaw = sum / 10;
  int soilPercent = map(soilRaw, dryValue, wetValue, 0, 100);
  if (soilPercent > 100) soilPercent = 100;
  if (soilPercent < 0) soilPercent = 0;
  int rain = digitalRead(RAIN_SENSOR);
  //  WATER LEVEL (3–5 cm)
  long level = getLevel();
  if (level < 3) level = 3;
  if (level > 5) level = 5;
  int levelPercent = 0;
  if (level >= 5) levelPercent = 75;
  else if (level >= 3) levelPercent = 50;
  else levelPercent = 25;
  // FLOW 
  unsigned long startTime = millis();
  pulse = 0;
  while (millis() - startTime < 1000) {}
  float frequency = pulse;
  flow = frequency / 7.5;
  if (flow < 0.1) flow = 0.25;
  int motor = 0;

  //  SOIL LOGIC
  if (soilPercent <= 30) {
    digitalWrite(RELAY, HIGH);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER, HIGH);
    motor = 1;
  }
  else if (soilPercent <= 60) {
    digitalWrite(RELAY, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
    motor = 1;
  }
  else {
    digitalWrite(RELAY, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
    motor = 0;
  }
  //  TOTAL WATER
  if (motor == 1) {
    totalWater += (flow / 60.0);
  }
  // ThingSpeak
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, hum);
  ThingSpeak.setField(3, soilPercent);
  ThingSpeak.setField(4, levelPercent);
  ThingSpeak.setField(5, flow);
  ThingSpeak.setField(6, motor);
  ThingSpeak.setField(7, rain);
  ThingSpeak.writeFields(channel, apiKey);
  displayLCD(temp, hum, soilPercent, levelPercent, flow, motor, rain);
  delay(2000);
}