/******* Command processing for PianoApp

Included in main.c

Motors met verschillende vertragingen:
het aantal tandwieltjes varieert en daarmee de richting waarin de motor draait!
Evt aanpassingen nodig:
 encoder_Event:   ++ en -- verwisselen
 control loop:    corr1 en corr2 minteken erbij of af
 calibreren       CALSPEED met of zonder minteken

*******/

#define VERSION 0

// set in main.c:  ble_evt_handler
bool connected = false;

// prototypes
void send_back(char * message);
void pwm_update_duty_cycle(uint8_t d_cycle1, uint8_t d_cycle2);
void encmotor(bool on);
void motors_stop(void);
void motorAB_speed(int8_t speedA, int8_t speedB);
void fstore_mydata(void);
void my_scheduler_event_handler(void *p_event_data, uint16_t event_size);

// my tasks  (NIET meerdere tasks tegelijkertijd)
enum Task  { None, Final, Cal, Test  };
uint8_t curTask = None;
// state within task, set when task is called
//   a single variable, only one task at a time.
uint8_t state = 0;

// To ignore commands while e.g. Calibrate is in progress
bool onhold = false;

// EEPROM data structure
struct SavedData {
  unsigned int pval1, ival1, dval1, pval2, ival2, dval2;
  int setpoint1, setpoint2;
  unsigned int serialnumber, version, eepromcnt, accucal, stepsdone, gear;
};
// default for accu calibration:   ACCUCAL/10^7 * 2^14 = 8 Volt
#define ACCUCAL 5400


struct SavedData mydata;

// version of mydata in flash, set in the linkerscript (to just before the bootloader)
extern const uint32_t mydatapage[100];

/****** TB6612 control, encoder ***  eerste prototype
#define stby  ARDUINO_A7_PIN
// MotorA (L)
#define ain1  ARDUINO_9_PIN
#define ain2  ARDUINO_8_PIN
#define pwma  ARDUINO_6_PIN
#define enca1 ARDUINO_2_PIN
#define enca2 ARDUINO_3_PIN
// MotorB (R)
#define bin1 ARDUINO_10_PIN
#define bin2 ARDUINO_11_PIN
#define pwmb ARDUINO_12_PIN
#define encb1 ARDUINO_4_PIN
#define encb2 ARDUINO_5_PIN
***/
/****** TB6612 control, encoder ***

   Note: enca and encb are interchanged in the schematics!!
         the error is in the schematic.
****/
#define stby      ARDUINO_A4_PIN
#define encpower  ARDUINO_12_PIN

// MotorA (L)
#define ain1 ARDUINO_6_PIN
#define ain2 ARDUINO_5_PIN
#define pwma ARDUINO_4_PIN
#define enca1 ARDUINO_2_PIN
#define enca2 ARDUINO_3_PIN

// MotorB (R)
#define bin1 ARDUINO_7_PIN
#define bin2 ARDUINO_8_PIN
#define pwmb ARDUINO_9_PIN
#define encb1 ARDUINO_10_PIN
#define encb2 ARDUINO_11_PIN

// analoge input van battery spanning
#define spanning ARDUINO_A3_PIN
//  in Nordic sdk: NRF_SAADC_INPUT_AIN5

// update firmware pin
#define update ARDUINO_A7_PIN

float battery = 8.0;
// an error occured 0; no error, 1 error, 2 notified app
int error = 0;
volatile uint32_t timer_counter = 0, test_counter = 0, starttime = 0, tellertje = 0, secondes = 0;

// motor controller
uint8_t control  = 0;
int speed1 = 0, speed2 = 0;    // intended motor speed

volatile int encoder1 = 0, encoder2 = 0;
volatile int oldenc1, oldenc2;
volatile int corr1, corr2, setp1, setp2;
int calenc1, calenc2;

// command response
char response[50];
char commbuffer[50];

float willekeurig;

// Testing, tijdelijk
uint32_t tijden[500];
int16_t encs[500];
int16_t corrs[500];
uint16_t tindex = 0;
uint16_t running = 0;
volatile uint32_t upp=0, downn=0;

/*
  wat is de vermenigvuldigingsfactor naar encoder waarden?
     x stappen per omwenteling
     y spoed spindel in mm verplaatsing per omwenteling
     z mm verplaatsing per stapje
  factor: z*x/y
  bv: 0,5 * 1000 / 2 = 250
*/
#define GEARVALUE 8300

// default PID values
#define Pdefault 4
#define Idefault 0
#define Ddefault 1

// goto error state when reached
#define TIMEOUT 4000

void timer_start(void) {
  uint32_t err_code;
  err_code = app_timer_start(my_timer_id, APP_TIMER_TICKS(10), NULL);
  APP_ERROR_CHECK(err_code);
  timer_counter = 0;
}

