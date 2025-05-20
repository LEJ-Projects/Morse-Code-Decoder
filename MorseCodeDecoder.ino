// MorseCodeDecoder
// February 23, 2025
// Lloyd Johnson
// 3/2/25 L. Johnson - Added 1602_I2C display
// 3/12/25 L. Johnson - Added code for LED
// 3/17/25 L. Johnson - Set new screen delay to 9.5 seconds
// 4/4/25 L. Johnson - Added "smart" scrolling
// 4/18/25 L. Johnson - Modified to prevent dormant time scrolling until
//                    a character is received.
// 5/19/25 L. Johnson - General cleanup (mostly comments)

#include <Wire.h>
#include<LiquidCrystal_I2C_Hangul.h>

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C_Hangul lcd(0x27, 16, 2);

#define TRUE 1
#define FALSE 0

// Time constants in milliseconds
#define DORMNT 6000 //Dormant threshold - 6000ms.  Amount of time
                    //allowed before automatic scrolling occurs.
#define ENDCHR 500 //Amount of dormant time before decoding of character ends
#define NOISTM 25  // Noise time - pulses less than 25 ms are discarded
#define MINDAH 250 // Minumum duration for a dah.  Anything shorter is a dit.
#define SPCTHR 1000 // Space Threshold - Dormant time longer than this results
                    // in a space inserted (assuming DORMNT is not exceeded)
int ocp=3;  // analog pin connected to optical coupler
float ocv;  // Analog voltage at optical coupler
int mtime;  // millisecond timer
int done;   // Flag indicating character input is complete
int prev_st;  // Previous state of key (1=pressed, 0=not pressed)
int dur[12];  // duration of symbol
int dbdur[12];  // debounced duration
int dbsymcnt;   // debounced symbol count 
int symcnt;  // symbol count - total number of Morse code symbols in character
int zdur;    // duration of zero time (time when no key is pressed)
int mc[12];  // =0 if dot.  =1 if dash.  
int i;   // loop increment variable
char mcd[2];  // Array containing the decoded character in mcd[0] and a string terminator (0) in mcd[1]
int spfl;   // Flag that when TRUE indicates the next character is to be 
            // preceded by a space
int  scrlnw;   // Scroll Now Flag 
char dl1[17]; // Display line 1
char dl2[17];  // Display line 2 
char dlt[17];  // Display line Temp
int ccnt;   // character count
int lstsp;  // Position of the last space
int snckf=FALSE;   // scroll now check flag

void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  dl2[16]=0;  // Terminate string
  ccnt=0;
  pinMode(7,OUTPUT);
}   // End of setup()

void loop() {

  // Task 1: Get a dit-dah character
  getddchar();  // Get a di/dah character


  // Task 2: Decode dit-dah charachter
  mcd[1]=0;  //Set string terminator of Morse Code Decoded string
  if (mc[0]==0) {  //Check for dit
    if (dbsymcnt==1) mcd[0]='E'; // if debounced symbol count = 1
    else dd0();  //First symbol is dit, but wait, there's more
  } 
  if (mc[0]==1) {  //Check for dah (what else could it be?)
    if (dbsymcnt==1) mcd[0]='T';  // Same logic as dit
    else dd1();
  } 

// After the above tests and nest function calls are made,
// we now have a decoded Morse Code Character in mcd[0].
// It's time to display it.

// Task 3: Display the decoded Morse Code character
 
// Perform a "scroll now" check
  if ( scrlnw==TRUE) {
    ccnt=0;
    lcd.clear();
    lcd.print(dl2);  // Print Line 2 on line 1.
    Serial.println("");
    lcd.setCursor(0, 1);
    spfl=FALSE;
  }
  
// Check if leading space needed
  if (spfl==TRUE) {
      if (ccnt==16) { // Scrolling on a space check
        ccnt=0;
        lcd.clear();
        lcd.print(dl2);  // Print on 1st line
        Serial.println("");
        lcd.setCursor(0,1);
        lstsp=-1;
      } else { // No scrolling required - print the space
        lcd.print(" ");
        Serial.print(" ");
        dl2[ccnt]=' ';
        lstsp=ccnt;  // Remember the last space position
        ccnt++;
      }
  }

// Print the decoded Morse Code character
  Serial.print(mcd);
  scrolltest();
  lcd.print(mcd);
  dl2[ccnt]=mcd[0];
  ccnt++;
  dl2[ccnt]=0;  //Terminate string

}  // End of loop()

