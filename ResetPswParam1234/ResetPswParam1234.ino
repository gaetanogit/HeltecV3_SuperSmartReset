#include <avr/eeprom.h>

void setup() {
  // --- SOGLIE TENSIONE ---
  // Il tuo firmware principale fa: (R_(0) << 8) | R_(1)
  // Per ottenere 3500 (0x0DAC), dobbiamo scrivere:
  // Cella 0: 0x0D (byte alto)
  // Cella 1: 0xAC (byte basso)
  eeprom_update_byte((uint8_t*)0, 3500 >> 8);  // Scrive 0x0D
  eeprom_update_byte((uint8_t*)1, 3500 & 255); // Scrive 0xAC
  
  eeprom_update_byte((uint8_t*)2, 3800 >> 8);  // Scrive 0x0E (byte alto di 3800)
  eeprom_update_byte((uint8_t*)3, 3800 & 255); // Scrive 0xDC (byte basso di 3800)
  
  // --- ORARI ---
  eeprom_update_byte((uint8_t*)4, 0);  // Ore ON
  eeprom_update_byte((uint8_t*)5, 0);  // Minuti ON
  eeprom_update_byte((uint8_t*)6, 23); // Ore OFF
  eeprom_update_byte((uint8_t*)7, 59); // Minuti OFF
  
  // --- PASSWORD ---
  // Il tuo firmware fa: (R_(8) << 8) | R_(9)
  eeprom_update_byte((uint8_t*)8, 1234 >> 8);
  eeprom_update_byte((uint8_t*)9, 1234 & 255);
}

void loop() {}