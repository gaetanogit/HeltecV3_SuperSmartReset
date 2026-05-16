#include <SoftSerial.h> // Libreria per la comunicazione seriale software
#include <TinyWireM.h>  // Libreria per la comunicazione I2C su ATtiny85

#define RX 4            // Pin 4 dell'ATtiny per ricevere dati (RX)
#define TX 3            // Pin 3 dell'ATtiny per trasmettere dati (TX)
#define MOS 1           // Pin 1 dell'ATtiny per il Gate del MOSFET (0=ACCESO, 1=SPENTO)
#define RTC 0x68        // Indirizzo I2C del modulo orologio DS3231/1307

SoftSerial myS(RX, TX); // Inizializzazione della porta seriale software

int oON, mON, oOFF, mOFF, pass; // Variabili per orari e Password
unsigned int sMin, sRes;        // Soglie batteria Minima e Rientro (in mV)

byte b2d(byte v) { return ((v / 16 * 10) + (v % 16)); } // Converte formato BCD dell'RTC in decimale
byte d2b(byte v) { return ((v / 10 * 16) + (v % 10)); } // Converte decimale in formato BCD per l'RTC

// Scrive un byte nella memoria permanente EEPROM dell'ATtiny
void EE_w(int a, byte d) { while(EECR & (1<<EEPE)); EEAR = a; EEDR = d; EECR |= (1<<EEMPE); EECR |= (1<<EEPE); }
// Legge un byte dalla memoria permanente EEPROM dell'ATtiny
byte EE_r(int a) { while(EECR & (1<<EEPE)); EEAR = a; EECR |= (1<<EERE); return EEDR; }

// Funzione per leggere la tensione della batteria (Internal Bandgap dell'ATtiny)
unsigned int readV() {
  ADMUX = 12; delay(5); ADCSRA |= 64; while (ADCSRA & 64); // Configura ADC e avvia campionamento
  return (unsigned int)(1090270L / (ADCL | (ADCH << 8))); // Calcola i millivolt effettivi
}

// Stampa numeri a due cifre sulla seriale (es: 09 invece di 9)
void p2(int v) { if(v < 10) myS.print('0'); myS.print(v); }

void setup() {
  pinMode(MOS, OUTPUT);        // Imposta il pin del MOSFET come uscita
  digitalWrite(MOS, 0);        // Logica P-MOS: accende subito l'Heltec all'avvio
  myS.begin(9600);             // Inizializza seriale software a 9600 baud
  TinyWireM.begin();           // Inizializza bus I2C per l'RTC
  
  sMin = (EE_r(0) << 8) | EE_r(1); // Ricostruisce soglia minima da EEPROM (byte 0 e 1)
  sRes = (EE_r(2) << 8) | EE_r(3); // Ricostruisce soglia rientro da EEPROM (byte 2 e 3)
  pass = (EE_r(8) << 8) | EE_r(9); // Ricostruisce password da EEPROM (byte 8 e 9)
  oON = EE_r(4); mON = EE_r(5);    // Carica ora e minuto di accensione (byte 4 e 5)
  oOFF = EE_r(6); mOFF = EE_r(7);  // Carica ora e minuto di spegnimento (byte 6 e 7)
  
  if (pass <= 0 || pass > 9999) pass = 1234; // Sicurezza: se password corrotta usa 1234
  if (sMin > 5000) sMin = 3400;              // Sicurezza: se soglia minima sballata usa 3.4V
}

