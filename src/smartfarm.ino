#include <LiquidCrystal_I2C.h> // LCD Library
#include <Adafruit_Sensor.h> // LCD Library

#include <DHT.h> // Humidity and Temperature sensor Library
#include <DHT_U.h> // Humidity and Temperature sensor Library

#include "RTClib.h" // timer library
#include <UltrasonicSensor.h>

#include <ESP32Servo.h>
#include <WiFiClient.h> 
#include <BlynkSimpleEsp32.h> // IOT app database library

#define DHTPIN A18 // RH&T sensor pin
#define DHTTYPE DHT22

#define Emea_Humid 4 // error sensor +-2%
#define Emea_Temp 1 // error sensor +-0.5 celcius
#define kalman_Loop 30

// proper humid range for cannabis
#define humid_Range1 60.00 
#define humid_Range2 70.00

// proper temp range for cannabis
#define temp_Range1 26.00
#define temp_Range2 30.00

#define fan1 A15 // fan group1 pin
#define fan2 A17 // fan group2 pin
#define water_Pump A14 // water pump pin
#define RH A16   // humidity maker pin
#define servoPin A10 // servo motor pin
#define BLYNK_PRINT Serial

Servo myservo; 
UltrasonicSensor ultrasonic(A12, A11);

//char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
float mea_Temp = 0, mea_Humid = 0, humidity=0, temp=0;
float KGT = 0, KGH= 0,distance;
float est_Temp = 0,est_Humid = 0, Eest_Temp = 1 , Eest_Humid = 4;
float est_Temp0 = 0, est_Humid0 = 0, Eest_Temp0 = 1, Eest_Humid0 = 4;

//  WIFI ssid, passward, and Token-based Authentication
char ssid[] = "12345";
char pass[] = "Theultimate111!";
char auth[] = "Q_4-SuAnvPrMEsL9G52cmSGYblZcBQh4";

RTC_DS3231 rtc; 
DateTime now;
  
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE); 

BlynkTimer timer; 

  byte customChar_H_good[] = { B00000,
  B00000,
  B01010,
  B00000,
  B10001,
  B01110,
  B00000,
  B00000};
  byte customChar_H_bad[] = {  B00000,
  B00000,
  B01010,
  B00000,
  B01110,
  B10001,
  B00000,
  B00000};
  byte customChar_T[] = {B00100,
  B01010,
  B01010,
  B01010,
  B01010,
  B10111,
  B10001,
  B01110};
  byte customChar_degree[] = {B00110,
  B01001,
  B01001,
  B00110,
  B00000,
  B00000,
  B00000,
  B00000
};
  byte customChar_clock[] = {B00000,
  B01110,
  B10101,
  B10111,
  B10001,
  B01110,
  B00000,
  B00000
};

void setup() {
  Serial.begin(9600);
  setupRelay();
  setupDHT();
  setupLCD();
  estimate_System();
  setupBlynk();
  setupUltra();
  setupServo();
}

void loop() {
  readData();
  showLCD();
  timer.run(); 
  // send data from kalman filter and raw data to server
  reset_Value();   
  Door();
}

void readData()
{
  now = rtc.now();
  humidity = dht.readHumidity();
  temp = dht.readTemperature(); 
}
void setupDHT () 
{
  dht.begin();
  delay(500);
}
void setupBlynk()
{
   Blynk.begin(auth, ssid, pass);
   timer.setInterval(2500, Sensor);
}

void setupRelay() 
{
  pinMode(water_Pump , OUTPUT); 
  delay(750);
  pinMode(RH, OUTPUT);
  delay(750); 
  pinMode(fan1, OUTPUT);
  delay(750);
  pinMode(fan2, OUTPUT);
  delay(750);
}

void setupTimer()
{
  
  #ifndef ESP8266
    while (!Serial); // for Leonardo/Micro/Zero
  #endif
  delay(3000); // wait for console opening

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
}

