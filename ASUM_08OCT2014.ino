/***************************************************
 *  Name:        Berkeley Advanced Stove Use Monitor
 *  Version:     V 1.00
 *  Authors:     Daniel Wilson, Advait Kumar, and Abhinav Saksena
 *  Description: Code that powers the Arduino Pro Mini-based 
 *               Advanced Stove Use Monitor (ASUM)
 ***************************************************/

//some libraries to inclue
#include <DS1374RTC.h>
#include <Time.h>
#include <Wire.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SdFat.h>
#include <math.h>

const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

//number of sleep cycles with ~8 seconds per sleep cycle
const int sleepCycles = 1; 

////some resistor values in Ohms for various fixed resistors on the ASUM board
//const int fixedResistance=100000; //thermistor's fixed resistor
//const int usbVoltageDivider1=100000, usbVoltageDivider2=200000, fanVoltageDivider1=100000, fanVoltageDivider2=200000;

//the analog pins that read voltages for the various sensors interfaced with the ASUM
const int thermistorIn = A0, usbIn = A1, fanIn = A2, potSwitchIn= A3;

//digital pins
const int thermPower=3, potSwitchPower=2, dcdc=4, faultLED=9, happyLED=8;

//the chip select pin is important for the microSD card interface
const int chipSelect = 10; 

//needed for watchdog timer and sleep power down mode
volatile int f_wdt=1; 

SdFat sd;
SdFile myFile;

//define and initialize some variable values
int i=0, thermistorValue=1, usbValue=1, potSwitchValue=1, count=0, k=0;
long fanValue=1;
//float thermistorResistance=1.0, resistanceRatio=1.0, usbVoltage=1.0, fanVoltage=1.0;

//formatting for date and time coming from the RTC
//char formatted[] = "00-00-00 00:00:00x";

// RTC stuff
tmElements_t tm;

/****************************************************
 * Code necessary for the watchdog timer and sleeping of the ASUM
 ****************************************************/

void softReset()
{
  asm volatile ("  jmp 0");
}


ISR(WDT_vect)
{
  if(f_wdt == 0)
  {
    f_wdt=1;
  }
  else
  {
    Serial.println("WDT Overrun!!!");
  }
}

void enterSleep(void)
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
  sleep_enable();
  sleep_mode(); // Now enter sleep mode. 
  /* The program will continue from here after the WDT timeout*/
  sleep_disable(); // First thing to do is disable sleep. 
  power_all_enable();   // Re-enable the peripherals. 
}

/***************************************************
 * end of sleepinging functions
 ***************************************************/

void setup() 
{

  //f_wdt = 0;

  pinMode(dcdc, OUTPUT);
  pinMode(potSwitchPower, OUTPUT);
  pinMode(thermPower, OUTPUT);
  pinMode(faultLED, OUTPUT);
  pinMode(happyLED, OUTPUT);

  // initialize serial communications at 9600 bps:
  Serial.begin(115200);
  Serial.print("Initializing SD card...");
  delay(10); //Allow for serial print to complete.

  while(!sd.begin(chipSelect, SPI_HALF_SPEED))  
  {
    //sd.initErrorHalt();
    Serial.println("initialization failed!");
    digitalWrite(faultLED,HIGH);  //if the SD card wont initialize, then blink the red LED
    delay(500);
    digitalWrite(faultLED,LOW);
    delay(500);
  }

  Serial.println("initialization done.");
  i=1;
  while(i<5) //if the SD card initializes, then blink the green LED quickly 5 times
  {
    digitalWrite(happyLED,HIGH);
    delay(100);
    digitalWrite(happyLED,LOW);
    delay(100);
    i=i+1;
  }

}

