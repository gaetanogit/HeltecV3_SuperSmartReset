# 📘 MANUALE TECNICO & ARCHITETTURA DI RETE

## Sistema di Gestione Energetica e Controllo Remoto per Nodi Meshtastic Off-Grid

---

## 1. INTRODUZIONE E MOTIVAZIONE DEL PROGETTO

Il presente progetto nasce dall'esperienza diretta sul campo nella gestione di nodi e gateway **Meshtastic** (basati su hardware **Heltec V3** o similari) installati in posizioni isolate o di difficile accesso (es. pali, tetti, vette montane) o semplicemente dall'altro lato della città. Nelle installazioni off-grid alimentate esclusivamente da pannelli solari e batterie, l'affidabilità del sistema è costantemente minacciate dalle stagioni invernali, dal maltempo prolungato e dai blocchi software improvvisi.

Questo sistema, basato su una scheda di sviluppo **Digispark (ATtiny85)** interfacciata con un modulo **RTC (orologio hardware)**, nasce per risolvere tre criticità fondamentali che mettono a rischio la sopravvivenza della rete:

### I. La Protezione dall'Effetto "Albero di Natale"
Quando una batteria si scarica sotto carico, la sua tensione crolla rapidamente causando lo spegnimento del nodo. Tuttavia, non appena il carico scompare, la tensione della cella risale "a vuoto" (fenomeno del rilassamento chimico). Senza un controllo esterno, il nodo vedrebbe una tensione nuovamente accettabile, si riaccenderebbe (attivando i moduli RF e Wi-Fi), causerebbe un nuovo crollo di tensione e si rispegnerà. Questo ciclo infinito di riavvii continui distrugge la batteria in pochi giorni e corrompe in modo irreversibile il file system della Flash dell'Heltec. Il nostro modulo introduce un'**isteresi intelligente** che congela lo spegnimento finché la batteria non è realmente carica o carica abbastanza da considerare sicuro la riaccensione.

### II. L'Ottimizzazione dei Consumi Notturni
Mentre l'Heltec V3 consuma in media tra i `70mA` e i `120mA` a causa dell'attività radio costante, la scheda Digispark, pur mantenendo attivi i LED di Power e BuiltIn ma rimuovento già il solor regolatore di tensione 7805, richiede un consumo di circa  **`8mA`**. Spostando la gestione del tempo sulla Digispark, possiamo spegnere completamente l'Heltec durante le ore notturne inutili (es. dall'una di notte alle sei del mattino), riducendo drasticamente il consumo giornaliero complessivo ed evitando che il nodo si spenga proprio nelle ore di picco del giorno successivo.

### III. La Necessità di un Canale di Comunicazione Bidirezionale e del Reset Fisico
Il vero cuore del progetto, oltre alla gestione energetica, è la creazione di un **canale di controllo bidirezionale e interattivo crittografato quindi sicuro** tra l'utente remoto e il nodo isolato. Attraverso la rete radio, l'utente è in grado di inviare comandi alla Digispark e ricevere telemetrie in tempo reale (stato della batteria, soglie impostate, orari operativi on/off e reset cadenzati).

Molto spesso, a causa di glitch software, saturazione della memoria o bug del firmware principale, i nodi sul palo smettono di trasmettere pur avendo la batteria completamente carica. In questi scenari, l'unica soluzione è un riavvio hardware. Il sistema implementa un comando di reset fisico forzato.  L'utente inoltre può programmare ad intervalli regolari il reset del nodo attraverso ATtiny85 Digispark togliendo l'alimentazione all'Heltec per 5 secondi tramite il MOSFET. **Questo permette di sbloccare e far ripartire regolarmente il nodo a prescindere dall'orario programmato**, salvando l'installazione senza dover fare un'uscita sul campo per tirare giù il palo.



## 2. ARCHITETTURA DI RETE E COMUNICAZIONE BIDIREZIONALE

Il vero punto di forza di questo modulo non è solo la gestione passiva dell'energia, ma la sua capacità di agire come un'estensione interattiva del nodo remoto. Il sistema stabilisce un vero e proprio ponte di comunicazione bidirezionale tra l'amministratore della rete (remoto) e la scheda Digispark (sul palo), sfruttando l'infrastruttura radio della rete Mesh.

