/***************************************************************************
Sensor component for WatchDog home monitoring program.

Authors: Ryan Smith, Theresa Brenier, Brad Thompson

Description:
   Program loops once per second and communicates with a middleware program. 
   Gets temperature reading from sensor and transmits to middleware. Also, monitors
   a motion sensor and sends message to middleware if it has been tripped.
   Program responds commands from the middleware and puts the Arduino in and out
   of msgMode (displays warning message and flashing lights to intruders), in and
   out of standby (stops transmitting current temperature and displays 'OFF' until
   reengaged), changes display on 7-Seg between celsius and fareignheit. 

Citation: The majority of this code come from I2C_7SEG_Temperature.pde, Copyright 2008, Gravitech.
          Original file modified to contain all functionality mentioned in description except 
          reading and transmitting the temperature and displaying celsius temp reading on 7-Seg
          display. The majority of these alterations occur within the infinite loop in the loop() function.

****************************************************************************/

#include <Wire.h> 
 
#define BAUD (9600)    /* Serial baud define */
#define _7SEG (0x38)   /* I2C address for 7-Segment */
#define THERM (0x49)   /* I2C address for digital thermometer */
#define EEP (0x50)     /* I2C address for EEPROM */
#define RED (3)        /* Red color pin of RGB LED */
#define GREEN (5)      /* Green color pin of RGB LED */
#define BLUE (6)       /* Blue color pin of RGB LED */

#define COLD (23)      /* Cold temperature, drive blue LED (23c) */
#define HOT (26)       /* Hot temperature, drive red LED (27c) */

const byte NumberLookup[16] =   {0x3F,0x06,0x5B,0x4F,0x66,
                                 0x6D,0x7D,0x07,0x7F,0x6F, 
                                 0x77,0x7C,0x39,0x5E,0x79,0x71};
                                 
/*'POLICE CALLED' message*/                            
int letters[20] = {0, 0, 0, 0B01110011, 0B00111111, 0B00111000, 
                    0B00000110, 0B00111001, 0B01111001, 0, 0B00111001, 
                    0B01110111, 0B00111000, 0B00111000, 0B01111001, 
                    0B00111111, 0, 0, 0, 0};
                    
const int inputPin = 2; /*Reading from motion sensor*/
int celsius = 1;        /*Indicates whether display should be in celsius*/
int standby = 0;        /*Indicates whether the Arduino is in standby*/
int msgMode = 0;        /*Indicates whether the Arduino is in message mode*/

/* Function prototypes */
void Cal_temp (int&, byte&, byte&, bool&);
void Dis_7SEG (int, byte, byte, bool);
void Send7SEG (byte, byte);
void SerialMonitorPrint (byte, int, bool);

/***************************************************************************
 Function Name: setup

 Purpose: 
   Initialize hardwares.
****************************************************************************/

void setup() 
{ 
  Serial.begin(BAUD);
  Wire.begin();        /* Join I2C bus */
  pinMode(RED, OUTPUT);    
  pinMode(GREEN, OUTPUT);  
  pinMode(BLUE, OUTPUT);   
  delay(500);          /* Allow system to stabilize */
  pinMode(inputPin, INPUT);
} 

/***************************************************************************
 Function Name: loop

 Purpose: 
   Run-time forever loop.
****************************************************************************/
 
