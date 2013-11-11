#include <Wire.h>

/********************************************************************************
 * Arduino-based serial EEPROM programmer
 * (C) 2013 Luke Maurits <luke@maurits.id.au>, 3-clause BSD license
 *
 * Implementation details:
 * Target EEPROM device is Atmel AT28C64B (and presumably compatible chips).
 * The Arduino accesses the EEPROM's 8 I/O lines using a Microchip MCP23008 I2C
 * port expander, whose 3 bit address is set to 000.
 * The Arduino accesses the EEPROM's 12 address lines using a Microchip MCP23016
 * I2C port expander, whose 3 bit address is set to 001.
 * The Arduino manipulates the EEPROM's read/write enable pins using two of its
 * digital IO pins, as defined by OE_PIN (read) and WE_PIN (write).
 ********************************************************************************/
 
// Protocol command bytes
#define READ_BYTE_COMMAND 0x00
#define READ_BYTES_COMMAND 0xFF
#define WRITE_BYTE_COMMAND 0x0F
#define WRITE_BYTES_COMMAND 0xF0
#define CLEAR_CHIP_COMMAND 0x33
#define ERROR_INDICATOR 0xCC

// Read/write enable pins
#define OE_PIN 14
#define WE_PIN 15

// Our model of the address pins
uint8_t model_address_low, model_address_high;

void set8BitBusToOutput() {
  /* Configure MCP23008 for output */
  Wire.beginTransmission(0x20);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
}

void set8BitBusToInput() {
  /* Configure MCP23008 for input */
  Wire.beginTransmission(0x20);
  Wire.write(0x00);
  Wire.write(0xFF);
  Wire.endTransmission();  
}  

void setData(uint8_t data) {
  /* Put data byte on MCP23008's output pins */
  Wire.beginTransmission(0x20);
  Wire.write(0x09);
  Wire.write(data);
  Wire.endTransmission();
}

void setAddress(uint16_t address) {
  /* Put address on MCP23016's output pins */
  uint8_t address_lo, address_hi;
  address_lo = (uint8_t) (address & 0xFF);
  address_hi = (uint8_t) (address>>8);
  if(address_lo != model_address_low) {
    Wire.beginTransmission(0x21);
    Wire.write(0x00);
    Wire.write(address_lo);
    Wire.endTransmission();      
    model_address_low = address_lo;
  }
  if(address_hi != model_address_high) {
    Wire.beginTransmission(0x21);
    Wire.write(0x01);
    Wire.write(address_hi);
    Wire.endTransmission();
    model_address_high = address_hi;
  }
}

void setAddressLowOrder(uint8_t address_lo) {
  Wire.beginTransmission(0x21);
  Wire.write(0x00);
  Wire.write(address_lo);
  Wire.endTransmission();
  model_address_low = address_lo;
} 

void setAddressHighOrder(uint8_t address_hi) {
  Wire.beginTransmission(0x21);
  Wire.write(0x01);
  Wire.write(address_hi);
  Wire.endTransmission();
  model_address_high = address_hi;
}

uint8_t readByte(uint16_t address) {
  /* Read and return byte from specified address */
  uint8_t data;
  set8BitBusToInput();
  setAddress(address);
  digitalWrite(OE_PIN, LOW);
  Wire.beginTransmission(0x20);
  Wire.write(0x09);
  Wire.endTransmission();
  Wire.requestFrom(0x20, 1);
  data = Wire.read();
  digitalWrite(OE_PIN, HIGH);
  return data;
}

void writeByte(uint16_t address, uint8_t data) {
  /* Write specified data byte to specified address */
  set8BitBusToOutput();
  setAddress(address);
  setData(data);
  digitalWrite(WE_PIN,LOW);
  digitalWrite(WE_PIN,HIGH);
  // Wait a bit for the write cycle to complete.
  // Any lower value than 5 here causes missed writes!
  delay(5);
}

void setup() {
  // Start listening for serial input
  Serial.begin(57600);
  
  // Need two IO pins for output enable (OE) and write enable (WE)
  pinMode(OE_PIN,OUTPUT);
  pinMode(WE_PIN,OUTPUT);
  
  // Pull OE, WE high (disable reading and writing)
  digitalWrite(OE_PIN,HIGH);
  digitalWrite(WE_PIN,HIGH);

  // Set all pins on MCP23016 output mode (it's only used for addressing)
  Wire.begin(); 
  Wire.beginTransmission(0x21);
  Wire.write(0x06);
  Wire.write(0x00);
  Wire.endTransmission();      
  Wire.beginTransmission(0x21);
  Wire.write(0x07);
  Wire.write(0x00);
  Wire.endTransmission();
  
  // Set address to zero and update our model
  setAddress(0);
  model_address_low = 0;
  model_address_high = 0;
}