void timer_stop(void) {
  uint32_t err_code;
  err_code = app_timer_stop(my_timer_id);
  APP_ERROR_CHECK(err_code);
  control = 0;
}

// initial data in flash
void set_mydata(void) {
  mydata.pval1 = Pdefault; mydata.ival1 = Idefault; mydata.dval1 = Ddefault;
  mydata.pval2 = Pdefault; mydata.ival2 = Idefault; mydata.dval2 = Ddefault;
  mydata.setpoint1 = 0; mydata.setpoint2 = 0;
  mydata.serialnumber = atoi(serial);
  mydata.version = VERSION;
  mydata.eepromcnt = 1; mydata.accucal = ACCUCAL;
  mydata.stepsdone = 0;
  mydata.gear = GEARVALUE;
  fstore_mydata();
}

// called at powerup in main.c
void mydata_init(void) {
  if (mydatapage[0] == 0xffffffff) {
    set_mydata();
 } else {
    memcpy((void *)&mydata, (void *)mydatapage, sizeof(mydata));
    encoder1 = mydata.setpoint1;
    encoder2 = mydata.setpoint2;
    sprintf(serial, "%d", mydata.serialnumber);
    strncpy(name+9,serial, 3);
    strncpy(((char *)passkey)+3, serial, 3);
  }
}

/**
Process commands.

 xypar1,par2
    x:    command char
    y:    l for left, otherwise right
    par1: first number
 optional
     ,;   separator
  par2:   second number

  We assume only correct commands!

 **/
