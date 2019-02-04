/*
 * Smart Peltier Condenser v1.9.1
 * SPC.cpp
 * Christian Masas
 * Senior Project Development
 *
 * Version Notes:
 * v1.0 Project started on 4/8/2018
 * v1.1 Added LCD controls and serial monitoring
 * v1.2 Added pump and humidity controls, and version notes
 * v1.3 Cleanup loop() so that each control is given a function
 * v1.4 Added subsystem notes to function comment headers 5/12/18
 * v1.5 Added wifi subsystem based off of WeMos D1 testing 5/24/18
 * v1.6 Transition to DHT11 and TMP36 libraries, added error handling 5/31/18
 * v1.6.1 Floating pin value range fix, global variable cleanup 7/23/18
 * v1.7 New updated GUI systemStatus function, solo DHT sensor usage 
 * Removal of WiFi, RC, and other control functions 8/11/18
 * v1.7.1 Added pinmodes for devices based on new hardware design 8/17/18
 * v1.8 Added psychrometric function using Magnus approx. formula 9/8/18
 * v1.9 Added tempControl function 9/20/18
 * v1.9.1 Final improvement pass through and function cleanup/removal 10/6/2018
 * 
 */
// use this next line when porting to another IDE
#include <Arduino.h>        // standard library
#include <LiquidCrystal.h>  // lcd library
#include <DHT.h>            // sensor library
#define DHT_PIN A5          // dht sensor pin
#define PUMP_PIN 11         // pump pin
#define FAN_PIN 10          // fan pin
#define TEC_PIN 12          // tec pin


// comment out the proper line to choose dhttype
#define DHT_TYPE DHT11    // dht11 sensor type
//#define DHT_TYPE DHT22      // dht22 sensor type

// sensor initialization
DHT dht(DHT_PIN, DHT_TYPE);

// lcd initialization
LiquidCrystal lcd(8, 13, 9, 4, 5, 6, 7);

/*
 * dhtReading() INPUT SENSOR SUBSYSTEM
 * Acquires and converts an analog temperature reading from the DHT sensor
 * into degrees Fahrenheit and Celcius using the DHT sensor library.
 * Displays conversion as "T: ##.##°F" on line 1 of the LCD shield.
 * Acquires and converts an analog humidity reading from the DHT sensor 
 * into percent relative humidity using the DHT sensor library.
 * Displays conversion as "H: ##.##%" on line 1 of the LCD shield.
 * Readings take roughly 250ms. Sensor readings may be up to 2 sec old.
 */
void dhtReading()
{
  // wait to measure
  delay(2000);
    
  // read dry bulb temp in fahrenheit
  float fDryTemp = dht.readTemperature(true);
  
  // read percent relative humidity
  float rH = dht.readHumidity();

  // read dry bulb temp in celcius
  float cDryTemp = dht.readTemperature();
  
  // error handling
  if (isnan(fDryTemp) || isnan(rH))
  {
    // set system status on error
    systemStatus(3);
    
    // serial monitor display
    Serial.print("T: ");
    Serial.print("N/A\t");
    Serial.print(" ");
    Serial.print("H: ");
    Serial.println("N/A\t");

    // lcd display
    lcd.setCursor(0, 1);
    lcd.print("T: ");
    lcd.print("N/A");
    lcd.setCursor(9, 1);
    lcd.print("H: ");
    lcd.print("N/A");
    
    return;
  }
  else
  // output readings
  {
     // serial monitor readings
    Serial.print("T: ");
    Serial.print(fDryTemp);
    Serial.print("°F\t");
    Serial.print("H: ");
    Serial.print(rH);
    Serial.print("%\t");
    
    // lcd readings
    lcd.setCursor(0, 1);
    lcd.print("T: ");
    lcd.print(fDryTemp);
    lcd.print((char)223);
    lcd.print("F");
    lcd.setCursor(9, 1);
    lcd.print("H: ");
    lcd.print(rH);
    lcd.print("%");
  } // end if

  // pass sensor readings
  psychroCalc(fDryTemp, rH, cDryTemp);
} // end dhtReading

