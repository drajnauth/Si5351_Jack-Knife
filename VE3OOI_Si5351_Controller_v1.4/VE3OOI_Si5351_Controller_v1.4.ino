/*

Program Written by Dave Rajnauth, VE3OOI to control the Si5351. 
It can output different frequencies from 8Khz to 160Mhz on Channel 0 and 2 on Si5351
It also provides functions to calibrate arduino and store the calibration value in EEPROM
*/
#include "Arduino.h"

#include <stdint.h>
#include <avr/eeprom.h>        // Needed for storeing calibration to Arduino EEPROM
#include <Wire.h>              // Needed to communitate I2C to Si5351
#include <SPI.h>               // Needed to communitate I2C to Si5351

#include "UART.h"              // VE3OOI Serial Interface Routines (TTY Commands)
#include "VE3OOI_Si5351_v1.3.h"         // VE3OOI Si5351 Routines
#include "Si5351_Controller.h"  // Defines for this program

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

// This defines the various parameter used to program Si5351 (See Silicon Labs AN619 Note)
// multisynch defines specific parameters used to determine Si5351 registers
// clk0ctl, clk1ctl, clk2ctl defined specific parameters used to control each clock
extern Si5351_def multisynth;
extern Si5351_CLK_def clk0ctl;
extern Si5351_CLK_def clk1ctl;
extern Si5351_CLK_def clk2ctl;