char * process(char * command)
{
  int par1, par2 = 0;
  char motL = !(command[1] - 'l');     // 'l' (motorL) else (motorR)

  // find separator
  char * sep = index(command, (int)',');
  if (sep == NULL)
    par1 = atoi(command+2);
  else {
    int s = (int)(sep-command);
    command[s] = 0;
    par1 = atoi(command+2);
    par2 = atoi(command+s+1);
  }
  
  // default response
  sprintf(response, "PApp: %.2f %d %d %d", battery, encoder1, encoder2, mydata.gear);
  if (onhold) return response;

  switch (command[0]) {
  case 'i':                   // return info
    NRF_LOG_INFO("--> Command: i");
    // Use default response
    break;
  case 'm':               //  set setpoints (small steps)
    mydata.setpoint1 = par1;
    mydata.setpoint2 = par2;
    state = 0;
    curTask = Final;
    control = 1;
    timer_start();
    break;
  case 'p':               //  set setpoints:  bijv. pp2,3
    if ((battery > 6.5) && (battery < 20)) {
      mydata.setpoint1 = mydata.gear*par1;
      mydata.setpoint2 = mydata.gear*par2;
      state = 0;
      curTask = Final;
      control = 1;
      timer_start();
      mydata.stepsdone += 1;
      secondes = 0;
    }
    else
      if (battery > 20)
	sprintf(response, "Error mode! Turn device off and then on.");
      else
	sprintf(response, "Battery voltage: %.2f. Charge battery!", battery);
    break;
  case 's' :              // test motors separately
    if (motL)
      speed1 = par1;
    else
      speed2 = par1;
    sprintf(response, "mAB speed: %d, %d", speed1, speed2);
    motorAB_speed(speed1, speed2);
    if (speed1 == 0 && speed2 == 0) encmotor(false);

    break;
  case 'e' :
    sprintf(response, "Corr: %d, %d, ee: %d, st: %d", corr1, corr2, mydata.eepromcnt, mydata.stepsdone);
    break;
  case 'f':                   // print PID values
    sprintf(response, "PID1: %d, %d, %d", mydata.pval1, mydata.ival1, mydata.dval1);
    send_back(response);
    sprintf(response, "PID2: %d, %d, %d", mydata.pval2, mydata.ival2, mydata.dval2);
    break;
  case 'g':                   // print mydata
    sprintf(response, "serial: %d, version: %d, gear: %d", mydata.serialnumber, mydata.version, mydata.gear);
    break;
  case 'X':
    sprintf(response, "Init mydata.");
    set_mydata();
    break;
  case 'Y':
    sprintf(response, "Zero encoders. Secs: %ld", secondes);
    encoder1 = 0; encoder2 = 0;
    /** tijdelijk
    for (uint32_t i=0; i<500; i++) {
      tijden[i] = 0; encs[i] = 0; corrs[i] = 0;
    }
    tellertje = 0;
    upp=0; downn=0;
    */
    break;
  case 'E':                   // error in positioning
    error = 1;
    sprintf(response, "Error in pos!:");
    // needed to make sure that calibrate after software upload works.
    fstore_mydata();
    break;
  case 'F':                   // flash mydatapage
    sprintf(response, "flash mydatapage.");
    fstore_mydata();
    break;
  case 'S':                   // set serial number (and passkey)
    if (par1 < 100 || par1 > 999) {
      sprintf(response, "Ignored!");
      break;
    }
    mydata.serialnumber = par1;
    sprintf(response, "new passkey: 100%d", par1);
    fstore_mydata();
    // new name and passkey will be picked at next reboot.
    break;
  case 'G':                   // set new gear value
    if (par1 < 1 || par1 > 100000) {
      sprintf(response, "Ignored!");
      break;
    }
    mydata.gear = par1;
    sprintf(response, "new gear value: %d", par1);
    fstore_mydata();
    break;
  case 'Z':
    for (uint32_t i=0; i<500; i++) {
      sprintf(response, "%lu, %d, %d", tijden[i], encs[i], corrs[i]);
      send_back(response);
    }
    break;
  case 'r' :
    // RTC counter
    sprintf(response, "Counter %ld", NRF_RTC0->COUNTER);
    for (uint32_t i=0; i<500; i++) {
      tijden[i] = i;
    }
    tindex = 0;
    break;
  case 'P':               // P values  (-1 to reset eeprom values)
    mydata.pval1 = par1;
    mydata.pval2 = par2;
    break;
  case 'I':               // I values
    mydata.ival1 = par1;
    mydata.ival2 = par2;
    break;
  case 'D':               // D values
    mydata.dval1 = par1;
    mydata.dval2 = par2;
    break;
  case 'A':               // set accucal data
    if (motL) {
      mydata.accucal = par1;
      sprintf(response, "New accucal %d", mydata.accucal);
    }
    else {
      sprintf(response, "Accucal %d", mydata.accucal);
    }
    break;
  case 'T' :
    for (uint32_t i=0; i<500; i++) {
      tijden[i] = 0; encs[i] = 0; corrs[i] = 0;
    }
    sprintf(response, "Test motors\n\r");
    test_counter = par1;
    if (test_counter == 0) {
      sprintf(response, "End test %ld", test_counter);
      if (curTask == Test)
        curTask = Final;
    } else {

      tellertje = 0;
 
      control = 1;
      state = 0;
      curTask = Test;
      timer_start();
    }
    break;
  case 'C' :
    sprintf(response, "Calibrate");
    state = 0;
    curTask = Cal;
    onhold = true;
    if (mydata.stepsdone == 1000) { // hack
      onhold = false;
      mydata.stepsdone = 0;
    }
    timer_start();
    break;
  case 't' :   // timer start en stop      Alleen voor testen
    if (motL) {
      timer_start();
    } else {
      timer_stop();
    }
    break;
  case 'q' :   // test high site switch   tijdelijk
    if (motL) {
      encmotor(true);
    } else {
      encmotor(false);
    }
    break;
  case 'c' : // control output on and off   Alleen voor testen
    if (motL) {
      // voorlopig
      //      mydata.setpoint1 = encoder1;
      //      mydata.setpoint2 = encoder2;
      starttime = NRF_RTC0->COUNTER;
      timer_counter = 0;
      control = 1;
      running = 1;
    }
    else {
      control = 0;
      motors_stop();
      mydata.setpoint1 = 0; mydata.setpoint1 = 0;
      encoder1 = 0; encoder2 = 0;
      curTask = None;
    }
    break;
  default:
    break;
  }

  return response;  
}

// Motor controller
void motors_stop(void) {
  // stop controller interrupts also to save power? after some delay to make this brake work?

  // short brake to stop motor quickly
  nrf_gpio_pin_set(ain1);
  nrf_gpio_pin_set(ain2);
  nrf_gpio_pin_set(bin1);
  nrf_gpio_pin_set(bin2);

  // put into standby.
  //   some delay (use timer interrupt?)
  /*  save current encoder and log when int occurs when enabled again?
      events when disabled?
   */
  motorAB_speed(0, 0);
  encmotor(false);
}

// control motor together
// 8 of 32 bit argumenten, wat is sneller?
void motorAB_speed(int8_t speedA, int8_t speedB) {
  
  uint8_t dirA = 0, dirB = 0;
  
  // turn motors on
  encmotor(true);

  if (speedA >= 0) dirA = 1;
  else speedA = -speedA;
  if (speedA > 100) speedA = 100;
  if (dirA) {
    nrf_gpio_pin_set(ain1);
    nrf_gpio_pin_clear(ain2);
  }
  else {
    nrf_gpio_pin_clear(ain1);
    nrf_gpio_pin_set(ain2);
  }

  if (speedB >= 0) dirB = 1;
  else speedB = -speedB;
  if (speedB > 100) speedB = 100;
  if (dirB) {
    nrf_gpio_pin_set(bin1);
    nrf_gpio_pin_clear(bin2);
  }
  else {
    nrf_gpio_pin_clear(bin1);
    nrf_gpio_pin_set(bin2);
  }

  // convert to pwm values
  speedA = 100 - speedA;
  speedB = 100 - speedB;
  pwm_update_duty_cycle(speedA, speedB);
}