/*
 * psychroCalc() MAIN SUBSYSTEM
 * Uses the Magnus dewpoint formula to approximate the dewpoint 
 * based on the dry bulb (air) temperature and the percent 
 * relative humidity. Formula courtesy of 
 * https://cals.arizona.edu/azmet/dewpoint.html
 */
void psychroCalc(double fDryTemp, double RH, double cDryTemp)
{  
  // dewpoint formula
  double bDewForm = (log(RH / 100) + ((17.271 * cDryTemp) / (237.3 + cDryTemp))) / 17.271;

  // dewpoint temp in degrees C
  double cDewTemp = (237.3 * bDewForm) / (1 - bDewForm);

  // convert dewpoint from degrees C to F
  double fDewTemp = cDewTemp * 1.8 + 32.0;

  // display dewpoint temp in F
  Serial.print("TDP: ");
  Serial.print(fDewTemp);
  Serial.println("°F\t");

  // pass sensor readings and calculated dewpoint temp
  tempControl(fDryTemp, fDewTemp);
} // end psychroCalc

/*
 * systemStatus() OUTPUT CONTROL SUBSYSTEM
 * Clears the previous LCD output then displays the current status of 
 * the system to the serial monitor and the LCD shield.
 * Displays as "SYSTEM: (status)" on line 0 of the LCD shield.
 */
void systemStatus(int n)
{
  // clear lcd
  lcd.clear();

  // status indicator
  switch (n) 
  {
    // 0 = INIT
    case 0:
      Serial.println("SYSTEM: INIT");
      lcd.setCursor(0, 0);
      lcd.print("SYSTEM: INIT");
      break;
    // 1 = ON
    case 1:
      Serial.println("SYSTEM: ON");
      lcd.setCursor(0, 0);
      lcd.print("SYSTEM: ON");
      break;
    // 2 = OFF
    case 2:
      Serial.println("SYSTEM: OFF");
      lcd.setCursor(0, 0);
      lcd.print("SYSTEM: OFF");
      break;
    // 3 = ERROR
    case 3:
      Serial.println("SYSTEM: ERROR");
      lcd.setCursor(0, 0);
      lcd.print("SYSTEM: ERROR");
      break;          
    default:
      break;
  } // end switch
} // end systemStatus

/*
 * tempControl() OUTPUT CONTROL SUBSYSTEM
 * Controls a device using the dewpoint to determine the 
 * threshold. OSHA recommends indoor temperature control 
 * in the range of 68-76°F.
 * https://www.osha.gov/laws-regs/standardinterpretations/2003-02-24
 * A relative humidity of 100% means dew point is the same 
 * as air temp. For 90% RH, dew point is 3°F lower than 
 * air temperature. For every 10 percent lower, dew point 
 * drops 3°F.
 */
void tempControl(double fDryTemp, double fDewTemp)
{  
  // for 60% RH, dew point should be within 12°F
  if ((fDryTemp - fDewTemp) <= 12.0)
  {
    // switch on devices
    digitalWrite(PUMP_PIN, HIGH);
    digitalWrite(FAN_PIN, HIGH);
    digitalWrite(TEC_PIN, HIGH);
    systemStatus(1);
  }
  else if ((fDryTemp - fDewTemp) > 12.0)
  {
    // switch off devices
    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(FAN_PIN, LOW);
    digitalWrite(TEC_PIN, LOW);
    systemStatus(2);
  }
  else
  {
    // switch off devices
    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(FAN_PIN, LOW);
    digitalWrite(TEC_PIN, LOW);
    systemStatus(3);
  } // end if

  delay(2000);
} // end tempControl

/*
 * setup() MAIN SUBSYSTEM
 * Sketch setup includes pinmodes, lcd controls, and serial monitoring.
 */
void setup()
{
  // serial monitor for debugging
  Serial.begin(9600);
  delay(1);

  // lcd setup
  lcd.clear();
  lcd.begin(16, 2);

  // start dht sensor
  dht.begin();

  // set pinmodes for pump and fans
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(TEC_PIN, OUTPUT);
  
  // system status
  systemStatus(0);
} // end setup

/*
 * loop() MAIN SUBSYSTEM
 * Functions placed here will be looped forever.
 */
void loop()
{
  dhtReading();           // start reading
} // end loop