// the setup function runs once when you press reset or power the board
void setup() {

  // define the baud rate for TTY communications. Note CR and LF must be sent by  terminal program
  Serial.begin(9600);
  Serial.println ("VE3OOI Si5351 Controller v1.4"); 
  Serial.write ("\r\nRDY> ");
  Serial.flush();
  ResetSerial ();
  
//  initialize the Si5351
  ResetSi5351 (SI_CRY_LOAD_8PF);

// Read XTAl correction value from Arduino eeprom memory
  EEPROMReadCorrection(); 
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
  
// This function called when serial input in present in the serial buffer
// The serial buffer is parsed and characters and numbers are scraped and entered
// in the commands[] and numbers[] variables.
  num = ParseSerial (str);

// Process the commands
// Note: Whenever a parameter is stated as [CLK] the square brackets are not entered. The square brackets means
// that this is a command line parameter entered after the command.
// E.g. F [CLK] [FREQ] would be mean "F 0 7000000" is entered (no square brackets entered)
  switch (commands[0]) {
    
    // Calibrate the Si5351.
    // Syntax: C [CAL] [FREQ], where CAL is the new Calibration value and FREQ is the frequency to output
    // Syntax: C , If no parameters specified, it will display current calibration value
    // Bascially you can set the initial CAL to 100 and check fequency accurate. Adjust up/down as needed
    // numbers[0] will contain the correction, numbers[1] will be the frequency in Hz
    case 'C':             // Calibrate
   
      // First, Check inputs to validate
      if (numbers[0] == 0 && numbers[1] == 0) {
        EEPROMReadCorrection();
        Serial.print ("Correction: ");
        Serial.println (multisynth.correction);
        break;
      } else if (numbers[0] == 0) {
        Serial.println ("Bad Calibration");
        break;
      } else if (numbers[1] < SI_MIN_OUT_FREQ || numbers[1] > SI_MAX_OUT_FREQ) {
        Serial.println ("Bad Frequency");
        break;
      }
      
      // New value defined so read the old values and display what will be done
      EEPROMReadCorrection();
      Serial.print ("Old Correction: ");
      Serial.println (multisynth.correction);
      Serial.print ("New Correction: ");
      Serial.println (numbers[0]);

      // Store the new value entered, reset the Si5351 and then display frequency based on new setting     
      multisynth.correction = numbers[0];
      EEPROMWriteCorrection();
  
      ResetSi5351 (SI_CRY_LOAD_8PF);
      EEPROMReadCorrection();
      SetFrequency (SI_CLK0, SI_PLL_B, numbers[1], SI_CLK_8MA);
      SetFrequency (SI_CLK1, SI_PLL_B, numbers[1], SI_CLK_8MA);
      SetFrequency (SI_CLK2, SI_PLL_B, numbers[1], SI_CLK_8MA);
      break;

    // This allow you do set a frequency based on PLL (A/B), PLL Freq (600-900Mhz), phase and output drive (2,4,6,8 mA)
    // Phase is based on the period of the PLL clock (600-900 Mhz).  At low output freqencies
    // the phase may not be very large. For example at 900 Mhz PLL and 7 Mhz output, phase shift is max 88 deg
    // Drive current also impacts output impedence.  2mA is close to 200R, 6mA is close to 100R, 8mA is 50R
    // Syntax: D [CLK] [FREQ] [PLL} [PLLFREQ] [PHASE] [mADrive]
    // CLK is 0,1,2.  Freq can be 8Khz to 160Mhz, PLL is A or B, PLLFREQ is between 600-900 Mhz, 
    // PHASE is in degrees and limited by the PLLFREQ (see AN619), and mADrive is 2,4,6,8 mA
    // numbers[0] is CLK, numbers[1] is FREQ, number[2] is PLLFREQ, numbers[3] is phase, numbers[4] is mADrive  
    // You must enter CLK and FREQ. If other parameters missing assumptions made
    // Note: If you select a frequency greater than 150 Mhz, you cannot select a fequency below this without resetting the Si5351
    case 'D':             // Set Frequency using details
      // Check inputs to validate
      if (numbers [0] > 2) {
        Serial.println ("Bad Channel");
        break;
      }
      
      if (numbers[1] < SI_MIN_OUT_FREQ || numbers[1] > SI_MAX_OUT_FREQ) {
        Serial.println ("Bad Freq");
        break;
      }

     if (!commands[1]) {
        Serial.println ("PLL missing. Assume A");
        commands[1] = SI_PLL_A;
      }

      if (numbers[2] == 0) {
        numbers[2] = SI_MAX_PLL_FREQ;
        Serial.println ("PLL Freq missing. Assume 900 MHz");
      } if (numbers[2] < SI_MIN_PLL_FREQ || numbers[2] > SI_MAX_PLL_FREQ) {
        Serial.println ("Bad PLL Freq");
        break;
      }

      if (numbers[3] > 360) {
        Serial.println ("Bad Phase");
        break;
      } 

      if (numbers[4] == 0) {
        Serial.println ("Drive missing. Assume 8mA");
        numbers[4] = 8;
      } else if (numbers[4] != 2 && numbers[4] != 4 && numbers[4] != 6 && numbers[5] != 8) {
        Serial.println ("Bad drive");
        break;
      }

      // The drive is an encoded digital number and not the number entered. Need to translate to register values
      switch (numbers[4]) {
        case 2:
          numbers[4] = SI_CLK_2MA;
          break;
        case 4:
          numbers[4] = SI_CLK_4MA;
          break;
        case 6:
          numbers[4] = SI_CLK_6MA;
          break;
        case 8:
          numbers[4] = SI_CLK_8MA;
          break;
        default:
          numbers[4] = SI_CLK_8MA;
      }
      
      // Set the frequency
      SetupFrequency (numbers[0], commands[1], numbers[2], numbers[1], (unsigned int) numbers[3], (unsigned char)numbers[4]);
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
      
      // Make assumtions and set frequency
      if (numbers[0] == 0) {
        SetFrequency (SI_CLK0, SI_PLL_A, numbers[1], SI_CLK_8MA);
      } else if (numbers[0] == 1) {
        SetFrequency (SI_CLK1, SI_PLL_A, numbers[1], SI_CLK_8MA);        
      } else if (numbers[0] == 2) {
        SetFrequency (SI_CLK2, SI_PLL_B, numbers[1], SI_CLK_8MA);        
      }
      break;

    // Help Screen. This consumes a ton of memory but necessary for those
    // without much computer or programming experience.
    case 'H':             // Help
      Serial.println ("C - Disp Calib");
      Serial.println ("\tE.g. C");
      Serial.println ("C [CAL] [HZ] - Set Calib for HZ on all Channels");
      Serial.println ("\tE.g. C 100 10000000 ");
      Serial.println ("D [C] [HZ] [PLL] [PLLFreq] [deg] [mA] - Set Freq on CLK C using PLL with phase deg and drive mA");
      Serial.println ("\tE.g. D 0 7000000 B 650000002 45 8");
      Serial.println ("F [C] [HZ] - Set Freq CLK C");
      Serial.println ("\tE.g. F 0 10000000");
      Serial.println ("I [C] [E] - Invert clock (180 phase shift). E=1 invert, E=0 return");
      Serial.println ("\tE.g. E 2 1");
      Serial.println ("L [C] [mA] - Set load mA for C");
      Serial.println ("\tE.g. L 0 6");
      Serial.println ("P [C] [deg] - Set phase in degrees C");
      Serial.println ("\tE.g. P 0 88");
      Serial.println ("S [S] [E] [I] [delay]- Sweep Start to End with Increment with delay ms on all Channels");
      Serial.println ("\tE.g. S 7000000 73000000 100");
      Serial.println ("T [F] [C] - Generate test PSK at Freq F for duation C (Cx64 ms)");
      Serial.println ("\tE.g. P 16 99 (99x64ms duration)");
      Serial.println ("R - Reset");
      break;

    // This command inverts the clock while its running. Its a 180 deg phase shift for square wave output
    // Syntax: I [CLK] [E], where CLK is output clock, E is set to 1 to invert and 0 to not invert (i.e. return to original state)
    case 'I':             // Set Load Current 
      if (numbers [0] > 2) {
        Serial.println ("Bad Channel");
        break;
      }
      
      if (numbers[1] > 1 ) {
        Serial.println ("1 or 0");
        break;
      }
      InvertClk (numbers[0], numbers[1]);
      break;

    // This command is used to change the output drive current for a running clock
    // Syntax: L [CLK] [mADrive] where CLK is the clock and mADrive is the output current (2,4,6,8 mA)
    case 'L':             // Set Load Current 
      if (numbers [0] > 2) {
        Serial.println ("Bad Channel");
        break;
      }
      
      if (numbers[1] != 2 && numbers[1] != 4 && numbers[1] != 6 && numbers[1] != 8) {
        Serial.println ("Bad current");
        break;
      }
      UpdateDrive (numbers[0], numbers[1]);
      break;

    // This command changes the phase on a running clock.  It needs to reset the PLL and there could ba a glitch in the frequency
    // Syntax: P [CLK] [Phase].  Where CLK is the output clock
    // PHASE is in degrees and limited by the PLLFREQ (see AN619).  Depending on the PLLFREQ and the Frequency (see UpdatePhase() routine for details)
    // of the output signal, the phase has a maximum value. E.g. Max 88 deg for 900 Mhz PLL and 7 Mhz output
    // This limitation in phase is due to the Si5351 and not this code
    // To warn the user, the command first displays the max angle allowed based on PLL frequency and output frequency
    // Note that if CLK0 and CLK2 are running different PLLs, you cannot measure the phase of either clock using a scope.
    // To properly measure phase, you need to have both clocks running from the same PLL.  Use the "D" command set configure the output clocks
    case 'P':             // Set Phase 
      if (numbers [0] > 2) {
        Serial.println ("Bad Channel");
        break;
      }

      if (numbers [1] > 360) {
        Serial.println ("Bad angle");
        break;
      }
      
      if (numbers[0] == 0 && numbers[1] == 0) {
        Serial.print ("Max angles: ");
        Serial.print (clk0ctl.maxangle);
        Serial.print (",");        
        Serial.print (clk1ctl.maxangle);
        Serial.print (",");
        Serial.println (clk2ctl.maxangle);
      }
      
      UpdatePhase (numbers[0], numbers[1]);
      break;
     
    // This command reset the Si5351.  A reset zeros all parameters including the correction/calibration value
    // Therefore the calibration must be re-read from eeprom
    case 'R':             // Reset
      ResetSi5351 (SI_CRY_LOAD_8PF);
      EEPROMReadCorrection();
      break;

    // This command is used to sweep between two frequencies. 
    // Syntax: S [START] [STOP] {INC] [DELAY] where START is the starting frequency, stop is the ending frequency
    // INC is the amount to increment the frequency by and [DELAY] is the pause in ms before changing frequeny
    // E.g. S 1000000 10000000 1000000 2000 , Sweep from 1 Mhz to 10 Mhz and increment by 1 Mhz.  Pause for 2 seconds before changing frequncy
    // The output is fixed to PLLA and all clocks display the frequencies
    case 'S':             // Scan Frequencies      
      // Validate the inputs
      if (numbers[0] < SI_MIN_OUT_FREQ || numbers[0] > SI_MAX_OUT_FREQ || numbers[1] < SI_MIN_OUT_FREQ || 
          numbers[1] > SI_MAX_OUT_FREQ) {
        Serial.println ("Bad Freq");
        break;
      }
      if (numbers[2] == 0 || numbers[2] > numbers[1]) {
        Serial.println ("Bad Inc");
        break;
      }
      if (numbers[3] == 0 || numbers[3] > 3000) {
        numbers[3] = 500;
      }
      Serial.print ("Ch 0 - Sweep Freq: ");
      Serial.print (numbers[0]);
      Serial.print (" to: ");
      Serial.print (numbers[1]);
      Serial.print (" Inc: ");
      Serial.println (numbers[2]); 
      Serial.flush();
     
     for (i=numbers[0]; i<=numbers[1]; i+=numbers[2]) { 
      SetFrequency (SI_CLK0, SI_PLL_A, i, 8);
      SetFrequency (SI_CLK1, SI_PLL_A, i, 8);
      SetFrequency (SI_CLK2, SI_PLL_A, i, 8);
      Serial.println (i);
      delay (numbers[3]);
     }
     break;
      
    // This command sends psk signal as a test.  
    case 'T':             // PSK Test
      // Validate frequency
      if (numbers[0] < SI_MIN_OUT_FREQ || numbers[1] > SI_MAX_OUT_FREQ) {
        Serial.println ("Frequency out of range");
        break;
      }
      if (numbers[1] > 100) {
        Serial.println ("Bad Duration");
        break;
      } else if (!numbers[1]) {
        numbers[1] = 30;
      }

      SetFrequency (SI_CLK0, SI_PLL_A, numbers[0], 8);

      for (i=0; i<numbers[1]; i++) {
//         UpdatePhase (SI_CLK0, numbers[1]);
         InvertClk (SI_CLK0, 1);
         delay (32);
//         UpdatePhase (SI_CLK0, 0);
         InvertClk (SI_CLK0, 0);
         delay (32);
      }
      break;

    // If an undefined command is entered, display an error message
    default:
      ErrorOut ();
  }
  
}

// This routines are NOT part of the Si5351 and should not be included as part of the Si5351 routines.
// Note that some arduino do not have eeprom and would generate an error during compile.
// If you plan to use a Arduino without eeprom then you need to hard code a calibration value.
void EEPROMWriteCorrection(void)
// write the calibration value to Arduino eeprom
{
  eeprom_write_dword((uint32_t*)0, multisynth.correction);
}

void EEPROMReadCorrection(void)
// read the calibratio value from Arduino eeprom
{
  multisynth.correction = eeprom_read_dword((const uint32_t*)0);
}