void loop() 
{ 
  int Decimal;
  byte Temperature_H, Temperature_L, counter, counter2;
  bool IsPositive;
  
  /* Configure 7-Segment to 12mA segment output current, Dynamic mode, 
     and Digits 1, 2, 3 AND 4 are NOT blanked */
     
  Wire.beginTransmission(_7SEG);   
  byte val = 0; 
  Wire.write(val);
  val = B01000111;
  Wire.write(val);
  Wire.endTransmission();
  
  /* Setup configuration register 12-bit */
     
  Wire.beginTransmission(THERM);  
  val = 1;  
  Wire.write(val);
  val = B01100000;
  Wire.write(val);
  Wire.endTransmission();
  
  /* Setup Digital THERMometer pointer register to 0 */
     
  Wire.beginTransmission(THERM); 
  val = 0;  
  Wire.write(val);
  Wire.endTransmission();
  
  /* Test 7-Segment */
  for (counter=0; counter<8; counter++)
  {
    Wire.beginTransmission(_7SEG);
    Wire.write(1);
    for (counter2=0; counter2<4; counter2++)
    {
      Wire.write(1<<counter);
    }
    Wire.endTransmission();
    delay (250);
  }
  
  /*This is the heart of operation. This loop continues ifinitely while the arduino has power
   *and handles all operations on the device and communication with the middleware.
   */
  while (1)
  {
    /*Trap loop for standby. Displays the message 'OFF'. Stops reporting current temp and sends the 
     *default value of -274.0 to indicate 'no reading' to the middleware once per second. Continuously
     *looks for command from the middleware to exit standby. Sets RGB light to Red.     
     */
    while (standby == 1){
      Send7SEG (4,0);
      Send7SEG (3,0b00111111);
      Send7SEG (2,0b1110001);
      Send7SEG (1,0b1110001);
      Serial.print("-274.0\n");
      int message = Serial.read();
      if (message == 115){
        standby = !standby;
      }
      digitalWrite(BLUE, LOW); 
      digitalWrite(RED, HIGH);
      digitalWrite(GREEN, LOW);
      delay (1000);
    }
    
    /*
     *Checks to see if the motion sensor has been tripped and sends the message 'tripped'
     *to the middleware if it has been.
     */
    int value= digitalRead(inputPin);
    if (value == HIGH)
    {
      Serial.print("tripped\n");
    }
    
    /*Sets RGB light to default color Green.*/
    digitalWrite(BLUE, LOW); 
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, HIGH);
    
    /*Reads any messages from the middleware and updates variables accordingly.
     *'c' - asks the arduino to toggle its display between celsius and fareignheit
     *'s' - asks the arduino to toggle standby mode
     *'m' - tells the arduino to engage msgMode when updating display
     *'r' - takes the arduino display out of msgMode if it was engaged
     */
    int message = Serial.read();
    if(message == 102){ //'c'
      celsius = !celsius;
    }
    else if (message == 115){ //'s'
      standby = !standby;
    }
    else if (message == 109){ //'m'
      msgMode = 1;
    }
    else if (message == 114){ //'r'
      if (msgMode)
        msgMode = 0;
    }
    
    /*Get temp reading*/
    Wire.requestFrom(THERM, 2);
    Temperature_H = Wire.read();
    Temperature_L = Wire.read();
    
    /* Calculate temperature */
    Cal_temp (Decimal, Temperature_H, Temperature_L, IsPositive);
    
    /* Display temperature on the serial monitor.*/
    SerialMonitorPrint (Temperature_H, Decimal, IsPositive);
    
    /*Chooses what should be displayed on the 7-Seg based on whether or not the device is in msgMode.
     * If in message mode, police message and lights will be displayed, otherwise temp and green light.
     */
    if (!msgMode){
      /* Display temperature on the 7-Segment */
      Dis_7SEG (Decimal, Temperature_H, Temperature_L, IsPositive);
    }
    else {
      
      /*Implements the scrolling effect for the message.*/
      for (int i = 0; i < 17; i++) {
        if (i % 2 == 0) {
          digitalWrite(BLUE, HIGH); 
          digitalWrite(RED, LOW);
          digitalWrite(GREEN, LOW);
        }
        else {
          digitalWrite(BLUE, LOW); 
          digitalWrite(RED, HIGH);
          digitalWrite(GREEN, LOW);
        }
        Send7SEG (4,letters[i]);
        Send7SEG (3,letters[i+1]);
        Send7SEG (2,letters[i+2]);
        Send7SEG (1,letters[i+3]);
        delay(1000);
      }
    }
    /* Delay makes this loop, temperature reading and display repeat once per second. */
    delay (1000);  
  }
} 

