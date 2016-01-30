/**************************************************************************/
/*! 
    @file     arduino-lockitron-nfc
    @author   Alfredo Rius
    @license  BSD

    This is an implementation of the NFC breakout for the PN532 from Adafruit
    and the Lockitron platform to allow access to doors.

    This program is based on the arduino-lockitron-nfc shield located (Eagle
    files in https://github.com/devalfrz/arduino-lockitron-nfc).

    Buttons: (buttons are set to have a pullup resistor so they ar inverted)
      - SLOT_SELECT: Selects a memory slot. STATUS_LED will blink n times depending
          on the slot that has been selected.
            
      - MEMORY_BUTTON: If it's pressed for 3 seconds while a card is read,
          it will either override the selected slot or delete the card if it
          was already registered.

      - LOCK_UNLOCK: This button will lock or unlock the system depending on its
          previous state.
          

*/
/**************************************************************************/

/**** INPUTS/OUTPUTS ****/
// Adafruit PN532 breakout board:
#define PN532_SCK  18
#define PN532_MOSI 17
#define PN532_SS   16
#define PN532_MISO 19
// Buttons
#define SLOT_SELECT 10
#define MEMORY_BUTTON 9
#define STATUS_LED 13
#define UNLOCKED 12
#define LOCKED 11
// Lockitron
#define MOTOR_A 6
#define MOTOR_B 5
#define SW1 2
#define SW2 3
#define LOCK_UNLOCK 8

/**** Other definitions ****/
#define TOTAL_SLOTS 4 // Arbitrary number of slots available
#define SLOT_SIZE 4
//#define DEBUG // Will show output on terminal



#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <EEPROM.h>


// Adafruit PN532 init SPI
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);


int memory_slot = 0;
boolean state = 0;


void setup(void) {
  pinMode(MEMORY_BUTTON,INPUT_PULLUP);
  pinMode(SLOT_SELECT,INPUT_PULLUP);
  pinMode(STATUS_LED,OUTPUT);
  pinMode(LOCKED,OUTPUT);
  pinMode(UNLOCKED,OUTPUT);
  pinMode(SW1,INPUT_PULLUP);
  pinMode(SW2,INPUT_PULLUP);
  pinMode(LOCK_UNLOCK,INPUT_PULLUP);
  pinMode(MOTOR_A,OUTPUT);
  pinMode(MOTOR_B,OUTPUT);
  
  #ifdef DEBUG
  Serial.begin(115200);
  #endif

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    #ifdef DEBUG
    Serial.print("Didn't find PN53x board");
    #endif
    digitalWrite(STATUS_LED,HIGH);
    while (1); // halt
  }
  #ifdef DEBUG
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  #endif
  
  // configure board to read RFID tags
  nfc.SAMConfig();
}


void loop(void) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;    // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  int i,j;
  boolean save_delete = 0;
  boolean access_granted = 0;
  
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,100);
  
  save_delete = (success && !digitalRead(MEMORY_BUTTON)) ? 1 : 0;
  
  if (success) {
    digitalWrite(STATUS_LED,HIGH);
    delay(100);
    digitalWrite(STATUS_LED,LOW);
    #ifdef DEBUG
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");
    #endif
    for(i=0;i<TOTAL_SLOTS;i++){ //Check all memory slots
      if(check_card(uid,i))
        access_granted = 1;
    }
    if(save_delete){
      access_granted = 0; //Reset Access
      #ifdef DEBUG
      Serial.println("Continue pressing to save or delete card");
      #endif
      delay(3000);
      if(!digitalRead(MEMORY_BUTTON)){
        if(save_delete_card(uid,memory_slot)){
          #ifdef DEBUG
          Serial.print("Save Card Slot: ");
          Serial.println(memory_slot);
          #endif
          blink_n_times(memory_slot+1,STATUS_LED);
        }else{
          #ifdef DEBUG
          Serial.println("Delete Card");
          #endif
          digitalWrite(STATUS_LED,HIGH);
          delay(2000);
          digitalWrite(STATUS_LED,LOW);
        }
      }
    }
    if(access_granted){
      #ifdef DEBUG
      Serial.println("Access Granted");
      #endif

      // Lock/Unlock system.
      digitalWrite(STATUS_LED,HIGH);
      state = lock(!state);
      digitalWrite(STATUS_LED,LOW);
      delay(100);

      for(i=0;i<TOTAL_SLOTS;i++){ // Usefull to know the slot in which the card
                                  //  is saved :) 
        if(check_card(uid,i))
          blink_n_times(i+1,STATUS_LED);
      }
    }
    else{
      #ifdef DEBUG
      Serial.println("Access Denied");
      #endif
    }
    access_granted = 0; //Reset Access
  }
  if(!digitalRead(SLOT_SELECT)){// Select memory slot
    while(!digitalRead(SLOT_SELECT));
    memory_slot = (memory_slot < TOTAL_SLOTS - 1) ? memory_slot + 1 : 0;
    #ifdef DEBUG
    Serial.print("Slot selected: ");
    Serial.println(memory_slot);
    #endif
    blink_n_times(memory_slot+1,STATUS_LED);
  }
  if(!digitalRead(LOCK_UNLOCK)){// Select slot
    state = lock(!state);
  }
}

boolean save_delete_card(uint8_t *uid,int addr){
  /*
   * If card doesn't exist in any slot, then it is saved in "addr", 
   *   else it is deleted from memory.
   */
  int i;
  int j;
  int memory_offset = SLOT_SIZE*addr;
  boolean save = 1;
  
  for(i=0;i<TOTAL_SLOTS;i++){ //Check if card is saved in any slot
    if(check_card(uid,i)){
      save = 0; //Delete if found
    }
  }
  if(save){
    for(i=0;i<SLOT_SIZE;i++){
      EEPROM.write(memory_offset+i,uid[i]);
    }
    return(1);
  }else{
    for(i=0;i<TOTAL_SLOTS;i++){ //Delete card from all slots
      if(check_card(uid,i)){
        memory_offset = SLOT_SIZE*i;
        for(j=0;j<SLOT_SIZE;j++){
          EEPROM.write(memory_offset+j,0);
        }
      }
    }
    return(0);
  }
}
boolean check_card(uint8_t *uid,int addr){
  /* 
   *  Checks if card (uid) is contained in the designed address (addr)
   */
  int i;
  int memory_offset = SLOT_SIZE*addr;
  for(i=0;i<SLOT_SIZE;i++){
    if(EEPROM.read(memory_offset+i)!=uid[i]) return 0;
  }
  return 1;
}
void blink_n_times(int n,int led_pin){
  /*
   * Blink n times
   */
  int i;
  for(i=0;i<n;i++){
    digitalWrite(led_pin,HIGH);
    delay(250);
    digitalWrite(led_pin,LOW);
    delay(250);
  }
}

int lock(int state){
  /* 
   *  Opens or closes Lockitron door
   */
  digitalWrite(STATUS_LED, HIGH);
  digitalWrite(UNLOCKED, LOW);
  digitalWrite(LOCKED, LOW);
  digitalWrite(MOTOR_A, state);
  digitalWrite(MOTOR_B, !state);
  if(state){
    while(digitalRead(SW1));
    while(!digitalRead(SW2));
  }
  else{
    while(digitalRead(SW2));
    while(!digitalRead(SW1));
  }
  digitalWrite(MOTOR_A, LOW);
  digitalWrite(MOTOR_B, LOW);
  digitalWrite(STATUS_LED, LOW);
  digitalWrite(UNLOCKED, state);
  digitalWrite(LOCKED, !state);
  return state;
}