/************  calibration *******

motor naar eindstop
 Na calibratie op +5
 Andere einde is -5

 *******/

#define CALSPEED  -70

// "position" before calibration
int current1, current2;

// start calibrate with state = 0
void calibrate(void) {
  char command[4];
  
  switch (state) {
  case 0:
    timer_counter = 0;
    // remember where we are supposed to be
    current1 = mydata.setpoint1;
    current2 = mydata.setpoint1;
    motorAB_speed(CALSPEED, CALSPEED);
    // print vanuit interrupt?
    //sprintf(response, "CAL start: %d, %d", encoder1, encoder2);
    //send_back(response);
    state = 1;
  case 1:
    // to get started
    if (timer_counter > 20) {
      //encoders
      calenc1 = encoder1;
      calenc2 = encoder2;
      timer_counter = 0;
      state = 2;
    }
    break;
  case 2:    // both motors
    if (timer_counter % 20 == 0) {
      // stop als het echt te lang duurt.
      if (timer_counter >  TIMEOUT) {
        state = 10;
        break;
      }
      // test if there is any movement
      if ((abs(calenc1 - encoder1) < 500)) {
	// we assume motorL is at stop
        motorAB_speed(0, CALSPEED);
        state = 3;
      }
      if ((abs(calenc2 - encoder2) < 500)) {
        if (state == 3) {
          motorAB_speed(0, 0);
          state = 5;
        }
        else {
          motorAB_speed(CALSPEED, 0);
          state = 4;
        }
      }
      calenc1 = encoder1;
      calenc2 = encoder2;
    }
    break;
  case 3:   // only motorR
    if (timer_counter % 20 == 0) {
      // stop als het echt te lang duurt.
      if (timer_counter >  TIMEOUT) {
        state = 10;
        break;
      }
      if ((abs(calenc2 - encoder2) < 500)) {
        motorAB_speed(0, 0);
        state = 5;
      }
      calenc2 = encoder2;
    }
    break;
  case 4:  // only motorL
    if (timer_counter % 20 == 0) {
      // noodstop als het echt te lang duurt.
      if (timer_counter >  TIMEOUT) {
        state = 10;
        break;
      }
      if ((abs(calenc1 - encoder1) < 500)) {
        motorAB_speed(0, 0);
        state = 5;
      }
      calenc1 = encoder1;
    }
    break;
  case 5:
    // Both at the stop, now move back one step.
    // We now are at 5*gear + 500 and need to go to current
    //
    encoder1 = 5*mydata.gear + 2000;
    encoder2 = 5*mydata.gear + 2000;
    mydata.setpoint1 = current1;
    mydata.setpoint2 = current2;
    //    sprintf(response, "CAL done: %d, %d", encoder1, encoder2);
    //    send_back(response);

    state = 6;
    break;
  case 6:
    onhold = false;
    // nu gewoon als een normaal commando afmaken.
    state = 0;
    curTask = Final;
    control = 1;
    timer_counter = 0;  // reset timeout
    break;
  case 10:   // Error
    motors_stop();
    timer_stop();
    // voorlopig voor testen
    mydata.setpoint1 = 0;
    mydata.setpoint2 = 0;
    encoder1 = mydata.setpoint1;
    encoder2 = mydata.setpoint2;
    mydata.stepsdone = 0;
    // 
    onhold = false;
    strcpy(command, "E");
    app_sched_event_put(&command, 2, my_scheduler_event_handler);
    curTask = None;
    break;
  default:
    break;
  }
}

