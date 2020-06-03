/*
*	Piano, eerste versie met Bluno m0 en adafruit shield
*            (Cortex-M0, 32 bit)
*             Note: 32 bit integers are atomic.
*    todo:
       pincode ??
       calibratie: vastloopdetectie
       waggel beter.
       p commando voor stapjes 0 t/m 10
*/
#define SERIAL  0
#define VERSION 1

#include <WMath.h>

#include <EEPROM.h>

#include <TimerOne.h>
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 
Adafruit_DCMotor *motorL = AFMS.getMotor(1);
Adafruit_DCMotor *motorR = AFMS.getMotor(2);

// Encoders
//   we only use 1 interrupt per encoder, resolution probably high enough
// bluno M0 gebruikt D0 en D1 voor uart, D2 en D3 voor i2c, daarom D7 en D8 interrupts gebruiken, patched library for extra interrupts)
#define  m_ena1 7    // Int. was 2
#define  m_ena2 9
#define  m_enb1 8    // Int. was 3
#define  m_enb2 10

/*
  wat is de vermenigvuldigingsfactor naar encoder waarden?
     x stappen per omwenteling
     y spoed spindel in mm verplaatsing per omwenteling
     z mm verplaatsing per stapje
  factor: z*x/y
  bv: 0,5 * 1000 / 2 = 250
*/
#define GEAR 250

// arduino ledje
#define  ledje 13

byte speed1 = 0, speed2 = 0;    // intended motor speed
byte received           = 0;    // # of characters received
volatile int encoder1, oldenc1, encoder2, oldenc2;
volatile int corr1, corr2, setp1, setp2;
int calenc1, calenc2;
byte mot;

/* control w, cal, m and p commands set control
   0: off
   1: w, cal
   5: m, p
 */
byte control  = 0;

// EEPROM data structure
struct SavedData {
  unsigned int pval1, ival1, dval1, pval2, ival2, dval2;
  int setpoint1, setpoint2;
  unsigned int serialnumber, version, eepromcnt;
};

  SavedData mydata;

void myEEPROMput(void) {
  mydata.eepromcnt++;
  EEPROM.put(0, mydata);
}


// default PID values
#define Pdefault 5
#define Idefault 0
#define Ddefault 2

//
byte comavail  = 0;   // a command is read
char command[10];     // command buffer
byte comind    = 0;   // index into buffer
byte comm      = 0;   // command to be executed
char printstr[22] ;    // string for printing. Langer dan 20 over ble lijkt onbetrouwbaar
int debugcnt = 0;

/* timer1_counter used for
   - w command
   - calibratie
   - setpoint command
  Set to zero at start.
   timews > 0  ==> waggel, else calibration or setpoint

 Twee modus
        1 normaal
	   timesw = 0
        2 testmode
	   timesw > 0
	   gebruik timesw om de motors te bewegen
  */
volatile int timer1_counter = 0, timesw = 0;

void motors_stop(void) {
      control = 0;
      motorL->run(RELEASE);
      motorR->run(RELEASE);
      motorL->setSpeed(0);
      motorR->setSpeed(0);
}

void motorL_speed(int speed) {
  if(speed > 0)
    motorL->run(FORWARD);
  else
    motorL->run(BACKWARD);
  speed = abs(speed);
  if(speed > 255)
    speed = 255;
  motorL->setSpeed((byte)speed);
}

void motorR_speed(int speed) {
  if(speed > 0)
    motorR->run(FORWARD);
  else
    motorR->run(BACKWARD);
  speed = abs(speed);
  if(speed > 255)
    speed = 255;
  motorR->setSpeed((byte)speed);
}

