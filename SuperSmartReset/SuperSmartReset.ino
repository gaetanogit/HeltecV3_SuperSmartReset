#include <SoftSerial.h>       // Libreria per comunicazione seriale software
#include <TinyWireM.h>        // Libreria per comunicazione I2C su ATtiny
#include <avr/wdt.h>          // Libreria per il Watchdog Timer (reset sistema)

#define R 4                   // Pin 4: ricezione seriale (RX)
#define T 3                   // Pin 3: trasmissione seriale (TX)
#define M 1                   // Pin 1: comando MOSFET
#define RTC 0x68              // Indirizzo I2C dell'RTC DS3231/1307

SoftSerial s(R, T);           // Istanza porta seriale software
int ON, OF, P;                // Variabili orario accensione, spegnimento e password
unsigned int sN, sS;          // Soglie tensione (minima e ripristino)
unsigned long tA, tH, lR, lH; // Timer watchdog, reset carico e relative memorie temporali

void out(unsigned long n) {   // Funzione per inviare numeri via seriale
  if(n >= 10) out(n / 10);    // Ricorsione per decomporre il numero cifra per cifra
  s.write((n % 10) + '0');    // Converte la cifra in formato ASCII e la scrive
}                             // Fine funzione out

void out2(int n) {            // Funzione per inviare numeri con zero iniziale (es. 09)
  if(n < 10) s.write('0');    // Aggiunge lo '0' se il numero è una sola cifra
  out(n);                     // Chiama la funzione di stampa base
}                             // Fine funzione out2

byte b2d(byte v) {            // Converte formato BCD da RTC in decimale
  return ((v >> 4) * 10) + (v & 15); // Logica di calcolo per conversione BCD
}                             // Fine funzione b2d

byte d2b(byte v) {            // Converte decimale in BCD per invio a RTC
  return ((v / 10) << 4) + (v % 10); // Logica di calcolo per conversione a BCD
}                             // Fine funzione d2b

void W(int a, byte d) {       // Scrittura su EEPROM
  while(EECR & (1 << EEPE));  // Attende che eventuali scritture precedenti finiscano
  EEAR = a;                   // Imposta l'indirizzo di memoria EEPROM
  EEDR = d;                   // Imposta il dato da scrivere
  EECR |= (1 << EEMPE);       // Abilita la procedura di scrittura
  EECR |= (1 << EEPE);        // Avvia la scrittura fisica
}                             // Fine funzione W

byte R_(int a) {              // Lettura da EEPROM
  while(EECR & (1 << EEPE));  // Attende che l'hardware EEPROM sia pronto
  EEAR = a;                   // Imposta l'indirizzo da leggere
  EECR |= (1 << EERE);        // Abilita la lettura
  return EEDR;                // Restituisce il valore letto
}                             // Fine funzione R_

unsigned int vV() {           // Lettura tensione batteria con offset diodo
  ADMUX = 12;                 // Imposta il riferimento ADC internamente (VCC)
  delay(5);                   // Breve pausa per stabilizzare la lettura
  ADCSRA |= 64;               // Avvia il processo di conversione analogico-digitale
  while(ADCSRA & 64);         // Attende il completamento della conversione
  unsigned int v = (unsigned int)(1090270L / (ADCL | (ADCH << 8))); // Calcolo voltaggio (mV)
  return v + 300;             // Aggiunge 300mV di offset per compensare il diodo Schottky
}                             // Fine funzione vV

void setup() {                // Setup iniziale
  pinMode(M, OUTPUT);         // Imposta pin MOSFET come output
  digitalWrite(M, LOW);       // MOSFET spento al boot
  wdt_disable();              // Disabilita watchdog prima di configurarlo
  s.begin(4800);              // Avvia la porta seriale a 9600 baud
  TinyWireM.begin();          // Inizializza il bus I2C
  sN = (R_(0) << 8) | R_(1);  // Carica soglia minima da EEPROM
  sS = (R_(2) << 8) | R_(3);  // Carica soglia ripristino da EEPROM
  P = (R_(8) << 8) | R_(9);   // Carica password da EEPROM
  ON = (R_(4) * 100) + R_(5); // Carica orario accensione
  OF = (R_(6) * 100) + R_(7); // Carica orario spegnimento
  tA = (unsigned long)R_(10) * 3600000UL; // Carica timer sistema A in ore
  tH = (unsigned long)R_(11) * 3600000UL; // Carica timer carico H in ore
  lR = millis();              // Inizializza timer reset A con tempo attuale
  lH = lR;                    // Inizializza timer reset H con tempo attuale
  wdt_enable(WDTO_8S);        // Abilita il Watchdog a 8 secondi
}                             // Fine setup

