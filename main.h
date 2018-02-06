#ifndef _MAIN_H_
#define _MAIN_H_



#define EEPROMAddress 0
#define EEPROMHDR 0xAA

struct EEPROMStrut {
  unsigned char flag;
  int correction;
  unsigned long Fxtal, Fxtalcorr;
} ;


void ExecuteSerial (char *str);

void EEPROMWrite(unsigned int addr);
void EEPROMRead(unsigned int addr);

void Reset (void);


#endif // _MAIN_H_