void final(void) {
  char command[4];
  
  switch (state) {
  case 2:
    // test for endposition
    if ( (abs(mydata.setpoint1 - encoder1) < 10) &&
         (abs(mydata.setpoint2 - encoder2) < 10)) {
      state = 3;
      timer_counter = 0;
    } else {
      if (timer_counter > TIMEOUT) {
        motors_stop();
        timer_stop();
        strcpy(command, "E");
        app_sched_event_put(&command, 2, my_scheduler_event_handler);
        curTask = None;
      }
    }
    break;
  case 0:
    timer_counter = 0;
    state = 2;
    break;
    //  case 1:
    // not needed
    //    if (timer_counter > 300)
    //      state = 2;
    //    break;
  case 3:
    if (timer_counter > 30)
      state = 4;
    break;
  case 4:
   // 2nd test for endposition
    if ( (abs(mydata.setpoint1 - encoder1) < 10) &&
         (abs(mydata.setpoint2 - encoder2) < 10)) {
      timer_counter = 0;
      state = 5;
    } else {
      if (timer_counter > 1000) {
        motors_stop();
        timer_stop();
        // error
        strcpy(command, "E");
        app_sched_event_put(&command, 2, my_scheduler_event_handler);
        curTask = None;
      }
    }
    break;
  case 5:
    if (timer_counter > 30) {
      motors_stop(); 
      timer_stop();  // kan dit in de interrupt routine?
      // save reached position in flash
      strcpy(command, "F");
      app_sched_event_put(&command, 2, my_scheduler_event_handler);
      curTask = None;
    }
    break;
  default:
    break;
  }

}

// random setpoint
uint32_t random(void) {
  char response[30];
  int willekeurig = mydata.gear*(rand()%12 - 6);
  sprintf(response, "rand %d", willekeurig);
  send_back(response);
  return willekeurig;
}

// nadeel, beide motoren moeten werken.
int ttt = 0;
void testing(void) {

  switch (state) {
  case 0:
    timer_counter = 0;
    if (test_counter == 0) {
      state = 2;
      break;
    } else {
      if (ttt == 0)
        ttt = 1000;
      else
        ttt = 0;
      mydata.setpoint1 = ttt;
      mydata.setpoint2 = ttt;
      state = 1;
    }
    test_counter--;
    break;
  case 1:
    if (timer_counter%200 == 0) {
      state = 0;
    }
    break;
  case 2:
    state = 0;
    curTask = Final;
    break;
  default:
    break;
  }
}

void testRandom(void) {

  switch (state) {
  case 0:
    timer_counter = 0;
    test_counter--;
    if (test_counter == 0) {
      state = 2;
      break;
    } else {
      mydata.setpoint1 = random();
      mydata.setpoint2 = random();
      state = 1;
    }
    break;
  case 1:
    if (timer_counter%300 == 0) {
      state = 0;
    }
    break;
  case 2:
    state = 0;
    curTask = Final;
    break;
  default:
    break;
  }
}


#define MAXSPEED 100
// set maximum torque at high speed to limit speed
#define LOWSPEED  60

/**@brief Handler with PID controller
 */
static void my_timer_handler(void * p_context)
{
  int diff, sp, enc;
  uint8_t speed;
  timer_counter++;
        
  // Calculate correction using PID
  // Encoder 1
  enc = encoder1;
  diff = enc - mydata.setpoint1;
  sp =  enc - oldenc1;      // speed: distance per 0.01 sec.
  // integrate: add diffs with a maximum value
  // intgr = ...
  oldenc1 = enc;
  // PID 1
  corr1 = mydata.pval1*diff + mydata.dval1*sp;   // + mydata.ival2*intgr;
  // limit correction, only at higher speed
  //   interrupts should not occur much faster than 1 per 500 microseconds (twice, once for each edge)
  //   meaning less than 20 increments per 10 milliseconds
  sp = abs(sp);
  if (sp > 20)
    speed = LOWSPEED;
  else
    speed = MAXSPEED;
  //
  corr1 = corr1/2;
  if (corr1 > speed) corr1 = speed;
  else if (corr1 < -speed) corr1 = -speed;

  // Encoder 2
  enc = encoder2;
  diff = enc - mydata.setpoint2;
  sp =  enc - oldenc2;      //  speed: distance per 0.01 sec.
  // intgr = ...
  oldenc2 = enc;
  // PID 2
  corr2 = mydata.pval2*diff + mydata.dval2*sp;   // + mydata.ival2*intgr;
  // limit speed
  sp = abs(sp);
  if (sp > 20)
    speed = LOWSPEED;
  else
    speed = MAXSPEED;
  //
  corr2 = corr2/2;
  if (corr2 > speed) corr2 = speed;
  else if (corr2 < -speed) corr2 = -speed;

  // motor actuation
  if (control) {
    motorAB_speed((int8_t)corr1, (int8_t)corr2);
  }

  // collect some data
  if (running) {
    tijden[tindex] = NRF_RTC0->COUNTER-starttime;
    encs[tindex] = encoder1;
    corrs[tindex] = corr1;
    tindex++;
    if (tindex > 500) { running = 0; tindex = 0; }
  }

  // other processing: calibration and motor testing
  switch (curTask) {
  case None:
    // normally not used, timer only running in a task.
    break;
  case Final:
    // switch off timer/controller after some time?
    //  start bij opgeven nieuw setpoint
    final();
    break;
  case Cal:
    calibrate();
    break;
  case Test:
    testing();
    break;
  }
}