/***************************************************************************
 Function Name: Cal_temp

 Purpose: 
   Calculate temperature from raw data.
****************************************************************************/
void Cal_temp (int& Decimal, byte& High, byte& Low, bool& sign)
{
  if ((High&B10000000)==0x80)    /* Check for negative temperature. */
    sign = 0;
  else
    sign = 1;
    
  High = High & B01111111;      /* Remove sign bit */
  Low = Low & B11110000;        /* Remove last 4 bits */
  Low = Low >> 4; 
  Decimal = Low;
  Decimal = Decimal * 625;      /* Each bit = 0.0625 degree C */
  
  if (sign == 0)                /* if temperature is negative */
  {
    High = High ^ B01111111;    /* Complement all of the bits, except the MSB */
    Decimal = Decimal ^ 0xFF;   /* Complement all of the bits */
  }  
}

/***************************************************************************
 Function Name: Dis_7SEG

 Purpose: 
   Display number on the 7-segment display.
****************************************************************************/
void Dis_7SEG (int Decimal, byte High, byte Low, bool sign)
{
  if (!celsius){                /* Translate to fahrenheit if the celcius boolean is not true. */
    Decimal = Decimal * 9 / 5 + 32;
    High = High * 9 / 5 + 32;
    Low = Low * 9 / 5 + 32;
  }
  byte Digit = 4;                 /* Number of 7-Segment digit */
  byte Number;                    /* Temporary variable hold the number to display */
  
  if (sign == 0)                  /* When the temperature is negative */
  {
    Send7SEG(Digit,0x40);         /* Display "-" sign */
    Digit--;                      /* Decrement number of digit */
  }
  
  if (High > 99)                  /* When the temperature is three digits long */
  {
    Number = High / 100;          /* Get the hundredth digit */
    Send7SEG (Digit,NumberLookup[Number]);     /* Display on the 7-Segment */
    High = High % 100;            /* Remove the hundredth digit from the TempHi */
    Digit--;                      /* Subtract 1 digit */    
  }
  
  if (High > 9)
  {
    Number = High / 10;           /* Get the tenth digit */
    Send7SEG (Digit,NumberLookup[Number]);     /* Display on the 7-Segment */
    High = High % 10;            /* Remove the tenth digit from the TempHi */
    Digit--;                      /* Subtract 1 digit */
  }
  
  Number = High;                  /* Display the last digit */
  Number = NumberLookup [Number]; 
  if (Digit > 1)                  /* Display "." if it is not the last digit on 7-SEG */
  {
    Number = Number | B10000000;
  }
  Send7SEG (Digit,Number);  
  Digit--;                        /* Subtract 1 digit */
  
  if (Digit > 0)                  /* Display decimal point if there is more space on 7-SEG */
  {
    Number = Decimal / 1000;
    Send7SEG (Digit,NumberLookup[Number]);
    Digit--;
  }

  if (Digit > 0 && celsius)                 /* Display "c" if there is more space on 7-SEG */
  {
    Send7SEG (Digit,0x58);
    Digit--;
  }
  else if (Digit > 0) {                     /* Display "f" if there is more space on 7-SEG */
    Send7SEG (Digit,0b1110001);
    Digit--;
  }
  
  if (Digit > 0)                 /* Clear the rest of the digit */
  {
    Send7SEG (Digit,0x00);    
  }  
}

/***************************************************************************
 Function Name: Send7SEG

 Purpose: 
   Send I2C commands to drive 7-segment display.
****************************************************************************/

void Send7SEG (byte Digit, byte Number)
{
  Wire.beginTransmission(_7SEG);
  Wire.write(Digit);
  Wire.write(Number);
  Wire.endTransmission();
}


/***************************************************************************
 Function Name: SerialMonitorPrint

 Purpose: 
   Print current read temperature to the serial monitor.
****************************************************************************/
void SerialMonitorPrint (byte Temperature_H, int Decimal, bool IsPositive)
{
    if (!IsPositive)
    {
      Serial.print("-");
    }
    Serial.print(Temperature_H, DEC);
    Serial.print(".");
    Serial.print(Decimal, DEC);
    Serial.print("\n");
}