void loop() {
  if (myS.available()) {                    // Se ci sono caratteri dalla seriale
    char c = myS.read();                    // Legge il carattere iniziale
    if (c == '!') {                         // Se inizia con '!', procedi al comando
      int pIn = 0;                          // Variabile per password ricevuta
      for(byte j=0; j<4; j++) {             // Ciclo per leggere le 4 cifre della pass
        unsigned long tO = millis();        // Timer per timeout ricezione
        while(!myS.available() && millis()-tO < 300); // Aspetta il carattere
        pIn = (pIn * 10) + (myS.read() - '0'); // Converte e somma la cifra
      }

      if (pIn == pass) {                    // Se la password coincide
        unsigned long tO = millis();        // Timer per timeout comando
        while(!myS.available() && millis()-tO < 300); // Aspetta comando
        char cmd = myS.read();              // Legge la lettera del comando
        char sub = ' ';                     // Variabile per sotto-comando
        if(cmd == 'O' || cmd == 'g') {      // Se comando O o g, serve un secondo carattere
           while(!myS.available());         // Aspetta il sotto-comando
           sub = myS.read();                // Legge il sotto-comando
        }

        long val = 0;                       // Variabile per il valore numerico
        while(true) {                       // Ciclo per estrarre il valore
          tO = millis();                    // Timeout per ogni cifra
          while(!myS.available() && millis()-tO < 400); // Aspetta cifra
          if(!myS.available()) break;       // Se non arriva nulla, esce
          char n = myS.peek();              // Sbircia il buffer
          if(n >= '0' && n <= '9') val = (val * 10) + (myS.read() - '0'); // Accumula numero
          else { myS.read(); break; }       // Esce se trova carattere non numerico
        }

        if (cmd == 'T') {                   // Comando IMPOSTA ORA (RTC)
           TinyWireM.beginTransmission(RTC); TinyWireM.send(0); TinyWireM.send(0); // Punta secondi
           TinyWireM.send(d2b(val%100));    // Invia minuti convertiti in BCD
           TinyWireM.send(d2b(val/100));    // Invia ore convertite in BCD
           TinyWireM.endTransmission(); myS.println('T'); // Fine I2C e conferma
        }
        else if (cmd == 'R') {              // Comando RESET FISICO HELTEC
           digitalWrite(MOS, 1); delay(5000); digitalWrite(MOS, 0); // Off 5s poi On
           myS.println('R');                // Conferma Reset
        }
        else if (cmd == 'M') { sMin=(unsigned int)val; EE_w(0,sMin>>8); EE_w(1,sMin&255); myS.println('M'); } // Salva sMin
        else if (cmd == 'W') { sRes=(unsigned int)val; EE_w(2,sRes>>8); EE_w(3,sRes&255); myS.println('W'); } // Salva sRes
        else if (cmd == 'O') {              // Comando ORARI ACCENSIONE
           if(sub == 'N') { oON=val/100; mON=val%100; EE_w(4,oON); EE_w(5,mON); } // Salva ON
           if(sub == 'F') { oOFF=val/100; mOFF=val%100; EE_w(6,oOFF); EE_w(7,mOFF); } // Salva OFF
           myS.println('K');                // Conferma OK
        }
        else if (cmd == 'g') {              // Comando GET (LETTURA DATI)
           if (sub == 'B') { myS.print(readV()); myS.print('-'); myS.print(sMin); myS.print('-'); myS.println(sRes); } // Batteria
           else if (sub == 'P') { p2(oON); myS.print(':'); p2(mON); myS.print('-'); p2(oOFF); myS.print(':'); p2(mOFF); myS.println(); } // Orari
           else if (sub == 'T') {           // Legge ora attuale dall'RTC
              TinyWireM.beginTransmission(RTC); TinyWireM.send(1); TinyWireM.endTransmission(); // Punta minuti
              TinyWireM.requestFrom(RTC, 2); // Chiede 2 byte
              byte m_rtc = b2d(TinyWireM.receive()); // Converte minuti
              byte h_rtc = b2d(TinyWireM.receive() & 63); // Converte ore
              p2(h_rtc); myS.print(':'); p2(m_rtc); myS.println(); // Stampa ora
           }
        }
        else if (cmd == 'P') { pass=(int)val; EE_w(8,pass>>8); EE_w(9,pass&255); myS.println('P'); } // Salva Password
      }
    }
  }

  static unsigned long l = 0;               // Timer statico per controllo ogni 10s
  static bool bloccoBatteria = false;       // Stato persistente della protezione batteria
  
  if (millis() - l > 10000) {               // Se passati 10 secondi dall'ultimo controllo
    l = millis();                           // Aggiorna timer controllo
    TinyWireM.beginTransmission(RTC); TinyWireM.send(1); TinyWireM.endTransmission(); // Punta minuti
    TinyWireM.requestFrom(RTC, 2);          // Chiede minuti e ore all'RTC
    int m = b2d(TinyWireM.receive());       // Legge minuti
    int h = b2d(TinyWireM.receive() & 63);  // Legge ore (filtro formato 24h)
    unsigned int vcc = readV();             // Legge tensione batteria in millivolt
    int t = h * 100 + m, tN = oON * 100 + mON, tF = oOFF * 100 + mOFF; // Calcola valori temporali
    bool in = (tN < tF) ? (t >= tN && t < tF) : (t >= tN || t <= tF); // Logica finestra oraria (anche mezzanotte)
    
    if (vcc > 2000) {                       // Agisce solo se rileva batteria valida (>2V)
        
        // 1. GESTIONE ISTERESI (Aggiornamento stato blocco)
        if (vcc < sMin) bloccoBatteria = true;   // Se scende sotto sMin, attiva blocco
        else if (vcc >= sRes) bloccoBatteria = false; // Se sale sopra sRes, toglie blocco

        // 2. DECISIONE FINALE (Priorità alla sicurezza batteria)
        if (bloccoBatteria) {               // Se il blocco batteria è attivo
            digitalWrite(MOS, 1);           // MOSFET SPENTO (Gate HIGH) a prescindere dal timer
        } 
        else {                              // Se la batteria è considerata sicura
            if (in) digitalWrite(MOS, 0);   // Se siamo in orario: ACCENDI (Gate LOW)
            else    digitalWrite(MOS, 1);   // Se siamo fuori orario: SPEGNI (Gate HIGH)
        }
    }
  }
}