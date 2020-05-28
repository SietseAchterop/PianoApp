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

#include <EEPROM.h>
#include <TimerOne.h>

// adresses in EEPROM
#define Pval1adr  0
#define Ival1adr  4
#define Dval1adr  8
#define Pval2adr 12
#define Ival2adr 16
#define Dval2adr 20
#define pos1adr  24
#define pos2adr  28
#define SERNUM   32
#define VERNUM   36
#define EEPCNT   40
#define freeadr  44

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
int setpoint1, setpoint2;
volatile int corr1, corr2, setp1, setp2, ran = 0;
int calenc1, calenc2;

byte mot;
/* control w, cal, m and p commands set control
   0: off
   1: w, cal
   5: m, p
 */
byte control  = 0;

// init from EEPROM/FSM
#define Pdefault 3
#define Idefault 0
#define Ddefault 1
unsigned int pval1, ival1, dval1, pval2, ival2, dval2;

// todo
unsigned int serialnumber, version, eepromcnt;

//
byte comavail  = 0;   // a command is read
char command[10];     // command buffer
byte comind    = 0;   // index into buffer
byte comm      = 0;   // command to be executed
char printstr[22] ;    // string for printing. Langer dan 20 over ble lijkt onbetrouwbaar
// debugging
char printstr2[22] ;
byte debug       = 1;
unsigned int debugcnt = 0;

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

  retrieve_from_eeprom();
  if (pval1 == -1) {
    // put initial values in eeprom
    pval1 = Pdefault; ival1 = Idefault; dval1 = Ddefault;
    pval2 = Pdefault; ival2 = Idefault; dval2 = Ddefault;
    encoder1 = 0; encoder2 = 0; setpoint1 = 0; setpoint2 = 0;
    serialnumber = SERIAL; version = VERSION; eepromcnt = 0;
    save_to_eeprom(false);
  } else {
    setpoint1 = encoder1;
    setpoint2 = encoder2;
  }
  
  
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
    setp1 = setpoint1;
    setp2 = setpoint2;
  }
  else { // iedere seconde
    if (timer1_counter%100 == 0) {
      digitalWrite(ledje, !digitalRead(ledje));
      setp1 = ran;
      setp2 = ran;
      ran = (ran + 321)%3000;
      timesw--;
      if (timesw == 0) {
	//afhandelen als een setpoint commando
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
  corr1 = pval1*diff + dval1*sp;   // + ival2*intgr;

  // Encoder 2
  diff = setp2- encoder2;
  sp =  oldenc2 - encoder2;      //  speed per 0.01 sec.
  // intgr = ...
  oldenc2 = encoder2;
  // PID 2
  corr2 = pval2*diff + dval2*sp;   // + ival2*intgr;


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
      sprintf(printstr, "SP:   %d, %d, ", setpoint1, setpoint2);
      Serial.print(printstr);
      sprintf(printstr, "ENC:  %d,   %d\n\r", encoder1, encoder2);
      comm = 10;   // to stop calibration and motor
      break;
    case 'J' :
      sprintf(printstr, "S: %d, %d, %d\n\r", serialnumber, version, eepromcnt);
      Serial.print(printstr);
      break;
    case 'a':                   // return pid values
      if (!mot) {
	sprintf(printstr, "PID1: %d, %d, %d\n\r", pval1, ival1, dval1);
      }
      else {
	sprintf(printstr, "PID2: %d, %d, %d\n\r", pval2, ival2, dval2);
      }
      break;
    case 'X':                   //  save/retrieve data to EEPROM
      if(command[1] == '0')
	save_to_eeprom(true);   // use encoder values
      else
	if  (command[1] == '2')
	  save_to_eeprom(false);  // use setpoints
	else
	  retrieve_from_eeprom();	  
      break;
    case 'k':
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
	pval1 = atoi(command+2);
      else
	pval2 = atoi(command+2);
      break;
    case 'I':               // I value
      if (!mot)
	ival1 = atoi(command+2);
      else
	ival2 = atoi(command+2);
      break;
    case 'D':               // D value
      if (!mot)
	dval1 = atoi(command+2);
      else
	dval2 = atoi(command+2);
      break;
    case 'm':               //  set setpoint
      timer1_counter = 0;
      control = 5;
      if (!mot)
	setpoint1 = atoi(command+2);
      else
	setpoint2 = atoi(command+2);
      break;

    case 'p':               //  set setpoint grof. Saves setpoints.
      timer1_counter = 0;
      control = 5;
      if (!mot)
	setpoint1 = GEAR*atoi(command+2);
      else
	setpoint2 = GEAR*atoi(command+2);
      save_to_eeprom(false);
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

    // initialisatie procedure, door motoren kort vast te laten lopen.
    case 'q':
      //   beide motoren even met snelheid x laten lopen, met timer1_counter:  qx
      spp = atoi(command+1);
      comm = 1;
      break;

    // zero encoder values
    case 'z':
      noInterrupts();
      encoder1 = 0;
      encoder2 = 0;
      setpoint1 = 0;
      setpoint2 = 0;
      interrupts();
      break;
    case 'S' :
      serialnumber = atoi(command+1);
      save_to_eeprom(false);
      break;
    default:
      // comm = 0;
      sprintf(printstr, "NACK\n\r");
      break;
    }
    Serial.print(printstr);
    // prepare for next command
    comavail = 0;
    comind = 0;
  }

  // execute 'q' command
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
    if (timer1_counter > 40) {
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
    setpoint1 = encoder1 + 100;
    setpoint2 = encoder2 + 100;
    control = 1;
    timer1_counter = 0;
    comm = 7;
    break;
  case 7:
    if (timer1_counter >  30) {
      motors_stop();
      // update variables
      setpoint1 = 0;
      setpoint2 = 0;
      encoder1 = setpoint1;
      encoder2 = setpoint2;
      save_to_eeprom(false);
      comm = 0;
    }
    break;
  case 10:
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
    if ( (abs(setpoint1 - encoder1) < 10) &&
	 (abs(setpoint2 - encoder2) < 10)) {
      timer1_counter = 0;
      control = 3;
    }
    break;
  case 3:
    if (timer1_counter > 20) {
      motors_stop();
      //      Serial.print("Stopped.\n\r");
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


void save_to_eeprom(bool savenc) {

  eepromcnt++;
  
  EEPROM.put(Pval1adr, pval1);
  EEPROM.put(Ival1adr, ival1);
  EEPROM.put(Dval1adr, dval1);
  EEPROM.put(Pval2adr, pval2);
  EEPROM.put(Ival2adr, ival2);
  EEPROM.put(Dval2adr, dval2);
  if (savenc) {
    EEPROM.put(pos1adr, encoder1);
    EEPROM.put(pos2adr, encoder2);
  } else {
    EEPROM.put(pos1adr, setpoint1);
    EEPROM.put(pos2adr, setpoint2);
  }
  EEPROM.put(SERNUM,  serialnumber);
  EEPROM.put(VERNUM,  version);
  EEPROM.put(EEPCNT,  eepromcnt);    
}

void retrieve_from_eeprom() {

  // indien -1, default waarde nemen
  EEPROM.get(Pval1adr, pval1);
  EEPROM.get(Ival1adr, ival1);
  EEPROM.get(Dval1adr, dval1);
  EEPROM.get(Pval2adr, pval2);
  EEPROM.get(Ival2adr, ival2);
  EEPROM.get(Dval2adr, dval2);

  EEPROM.get(pos1adr, encoder1);
  EEPROM.get(pos2adr, encoder2);

  EEPROM.get(SERNUM,  serialnumber);
  EEPROM.get(VERNUM,  version);
  EEPROM.get(EEPCNT,  eepromcnt);    
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
