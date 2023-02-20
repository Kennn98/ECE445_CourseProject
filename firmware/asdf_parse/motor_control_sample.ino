//include Arduino stepper library v1.2.0 from https://github.com/Attila-FIN/Stepper
#include <Stepper.h>

//number of steps for 17HS4401
const int stepsPerRev = 200;
//MAXIMUM RPM
#define MAX_SPED = 2.1
//potentiometer offset 1
#define SB_POT_OFFSET  590 // lowest value for SB_POT
#define L_POT_OFFSET 620
#define R_POT_OFFSET 550
//Limit the range of steppers to be 90 degrees
#define POS_INIT 0
#define POS_FINAL 50

/* 
Pins settings for each motor-potentiometer pairs and buttons.  
Notice: the output of potentiometer should be assign to the analog pins.
*/

// SPED/BREAK
#define SB_POT A0
#define SB_AIN1 0
#define SB_AIN2 1
#define SB_BIN1 2
#define SB_BIN2 3

// L ENGINE
//Notice: this motor runs in CCW as it goes from 0 to 90 degrees
#define L_POT A1
#define L_AIN1 4
#define L_AIN2 5
#define L_BIN1 6
#define L_BIN2 7

// R ENGINE
#define R_POT A2
#define R_AIN1 8
#define R_AIN2 9
#define R_BIN1 10
#define R_BIN2 11

//BUTTON PINS(analogRead)
#define L_BUTTON A4
#define R_BUTTON A5

// initialize stepper motors
Stepper SB_Stepper(stepsPerRev, SB_AIN1, SB_AIN2, SB_BIN1, SB_BIN2);
Stepper L_Stepper(stepsPerRev, L_AIN1, L_AIN2, L_BIN1, L_BIN2);
Stepper R_Stepper(stepsPerRev, R_AIN1, R_AIN2, R_BIN1, R_BIN2);

// Define the initial positions and relating variables, assuming the initial position is 0 for 0 degree
int SB_pos = 0;
int L_pos = 0;
int R_pos = 0;

int SB_dest = 0;
int L_dest = 0;
int R_dest = 0;

int SB_pos_step = 0;
int L_pos_step = 0;
int R_pos_step = 0;

int SB_dest_step = 0; // dest_ will be given by host program
int L_dest_step = 0;
int R_dest_step = 0;

int SB_diff_step = 0; // step difference between 
int L_diff_step = 0;
int R_diff_step = 0;

// Button reads
int L_bval = 0;
int R_bval = 0;

//Flag for manual override and A/T
int MO = 0;
int AT = 0;
int Threshold = 100; //Threshold for MO detection 

void setup() {
    // initialize the serial port:
    Serial.begin(9600);
    // initialize the analog ports
    pinMode(SB_POT, INPUT);
    pinMode(L_POT, INPUT);
    pinMode(R_POT, INPUT);

    pinMode(L_BUTTON, INPUT_PULLUP);
    pinMode(R_BUTTON, INPUT_PULLUP);

    Serial.write("Pins initialized");
}

void loop() {

    // Reading the values for levers and buttons
    SB_pos = analogRead(SB_POT);
    L_pos = analogRead(L_POT);
    R_pos = analogRead(R_POT);

    // TODO: update pos_dest from the host program

    L_bval = analogRead(L_BUTTON);
    R_bval = analogRead(R_BUTTON);

    if ( AT == 1 ){

        // A/T engaged, 
        // Mapping position into step
        SB_pos_step = map(SB_pos, SB_POT_OFFSET, 1023, 0, 50);
        L_pos_step = map(L_pos, L_POT_OFFSET, 1023, 0, 50);
        R_pos_step = map(R_pos, R_POT_OFFSET, 1023, 0, 50);

        SB_dest_step = map(SB_dest, SB_POT_OFFSET, 1023, 0, 50);
        L_dest_step = map(SB_dest, L_POT_OFFSET, 1023, 0, 50);
        R_dest_step = map(SB_dest, R_POT_OFFSET, 1023, 0, 50);

        // Position tracking
        SB_diff_step = PosTracking(SB_dest_step, SB_pos_step);
        L_diff_step = PosTracking(L_dest_step, L_pos_step);
        R_diff_step = PosTracking(R_dest_step, R_pos_step);

        SB_Stepper.step(SB_diff_step);
        L_Stepper.step(L_diff_step);
        R_Stepper.step(R_diff_step);

        // Update positions for MO detection
        SB_pos = analogRead(SB_POT);
        L_pos = analogRead(L_POT);
        R_pos = analogRead(R_POT);

        //checking MO
        if (( SB_dest - SB_pos > Threshold) || ( L_dest - L_pos > Threshold) || ( R_dest - R_pos > Threshold)){
            // MO detected
            MO = 1;
            AT = 0;
        }
    }
    else{
        // Wait for A/T 

        // TODOTODO ASDFASDF
        
        if (AT == 1){
            MO = 0;
        }
    }


}

int PosTracking( int pos_dest, int pos_curr){
    int travel = pos_dest - pos_curr;

    if( pos_curr + travel < POS_INIT ){
        travel = POS_INIT - pos_curr;
        return travel;
    }
    if( pos_curr + travel > POS_FINAL){
        travel = POS_FINAL - pos_curr;
        return travel;
    }

    return travel;
}
