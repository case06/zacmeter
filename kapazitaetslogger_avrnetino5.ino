// ZACmeter
// 
// Software for metering and logging the capacity of an accu or battery 
// or ZAC+-Device on an Arduino-like Hardware with measuring interface.
// This Version is especially comatible with an AVR-NetIO-Board running
// AVR-Netino as Arduino-Bootloader.
//
// Description of the whole project is available on the OSEG-Website:
// http://wiki.opensourceecology.de/ZACmeter
//
// C-BY-SA 3.0: This work is Open Source and licensed under the 
// Creative Commons Attribution-ShareAlike 3.0 License.
//
// 15.04.2013 Case06

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#include <EtherCard.h>

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
static byte myip[] = { 192,168,178,4 };

byte Ethernet::buffer[500];
BufferFiller bfill;

// word len;
// word pos;

LiquidCrystal_I2C lcd(0x20,8,2);  // set the LCD address to 0x20 for a 16 chars and 2 line display

const int s1 = 10;     // the number of the pushbutton pin
const int s2 = 9;     // the number of the pushbutton pin

// Referenzwert zur Multiplikation mit den Werten der Analogports (0...1023), 
// um auf die Voltzahl zu kommen (0...5). 
// Ergibt sich aus 5/1024. (d.h., Systemspanung / Bit-Auflösung)
// z.B. const float referenzwert = 0.0048828125;	// Referenzwert in Volt
// oder const float referenzwert = 4.8828125;	        // Referenzwert in mV
const float Usys = 5.16;   // Systemspannung an AVR-NetIO mit PC-Netzteil
// const float Usys = 4.89;   // Systemspannung an Arduino-Mega mit USB-Stromversorgung
// const float Usys = 3.30;   // Referenzspannung an Aref über Linearregler LM317
float referenzwert = Usys*1000/1024; // Referenzwert in mV

#define  PINUbatt   13       // Analog PIN Batteriespannung
#define  PINUDS     14       // Analog PIN Spannungsabfall am FET (Drain-Source)
#define  PINRelais  15       // Digital PIN Relais oder MOSFET
#define  PINStart   9       // Digital PIN Startknopf 
#define  PINStart2  10       // Digital PIN Startknopf 

#define  Umin 950           // mV Spannung bei der Entladung abgebrochen wird
#define  INTERVAL  2000     // Zeit fuer Schleife


// div. Globals
long loopTime = INTERVAL;
long loopUsedTime = INTERVAL;
long loopStart = 0;
long uhrzeit = 0;
long verstrichen = 0;
float U = 0;
float Ufet = 0;
float Udiff = 0;
float Uopen = 0;
float I = 0;
float P = 0;
float Q = 0;
float Ah = 0;
// Lastwiderstand Rlast vorher genau messen:
// Für kurze Zeit eine möglichst hohe Spannung an den 
// Widerstand anlegen und dabei den Strom messen, damit 
// man einen genauen Widerstandswert erhält. Maximale 
// Belastbarkeit beachten!
float Rlast = 1.6032; // Lastwiderstand zur Entladung
float Rbat = 0;  // Innenwiderstand der Batterie

long loopCount = 0;
boolean start = false;
boolean ende = false;

void setup()  {
  //Serial.begin(19200);
  pinMode(PINRelais, OUTPUT);
  pinMode(PINStart, INPUT); // Start Taster 
  //digitalWrite(PINStart, HIGH); // je nachdem, ob High- oder Low-aktiver Button
  digitalWrite(PINStart, LOW);
  
  digitalWrite(PINRelais,LOW);   // Relais bzw. Mosfet aus 
  
  lcd.init();                      // initialize the lcd 
 
  if (ether.begin(sizeof Ethernet::buffer, mymac,28) == 0)
  {
    // Serial.println( "Failed to access Ethernet controller");
    lcd.setCursor(0,0);
    lcd.print("eth0 failed");
    delay(1000);
    lcd.clear();
  }  
  ether.staticSetup(myip);
 
  // Print a message to the LCD.
  lcd.setCursor(0,0);
  lcd.print("Kap-tester V.006");
  lcd.setCursor(0,1);
  lcd.print("by OS 04/2013");
  delay(2000);
  lcd.clear();
  
  Uopen = analogRead(PINUbatt)*referenzwert;       // in mV
  
}