void loop() 
{ 
  /**********
   * In this block of code, we write the power pins for sensors high, take sensor values, 
   * then write the power pins low again. We do it this way to save power
   * (no need to power the sensors when the ASUM is sleeping). 
   ***********/
  digitalWrite(thermPower,HIGH);
  digitalWrite(dcdc,HIGH);
  digitalWrite(potSwitchPower,HIGH);
  delay(100);//let the transients settle and sensors stabilize
  thermistorValue = analogRead(thermistorIn);
  potSwitchValue  = analogRead(potSwitchIn);
  //RTC.readClock();
  delay(10);//Danny Thinks we need time to read analog signals
  digitalWrite(thermPower,LOW);
  digitalWrite(dcdc,LOW);
  digitalWrite(potSwitchPower,LOW);
  count++;

  //grab some other sensor values that don't require powered pins
  usbValue =analogRead(usbIn);
  fanValue = analogRead(fanIn);
  tmElements_t tm;
  Serial.print(count);
  Serial.println(": ");
  if (RTC.readTime(tm)) {
    Serial.print(tmYearToCalendar(tm.Year));
    Serial.write('-');
    Serial.print(tm.Month);
    Serial.write('-');
    Serial.print(tm.Day);
    Serial.write(' ');
    print2digits(tm.Hour);
    Serial.write(':');
    print2digits(tm.Minute);
    Serial.write(':');
    print2digits(tm.Second);
    Serial.println();
  } 
  else {
    if (RTC.chipPresent()) {
      Serial.println("The DS1374 is stopped.  Please run the SetTime");
      Serial.println("example to initialize the time and begin running.");
      Serial.println();
    } 
    else {
      Serial.println("DS1374 read error!  Please check the circuitry.");
      Serial.println();
    }
    delay(9000);
  }
  Serial.print("sensor thermistor a0 = " );                       
  Serial.println(thermistorValue);
  Serial.print("sensor usb a1 = " );
  Serial.println(usbValue);
  Serial.print("sensor fan a2 = " );
  Serial.println(fanValue);
  Serial.print("sensor pot switch a3 = ");
  Serial.println(potSwitchValue);

  //if we can't find the SD card, restart the ASUM
  if (!myFile.open("DATA.txt", O_RDWR | O_CREAT | O_AT_END)) 
  {
    digitalWrite(faultLED,HIGH);
    delay(1000);
    digitalWrite(faultLED,LOW);
    softReset();
  }
  else
  {
    Serial.print("Writing to test.txt...");
    digitalWrite(happyLED, HIGH); //make happyLED light up while writiing to the SD card

    myFile.print(tmYearToCalendar(tm.Year));
    myFile.print('-');
    myFile.print(tm.Month);
    myFile.print('-');
    myFile.print(tm.Day);
    myFile.print(' ');
    myFile.print(tm.Hour);
    myFile.print(':');
    myFile.print(tm.Minute);
    myFile.print(':');
    myFile.print(tm.Second);
    myFile.print(",");
    myFile.print(thermistorValue);
    myFile.print(",");
    myFile.print(usbValue);
    myFile.print(",");
    myFile.print(fanValue);
    myFile.print(",");
    myFile.println(potSwitchValue);
    myFile.close();

    Serial.println("done.");
    Serial.print("\n");
    digitalWrite(happyLED, LOW);
    delay(1); // to allow the serial communication to finish
  }

  /**********
   * Put the microcontroller to sleep
   ***********/

  /*** Setup the WDT ***/
  MCUSR &= ~(1<<WDRF);   /* Clear the reset flag. */
  /* In order to change WDE or the prescaler, we need to
   * set WDCE (This will allow updates for 4 clock cycles).
   */
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  /* set new watchdog timeout prescaler value */
  WDTCSR = 1<<WDP0 | 1<<WDP3;/* 8.0 seconds */
  /* Enable the WD interrupt (note no reset). */
  WDTCSR |= _BV(WDIE);

  int sleepCnt=0;
  while (sleepCnt < sleepCycles)
  {
    if(f_wdt == 1)
    {
      f_wdt = 0; /* Don't forget to clear the flag. */
      enterSleep(); /* Re-enter sleep mode. */
      sleepCnt++;
    }
  }
  //hooray! We're done with this loop.
}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}