void setupLCD() //step3
{
  lcd.begin();
  delay(500);
  lcd.home();  
  lcd.clear();
  delay(500);
}  
void setupServo()
{
  myservo.setPeriodHertz(180);    // standard 50 hz servo
  myservo.attach(servoPin, 1000, 2000);
}
void setupUltra()
{
  int temperature = 22;
  ultrasonic.setTemperature(temperature);
}
void showLCD()
{
  kalman();
  if ( est_Temp > humid_Range2 || est_Humid < humid_Range1 )
  {
    lcd.setCursor(3,0);
    lcd.write(0); lcd.createChar(0, customChar_clock); 
    lcd.setCursor(4,0);
    lcd.print(now.hour(), DEC); lcd.print(":"); lcd.print(now.minute(), DEC);  lcd.print(":"); lcd.print(now.second(), DEC); lcd.print("  ");  
    lcd.setCursor(0,1); 
    lcd.write(1); lcd.createChar(1, customChar_T);
    lcd.setCursor(1,1); lcd.print(est_Temp); 
    lcd.write(2); lcd.createChar(2, customChar_degree);  lcd.setCursor(7,1); lcd.print("C "); 
    lcd.write(3); lcd.createChar(3, customChar_H_bad); lcd.setCursor(10,1); lcd.print(est_Humid); lcd.print("%");
  }
  else
  {
    lcd.setCursor(3,0);
    lcd.write(0); lcd.createChar(0, customChar_clock); 
    lcd.setCursor(4,0);
    lcd.print(now.hour(), DEC); lcd.print(":"); lcd.print(now.minute(), DEC);  lcd.print(":"); lcd.print(now.second(), DEC); 
    lcd.setCursor(0,1); 
    lcd.write(1); lcd.createChar(1, customChar_T);
    lcd.setCursor(1,1); lcd.print(est_Temp); 
    lcd.write(2); lcd.createChar(2, customChar_degree);  lcd.setCursor(7,1); lcd.print("C "); 
    lcd.write(3); lcd.createChar(3, customChar_H_good); lcd.setCursor(10,1); lcd.print(est_Humid); lcd.print("%");
  }
  enable_Fan();
  enable_Humidifier();
  delay(1000);
}

void enable_Fan()
{ 
  if ( est_Temp >= 26.00 )
  {  digitalWrite(fan1, 1);}
  else if( est_Temp >= 28.00 ){
    digitalWrite(fan2, 1);// turn on fan, reducing heat
    digitalWrite(fan1, 1);}
  else{
    digitalWrite(fan2, 0);
    digitalWrite(fan1, 0);} // turn off fan, reducing heat
}
void enable_Humidifier()
{
    if (est_Humid <= 60.00)
       digitalWrite(RH, 1); // turn on humid maker
    else
      digitalWrite(RH, 0); // turn off humid maker
}

void enable_water_Pump()
{    
  if (now.hour() == 17 && now.minute() == 1 && now.second() > 57) 
  {
     digitalWrite(water_Pump, 1);// turn on water pump
     delay(2000);
  }
  else if (now.hour() == 5 && now.minute() == 1 && now.second() > 57)
  {
     digitalWrite(water_Pump, 1);
     delay(2000);
  }
  else
  {
     digitalWrite(water_Pump, 0);
  }  
}


void estimate_System(){ // estimate temp & Humidity to use in kalman filter
  int count = 0;
  lcd.print("estimating");


  for(int i = 0 ; i < kalman_Loop ; i++ )
  {
    readData();
    est_Temp0 += temp; //estimate 
    est_Humid0 += humidity;
    delay(500);
    
    if((i%10) == 0)
    {
      lcd.print(".");
      if(count == 3)
      {
        lcd.setCursor(11,0);
        lcd.print("   ");
        lcd.setCursor(11,0);
        count = 0;
      }
      count++; 
    }
  }
  est_Temp0 /= kalman_Loop;
  est_Humid0 /= kalman_Loop; // Temp&Humid initial
  lcd.clear();
  delay(500);
  lcd.print("ready");
  delay(500);
  lcd.clear();
}


void kalman()
{
    mea_Temp = temp;
    KGT = ( Eest_Temp ) / ( Eest_Temp + Emea_Temp );
    est_Temp = ( est_Temp0 ) + (KGT * ( mea_Temp - est_Temp0 ));
    Eest_Temp = ( Eest_Temp0 * (1 - KGT));
  
    mea_Humid = humidity;
    KGH = ( Eest_Humid ) / ( Eest_Humid + Emea_Humid );
    est_Humid =( est_Humid0 ) + (KGH * ( mea_Humid - est_Humid0 ));
    Eest_Humid = ( Eest_Humid0 * (1 - KGH));

}

void reset_Value() //reset data from kalman filter
{
  est_Temp0 = est_Temp;
  est_Humid0 = est_Humid;
  Eest_Temp0 = Eest_Temp;
  Eest_Humid0 = Eest_Humid;
}
void Door()
{
  distance = ultrasonic.distanceInCentimeters();
  delay(500);
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance >=3 && distance <= 10)
  {
   myservo.write(180);
    delay(10000);
  }
  else
  {
    myservo.write(15);
    delay(10000);
  }
}
void Sensor()
{

  Blynk.virtualWrite(V1, est_Humid);
  Blynk.virtualWrite(V2, est_Temp);
  Blynk.virtualWrite(V3, humidity);
  Blynk.virtualWrite(V4, temp);
  Blynk.virtualWrite(V5, Eest_Temp);
  Blynk.virtualWrite(V6, Eest_Humid);
}