void setup()
{
  // Bluetooth init
  Serial.begin(115200);

  pinMode(ledje, OUTPUT);

  EEPROM.get(0, mydata);
  if (mydata.pval1 == -1) {
    // put initial values in eeprom
    mydata.pval1 = Pdefault; mydata.ival1 = Idefault; mydata.dval1 = Ddefault;
    mydata.pval2 = Pdefault; mydata.ival2 = Idefault; mydata.dval2 = Ddefault;
    mydata.setpoint1 = 0; mydata.setpoint2 = 0;
    mydata.serialnumber = SERIAL; mydata.version = VERSION; mydata.eepromcnt = 0;
    EEPROM.put(0, mydata);
  } else {
    encoder1 = mydata.setpoint1;
    encoder2 = mydata.setpoint2;
  }
  
  randomSeed(encoder1+encoder2+mydata.eepromcnt);
  
  // motor shield init
  AFMS.begin();
  motorL->setSpeed(0);
  motorL->run(RELEASE);
  motorR->setSpeed(0);
  motorR->run(RELEASE);

  //  encoders
  pinMode(m_ena1, INPUT);
  pinMode(m_ena2, INPUT);
  pinMode(m_enb1, INPUT);
  pinMode(m_enb2, INPUT);

  attachInterrupt(4, encoder1Event, CHANGE);  // PC2, D7
  attachInterrupt(5, encoder2Event, CHANGE);  // PC3, D8

  Timer1.initialize(10000);  // 0.01 second period
  Timer1.attachInterrupt(timer1_int);
}

void timer1_int(void)        // interrupt service routine 
{
  int diff, sp;

  timer1_counter++;
  debugcnt++;
  
  // set actual setpoints
  if (timesw == 0) {
    setp1 = mydata.setpoint1;
    setp2 = mydata.setpoint2;
  }
  else { // new position every second
    if (timer1_counter%100 == 0) {
      digitalWrite(ledje, !digitalRead(ledje));
      setp1 = GEAR*random(0,20);
      setp2 = GEAR*random(0,20);
      timesw--;
      if (timesw == 0) {
	motors_stop();
      }
    }
  }
	
  // Calculate correction using PID
  // Encoder 1
  diff = setp1 - encoder1;
  sp =  oldenc1 - encoder1;      // speed: distance per 0.01 sec.
  // integrate: add diffs with a maximum value
  // intgr = ...
  oldenc1 = encoder1;
  // PID 1
  corr1 = mydata.pval1*diff + mydata.dval1*sp;   // + ival2*intgr;

  // Encoder 2
  diff = setp2- encoder2;
  sp =  oldenc2 - encoder2;      //  speed per 0.01 sec.
  // intgr = ...
  oldenc2 = encoder2;
  // PID 2
  corr2 = mydata.pval2*diff + mydata.dval2*sp;   // + ival2*intgr;


  if (control) { // control loop active
    // enable interrupts: needed for Wire library used for motor shield.
    interrupts();
    motorL_speed(corr1);
    motorR_speed(corr2);
  }
  debugcnt--;
}


