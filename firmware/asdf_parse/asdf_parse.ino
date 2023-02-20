#include "E:\SP2021\Arduino\libraries\Arduino_SoftwareReset-3.0.0\src\SoftwareReset.h"
#include <avr/wdt.h>


void debugLED () {
      digitalWrite(13, HIGH);
      delay(500);
      digitalWrite(13, LOW);
}

void hardReset(){ // WatchDog reset via instruction timeout
  wdt_enable(WDTO_15MS);
  for(;;) {};
}

void softReset(){ // Software reset (Does not reset IO or clear registers)
  asm volatile (" jmp 0");
}

int bootDone (){
  Serial.write("DONE");
  return 1;
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1);
  while (!Serial) {} // Wait for serial ready
}

char str1[] = "128\r";
int resetFlag = 0;

void loop () {
  
  // Read position of all levers / buttons
  int stat[7] = {0,0,0,0,0,0,0};
  stat[0] = 2;
  stat[1] = analogRead(A0);
  stat[2] = analogRead(A1);
  stat[3] = analogRead(A2);
  stat[4] = analogRead(A3);
  stat[5] = analogRead(A4);
  stat[6] = 0; // sacrificial position, not used for interpretation

  
  if (resetFlag == 0){ // report reset done
    resetFlag = bootDone();
  }
  
  int input;
  while(!Serial.available()){}
  input = Serial.readString().toInt();

  // input parser
  switch (input) {
    
    case 128 : // CMD_RESET - reset hardware
    {
      hardReset();
      break;
    }
    
    case 129 : // CMD_POLL - reset
    {
      for(int i = 0; i < 7; i++){ // Breakdown ints to be sent as bytes.
        byte low = stat[i] % 256;
        byte high = stat[i] / 256;
        Serial.write(low);
        Serial.write(high);
      }
      break;
    }

    case 131 : // CMD_LVR_RELS
    {
       // TODO: thrust lever release function
       Serial.write(131); // report release done
       break;
    }

    // CMD_LVR_SET cases

    case 130 :
    {
      int pos[3] = {0,0,0}; 
      break;  
    }

    case 146 :
    {
      int pos[3] = {0,0,0}; 
      break;  
    }
    
    case 162 :
    {
      int pos[3] = {0,0,0}; 
      break;  
    }
    
    case 178 :
    {
      int pos[3] = {0,0,0}; 
      break;  
    }

    case 194 :
    {
      int pos[3] = {0,0,0}; 
      break;  
    }

    case 210 :
    {
      int pos[3] = {0,0,0}; 
      break;  
    }

    case 226 :
    {
      int pos[3] = {0,0,0}; 
      break;  
    }

    case 242 :
    {
      int pos[3] = {0,0,0}; 
      break;  
    }

    // end of CMD_LVR_SET cases
    
//    Unused  
//    case 255 :
//    {
//         
//    } 
  }
  

}
