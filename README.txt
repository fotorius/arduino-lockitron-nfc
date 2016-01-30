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
      - SLOT_SELECT: Selects a memory slot. STATUS_LED will blink n times depending
          on the slot that has been selected.
            
      - MEMORY_BUTTON: If it's pressed for 3 seconds while a card is read,
          it will either override the selected slot or delete the card if it
          was already registered.

      - LOCK_UNLOCK: This button will lock or unlock the system depending on its
          previous state.
          
