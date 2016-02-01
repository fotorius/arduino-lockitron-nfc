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
      - LOCK_UNLOCK: This button will lock or unlock the system depending on its
          previous state.
          
      - SAVE_CARD: If it's pressed for 3 seconds while a card is read,
          it will either save the current card and blink 3 times or delete the
          card and blink one time for 2 seconds.
          
      - SLOT_SELECT: Keep this button pressed and this will clear all cards in
          memory.
          

*/
/**************************************************************************/

/**** INPUTS/OUTPUTS ****/
// Adafruit PN532 breakout board:
#define PN532_SCK  A4
#define PN532_MOSI A3
#define PN532_SS   A2
#define PN532_MISO A5
// Buttons
#define SLOT_SELECT 10
#define SAVE_CARD 9
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
#define TOTAL_SLOTS 64 // Arbitrary number of slots available
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
  pinMode(SAVE_CARD,INPUT_PULLUP);
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
  
  save_delete = (success && !digitalRead(SAVE_CARD)) ? 1 : 0;
  
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
    memory_slot = check_card(uid);
    access_granted = (memory_slot == -1) ? 0 : 1;
    
    if(save_delete){
      access_granted = 0; //Reset Access
      #ifdef DEBUG
      Serial.println("Continue pressing to save or delete card");
      #endif
      delay(3000);
      if(!digitalRead(SAVE_CARD)){
        if(memory_slot == -1){
          memory_slot = save_card(uid);
          #ifdef DEBUG
          Serial.print("Save Card Slot: ");
          Serial.println(memory_slot+1);
          blink_n_times(memory_slot+1,STATUS_LED,250);
          delay(1000);
          #endif
          blink_n_times(3,STATUS_LED,100);
        }else{
          delete_card(memory_slot);
          #ifdef DEBUG
          Serial.println("Card deleted!");
          #endif
          blink_n_times(1,STATUS_LED,2000);
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
      #ifdef DEBUG
        if(check_card(uid)) blink_n_times(i+1,STATUS_LED,250); 
      #endif
    }else{
      #ifdef DEBUG
      Serial.println("Access Denied");
      #endif
    }
    access_granted = 0; //Reset Access
  }
  if(!digitalRead(LOCK_UNLOCK)){// Unlock or lock door
    state = lock(!state);
  }
  if(!digitalRead(RESET_MEMORY)){// Swipe memory
    digitalWrite(STATUS_LED,HIGH);
    delay(5000);
    digitalWrite(STATUS_LED,LOW);
    if(!digitalRead(RESET_MEMORY)){
      clear_memory();
      blink_n_times(4,STATUS_LED,100);
      delay(1000);
    }
  }
}

int check_card(uint8_t *uid){
  /* 
   *  Checks if card is saved, returns the address of the card if it saved
   *  or -1 if it is not found.
   */
  int i,j;
  int memory_offset;

  for(i=0;i<TOTAL_SLOTS;i++){
    memory_offset = i*SLOT_SIZE;
    for(j=0;j<SLOT_SIZE;j++){
      if(EEPROM.read(memory_offset+j)!=uid[j]) break;
    }
    if(j==SLOT_SIZE){
      return i;
    }
  }
  return -1;
}
int save_card(uint8_t *uid){
  /* 
   *  Saves card to empty slot
   */
  uint8_t blank_card[] = {0,0,0,0};
  int card_addr = check_card(uid);// Search card
  int i, new_addr, memory_offset;
  if(card_addr == -1){
    new_addr = check_card(blank_card);// Search for a empty slot
    if(new_addr != -1){
      memory_offset = SLOT_SIZE*new_addr;
      for(i=0;i<SLOT_SIZE;i++){
        EEPROM.write(memory_offset+i,uid[i]);
      }
    }
    return new_addr;
  }
  return -1;// Already saved

}
int delete_card(int addr){
  /* 
   *  Deletes card from memory slot (addr)
   */
  int memory_offset = SLOT_SIZE*addr;
  int i;

  for(i=0;i<SLOT_SIZE;i++){
    EEPROM.write(memory_offset+i,0);
  }
}
void clear_memory(void){
  int i;
  int total = TOTAL_SLOTS*SLOT_SIZE;
  for(i=0;i<total;i++){
    EEPROM.write(i,0);
  }
}
void blink_n_times(int n,int led_pin,int blink_interval){
  /*
   * Blink led_pin n times for blink_interval
   */
  int i;
  for(i=0;i<n;i++){
    digitalWrite(led_pin,HIGH);
    delay(blink_interval);
    digitalWrite(led_pin,LOW);
    delay(blink_interval);
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
