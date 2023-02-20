#include <avr/wdt.h> 
#include <Stepper.h>

// command codes
#define CMD_RESET	 ((unsigned char) 0x80)
#define CMD_POLL	 ((unsigned char) 0x81)
#define CMD_LVR_RELS ((unsigned char) 0x83)
#define CMD_ASDF	 ((unsigned char) 0xFF)

// CMD_LVR_SET command list
// cmd[6:4] => 000, 001, 010, 011, 100, 101, 110, 111
constexpr unsigned int CMD_LVR_SET_LIST[] = {0x82, 0x92, 0xA2, 0xB2, 0xC2, 0xD2, 0xE2, 0xF2};

// return if lever @i should be set, provided command @cmd
#define shouldSetLever(cmd, i)	((cmd) & (1 << (6 - i)))

// response codes
#define ASDF_ERROR		((unsigned char) 0xFF)
#define	ASDF_ACK		((unsigned char) 0x00)
#define	ASDF_RESET		((unsigned char) 0x01)
#define	ASDF_POLL_OK	((unsigned char) 0x02)

// ASDF_LVR_RELS responses
#define ASDF_LVR_RELS_PILOT	((unsigned char) 0x03)
#define ASDF_LVR_RELS_RESP	((unsigned char) 0x83)

enum throttle_idx_t {
	THROTTLE_LEFT = 0,
	THROTTLE_RIGHT = 1
};

enum button_idx_t {
	BUTTON_TOGA = 0,
	BUTTON_AP_DISENGAGE = 1
};

// returns true if @b is a asdf command byte, and false if @b is a data byte
#define isCommand(b) ((b) & 0x80)

// return status of button @i, given bitmap @btmp
#define getButtonStatus(btmp, i)	((btmp) & (0x1 << i))

// generate button status bitmap for (@b0, @b1)
#define ButtonStatus(b0, b1)	(((!(b1)) << 1) | (!(b0)))

// lever and button status variables
unsigned char speed_brake_level = 0;
unsigned char throttle_level[2] = { 0, 0 };
unsigned char button_status = 0;	// button status bitmap; bit 0 -> button 0; bit 1 -> button 1

// A/T mode
unsigned char AT_Engaged = 0;

// max value of ASDF data byte (0-127)
#define MAX_ASDF	(0x7F)

// max value of analogRead() (0-1024 for Leonardo)
#define MAX_ANALOG	(1 << 10)

// normalize raw analog reads to asdf ranges ([0,1023] -> [0,127])
//#define raw2asdf(val)	((unsigned char) ((val) * ((unsigned int) MAX_ASDF) / MAX_ANALOG))

// SPEED/BREAK
#define SB_POT A3
#define SB_AIN1 8
#define SB_AIN2 9
#define SB_BIN1 10
#define SB_BIN2 11

// L ENGINE
//Notice: this motor runs in CCW as it goes from 0 to 90 degrees
#define L_POT A4
#define L_AIN1 8
#define L_AIN2 9
#define L_BIN1 10
#define L_BIN2 11

// R ENGINE
#define R_POT A5
#define R_AIN1 0
#define R_AIN2 1
#define R_BIN1 2
#define R_BIN2 3

// BUTTON PINS(DigitalRead)
#define L_BUTTON A0
#define R_BUTTON A1

// define all motors settings
const int stepsPerRevolution = 200; // step size for 17HS4401

// initialize stepper motors
//Stepper SB_Stepper(stepsPerRevolution, SB_AIN1, SB_AIN2, SB_BIN1, SB_BIN2);
Stepper L_Stepper(stepsPerRevolution, L_AIN1, L_AIN2, L_BIN1, L_BIN2);
Stepper R_Stepper(stepsPerRevolution, R_AIN1, R_AIN2, R_BIN1, R_BIN2);

void hardReset(){ // WatchDog reset via instruction timeout
  wdt_enable(WDTO_15MS);
  for(;;) {};
}

void haltMotor() {
	digitalWrite(L_AIN1, HIGH);
	digitalWrite(L_AIN2, HIGH);
	digitalWrite(L_BIN1, HIGH);
	digitalWrite(L_BIN2, HIGH);
	digitalWrite(R_AIN1, HIGH);
	digitalWrite(R_AIN2, HIGH);
	digitalWrite(R_BIN1, HIGH);
	digitalWrite(R_BIN2, HIGH);
}

int SB_min = 0;
int TL_min = 0;
int TR_min = 0;
int SB_max = 0;
int TL_max = 0;
int TR_max = 0;

void setup() {
	Serial.begin(115200);
	Serial.setTimeout(1);
	while (!Serial) {} // Wait for serial ready
	delay(1000);
	Serial.write(ASDF_RESET);	// report init complete

	// for debug purposes
	// buttons
	pinMode(L_BUTTON, INPUT_PULLUP); // button TOGA (left)
	pinMode(R_BUTTON, INPUT_PULLUP); // button AT disengage (right)

	//SB_Stepper.setSpeed(2.1);
	L_Stepper.setSpeed(2.1);
	R_Stepper.setSpeed(2.1);

	SB_min = 554;//analogRead(SB_POT);
	SB_max = 1023;
	
	TL_min = 555;//analogRead(L_POT);
	TL_max = 995;
	
	TR_min = 510;//analogRead(R_POT);
	TR_max = 980;
	
	AT_Engaged = 0;
}