//
// Functions
//

void scrolltest()  // Scroll Smart
{
  if (ccnt==16) {
    strcpy(dl1, dl2);
    lcd.clear();
    if (lstsp>0){
      dl1[lstsp]=0;
      lcd.print(dl1);
      Serial.println("");
      lcd.setCursor(0, 1); 
      strcpy(dl2,&dl1[lstsp+1]);     
      lcd.print(dl2);
      ccnt=strlen(dl2);
      lstsp=-1;
    } else {  // no spaces scroll entire line
     ccnt=0;
     lcd.print(dl2);  // Print Line 2 on line 1.
     Serial.println("");
     lcd.setCursor(0, 1);
    }
  }
}

void getddchar() {   // Get dot/dash (dit/dah) character

  //1. Initialize Variables
  mtime=0;   // initialize millisecond timer
  symcnt=0;  //initialize symbol count
  prev_st=0;   // Assume we start with key not pressed
  zdur=0;  // Initialize zero duration counter
  spfl=FALSE;
  scrlnw = FALSE;  // Reset Scroll Now Flag
  done=FALSE;

// 2. Gather duration of key taps
  while (done==FALSE)
  {
    ocv = analogRead(ocp) * .00494; // Get/scale opitcal coupler voltage
    if (ocv>2.5) {  //Check if key is pressed
    // Key is pressed
      digitalWrite(7, HIGH);  //Turn on panel LED
      if (prev_st==0)
      {
        prev_st=1;   // set previous state for next time
        dur[symcnt]=0;
      }  // if (prev_st==0)
      else dur[symcnt]++;  // prev_st not 0
    } else {
    // Key is not pressed
      digitalWrite(7, LOW); //Turn off panel LED
      if (prev_st==1) {
        prev_st=0;
        zdur=0;
        symcnt++;
      }

      // Additional evaluations when the key is not pressed.
      // (These are performed independent  of previous state.)
      // 1. Should we increment the millisecond timer, mtime?
      if ((symcnt==0) && ( scrlnw==FALSE)) mtime++;

      // 2. Perhaps the leading space needed flag needs setting?
      if ((mtime>SPCTHR) && (ccnt>0)) spfl=TRUE;

      // 3. Set Scroll Now Flag if Scroll Now Check Flag is True and 
      //    mtime>DORMNT.
      if ((mtime>DORMNT) && (snckf==TRUE)) scrlnw=TRUE; //Set Scroll Now Flag

      // 4. Increment the zero duration variable
      if (symcnt>0) zdur++;  //The zero duration increments after 1st symbol
    }  //  if (ocv<=2.5)
    
    // Some cleanup
    delay(1);   // A 1 millisecond delay)
    if (zdur>(ENDCHR)) done=TRUE;  // A delay this long ends the character by
                                   // setting done to TRUE.
  }  //while (done==FALSE)


  //3. Debounce key tap duration and convert to Morse Code
  dbsymcnt=0;  // initialize the debounced symbol count
  for (i=0;i<symcnt;i++) {
    if (dur[i]>NOISTM) {   // ignore durations less than NOISTM ms
      dbdur[dbsymcnt]=dur[i];
      // Convert to a dah if greater than MINDAH otherwise its a dit
      if (dbdur[dbsymcnt] > MINDAH) mc[dbsymcnt]=1; else mc[dbsymcnt]=0;
      dbsymcnt++;
    } 
  }

  snckf=TRUE;  // A character was obtained,  enable new scroll check.
}

void dd0() {
  if (mc[1]==0) {
    if (dbsymcnt==2) mcd[0]='I';
    else dd00();
  } 
  if (mc[1]==1) {
    if (dbsymcnt==2) mcd[0]='A';
    else dd01();
  } 
}
void dd1() {
  if (mc[1]==0) {
    if (dbsymcnt==2) mcd[0]='N';
    else dd10();
  } 
  if (mc[1]==1) {
    if (dbsymcnt==2) mcd[0]='M';
    else dd11();
  } 
}