### Il Canale 0 Cifrato Privato (Sicurezza e Isolamento)
Nelle reti pubbliche Meshtastic (come il canale generico `MediumFast`), chiunque può teoricamente ascoltare il traffico o tentare di inviare stringhe di testo. Per evitare che utenti terzi o malintenzionati possano alterare i parametri del tuo nodo, spegnerlo o resettarlo, l'architettura del progetto prevede una compartimentazione rigida:

* **Canale Primario (Slot 0):** Viene configurato come un **canale privato con chiave crittografica simmetrica personalizzata (AES-256)**. Questo canale è invisibile e inaccessibile agli utenti della rete pubblica e viene utilizzato esclusivamente dai gestori del nodo per l'invio dei comandi di telemetria e configurazione.
* **Canali Secondari (Slot 1, 2, ecc.):** Vengono utilizzati per ospitare i canali pubblici (es. la chat di zona, i canali tematici o i servizi meteo). 

L'Heltec V3 è configurato in modo da inoltrare sulla sua porta seriale fisica (GPIO 45/46) i messaggi in arrivo e viceversa di inviare sul Canale 0 dati/messaggi recevuti da ATtiny85. **Si instaura cosi un canale di comunicazione bidirezionale tra ATtiny85 e Heltec.**

---

### Guida Pratica: Configurazione del Canale Privato tramite App Smartphone
Per blindare la comunicazione, dobbiamo fare in modo che lo Slot 0 (il canale primario del nodo) diventi privato e protetto da una chiave che conosci solo tu. Questa operazione va eseguita sia sull'applicazione del tuo smartphone sia sull'Heltec V3 destinato al palo.

Ecco i passaggi da seguire all'interno dell'app ufficiale **Meshtastic** (iOS o Android):

1. **Isolare il Canale Primario:** Apri l'applicazione, connettiti al nodo tramite Bluetooth e vai nella sezione dedicata alla gestione dei **Canali** (*Channels*). Seleziona lo **Slot 0**, che di fabbrica è impostato come *MediumFast*. Il nostro obiettivo è modificarlo radicalmente per farlo diventare il tuo canale di amministrazione.

2. **Personalizzare il Nome e Generare la Chiave (PSK):** Entra nelle impostazioni dello Slot 0 e cambia il nome del canale (ad esempio inserendo *"MioNodoAdmin"* o un nome di fantasia a tua scelta). Subito sotto, cerca la voce relativa alla chiave precondivisa (*Pre-Shared Key* o *PSK*). Di default è impostata su *Default* (quella pubblica); clicca sull'opzione per **generare una chiave casuale a 256 bit** (indicata spesso da un'icona a forma di chiave o un pulsante per il reset della chiave). L'app la imposterà automaticamente.

3. **Fissare il Ruolo su PRIMARY:** Assicurati che nelle opzioni avanzate di questo canale lo stato sia impostato su **PRIMARY** (Primario). Questo passaggio dice all'Heltec che la chat principale del sistema, e di conseguenza i testi che devono essere girati sulla seriale verso la Digispark, sono quelli che viaggiano su questa frequenza cifrata.

4. **Condividere il Canale con il tuo Telefono:** Una volta configurato l'Heltec del palo, clicca sul pulsante per generare il **codice QR del canale** o seleziona *"Condividi canale"*. Invia questo link o inquadra il QR code con lo smartphone che userai a casa o in giro per controllare il nodo. In questo modo, la tua app personale importerà la chiave segreta e sarai l'unico a poter comunicare con il palo.

5. **Ripristinare la Chat Pubblica (Opzionale):** Se vuoi che il tuo nodo continui a fare da ripetitore per la comunità e vuoi mantenere l'accesso alla chat pubblica della tua zona, torna nella lista dei canali, seleziona uno slot vuoto (es. lo **Slot 1**) e aggiungi un nuovo canale. Imposta il nome su `LongFast`, lascia la chiave su `Default` e configura il suo ruolo come **SECONDARY** (Secondario). In questo modo il nodo farà transitare normalmente i messaggi di tutti, ma la Digispark rimarrà al sicuro, ascoltando solo lo Slot 0 privato.

---