void loop() {
	// move motors
	if (AT_Engaged) {
		int throttle_level_l_curr = analogRead(L_POT);

		// map input to range of motors' steps(0~50), assume range of the potentiometers start from 512
		int throttle_level_l_step = map(throttle_level[0], 0, 128, 0, 50);
		int throttle_level_l_curr_step = map(throttle_level_l_curr, TL_max, TL_min, 0, 50);

		int throttle_level_l_diff = PosTracking(throttle_level_l_step, throttle_level_l_curr_step);
		
		int throttle_level_r_curr = analogRead(R_POT);

		// map input to range of motors' steps(0~50), assume range of the potentiometers start from 512
		int throttle_level_r_step = map(throttle_level[1], 0, 128, 0, 50);
		int throttle_level_r_curr_step = map(throttle_level_r_curr, TR_max, TR_min, 0, 50);

		int throttle_level_r_diff = PosTracking(throttle_level_r_step, throttle_level_r_curr_step);

		if (throttle_level_l_diff > 0)
			L_Stepper.step(-1);
		else if (throttle_level_l_diff < 0)
			L_Stepper.step(1);
		delay(100);
		//L_Stepper.halt();
		//if (throttle_level_r_diff > 0)
		//	R_Stepper.step(1);
		//else if (throttle_level_r_diff < 0)
		//	R_Stepper.step(-1);
		//delay(100);
		//R_Stepper.halt();
	}
	
	speed_brake_level = map(analogRead(SB_POT), SB_min, SB_max, 0, 127);
	if (!AT_Engaged) {
		throttle_level[0] = map(analogRead(L_POT), TL_max, TL_min, 0, 127); // lever for L engine
		throttle_level[1] = map(analogRead(R_POT), TR_max, TR_min, 0, 127); // lever for R engine
	}
	button_status = ButtonStatus(digitalRead(L_BUTTON) == HIGH ? 1 : 0, digitalRead(R_BUTTON) == HIGH ? 1 : 0);
	
	unsigned char input = 0;	// 0x00 is not a command
	if (Serial.available())
		input = Serial.read();

	if (!isCommand(input))
		return;
	
	// input parser
	switch (input) {
		case CMD_RESET:
		{
			hardReset();
			break;
		}
		
	    case CMD_POLL:
		{
			unsigned char resp_pkt[] = {
				ASDF_POLL_OK,			// response code
				button_status,			// button status
				speed_brake_level & 0x7F,		// lever positions
				throttle_level[0] & 0x7F,
				throttle_level[1] & 0x7F
			};
			Serial.write(resp_pkt, sizeof(resp_pkt));	// send resposne packet to host
			break;
		}
	
	    case CMD_LVR_RELS:
		{
			AT_Engaged = 0;
			// TODO: thrust lever release function
			Serial.write(ASDF_LVR_RELS_RESP); // report release done
			break;
		}
	
	    // CMD_LVR_SET cases
	    case CMD_LVR_SET_LIST[0]:
	    case CMD_LVR_SET_LIST[1]:
	    case CMD_LVR_SET_LIST[2]:
	    case CMD_LVR_SET_LIST[3]:
	    case CMD_LVR_SET_LIST[4]:
	    case CMD_LVR_SET_LIST[5]:
	    case CMD_LVR_SET_LIST[6]:
	    case CMD_LVR_SET_LIST[7]:
		{
			AT_Engaged = 1;
//			if (shouldSetLever(input, 0)) {
//				while (!Serial.available()) {} 	// wait until a byte is ready
//				speed_brake_level = Serial.read();
//				int speed_brake_level_curr = analogRead(SB_POT);
//
//        		// map input to range of motors' steps(0~50), assume range of the potentiometers start from 512
//        		int speed_brake_level_step = map(speed_brake_level, 0, 127, 0, 50);
//        		int speed_brake_level_curr_step = map(speed_brake_level_curr, 512, 1023, 0, 50);
//
//        		int speed_brake_step_diff = PosTracking(speed_brake_level_step, speed_brake_level_curr_step);
//        		SB_Stepper.step(speed_brake_step_diff);
//			}
			if (shouldSetLever(input, 1)) {
				while (!Serial.available()) {} 	// wait until a byte is ready
				throttle_level[0] = Serial.read() & 0x7F;
			}
			if (shouldSetLever(input, 2)) {
				while (!Serial.available()) {} 	// wait until a byte is ready
				throttle_level[1] = Serial.read() & 0x7F;
			}

			Serial.write(ASDF_ACK);
			break;
		}

		case CMD_ASDF:	// debug only
		{
			Serial.write(ASDF_ACK);
			break;
		}
		
		default:	// TODO: unrecongized command; send ASDF_ERROR?
			break;
	}
}

int PosTracking( int pos_dest, int pos_curr){
    int travel = pos_dest - pos_curr;

//    if( pos_curr + travel < 0 ){
//        travel = 0 - pos_curr;
//        return travel;
//    }
//    if( pos_curr + travel > 50){
//        travel = 50 - pos_curr;
//        return travel;
//    }
    return travel;
}