static void battery_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    nrf_drv_saadc_sample();
    secondes += 1;
    // zet computertje uit na 24 uur geen pp commando
    if (secondes > 24*60*60) sd_power_system_off();
    //if (secondes > 2*60) sd_power_system_off();
    // en bij heel lage spanning
    if (battery < 6.0) sd_power_system_off();
}

static void create_timers()
{
  ret_code_t err_code;
  // Create timers
  err_code = app_timer_create(&my_timer_id,
                              APP_TIMER_MODE_REPEATED,
                              my_timer_handler);
  APP_ERROR_CHECK(err_code);

  err_code = app_timer_create(&m_battery_timer_id,
                              APP_TIMER_MODE_REPEATED,
                              battery_handler);
  APP_ERROR_CHECK(err_code);

  // runs always
  err_code = app_timer_start(m_battery_timer_id, APP_TIMER_TICKS(1000), NULL);
  APP_ERROR_CHECK(err_code);
}

// encoder motorL met 1 interrupt
void encoder1Event(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{

  /*
  if (tellertje < 500) {
    
    tijden[tellertje] = tellertje;
    encs[tellertje]   = encoder1;
    corrs[tellertje]  = encoder2;

    tellertje++;
  }
  */

  // PIN zou encoder moeten volgen. Wat is de vertraging?

  if (nrf_drv_gpiote_in_is_set(enca1)) {
    upp++;
    //    nrf_gpio_pin_set(ARDUINO_A5_PIN);
    if (!nrf_gpio_pin_read(enca2)) {
      encoder1++;
      //      nrf_gpio_pin_clear(ARDUINO_1_PIN);
      //      nrf_gpio_pin_clear(ARDUINO_0_PIN);
    } else {
      //      nrf_gpio_pin_set(ARDUINO_1_PIN);
      //      nrf_gpio_pin_set(ARDUINO_0_PIN);
      encoder1--;
    }
  } else {
    downn++;
    //    nrf_gpio_pin_clear(ARDUINO_A5_PIN);
    if (!nrf_gpio_pin_read(enca2)) {
      encoder1--;
      //      nrf_gpio_pin_set(ARDUINO_1_PIN);
      //      nrf_gpio_pin_set(ARDUINO_1_PIN);
    } else {
      encoder1++;
      //      nrf_gpio_pin_clear(ARDUINO_1_PIN);
      //      nrf_gpio_pin_clear(ARDUINO_1_PIN);
    }
  }
}

// encoder motorR met 1 interrupt
void encoder2Event(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
  if (nrf_drv_gpiote_in_is_set(encb1)) {
    //    nrf_gpio_pin_set(ARDUINO_A6_PIN);
    if (!nrf_gpio_pin_read(encb2)) {
      encoder2++;
    } else {
      encoder2--;
    }
  } else {
    //    nrf_gpio_pin_clear(ARDUINO_A6_PIN);
    if (!nrf_gpio_pin_read(encb2)) {
      encoder2--;
    } else {
      encoder2++;
    }
  }
}

// stby is met encpower niet meer nodig? (na testen de stby pin gebruiken?)
// pullup weerstand 10K ->  10uA standby
// Wel dus!
// Interrupts moeten ook nog uit? hoewel na UIT en AAN is er niks veranderd. (in alle gevallen?)
//   Dit zorgt wel voor fouten?
//   Niveau gaat naar vaste 2 Volt. 1 of 0?
void encmotor(bool on) {
  if (on) {
      nrf_gpio_pin_clear(encpower);
      nrf_gpio_pin_set(stby);
    }
  else {
    nrf_gpio_pin_set(encpower);
    nrf_gpio_pin_clear(stby);
  }
}


// Output via GPIO en input met interrupt met GPIOTE

static void gpio_init(void)
{
  
  // for low power (lijkt niet uit te maken)
  nrf_gpio_cfg_output(PIN_ENABLE_SENSORS_3V3);
  nrf_gpio_cfg_output(PIN_ENABLE_I2C_PULLUP);
  nrf_gpio_pin_clear(PIN_ENABLE_SENSORS_3V3);
  nrf_gpio_pin_clear(PIN_ENABLE_I2C_PULLUP);

  nrf_gpio_cfg_output(encpower);
  nrf_gpio_cfg_output(stby);
  nrf_gpio_cfg_output(ain1);
  nrf_gpio_cfg_output(ain2);
  nrf_gpio_cfg_output(pwma);
  nrf_gpio_cfg_output(bin1);
  nrf_gpio_cfg_output(bin2);
  nrf_gpio_cfg_output(pwmb);

  nrf_gpio_cfg_output(ARDUINO_A0_PIN);
  nrf_gpio_cfg_output(ARDUINO_A1_PIN);
  // test
  nrf_gpio_cfg_output(ARDUINO_A5_PIN);
  nrf_gpio_cfg_output(ARDUINO_A6_PIN);

  //nrf_gpio_cfg_output(LED_DL1);
  //nrf_gpio_cfg_output(LED_DL2);

  //nrf_gpio_pin_clear(LED_DL1);
  //nrf_gpio_pin_clear(LED_DL2);
  nrf_gpio_pin_set(LED_DL3_RED);
  nrf_gpio_pin_set(LED_DL3_GRN);
  nrf_gpio_pin_set(LED_DL3_BLU);

  encmotor(false);
  
  ret_code_t err_code;
  if(!nrf_drv_gpiote_is_init())
    {
      err_code = nrf_drv_gpiote_init();
      nrf_gpio_pin_set(LED_DL3_BLU);
      APP_ERROR_CHECK(err_code);
    }
 
  nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
  in_config.pull = NRF_GPIO_PIN_NOPULL;
  
  err_code = nrf_drv_gpiote_in_init(enca1, &in_config, encoder1Event);
  APP_ERROR_CHECK(err_code);
  err_code = nrf_drv_gpiote_in_init(encb1, &in_config, encoder2Event);
  APP_ERROR_CHECK(err_code);

  nrf_drv_gpiote_in_event_enable(enca1, true);
  nrf_drv_gpiote_in_event_enable(encb1, true);

  nrf_gpio_cfg_input(enca2, NRF_GPIO_PIN_NOPULL);
  nrf_gpio_cfg_input(encb2, NRF_GPIO_PIN_NOPULL);

  // test pin voor update via bootloader
  nrf_gpio_cfg_input(update, NRF_GPIO_PIN_PULLUP);

}


// saadc

#define SAMPLES_IN_BUFFER 1
static nrf_saadc_value_t     m_buffer_pool[2][SAMPLES_IN_BUFFER];
static uint32_t              m_adc_evt_counter;

void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        ret_code_t err_code;

        err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, SAMPLES_IN_BUFFER);
        APP_ERROR_CHECK(err_code);

        int i;
        NRF_LOG_INFO("ADC event number: %d", (int)m_adc_evt_counter);

        for (i = 0; i < SAMPLES_IN_BUFFER; i++)
        {
            NRF_LOG_INFO("%d", p_event->data.done.p_buffer[i]);
        }
        m_adc_evt_counter++;
        battery = (float)p_event->data.done.p_buffer[0] * ((float)mydata.accucal)/10000000;
        // to indicate error in the GUI
        if (error == 1) {
          battery = 100;
          sprintf(response, "PApp: %.2f %d %d %d", battery, encoder1, encoder2, mydata.gear);
          send_back(response);
          error = 2;
        }
        if (error == 2) {
          // to send only once
          battery = 100;
        }
    }
}