### Configurazione della Porta Seriale Hardware dall'App
Dopo aver configurato il canale, è necessario istruire l'Heltec V3 affinché sappia dove sputare fuori i dati in formato testo e a quale velocità parlare con la Digispark. 

Sempre all'interno dell'applicazione dello smartphone, entra nelle **Impostazioni del Nodo** (*Radio Configuration*), cerca la sezione **Serial** (o *Module Config* $\rightarrow$ *Serial*) e imposta i parametri seguendo tassativamente questa tabella:

| Voce di Menu nell'App | Valore da Impostare | Spiegazione Tecnica |
| :--- | :--- | :--- |
| **Serial Output Enabled** | `ON` (Attivo) | Abilita l'Heltec a inviare/ricevere dati sulla porta fisica. |
| **Serial Mode** | `TEXTMSG` | **FONDAMENTALE:** Dice al firmware di inviare sulla seriale i messaggi di testo in chiaro invece dei pacchetti binari (ProtoBuf). |
| **Baud Rate** | `9600` | Velocità di comunicazione stabilita con la libreria `SoftSerial` della Digispark. |
| **Timeout / Window** | `15` | Tempo di attesa espresso in millisecondi per l'invio e la ricezione dei pacchetti, ottimizzato per evitare la frammentazione delle stringhe CLI. |
| **Echo Enabled** | `ON` (Attivo) | **FONDAMENTALE:** Consente la re-immissione in rete delle stringhe ricevute sulla seriale, permettendo alla Digispark di trasmettere i feedback dei comandi eseguiti e i dati di telemetria verso l'app remota. |
| **RX Pin (GPIO)** | `45` | Pin dell'Heltec che riceve i dati inviati dalla Digispark. |
| **TX Pin (GPIO)** | `46` | Pin dell'Heltec che invia i dati verso la Digispark. |

---

### Sfruttamento del Pacchetto Ufficiale `TEXT_MESSAGE`
Per mantenere il codice della Digispark estremamente leggero ed evitare l'inclusione di pesanti librerie di parsing dei protocolli radio, il sistema non utilizza pacchetti di dati custom o strutturati (Protocol Buffers). Sfrutta invece il tipo di pacchetto ufficiale più semplice di Meshtastic: il **`TEXT_MSG`** (un normale messaggio di testo).

Quando l'amministratore invia un comando dalla propria app dello smartphone, questo viaggia nella rete radio come un normale testo cifrato. Quando l'Heltec remoto riceve il pacchetto sul Canale 0, lo decifra e, grazie alla modalità `TEXTMSG` e al timeout di `15ms` attivati nel menu Serial, lo sputa fuori sotto forma di stringa ASCII pura sulla sua seconda porta seriale (pin 46) a 9600 baud. 

La Digispark rimane costantemente in ascolto su quella linea seriale (tramite la libreria `SoftSerial`) analizzando i caratteri in tempo reale. Il flusso logico si attiva solo quando la scheda intercetta il carattere iniziatore **`!`**, seguito dalle 4 cifre del PIN.