void loop()
{
  int spp;

  // Default response
  sprintf(printstr, "ack\n\r");

  // read a line
  if(Serial.available() > 0) 
    {
      received = Serial.read();
      switch(received) {
      case '\r' :
	if(comind > 0) {
	  comavail = 1;
	  command[comind] = 0;
	  command[comind+1] = 0;
	  command[comind+2] = 0;
	  // verbinding betrouwbaar maken?
	  //   add checksum, ack, retries
	}
	
	break;
      default:
	command[comind] = received;
	comind++;
	if(comind > 9)  // throw away commands that are too long.
	  comind = 0;
	break;
      }
    }

  // parse command, clears buffer
  if(comavail) {

    /* Command structure
       First char: command type.
       Optional second char: boolean,
                             motor number 1,2
       Optional last values: int value

       We assume all commands are correct.
     */

    mot = command[1] - 'l';     // 'l' (motorL) else (motorR)

    switch (command[0]) {
    case 'i':                   // return info
      sprintf(printstr, "Info: %d, %d, ", control, timesw);
      Serial.print(printstr);
      sprintf(printstr, "SP:   %d, %d, ", mydata.setpoint1, mydata.setpoint2);
      Serial.print(printstr);
      sprintf(printstr, "ENC:  %d,   %d\n\r", encoder1, encoder2);
      comm = 11;   // to stop calibration and motor
      break;
    case 'J' :
      sprintf(printstr, "S: %d, %d, %d\n\r", mydata.serialnumber, mydata.version, mydata.eepromcnt);
      break;
    case 'a':                   // return pid values
      if (!mot) {
	sprintf(printstr, "PID1: %d, %d, %d\n\r", mydata.pval1, mydata.ival1, mydata.dval1);
      }
      else {
	sprintf(printstr, "PID2: %d, %d, %d\n\r", mydata.pval2, mydata.ival2, mydata.dval2);
      }
      break;
    case 'X':                   //  save/retrieve data to EEPROM
      if(command[1] == '0')
        myEEPROMput();
      else
        EEPROM.get(0, mydata);
      break;
    case 'k':
      timesw = 0;
      motors_stop();
      break;

    // next cases have motor number as a second char, use mot variable
    case 's':               // set speed motor
      if (!mot) {
	speed1 = atoi(command+2);
	sprintf(printstr, "speedL: %d, %d\n\r", speed1, speed2);
	motorL_speed(speed1);
      }
      else {
	speed2 = atoi(command+2);
	sprintf(printstr, "speedR: %d, %d\n\r", speed1, speed2);
	motorR_speed(speed2);
      }
      break;
    case 'e':               // return encoder values
      sprintf(printstr, "Enc: %d, %dX\n\r", encoder1, encoder2);
      break;
    case 'P':               // P value  (-1 to reset eeprom values)
      if (!mot)
	mydata.pval1 = atoi(command+2);
      else
	mydata.pval2 = atoi(command+2);
      break;
    case 'I':               // I value
      if (!mot)
	mydata.ival1 = atoi(command+2);
      else
	mydata.ival2 = atoi(command+2);
      break;
    case 'D':               // D value
      if (!mot)
	mydata.dval1 = atoi(command+2);
      else
	mydata.dval2 = atoi(command+2);
      break;
    case 'm':               //  set setpoint
      timer1_counter = 0;
      if (!mot)
	mydata.setpoint1 = atoi(command+2);
      else
	mydata.setpoint2 = atoi(command+2);
      control = 5;
      break;

    case 'p':               //  set setpoint grof.
      timer1_counter = 0;
      if (!mot)
	mydata.setpoint1 = GEAR*atoi(command+2);
      else
	mydata.setpoint2 = GEAR*atoi(command+2);
      control = 5;
      break;

    // waggel x keer heen en weer
    case 'w':               //  set setpoint
      timesw  = atoi(command+1);
      sprintf(printstr, "Timesw: %d\n\r", timesw);
      timer1_counter = 0;
      control = 1;
      break;

    case 'c':               // control 0: off, else on
      control = !(command[1] == '0');
      sprintf(printstr, "Control: %d\n\r", control);
      break;

    // calibrate positions. Make it run into the end-stops.
    case 'q':
      spp = atoi(command+1);
      comm = 1;
      break;

    // zero encoder values
    case 'z':
      noInterrupts();
      encoder1 = 0;
      encoder2 = 0;
      mydata.setpoint1 = 0;
      mydata.setpoint2 = 0;
      interrupts();
      break;
    case 'S' :
      mydata.serialnumber = atoi(command+1);
      break;
    case 'V' :
      mydata.version = atoi(command+1);
      break;
    default:
      // comm = 0;
      sprintf(printstr, "NACK\n\r");
      break;
    }
    // evt. alleen print indien ongelijk aan "ack"
    Serial.print(printstr);
    // prepare for next command
    comavail = 0;
    comind = 0;
  }

  // execute calibrate, 'q', command
  //  noodstop via command: i
  switch (comm) {
  case 0:
    break;
  case 1:
    control = 0;   // no controlling now
    timer1_counter = 0;
    motorL_speed(-spp);
    motorR_speed(-spp);
    comm = 2;
    break;
  case 2:
    // to get started
    if (timer1_counter > 10) {
      //encoders
      calenc1 = encoder1;
      calenc2 = encoder2;
      timer1_counter = 1;
      comm = 3;
    }
    break;
  case 3:    // both motors
    if (timer1_counter % 20 == 0) {
      timer1_counter++;
      // stop als het echt te lang duurt.
      if (timer1_counter >  700) {
	comm = 10;
	break;
      }
      if ((abs(calenc1 - encoder1) < 10)) {
	motorL->run(RELEASE);
	motorL->setSpeed(0);
	comm = 4;
      }
      if ((abs(calenc2 - encoder2) < 10)) {
      motorR->run(RELEASE);
      motorR->setSpeed(0);
      if (comm == 4) comm = 6;
      else comm = 5;
      }
      else {
      calenc1 = encoder1;
      calenc2 = encoder2;
      }
    }
    break;
  case 4:   // only motorR
    if (timer1_counter % 20 == 0) {
      timer1_counter++;
      // stop als het echt te lang duurt.
      if (timer1_counter >  700) {
	comm = 10;
	break;
      }
      if ((abs(calenc2 - encoder2) < 10)) {
      motorR->run(RELEASE);
      motorR->setSpeed(0);
      comm = 6;
      }
      else {
      calenc2 = encoder2;
      }
    }
    break;
  case 5:  // only motorL
    if (timer1_counter % 20 == 0) {
      timer1_counter++;
      // noodstop als het echt te lang duurt.
      if (timer1_counter >  700) {
	comm = 10;
	break;
      }
      if ((abs(calenc1 - encoder1) < 10)) {
      motorL->run(RELEASE);
      motorL->setSpeed(0);
      comm = 6;
      }
      else {
      calenc1 = encoder1;
      }
    }
    break;
  case 6: // both, move a bit out of the stop
    mydata.setpoint1 = encoder1 + 100;
    mydata.setpoint2 = encoder2 + 100;
    control = 1;
    timer1_counter = 0;
    comm = 7;
    break;
  case 7:
    if (timer1_counter > 30) {
      motors_stop();
      // update variables
      mydata.setpoint1 = 0;
      mydata.setpoint2 = 0;
      encoder1 = mydata.setpoint1;
      encoder2 = mydata.setpoint2;
      myEEPROMput();
      comm = 0;
      Serial.print("Caldone\n\r");
    }
    break;
  case 10:
    motors_stop();
    comm = 0;
    Serial.print("Calfailed\n\r");
    break;
  case 11:
    motors_stop();
    comm = 0;
    break;
  default:
    comm = 0;
    break;
  }

  // Monitor position commands: stop controlling, release motors
  switch (control) {
  case 5:
    if (timer1_counter > 30) control = 4;
    break;
  case 4:
    if ( (abs(mydata.setpoint1 - encoder1) < 10) &&
	 (abs(mydata.setpoint2 - encoder2) < 10)) {
      timer1_counter = 0;
      control = 3;
    } else {
      if (timer1_counter > 300) {
	motors_stop();
	Serial.print("P failed!\n\r");
      }
    }
    break;
  case 3:
    if (timer1_counter > 20) {
      motors_stop();
    }
    break;
  default:
    break;
  }

  // tijdelijk
  if (debugcnt) {
    sprintf(printstr, "Debugcnt; %d\n\r", debugcnt);
    Serial.print(printstr);
  }
}


// encoder event interrupts
void encoder1Event() {
  if (digitalRead(m_ena1) == HIGH) {
    if (digitalRead(m_ena2) == LOW) {
      encoder1++;
    } else {
      encoder1--;
    }
  } else {
    if (digitalRead(m_ena2) == LOW) {
      encoder1--;
    } else {
      encoder1++;
    }
  }
}

void encoder2Event() {
  if (digitalRead(m_enb1) == HIGH) {
    if (digitalRead(m_enb2) == LOW) {
      encoder2++;
    } else {
      encoder2--;
    }
  } else {
    if (digitalRead(m_enb2) == LOW) {
      encoder2--;
    } else {
      encoder2++;
    }
  }
}