void saadc_init(void)
{
    ret_code_t err_code;
    nrf_saadc_channel_config_t channel_config =
      NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN5);
    channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

    err_code = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);

    nrf_saadc_resolution_set(NRF_SAADC_RESOLUTION_14BIT);
    nrf_saadc_oversample_set(NRF_SAADC_OVERSAMPLE_8X);
    nrf_saadc_continuous_mode_disable();
    nrf_saadc_burst_set(0, NRF_SAADC_BURST_ENABLED);
    // wordt dit allemaal goed overgenomen?

    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[0], SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[1], SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);

}




// pwm 

static nrf_drv_pwm_t m_pwm0 = NRF_DRV_PWM_INSTANCE(0);

// Declare variables holding PWM sequence values. In this example only one channel is used 
nrf_pwm_values_individual_t seq_values = {0, 0, 0, 0};
nrf_pwm_sequence_t const seq =
{
    .values.p_individual = &seq_values,
    .length          = NRF_PWM_VALUES_LENGTH(seq_values),
    .repeats         = 0,
    .end_delay       = 0
};

// Set duty cycle between 0 and 100%
void pwm_update_duty_cycle(uint8_t d_cycle1, uint8_t d_cycle2)
{
  // precondition: parameters correct
  //  seq_values->channel_0 = 100 - d_cycle1;
  seq_values.channel_0 = d_cycle1;  
  seq_values.channel_1 = d_cycle2;

  //  sprintf(response, "PWM: %d, %d\n\r", d_cycle1, d_cycle2);
  //  send_back(response);

  nrf_drv_pwm_simple_playback(&m_pwm0, &seq, 1, NRF_DRV_PWM_FLAG_LOOP);
}