### Risposta Radio e Telemetria Interattiva
La bidirezionalità si completata quando la Digispark deve rispondere a un comando o inviare i dati di diagnostica (es. quando richiedi la tensione o l'orario memorizzato). 

Poiché la Digispark non ha un modulo radio proprio, scrive la stringa di risposta sulla seriale verso l'Heltec (pin 45). L'Heltec riceve la stringa e, interpretandola come dati da trasmettere (grazie anche all'opzione `Echo Enabled` attiva), genera a sua volta un pacchetto `TEXT_MSG` che viene irradiato sulla rete Mesh e recapitato direttamente sul display dell'applicazione del gestore. Questo permette di fare una vera e propria sessione di "interrogazione" e programmazione interattiva a distanza di chilometri.

## Risoluzione Problemi di Livelli Logici (Il Diodo Schottky)
Nelle installazioni solari, la tensione di batteria può raggiungere i 4.2V-4.3V durante i picchi di carica. Questo valore, applicato direttamente all'ATtiny e all'RTC, può causare instabilità nei livelli logici della comunicazione seriale con l'Heltec.

- Soluzione: È stato inserito un diodo Schottky in serie al ramo positivo di alimentazione dell'ATtiny e del modulo RTC. Questo crea una caduta di tensione (offset) di circa 300mV, mantenendo la logica di controllo in un range operativo sicuro e stabile, compatibile con i livelli dell'Heltec alimentato direttamente dalla batteria. Il firmware applica un offset software di +300mV nella funzione di lettura ADC per compensare questa caduta e restituire un valore di tensione reale.



## 3. ARCHITETTURA HARDWARE E CONSUMI (DIGISPARK)

La scelta hardware di questo progetto è ricaduta sulla microboard **Digispark ATtiny85** invece del chip Atmel ATTiny85 in formato DIP-8 (ragnetto) nudo e crudo. Questa decisione è legata principalmente a due fattori:
1. **Estrema facilità di programmazione:** La Digispark integra a bordo un bootloader USB che permette di flashare lo sketch direttamente dal PC tramite un comune cavo Micro-USB, senza la necessità di utilizzare programmatori hardware esterni (come l'USBtinyISP o un Arduino configurato come ISP) e senza dover impazzire con i collegamenti dei pin di reset e clock.
2. **Praticità nei test sul banco:** Permette di monitorare la seriale e fare debug in modo rapido e indolore durante la fase di sviluppo.

Tuttavia, il progetto non esclude la possibilità, per chi volesse abbattere ulteriormente i consumi all'osso nelle installazioni più estreme, di migrare lo stesso identico codice direttamente su un chip ATTiny85 DIP-8 indipendente montato su basetta millefori.

### Analisi dei Consumi e Modifica Hardware (Rimozione del Regolatore)
Durante i test statici sul banco di prova, il modulo fa registrare un consumo fisso e costante di circa **`8mA`**. Questo assorbimento non è generato dal microcontrollore in sé, ma principalmente dal LED rosso di Power (PWR) sempre attivo sulla board (che dissipa da solo tra i 3mA e i 5mA) e dalle piccole correnti di fuga dei diodi zener della linea USB.

Per ottimizzare l'efficienza energetica del nodo ed eliminare i consumi parassiti inutili, **è stato rimosso fisicamente il regolatore di tensione lineare a bordo (78M05)**. 

Questa modifica è sicura e perfettamente logica per due motivi:
* Il regolatore lineare dissipa energia a vuoto sotto forma di calore per "portare" la tensione a 5V stabili.
* Per la natura stessa delle batterie utilizzate in questi nodi off-grid (tipicamente celle LiPo o LiFePO4 con BMS integrato di gestione e protezione batteria), le tensioni del circuito non supereranno mai la soglia critica dei 5V. La Digispark viene quindi alimentata direttamente dalla cella, escludendo l'autospegnimento causato dalla caduta di tensione (*dropout*) del regolatore quando la batteria scende.

### Schema di Collegamento e Gestione del MOSFET a Canale P
Il controllo dell'alimentazione dell'Heltec V3 avviene interrompendo il ramo positivo ($VCC$) tramite un **MOSFET a Canale P (P-MOS)**. Questa scelta è obbligatoria: interrompere la massa ($GND$) impedirebbe il funzionamento della comunicazione seriale (TX/RX) e del bus I2C, isolando i chip tra loro.

La massa ($GND$) deve rimanere sempre in comune tra la batteria, la Digispark, l'RTC e l'Heltec.

#### Mappatura dei Collegamenti della Digispark:

* **Pin `5V`:** Alimentazione tramite Diodo Schottky (positivo batteria -> Diodo -> VCC ATtiny/RTC).
* **Pin `GND`:** Massa comune di tutto il sistema.
* **Pin `P0` (SDA):** Linea dati I2C collegata al pin SDA del modulo RTC (DS3231 o DS1307).
* **Pin `P1` (Controllo):** Collegato al **Gate** del MOSFET a Canale P attraverso una resistenza di limitazione (es. 220 $\Omega$). Un resistore di pull-up da 10k $\Omega$ tra il Gate e il positivo della batteria assicura che l'Heltec rimanga spento di sicurezza se la Digispark dovesse resettarsi o spegnersi.
* **Pin `P2` (SCL):** Linea clock I2C collegata al pin SCL del modulo RTC.
* **Pin `P3` (RX Seriale):** Collegato al pin **TX 46** dell'Heltec V3 per ricevere i comandi radio decodificati dal PIN di sicurezza.
* **Pin `P4` (TX Seriale):** Collegato al pin **RX 45** dell'Heltec V3 per inviare le stringhe di risposta e telemetria.

### Logica Invertita del P-MOS
Il firmware pilota il Gate del MOSFET sfruttando la logica invertita tipica dei canali P:
* **Scrittura pin P1 a `LOW` (0V):** Il Gate va a massa, il MOSFET conduce e permette il passaggio di corrente $\rightarrow$ **Heltec ACCESO**.
* **Scrittura pin P1 a `HIGH` (VCC):** Il Gate va alla tensione di batteria, il MOSFET si isola completamente interrompendo il flusso $\rightarrow$ **Heltec SPENTO**.



## 4. LOGICA DI FUNZIONAMENTO E ALGORITMO AD IMBUTO
Il firmware esegue il ciclo di controllo ogni 10 secondi. Questo intervallo è stato scelto per tre ragioni: riduce il consumo della Digispark, evita di stressare il bus I2C e agisce come un filtro naturale contro i micro-sbalzi di tensione causati dai picchi di trasmissione radio dell'Heltec V3.

La stabilità totale del sistema è garantita da una struttura logica a imbuto, dove le condizioni elettriche della batteria hanno la priorità assoluta su qualsiasi impostazione oraria. Il sistema gestisce due soglie fondamentali in millivolt salvate nella EEPROM:

- sMin (Soglia Minima, default 3600mV): Il punto di crollo oltre il quale la cella rischia la scarica profonda.

- sRes (Soglia di Rientro/Wakeup, default 3800mV): La tensione di sicurezza che garantisce che la batteria stia accumulando vera energia dal sole.

L'algoritmo crea una "Zona Grigia" intelligente tra queste due soglie, gestita tramite un flag logico (bloccoBatteria) che memorizza lo stato storico del circuito:

- In fase di scarica (Discesa): Se la batteria è carica e scende lentamente, il sistema rimane in stato OK. L'Heltec resta acceso e continua a seguire la programmazione oraria. La zona grigia viene ignorata perché la batteria non ha ancora toccato il fondo (sMin).

- In fase di emergenza (Crollo): Se la tensione scende sotto i 3600mV, scatta istantaneamente il blocco (bloccoBatteria = true). Il MOSFET viene forzato a HIGH (SPENTO). Da questo momento, il timer viene completamente bypassato e ignorato.

- In fase di ricarica (Risalita): Il giorno dopo sorge il sole e il pannello ricarica la cella. La tensione sale (es. 3700mV). Il sistema si trova nella zona grigia, ma si ricorda del blocco precedente. Di conseguenza, mantiene l'Heltec spento. L'Heltec riceverà il permesso di riaccendersi solo e soltanto quando la tensione eguaglierà o supererà i 3800mV (sRes).

Questo pilastro azzera l'effetto "Albero di Natale", permettendo al pannello solare di ricaricare la batteria a vuoto (senza il carico da 100mA dell'Heltec) nella sua fase più critica.

### Gestione Resilienza: Timer di Reset Periodici
Per garantire che il sistema sia sempre raggiungibile anche in caso di rari blocchi software o interruzioni seriali, sono stati implementati due timer di reset indipendenti (configurabili da 0 a 99 ore):

- **Timer A** (!1234A): Reset forzato dell'intero ATtiny tramite Watchdog Timer. È il riavvio totale del sistema.

- **Timer H** (!1234H): Reset fisico del carico (Heltec). Il MOSFET apre il circuito per 5 secondi e poi lo richiude.

- **Nota:** Se il valore è impostato a 0, il timer è disabilitato.
 
**Va sottolineato che i due timer non sono totalmente indipendenti, va da se che il timer A (quello che gestisce il reset di Attiny85) una volta che interviene porta ad un reset del microcontrollore il quale, riavviandosi, necessariamente azzera anche il timer H (quello dell heltec). Tenuto conto di questa "dipendenza" il calcolo delle ore cadenzate del reset di H non deve mai superare le ore del reset di A per una corretta ciclicità dei reset A e H.**

### Strategia di Reset Consigliata

| Componente | Strategia di Reset (Esempio) | Obiettivo |
| :--- | :--- | :--- |
| **Heltec (Carico)** | Ogni 47 ore | Pulizia ciclica della periferica |
| **ATtiny (Sistema)** | Ogni 96 ore | Reset "Hard" globale del supervisore |

## 5. NOTE TECNICHE E RISOLUZIONE PROBLEMI (Troubleshooting)
Stabilità dei livelli logici (Diodo Schottky):
Per garantire una comunicazione seriale affidabile tra l'ATtiny (alimentato via diodo) e l'Heltec (alimentato diretto), è stato inserito un diodo Schottky in serie al ramo positivo dell'ATtiny. Questo crea un offset di ~300mV, armonizzando i livelli logici anche a piena carica solare. Il firmware applica una compensazione software per mantenere precisa la lettura ADC.
Si sarebbe dovuto usare un convertitori di livelli logici di segnale da 3.3v a 5.0V che se volete potete interporre nei collegamenti rx/tx tra heltec e ATtiny, ma nella logica della **"estrema semplicità di cotruzione"** ho voluto evitare.



## 6. INTERFACCIA COMANDI CLI (SERIALE VIA RADIO) E SICUREZZA DEL PIN

### Modalità TEXTMSG Seriale
Il controllo remoto e la telemetria del sistema si basano su un'interfaccia a riga di comando (CLI) protetta. Per garantire che il flusso di dati sia leggibile e gestibile in tempo reale dalla Digispark, il firmware dell'Heltec V3 deve essere configurato tassativamente in modalità TEXTMSG (Serial Mode).

Questa specifica modalità di funzionamento è cruciale: essa istruisce l'Heltec a convertire ogni pacchetto dati ricevuto dalla rete radio in una semplice stringa di testo ASCII sulla sua porta seriale fisica (GPIO 45/46). In questo modo, la Digispark intercetta il flusso di caratteri in tempo reale senza la necessità di complessi parser per protocolli binari, permettendo una risposta immediata ai comandi.

### Meccanismo di Sicurezza: Il PIN a 4 Cifre
Il sistema di controllo remoto è protetto da un PIN numerico a 4 cifre (impostato di default a 1234), necessario per validare ogni comando inviato via radio. La sintassi universale di ogni stringa di comando è: ![PIN][COMANDO][VALORE].

Validazione: La Digispark intercetta il carattere iniziale ! e verifica immediatamente se le 4 cifre successive corrispondono a quelle memorizzate nella sua EEPROM.

Scarto: Se il PIN non coincide, il sistema scarta l'intera stringa in modo silenzioso, prevenendo tentativi di manomissione o invii accidentali da parte di altri nodi sulla rete.

**Recupero/Modifica in caso di errore**: In caso di errore umano durante la programmazione, se il PIN venisse inviato accidentalmente senza il nuovo pin (es. !1234P" automaticamente viene impostato su 0000, anche gli altri comandi che richiedono inserimento numerico, in caso di assenza verra impostato a 00. Ritornando all'eventualità di aver dato comando di cambio PIN con es. !1234P verra accettato e il nuovo pin sarà 0000. Bastera quindi reimpostare in pin corretto digitando **!0000P[NuovoPIN]**.

### Ottimizzazione dei Feedback (Risposte a Carattere Singolo)
Poiché la memoria Flash della Digispark è occupata al **99%**, è stato impossibile inserire stringhe di testo descrittive per le risposte (es: `"Ora modificata con successo"`). Ogni stringa di testo estesa avrebbe occupato centinaia di byte, causando il fallimento della compilazione dello sketch.

Per ovviare a questo limite hardware, il firmware implementa un sistema di **Feedback a Carattere Singolo**. Quando un comando viene eseguito correttamente, la Digispark risponde sputando sulla seriale (e di conseguenza sulla tua app Meshtastic) un unico carattere maiuscolo indicativo dell'operazione avvenuta:

* **`T`** $\rightarrow$ **Time:** Conferma che l'orario dell'orologio hardware RTC è stato aggiornato.
* **`M`** $\rightarrow$ **Minimum:** Conferma che la soglia minima di batteria (`sMin`) è stata modificata.
* **`W`** $\rightarrow$ **Wakeup:** Conferma che la soglia di rientro dall'isteresi (`sRes`) è stata modificata.
* **`K`** $\rightarrow$ **OK:** Conferma che l'orario programmato di accensione (`ON`) o spegnimento (`OF`) è stato salvato.
* **`P`** $\rightarrow$ **PIN:** Conferma che il PIN di sicurezza a 4 cifre è stato cambiato con successo.
* **`R`** $\rightarrow$ **Reset:** Conferma che il comando di Hard Reset fisico è stato preso in carico.
* **`A`** $\rightarrow$ **ATtiny**: Conferma che il timer di reset periodico dell'ATtiny è stato impostato.
* **`H`** $\rightarrow$ **Heltec**: Conferma che il timer di reset fisico del carico è stato impostato.
* **`E`** $\rightarrow$ **Error**: Errore invio del comando. Comando non riconosciuto.

 ### Nota bene sui comandi di lettura (Get)
 ##### I comandi che iniziano con g (es. gA, gH, gB, gP, gT) non restituiscono un feedback di conferma K, ma inviano direttamente il valore corrente del parametro richiesto sulla seriale, permettendoti di monitorare lo stato del nodo in tempo reale.
---

### Dizionario Completo dei Comandi di Configurazione

Tutti i valori di tensione devono essere espressi in millivolt (es. 3.4V diventa `3400`). Tutti i valori temporali devono essere espressi nel formato a 4 cifre `HHMM` (es. le 8 del mattino diventano `0800`).

| Stringa Comando | Azione Svolta dal Firmware | Feedback | Esempio Pratico |
| :--- | :--- | :---: | :--- |
|**`!1234A[ore]`**	|Imposta timer reset ATtiny (0-99h)	|`A`	|!1234A24|
|**`!1234H[ore]`**	|Imposta timer reset Heltec (0-99h)	|`H`	|!1234H12|
| **`!1234T[HHMM]`** | Sincronizza l'ora e i minuti correnti sul modulo orologio hardware RTC. | `T` | `!1234T1435` (Imposta l'orologio alle 14:35) |
| **`!1234M[mV]`** | Modifica la soglia minima di spegnimento d'emergenza (`sMin`). | `M` | `!1234M3400` (Imposta lo spegnimento a 3.40V) |
| **`!1234W[mV]`** | Modifica la soglia di sblocco e risveglio dall'isteresi (`sRes`). | `W` | `!1234W3650` (Imposta il wakeup a 3.65V) |
| **`!1234ON[HHMM]`**| Imposta l'orario del timer giornaliero per l'accensione dell'Heltec. | `K` | `!1234ON0815` (Accensione automatica alle 08:15) |
| **`!1234OF[HHMM]`**| Imposta l'orario del timer giornaliero per lo spegnimento dell'Heltec. | `K` | `!1234OF2315` (Spegnimento automatico alle 23:15) |
| **`!1234P[Nuovo]`** | Sostituisce il vecchio PIN di sicurezza con un nuovo codice a 4 cifre. | `P` | `!1234P5678` (Cambia il PIN d'accesso in 5678) |
| **`!1234R0`** | **Hard Reset Fisico:** Forza il MOSFET a spegnere l'Heltec per 5s e poi lo riaccende. | `R` | `!1234R0` (Sblocca l'Heltec se congelato o in crash) |



---

### Comandi di Diagnostica e Telemetria (Get)

Inviando questi comandi, l'interfaccia risponderà restituendo una stringa compatta contenente i dati archiviati nella memoria non volatile (EEPROM) o estratti in tempo reale dai sensori.

#### 1. Richiesta Dati Batteria (`!1234gB`)
Restituisce lo stato elettrico attuale del nodo. 
* *Formato Risposta:* `TensioneAttuale-sMin-sRes`
* *Esempio:* Se invii `!1234gB` e ricevi `3580-3600-3800`, significa che la batteria gode di ottima salute ed è a 3.58V, lo spegnimento è tarato a 3.6V e il rientro a 3.8V.

#### 2. Richiesta Programmazione Oraria (`!1234gP`)
Restituisce la finestra oraria operativa in cui il nodo ha il permesso di rimanere acceso.
* *Formato Risposta:* `HH:MM-HH:MM`
* *Esempio:* Ricevere `08:15-23:15` indica che il nodo si spegnerà automaticamente ogni notte alle 23:15 per poi risvegliarsi alle 08:15 del mattino successivo.

#### 3. Richiesta Orario RTC (`!1234gT`)
Interroga direttamente i registri del modulo RTC per verificare la sincronizzazione dell'orologio di sistema.
* *Formato Risposta:* `HH:MM`
* *Esempio:* Ricevere `21:45` permette di verificare da remoto se l'orologio interno del palo è allineato con l'ora reale o se ha accumulato deriva temporale.
####  4. Richiesta Timer Reset Heltec (`!1234gH`)
Restituisce il valore del timer di reset ciclico del carico (Heltec).

- Formato Risposta: `HH` (ore)

- Esempio: Ricevere `12` indica che il sistema è configurato per resettare fisicamente l'Heltec ogni 12 ore.

#### 5. Richiesta Timer Reset ATtiny (`!1234gA`)
Restituisce il valore del timer di reset ciclico del microcontrollore ATtiny.

- Formato Risposta: `HH` (ore)

- Esempio: Ricevere `24` indica che il sistema effettuerà un riavvio totale (Watchdog) ogni 24 ore.



## 7. OTTIMIZZAZIONE LOW-LEVEL E GUIDA AL CARICAMENTO

L'intero firmware è stato sviluppato seguendo criteri di ottimizzazione estrema per consentire la coesistenza di funzionalità complesse (seriale software, calcoli temporali, gestione EEPROM e bus I2C) entro i limiti fisici della Flash della Digispark, occupando ben il **99% dello spazio disponibile**.

### Trucchi di Ottimizzazione a Basso Livello
Per evitare il fallimento della compilazione dovuto alla saturazione della memoria, il codice implementa tre strategie ingegneristiche:
1. **Manipolazione Diretta dei Registri EEPROM:** L'inclusione della libreria standard `<EEPROM.h>` di Arduino introduce un sovraccarico (*overhead*) di memoria non sostenibile. Il firmware scrive e legge i dati interfacciandosi direttamente con i registri macchina nativi del chip ATtiny85: `EECR` (Control Controllo), `EEAR` (Controllo Indirizzo) e `EEDR` (Registro Dati).
2. **Rimozione di `Wire.h` e `RTClib.h`:** Le librerie standard per l'orologio gestiscono strutture dati pesanti (stringhe dei giorni, calcoli degli anni bisestili, timestamp Unix a 32 bit). Questo firmware interroga l'RTC tramite la libreria essenziale `TinyWireM.h` puntando direttamente ai singoli registri di ore (`0x02`) e minuti (`0x01`), convertendo i dati tramite macro matematiche bit-a-bit ad alte prestazioni (`b2d` e `d2b`).

---

### Procedura di Configurazione e Flash (IDE Arduino)

Per garantire la stabilità della seriale software a 9600 baud ed evitare la corruzione dei caratteri ASCII via radio, la Digispark deve essere configurata tassativamente a **8 MHz**. Segui questa procedura guidata per il caricamento:

1. **Installazione del Core:** Apri l'IDE di Arduino, vai in *Impostazioni* e aggiungi questo URL nei "Gestori di schede aggiuntive": `http://drazzy.com/package_drazzy.com_index.json` (Core **ATTinyCore** di Spence Konde, il migliore e più ottimizzato per la gestione dei timer).
2. **Installazione Librerie:** Accedi al Gestore Librerie di Arduino e installa la libreria **`TinyWireM`**.
3. **Configurazione Parametri:** Nel menu *Strumenti*, seleziona esattamente queste voci:
   * **Scheda:** `ATtiny25/45/85 (No bootloader)`
   * **Chip:** `ATtiny85`
   * **Clock:** `8 MHz (Internal)` $\leftarrow$ *FONDAMENTALE: Se lasci l'impostazione di fabbrica a 1 MHz, il timing della seriale sballerà e i comandi radio risulteranno illeggibili.*
4. **Scrittura dei Fuse:** Collega la Digispark al PC, seleziona il tuo programmatore e clicca sulla voce **"Scrivi Bootloader"**. Questo passaggio non carica lo sketch, ma modifica i *fuse* interni del chip forzando l'oscillatore interno a lavorare a 8 MHz. *(Operazione obbligatoria solo la prima volta).*
5. **Caricamento Firmware:** Clicca sulla freccia di caricamento (o *Carica tramite un programmatore*) per flashare lo sketch definitivo.
