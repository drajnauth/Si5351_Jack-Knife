/*
Application to demonstrate the use of the Si5251 to generate frequencies between 2.5Khz to 200 Mhz.

*/

#include "Arduino.h"
#include <stdint.h>

#include "main.h"
#include "UART.h"                       // VE3OOI Serial Interface Routines (TTY Commands)
#include "VE3OOI_Si5351_v2.0.h"         // VE3OOI Si5351 Routines
#include <EEPROM.h>           


// These variables are defined in UART.cpp and used for Serial interface
// rbuff is used to store all keystrokes which is parsed by Execute() 
// commands[] and numbers[] store all characters or numbers entered provided they
// are separated by spaces.  
// ctr is counter used to process entries
// command_entries contains the total number of charaters/numbers entered
extern char rbuff[RBUFF]; 
extern char commands[MAX_COMMAND_ENTRIES];
extern unsigned char command_entries;
extern unsigned long numbers[MAX_COMMAND_ENTRIES];
extern unsigned char ctr;

unsigned long flags;

volatile EEPROMStrut ee;



// the setup function runs once when you press reset or power the board
void setup() {
  pinMode(13, OUTPUT);
  // define the baud rate for TTY communications. Note CR and LF must be sent by  terminal program
  Serial.begin(9600);
  Serial.println ("Si5351 Controller 1.5"); 
  Serial.write ("\r\nRDY> ");
  Serial.flush();
  ResetSerial ();


// Read XTAl correction value from Arduino eeprom memory
  EEPROMRead(EEPROMAddress); 

  if (ee.flag != EEPROMHDR) {
    memset((char *)&ee, 0, sizeof(ee));
    ee.flag = EEPROMHDR;
    ee.Fxtal =  SI_CRY_FREQ_25MHZ;
    ee.correction = 0;
  }
  
//  initialize the Si5351
  setupSi5351 (ee.correction);


  Reset();
}


// the loop function runs over and over again forever
void loop() {

// Look for characters entered from the keyboard and process them
// This function is part of the UART package.
  ProcessSerial ();

 
}



void ExecuteSerial (char *str)
{
  
// num defined the actual number of entries process from the serial buffer
// i is a generic counter
  unsigned char num;
  unsigned long i;
  int j;

 
// This function called when serial input in present in the serial buffer
// The serial buffer is parsed and characters and numbers are scraped and entered
// in the commands[] and numbers[] variables.
  num = ParseSerial (str);

// Process the commands
// Note: Whenever a parameter is stated as [CLK] the square brackets are not entered. The square brackets means
// that this is a command line parameter entered after the command.
// E.g. F [CLK] [FREQ] would be mean "F 0 7000000" is entered (no square brackets entered)
  switch (commands[0]) {

    case 'C':             // Calibrate
      if (!numbers[1] && !numbers[0]) {
        EEPROMRead(EEPROMAddress); 
        Serial.print ("Corr: ");
        Serial.println (ee.correction);
        break; 
      } else if (numbers[1] < SI_MIN_OUT_FREQ || numbers[1] > SI_MAX_OUT_FREQ) {
        Serial.println ("Bad Freq");
        break;
      }
        
      // New value defined so read the old values and display what will be done
      EEPROMRead(EEPROMAddress); 
      Serial.print ("Old Corr: ");
      Serial.println (ee.correction);
      Serial.print ("Enter New Corr: ");
      Serial.println (numbers[0]);

      // Store the new value entered, reset the Si5351 and then display frequency based on new setting     
      ee.correction = numbers[0];
      EEPROMWrite(EEPROMAddress); 
      
      setupSi5351 (ee.correction);
      EEPROMRead(EEPROMAddress); 
      SetFrequency (SI_CLK0, SI_PLL_A, numbers[1]);
      SetFrequency (SI_CLK1, SI_PLL_A, numbers[1]);
      SetFrequency (SI_CLK2, SI_PLL_A, numbers[1]);
      break;

    // This command is used to quickly set the frequency. The function will decide the 
    // best PLL frequecy to use. It also assumes that CLK0 and CLK1 will share PLLA and 
    // CLK2 will use PLLB. Phase is set to 0 and 8mA output is used
    // Syntax: F [CLK] [FREQ], where CLK is output clock, FREQ is output frequency between 8Khz to 160Mhz
    // Note: If you select a frequency greater than 150 Mhz, you cannot select a fequency below this without resetting the Si5351
    case 'F':             // Set Frequency
      // Validate inputs
      if (numbers [0] > 2) {
        Serial.println ("Bad Channel");
        break;
      }
      
      if (numbers[1] < SI_MIN_OUT_FREQ || numbers[1] > SI_MAX_OUT_FREQ) {
        Serial.println ("Frequency out of range");
        break;
      }
      if (numbers[0] != 2) Serial.println ("CLK0 & CLK1 Share PLLA");    
      else Serial.println ("CLK2 Uses PLLB");
      
      // set frequency
      if (numbers[0] == 0) {
        if (commands[1] == 'B') SetFrequency (SI_CLK0, SI_PLL_B, numbers[1]);
        else SetFrequency (SI_CLK0, SI_PLL_A, numbers[1]);
        
      } else if (numbers[0] == 1) {
        if (commands[1] == 'B') SetFrequency (SI_CLK1, SI_PLL_B, numbers[1]);
        else SetFrequency (SI_CLK1, SI_PLL_A, numbers[1]);  
             
      } else if (numbers[0] == 2) {
        if (commands[1] == 'B') SetFrequency (SI_CLK2, SI_PLL_B, numbers[1]);
        else SetFrequency (SI_CLK2, SI_PLL_A, numbers[1]);  
       
      }
      break;


 
      
    // Help Screen. This consumes a ton of memory but necessary for those
    // without much computer or programming experience.
    case 'H':             // Help
      Serial.println ("C [n] [Hz]- Si5351 Freq Cal");
      Serial.println ("\tE.g. C");
      Serial.println ("\tE.g. C 110 10000000");
      Serial.println ("F c p HZ [e] - Set Freq CLK c PLL p");
      Serial.println ("\tE.g. F 0 a 10000000");
      Serial.println ("\tE.g. F 1 b 10000000");
      Serial.println ("R - Reset");
      break;

     
    // This command reset the Si5351.  A reset zeros all parameters including the correction/calibration value
    // Therefore the calibration must be re-read from eeprom
    case 'R':             // Reset
      Reset ();
      break;
   
    // If an undefined command is entered, display an error message
    default:
      ErrorOut ();
  }
  
}

void Reset (void)
{
  // Zero variables
  flags = 0;

  ResetSerial ();
  
  EEPROMRead(EEPROMAddress);
  
  setupSi5351 (ee.correction); 
}


// This routines are NOT part of the Si5351 and should not be included as part of the Si5351 routines.
// Note that some arduino do not have eeprom and would generate an error during compile.
// If you plan to use a Arduino without eeprom then you need to hard code a calibration value.
void EEPROMWrite (unsigned int memAddr)
// write the calibration value to Arduino eeprom
{
  unsigned int i;
  char *cptr;
  cptr = (char *)&ee;
  for (i=0; i<sizeof(ee); i++) {
    EEPROM.write((memAddr+i), (unsigned char)*cptr);
    cptr++;
  }
}

void EEPROMRead (unsigned int memAddr)
// read the calibratio value from Arduino eeprom
{
  unsigned int i;
  char *cptr;
  cptr = (char *)&ee;
  for (i=0; i<sizeof(ee); i++) {
    *cptr =  EEPROM.read(memAddr+i);
    cptr++;
  }
}