// top_value = 100 -> 10kHz
static void pwm_init(void)
{
  nrf_drv_pwm_config_t const config0 =
    {
      .output_pins =
      {
        pwma,                                   // channel 0
        pwmb,                                   // channel 1
        //      LED_DL3_BLU | NRF_DRV_PWM_PIN_INVERTED, // channel 1
        NRF_DRV_PWM_PIN_NOT_USED,               // channel 2
        NRF_DRV_PWM_PIN_NOT_USED,               // channel 3
      },
      .irq_priority = APP_IRQ_PRIORITY_LOWEST,
      .base_clock   = NRF_PWM_CLK_1MHz,
      .count_mode   = NRF_PWM_MODE_UP,
      .top_value    = 100,
      .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
      .step_mode    = NRF_PWM_STEP_AUTO
    };
    // Init PWM without error handler
  APP_ERROR_CHECK(nrf_drv_pwm_init(&m_pwm0, &config0, NULL));
    
}


//********  flash  erase and write

static void fstorage_evt_handler(nrf_fstorage_evt_t * p_evt)
{
    if (p_evt->result != NRF_SUCCESS)
    {
        NRF_LOG_INFO("--> Event received: ERROR while executing an fstorage operation.");
        return;
    }

    switch (p_evt->id)
    {
        case NRF_FSTORAGE_EVT_WRITE_RESULT:
        {
            NRF_LOG_INFO("--> Event received: wrote %d bytes at address 0x%x.",
                         p_evt->len, p_evt->addr);
        } break;

        case NRF_FSTORAGE_EVT_ERASE_RESULT:
        {
            NRF_LOG_INFO("--> Event received: erased %d page from address 0x%x.",
                         p_evt->len, p_evt->addr);
        } break;

        default:
            break;
    }
}



NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) =
{
    /* Set a handler for fstorage events. */
    .evt_handler = fstorage_evt_handler,

    /* These below are the boundaries of the flash space assigned to this instance of fstorage.
     * You must set these manually, even at runtime, before nrf_fstorage_init() is called.
     * The function nrf5_flash_end_addr_get() can be used to retrieve the last address on the
     * last page of flash available to write data. */
    //  adres  mydatapage (0xf0000) nog netjes doen.  Moet hele pages (0x1000) zijn?
    .start_addr = 0xf0000,
    .end_addr   = 0xf0fff,
};

nrf_fstorage_api_t * p_fs_api = &nrf_fstorage_sd;

void fstore_init() {
  ret_code_t rc;
  rc = nrf_fstorage_init(&fstorage, p_fs_api, NULL);
  APP_ERROR_CHECK(rc);

}

/**@brief   Sleep until an event is received. */
static void power_manage(void)
{
#ifdef SOFTDEVICE_PRESENT
    (void) sd_app_evt_wait();
#else
    __WFE();
#endif
}

void wait_for_flash_ready(nrf_fstorage_t const * p_fstorage)
{
    /* While fstorage is busy, sleep and wait for an event. */
    while (nrf_fstorage_is_busy(p_fstorage))
    {
        power_manage();
    }
}

void fstore_mydata(void) {

  mydata.eepromcnt++;

  ret_code_t rc;
  rc = nrf_fstorage_erase(&fstorage, (uint32_t)mydatapage, 1, NULL);
  //if (rc != NRF_SUCCESS)
  //  nrf_gpio_pin_set(LED_DL2);
  APP_ERROR_CHECK(rc);

  wait_for_flash_ready(&fstorage);
  nrf_gpio_pin_set(LED_DL3_GRN);  
  // put it in mydatapage
  rc = nrf_fstorage_write(&fstorage, (uint32_t)mydatapage, &mydata, sizeof(mydata), NULL);
  if (rc != NRF_SUCCESS)
    NRF_LOG_INFO("--> fstore_mydata error!!");
    nrf_gpio_pin_set(LED_DL3_RED);
  APP_ERROR_CHECK(rc);
  wait_for_flash_ready(&fstorage);
  nrf_gpio_pin_set(LED_DL3_BLU);  
}

void send_back(char * message)
{
  uint32_t err_code;
  uint16_t length = strlen(message);
  do
    {
      err_code = ble_nus_data_send(&m_nus, (uint8_t *)message, &length, m_conn_handle);
      if ((err_code != NRF_ERROR_INVALID_STATE) &&
          (err_code != NRF_ERROR_RESOURCES) &&
          (err_code != NRF_ERROR_NOT_FOUND))
        {
          APP_ERROR_CHECK(err_code);
        }
    } while (err_code == NRF_ERROR_RESOURCES);
}


void my_scheduler_event_handler(void *p_event_data, uint16_t event_size)
{
  // process command, should be a proper string
  char * response = process((char *)p_event_data);

  send_back(response);
}