void loop()  {
 
  // start = true;   // Für (Batterie-)Messung bis zum bitteren Ende, d.h. Tiefentladung
  
  // erstmal Spannung messen  - solange der Starttknopf nicht gedrueckt wurde - nach dem Test auch wieder hier rein...
  while (start==false) 
  { 
    U = analogRead(PINUbatt)*referenzwert;       // in mV
    Uopen = U;
    //print_ser();
    print_lcd();
    //if (digitalRead(PINStart)==LOW) {
    if (digitalRead(PINStart)==HIGH) {
    start = true;
    ende = false;
  }
 
 
  uhrzeit = millis();
  verstrichen = millis()-uhrzeit;

  while(verstrichen < INTERVAL-10)
  {
    word len = ether.packetReceive();
    word pos = ether.packetLoop(len);  
    if (pos)  // check if valid tcp data is received
      ether.httpServerReply(homePage()); // send web page data
      
    verstrichen = millis()-uhrzeit;
  } // end of while  
   
  //delay(2000);
  } // end of while
  
  digitalWrite(PINRelais,HIGH);   // Relais ein  
  
  loopStart = millis();
  
  U = analogRead(PINUbatt)*referenzwert;      // in mV
  Ufet = analogRead(PINUDS)*referenzwert;     // in mV
  Udiff = U-Ufet;
  
  //I = Udiff/1.5;     // in mA
  //I = U-Ufet/1.5;    // in mA  Achtung: Falsche Schreibweise !!! Punkt vor Strichrechnung !!!
  I = Udiff/Rlast;   // in mA

  // Leistung
  P = (I*U)/1000;   // in mW
  Q += P * loopTime / 3600 / 1000;         // in mWh
  
  // Kapazität in mAh
  Ah += I * loopTime / 3600 / 1000;          // in mAh
  
  // Innenwiderstand der Batterie
  Rbat = (Uopen - U) / I;
  
  //print_ser();
  print_lcd();
  
  if(U <= Umin) {                     // wenn Batteriespannung unter eingest. Mindestspannung faellt
    digitalWrite(PINRelais,LOW);      // Test abbrechen
    start = false;
    //Serial.println("Stop");
    ende = true;
   }

  loopCount++;
  loopUsedTime = millis()-loopStart;
 

  while(loopUsedTime < INTERVAL-10)
  {
    word len = ether.packetReceive();
    word pos = ether.packetLoop(len);  
    if (pos)  // check if valid tcp data is received
      ether.httpServerReply(homePage()); // send web page data
      
    loopUsedTime = millis()-loopStart;
  } // end of while  
  
  loopTime = millis() - loopStart;
  
}


void print_lcd()  {
  lcd.clear();
  lcd.print(U/1000,2);    // Spannung in Volt anzeigen
  lcd.print("V");
  lcd.setCursor(6,0);
  format_print(I);
  lcd.print("mA"); 
  lcd.setCursor(13,0);
  if (start==false && ende == false) lcd.print("STA");   //  ST - Startknopf druecken
  if (start==true) lcd.print("RUN");                     //  RU - Test runnig...
  if (ende==true) lcd.print(" END");                      //  E  - Test Ende
  lcd.setCursor(0,1);
  format_print(Q);
  lcd.print("mWh");
  
/*  lcd.setCursor(10,1);
  //lcd.print(Ufet/1000,4);
  lcd.print(Udiff/1000,4);
*/


  lcd.setCursor(11,1);
  lcd.print(loopCount/(60000/INTERVAL));                
  lcd.print(":");
  if(((loopCount%(60000/INTERVAL))*INTERVAL/1000)<10)  lcd.print("0");
  lcd.print((loopCount%(60000/INTERVAL))*INTERVAL/1000);  
   
  }

void format_print (float value){ // in Ausgabe passende Anzahl Spaces einfuegen - 4stellige Zahl
  if (value < 1000)lcd.print(" ");
  if (value < 100) lcd.print(" ");
  if (value < 10)  lcd.print(" ");
  lcd.print(value,0);
}

//void print_ser()  {
//  Serial.print(loopCount/(60000/INTERVAL));
//  Serial.print(":");
//  if(((loopCount%(60000/INTERVAL))*INTERVAL/1000)<10)  Serial.print("0");
//  Serial.print((loopCount%(60000/INTERVAL))*INTERVAL/1000);
//  Serial.print("    ");
//  Serial.print((int)U);                       Serial.print(" mV   ");
//  Serial.print((int)Ufet);                    Serial.print(" mV   ");
//  Serial.print((int)I);                       Serial.print(" mA   ");
//  Serial.print((int)P);                       Serial.print(" mW       ");
//  Serial.print((int)Q);                       Serial.println(" mAh");
//}


static word homePage() {
  long t = millis() / 1000;
  word h = t / 3600;
  byte m = (t / 60) % 60;
  byte s = t % 60;
  
  word Up = U;
  word Ip = I;
  word Qp = Q;
  word Ap = Ah;
  word Rp = Rbat;
  
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<meta http-equiv='refresh' content='10'/>"
    "<title>Kapazitaetslogger</title>" 
    // "<h1>$D$D:$D$D:$D$D <br><br> $DmV $DmA $DmAh</h1>"),
    "<h1>$D$D:$D$D:$D$D $D $D $D $D $D </h1>"),
      h/10, h%10, m/10, m%10, s/10, s%10, Up, Ip, Qp, Ap, Rp);
  return bfill.position();
}





