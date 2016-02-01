Author:   Alfredo Rius
License:  BSD(LICENSE)

    This is an implementation of the NFC breakout for the PN532 from Adafruit
    and the Lockitron platform to allow access to doors.

    This program is based on the readMifare.pde from Adafruit. It takes the
    first 4 bytes of a Mifare card uid and compares it to flash memory. Since
    it is saved to the flash, it will remember all cards even if the arduino
    is disconnected.

    This program is based on the arduino-lockitron-nfc shield located (Eagle
    files in https://github.com/devalfrz/arduino-lockitron-nfc).

    Buttons: (buttons are set to have a pullup resistor so they ar inverted)
      - LOCK_UNLOCK: This button will lock or unlock the system depending on its
          previous state.
          
      - SAVE_CARD: If it's pressed for 3 seconds while a card is read,
          it will either save the current card and blink 3 times or delete the
          card and blink one time for 2 seconds.
          
      - RESET_MEMORY: Keep this button pressed and this will clear all cards in
          memory.
          
