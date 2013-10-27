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
  // Endlessly write increasing values to address zero, and read them back
  uint8_t data;
  while(1) {  
    data = readByte(0);
    Serial.print("I read: ");
    Serial.println(data,HEX);
    data++;
    writeByte(0,data);
    Serial.print("I just tried to write ");
    Serial.println(data,HEX);
    delay(1000);
  }
}