void loop() {
  /* Endlessly handle binary protocol commands */
  
  uint16_t address;
  uint8_t a, b;
  uint8_t buffer[128];

  while(1) {
    // Read a command byte
    while(!Serial.available()) delay(7);
    a = Serial.read();
    switch(a) {
      case READ_BYTE_COMMAND:
        /* Read a single byte from the chip and send it over the serial channel.
         * Command format:
         * READ_BYTE_COMMAND, ADDRESS_LOW_ORDER, ADDRESS_HI_ORDER
         */         
        // Read address, low order byte then high order
        a = Serial.read();
        b = Serial.read();
        address = ((uint16_t) b) << 8 | a;
        // Read data from chip
        a = readByte(address);
        // Write data back to serial host
        Serial.write(a);
        break;
      case READ_BYTES_COMMAND:
        /* Read a variable number of bytes (up to 255) from the chip
         * and send them over the serial channel.
         * Command format:
         * READ_BYTES_COMMAND, START_ADDRESS_LOW_ORDER, START_ADDRESS_HI_ORDER,
         * BYTE_COUNT
         */
        // Read starting address, low order byte then high order
        a = Serial.read();
        b = Serial.read();
        address = ((uint16_t) b) << 8 | a;
        // Read byte count
        a = Serial.read();
        // Read count consecutive bytes       
        for(; a>0; a--) {
          b = readByte(address);
          Serial.write(b);
          address++;
        }
        break;
      case WRITE_BYTE_COMMAND:
        /* Read a single byte from the serial channel and store it in the chip
         * at the specified address.  Read the byte back, and send 0x00 over
         * the serial channel if it matches what was written, or send
         * ERROR_INDICATOR if it doesn't match.
         * Command format:
         * WRITE_BYTE_COMMAND, ADDRESS_LOW_ORDER, ADDRESS_HI_ORDER, DATA_BYTE
         */
        // Read address, low order byte then high order
        a = Serial.read();
        b = Serial.read();
        address = ((uint16_t) b) << 8 | a;
        // Read data to write
        a = Serial.read();
        // Write data to chip
        writeByte(address,a);
        // Verify
        b = readByte(address);
        if(a == b) {
          // Send success
          Serial.write((byte)0x0);
        } else {
          Serial.write(ERROR_INDICATOR);
        }
        break;
      case WRITE_BYTES_COMMAND:
        /* Read a variable (up to 255) number of bytes from the serial channel
         * and store them in the chip at consecutive addresses beginning at the
         * specified address.  Currently return 0x00 afterward to indicate
         * completion without checking anything (obviously subject to change)
         * Command format:
         * WRITE_BYTES_COMMAND, START_ADDRESS_LOW_ORDER, START_ADDRESS_HI_ORDER,
         * BYTE_COUNT, DATA_BYTE_001, DATA_BYTE_002, ... , FINAL_DATA_BYTE 
         */
        // Read starting address, low order byte then high order
        while(!Serial.available());
        a = Serial.read();
        while(!Serial.available());
        b = Serial.read();
        address = ((uint16_t) b) << 8 | a;
        // Read byte count
        while(!Serial.available());
        a = Serial.read();
        // Read and write a consecutive bytes
        for(; a>0; a--) {
          while(!Serial.available());
          b = Serial.read();
          writeByte(address, b);
          address++;
        }
        Serial.write((byte)0x00);
        break;
      case CLEAR_CHIP_COMMAND:
        /* Set all addresses to 0xFF and return 0x00 to indicate completion.
         * Command format:
         * CLEAR_CHIP_COMMAND 
         */
        for(address=0; address <= 0x1FFF; address++) {
          writeByte(address, 0xFF);
        }
        Serial.write((byte) 0x00);
        break;
      default:
        /* If sent any byte other than one of the specified protocol command bytes,
         * at a time when a command byte is expected, send the ERROR_INDICATOR byte
         * over the serial channel.
         */
        Serial.write(ERROR_INDICATOR);
    }
  }
}