void dd00() {
  if (mc[2]==0) {
    if (dbsymcnt==3) mcd[0]='S';
    else dd000();
  } 
  if (mc[2]==1) {
    if (dbsymcnt==3) mcd[0]='U';
    else dd001();
  } 
}
void dd01() {
  if (mc[2]==0) {
    if (dbsymcnt==3) mcd[0]='R';
    else dd010();
  } 
  if (mc[2]==1) {
    if (dbsymcnt==3) mcd[0]='W';
    else dd011();
  } 
}


void dd10() {
  if (mc[2]==0) {
    if (dbsymcnt==3) mcd[0]='D';
    else dd100();
  } 
  if (mc[2]==1) {
    if (dbsymcnt==3) mcd[0]='K';
    else dd101();
  } 
}
void dd11() {
  if (mc[2]==0) {
    if (dbsymcnt==3) mcd[0]='G';
    else dd110();
  } 
  if (mc[2]==1) {
    if (dbsymcnt==3) mcd[0]='O';
    else dd111();
  } 
}


void dd000() {
  if (mc[3]==0) {
    if (dbsymcnt==4) mcd[0]='H';
    else dd0000();
  } 
  if (mc[3]==1) {
    if (dbsymcnt==4) mcd[0]='V';
    else dd0001();
  } 
}
void dd001() {
  if (mc[3]==0) {
    if (dbsymcnt==4) mcd[0]='F';
    else mcd[0]='F';
  } 
  if (mc[3]==1) {
    if (dbsymcnt==4) mcd[0]=' ';
    else dd0011();
  } 
}
void dd010() {
  if (mc[3]==0) {
    if (dbsymcnt==4) mcd[0]='L';
    else mcd[0]=' ';
  } 
  if (mc[3]==1) {
    if (dbsymcnt==4) mcd[0]=' ';
    else dd0101();
  } 
}
void dd011() {
  if (mc[3]==0) {
    if (dbsymcnt==4) mcd[0]='P';
    else mcd[0]=' ';
  } 
  if (mc[3]==1) {
    if (dbsymcnt==4) mcd[0]='J';
    else dd0111();
  } 
}
void dd100() {
  if (mc[3]==0) {
    if (dbsymcnt==4) mcd[0]='B';
    else dd1000();
  } 
  if (mc[3]==1) {
    if (dbsymcnt==4) mcd[0]='X';
    else dd1001();
  } 
}
void dd101() {
  if (mc[3]==0) {
    if (dbsymcnt==4) mcd[0]='C';
    else mcd[0]=' ';
  } 
  if (mc[3]==1) {
    if (dbsymcnt==4) mcd[0]='Y';
    else mcd[0]=' ';
  } 
}
void dd110() {
  if (mc[3]==0) {
    if (dbsymcnt==4) mcd[0]='Z';
    else dd1100();
  } 
  if (mc[3]==1) {
    if (dbsymcnt==4) mcd[0]='Q';
    else mcd[0]=' ';
  } 
}
void dd111() {
  if (mc[3]==0) {
    if (dbsymcnt==4) mcd[0]=' ';
    else dd1110();
  } 
  if (mc[3]==1) {
    if (dbsymcnt==4) mcd[0]=' ';
    else dd1111();
  } 
}


void dd0000() {
  if (mc[4]==0) mcd[0]='5';
  if (mc[4]==1) mcd[0]='4';
}
void dd0001() {
  if (mc[4]==0) mcd[0]=' ';
  if (mc[4]==1) mcd[0]='3';
}

void dd0011() {
  if (mc[4]==0) mcd[0]=' ';
  if (mc[4]==1) mcd[0]='2';
}

void dd0101() {
  if (mc[4]==0) mcd[0]='+';
  if (mc[4]==1) mcd[0]=' ';
}
void dd0111() {
  if (mc[4]==0) mcd[0]=' ';
  if (mc[4]==1) mcd[0]='1';
}


void dd1000() {
  if (mc[4]==0) mcd[0]='6';
  if (mc[4]==1) mcd[0]='=';
}
void dd1001() {
  if (mc[4]==0) mcd[0]='/';
  if (mc[4]==1) mcd[0]=' ';
}

void dd1100() {
  if (mc[4]==0) mcd[0]='7';
  if (mc[4]==1) mcd[0]=' ';
}

void dd1110() {
  if (mc[4]==0) mcd[0]='8';
  if (mc[4]==1) mcd[0]=' ';
}
void dd1111() {
  if (mc[4]==0) mcd[0]='9';
  if (mc[4]==1) mcd[0]='0';
}