void loop() {                 // Ciclo continuo
  wdt_reset();                // "Nutre" il watchdog per evitare reset involontari
  //  era la riga sotto if(millis() - lR > 3600000 || (tA > 0 && millis() - lR > tA)) { // Controllo tempo reset sistema
  if(tA > 0 && millis() - lR > tA){ // Controllo tempo reset sistema
    s.write("RST A");         // Invia notifica via seriale
    s.flush();                // Assicura che la stringa esca prima di resettare
    delay(3000);              // Pausa per evitare transitori e far vedere la notifica
    wdt_enable(WDTO_15MS);    // Attiva watchdog a 15ms per forzare il riavvio
    while(1);                 // Loop infinito che scatena il reset del WDT
  }                           // Fine blocco Reset A
  if(tH > 0 && millis() - lH > tH) { // Controllo tempo reset carico
    s.write("RST H");         // Invia notifica seriale
    delay(5000);              // Pausa di 5s per completare la trasmissione
    digitalWrite(M, HIGH);    // Spegne il MOSFET (carico OFF)
    for(int i = 0; i < 50; i++) { wdt_reset(); delay(100); } // Attende 5s con WDT attivo
    digitalWrite(M, LOW);     // Accende il MOSFET (carico ON)
    lH = millis();            // Resetta il timer H
  }                           // Fine blocco Reset H

  if(s.available() && s.read() == '!') { // Parser comandi: attende il carattere '!'
    int p = 0;                // Variabile temporanea per inserimento password
    for(byte j = 0; j < 4; j++) { // Legge le 4 cifre della password
      unsigned long t = millis();
      while(!s.available() && millis() - t < 300); // Timeout lettura
      p = (p * 10) + (s.read() - '0'); // Accumula cifre
    }
    if(p == P) {              // Se password corretta, esegue comandi
      unsigned long t = millis();
      while(!s.available() && millis() - t < 300);
      char c = s.read();      // Legge il comando
      char s_ = 0;            // Eventuale sottocomando
      unsigned int v = 0;     // Eventuale valore numerico
      if(c == 'O' || c == 'g') { // Se comando prevede sottocomando
        t = millis();
        while(!s.available() && millis() - t < 300);
        s_ = s.read();
      }
      while(true) {           // Legge il valore numerico inviato
        t = millis();
        while(!s.available() && millis() - t < 300);
        if(!s.available()) break;
        char n = s.peek();    // Anteprima carattere
        if(n >= '0' && n <= '9') v = (v * 10) + (s.read() - '0'); // Accumula valore
        else { s.read(); break; }
      }
      if(c == 'A' || c == 'H') { // Comando imposta timer
        if(c == 'A') { tA = (unsigned long)v * 3600000UL; W(10, v); } // Salva in ore
        else { tH = (unsigned long)v * 3600000UL; W(11, v); }        // Salva in ore
        s.write('K');         // Conferma OK
      }
      else if(c == 'N' || c == 'F') { // Imposta orari ON/OFF
        W(c == 'N' ? 4 : 6, v / 100); W(c == 'N' ? 5 : 7, v % 100);
        if(c == 'N') ON = v; else OF = v; s.write('K');
      }
      else if(c == 'g') {     // Comando di lettura dati (get)
          if(s_ == 'A' || s_ == 'H') out(s_ == 'A' ? tA / 3600000UL : tH / 3600000UL); // Legge ore
          else if(s_ == 'B') { out(vV()); s.write('-'); out(sN); s.write('-'); out(sS); } // Legge batt
          else if(s_ == 'T') { // Legge ora RTC
            TinyWireM.beginTransmission(RTC); TinyWireM.send(1); TinyWireM.endTransmission();
            TinyWireM.requestFrom(RTC, 2); byte m = b2d(TinyWireM.receive()); byte h = b2d(TinyWireM.receive() & 63);
            out2(h); s.write(':'); out2(m);
          }
          else if(s_ == 'P') { // Legge orari configurati
            out2(ON / 100); s.write(':'); out2(ON % 100); s.write('-'); out2(OF / 100); s.write(':'); out2(OF % 100);
          }
          else s.write('E'); // Comando errato
      }
      else if(c == 'T') { // Imposta ora RTC
        TinyWireM.beginTransmission(RTC); TinyWireM.send(0); TinyWireM.send(0); TinyWireM.send(d2b(v % 100)); TinyWireM.send(d2b(v / 100)); TinyWireM.endTransmission();
        s.write('K');
      }
      else if(c == 'M' || c == 'W') { // Salva soglie tensione
        if(c == 'M') { sN = v; W(0, v >> 8); W(1, v & 255); } else { sS = v; W(2, v >> 8); W(3, v & 255); }
        s.write('K');
      }
      else if(c == 'P') { P = v; W(8, v >> 8); W(9, v & 255); s.write('K'); }
      else if(c == 'R') { // Reset manuale carico
        s.write("RST H"); delay(6000); digitalWrite(M, HIGH);
        for(int i = 0; i < 50; i++) { wdt_reset(); delay(100); }
        digitalWrite(M, LOW);
      }
      else s.write('E'); // Comando errore
    }
  }

  static unsigned long l = 0; // Timer per il controllo periodico 10s
  static bool b = false;       // Flag blocco batteria   //era b=true a in fase di accensione in caso di tensione di batt nel mezzo della finestra operativa si spegneva immediatamente il nodo.
  if(millis() - l > 10000) {  // Ogni 10 secondi
    l = millis();
    TinyWireM.beginTransmission(RTC); TinyWireM.send(1); TinyWireM.endTransmission();
    TinyWireM.requestFrom(RTC, 2); byte m_raw = TinyWireM.receive(); byte h_raw = TinyWireM.receive();
    int ora = (b2d(h_raw & 63) * 100) + b2d(m_raw); // Legge orario corrente
    int V = vV();             // Legge voltaggio
    if (V >= sS) b = false;   // Sblocca se batteria sopra soglia ripristino
    else if (V < sN) b = true;// Blocca se sotto soglia minima
    bool ok = false;          // Verifica finestra oraria
    if (ON < OF) { if (ora >= ON && ora < OF) ok = true; }
    else { if (ora >= ON || ora < OF) ok = true; }
    if (b) digitalWrite(M, HIGH); // Forza OFF se batteria bassa
    else digitalWrite(M, ok ? LOW : HIGH); // Gestione oraria (MOSFET LOW = Acceso)
  }
}