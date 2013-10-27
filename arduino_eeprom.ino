#include <Wire.h>

// Implementation details:
// Target EEPROM device is Atmel AT28C64B (and presumably compatible chips).
// The Arduino accesses the EEPROM's 8 I/O lines using a Microchip MCP2008 I2C port expander
// The MCP2008's 3 bit address is set to 0
// The Arduino accesses the EEPROM's 12 address lines using a Microchip MCP2016 I2C port expander
// the MCP2016's 3 bit address is set to 1
// The Arduino manipulates the EEPROM's read/write enable pins using two of its digital IO pins

#define OE_PIN 2
#define WE_PIN 3

void setData(uint8_t data) {
  Wire.beginTransmission(0x20);
  Wire.write(0x09);
  Wire.write(data);
  Wire.endTransmission();
}

void setAddress(uint16_t address) {  
  Wire.beginTransmission(0x21);
  Wire.write(0x00);
  Wire.write((uint8_t) address & 255 );
  Wire.endTransmission();      
  Wire.beginTransmission(0x21);
  Wire.write(0x01);
  Wire.write((uint8_t) (address>>8) & 255 );
  Wire.endTransmission(); 
}

void set8BitBusToOutput() {
  Wire.beginTransmission(0x20);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
}

void set8BitBusToInput() {
  Wire.beginTransmission(0x20);
  Wire.write(0x00);
  Wire.write(0xFF);
  Wire.endTransmission();  
}  

uint8_t readByte(uint16_t address) {
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
  set8BitBusToOutput();
  setAddress(address);
  setData(data);
  digitalWrite(WE_PIN,LOW);
  delay(10);
  digitalWrite(WE_PIN,HIGH);
  delay(10);
}

void setup() {
  Serial.begin(57600);
  
  // Need two IO pins for output enable (OE) and write enable (WE)
  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);
  // Pull OE, WE high
  digitalWrite(OE_PIN,HIGH);
  digitalWrite(WE_PIN,HIGH);
  
  Wire.begin();  
  // Set all pins on 16bit bus to output mode
  Wire.beginTransmission(0x21);
  Wire.write(0x06);
  Wire.write(0x00);
  Wire.endTransmission();      
  Wire.beginTransmission(0x21);
  Wire.write(0x07);
  Wire.write(0x00);
  Wire.endTransmission();
}

void loop() {
  // Endlessly handle PEEK and POKE commands
  uint16_t address, max_address;
  uint8_t data;
  char input[13];
  char tmp[2];
  char *command;
  char *address_str;
  byte pos = 0;
  while(1) {
    // Read from Serial until we hit a newline or 12 characters
    while(pos <= 12) {
      while(!Serial.available()) delay(10);
      input[pos] = Serial.read();
      if(input[pos] == '\n') break;
      pos++;
    }
    // First word of input is the command
    command = strtok(input," ");
    if(!strcmp(command,"PEEK")) {
      // Handle PEEK
      address_str = strtok(NULL, "\n");
      address = strtoul(address_str, NULL, 16);
      if(address >= 0xFFF) {
        Serial.println("MAXIMUM ADDRESS IS 0xFFE.");
      } else {      
        data = readByte(address);
        sprintf(tmp, "%02X", data);
        Serial.println(tmp);
      }
    } else if(!strcmp(command,"POKE")) {
      // Handle POKE
      address_str = strtok(NULL, " ");
      address = strtoul(address_str, NULL, 16); 
      data = (uint8_t) strtoul(strtok(NULL, "\n"), NULL, 16);
      if(address >= 0xFFF) {
        Serial.println("MAXIMUM ADDRESS IS 0xFFE.");
      } else {      
        writeByte(address,data);
        data = readByte(address);
        sprintf(tmp, "%02X", data);
        Serial.println(tmp);
      }            
    } else if(!strcmp(command,"DUMP")) {
      // Handle DUMP
      address_str = strtok(NULL, " ");
      max_address = strtoul(address_str, NULL, 16); 
      if(max_address >= 0xFFF) {
        Serial.println("MAXIMUM ADDRESS IS 0xFFE.");
      } else {
        for(address=0; address<=max_address; address++) {
          if(address % 16 == 0) {
            sprintf(tmp, "%03X", address);
            Serial.print(tmp);
            Serial.print("\t");
          }
          data = readByte(address);
          sprintf(tmp, "%02X", data);
          if(address % 16 == 15) {
            Serial.println(tmp);
          } else {
            Serial.print(tmp);
            Serial.print(" ");
          }
        }
      }
    } else {
      // Handle anything else
      Serial.println("UNKNOWN COMMAND.  USE PEEK, POKE OR DUMP.");
    }
    // Scrub input buffer
    memset(input, 0, 13);
    pos = 0;    
  }
}


