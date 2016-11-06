// Code to program an Arduino Uno connected to an NFC Reader(MFRC-522)using SPI Communication and an Electromagnetic Lock
// through a Relay board. 

#include <RFID.h>
#include <EEPROM.h>
#include <SPI.h>

const int tagSize = 7;    //size of the tags 
const int rst = 6;        // A pin connected to the Reset pin of the arduino. This was done in response to an error.
const int lock = 2;       // Pin connected to a relay used to switch the electromagnetic lock. 
const int ledgr = 3;      // A green LED near the NFC Reader  
const int ledred = 4;     //A red LED near the NFC Reader
const int ir = 7;         // An IR sensor placed on the door handle on the inside used to unlock the door when someone is going outside.

// A data structure that holds the id and tag data 

struct NFCDATA {
  int id;
  byte data[tagSize];
};

RFID rfid(10,5);    // rfid variable with SS/SDA pin at DIO 10 and RST pin at DIO 5

NFCDATA tag; //global data object of NFCDATA type

int rid = 0;              // id to be removed
int address = 0;         //stores the tag addresses value
byte serNum[tagSize];     //stores the read rfid data
byte master[tagSize] = {0x4A,0x09F,0xE1,0x0D3,0x5F,0x00,0x00}; // master card data(do not store in the eeprom someone might read from it) 

void setup(){  
  Serial.begin(9600); // initialize serial communication
  SPI.begin(); // initialize SPI communication for RFID
  pinMode(rst, INPUT);
  pinMode(lock,OUTPUT);
  pinMode(ir, INPUT);
  pinMode(ledgr,OUTPUT);
  pinMode(ledred,OUTPUT); 
  address = getcurAddress();
  rfid.init(); // initialize the RFID  
  Serial.println("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  Serial.println("+                    FREA-K (version 3.2)                   +");
  Serial.println("+           Please choose from the following:               +");
  Serial.println("+           - Scan your card to open the lock               +");
  Serial.println("+           - Use Master Card for advanced options          +");
  Serial.println("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

}
//This was the fix for the error I mentioned earlier
void reset()
{
    if(millis()>=3600000)
  {
      Serial.println("Resetting..");
      delay(1000);
      digitalWrite(rst, HIGH);  
      while(1);    
  }
}
 
void loop(){
  sreadTag();
  if(compareTag(master,tag.data)) {
    Serial.println("Add a tag or swipe once more to remove a tag");
    delay(1000);
    sreadTag();
    if(compareTag(master,tag.data)) {
      Serial.println("Remove a Tag");
      rid = inputID();
      removeTag(rid);
    }
    else {
      addTag();
    }
  }
  else if(checkTag(tag) == 0 || (digitalRead(ir) == HIGH)) {
    Serial.println("Access Granted");
    digitalWrite(lock,HIGH);
    digitalWrite(ledgr,HIGH);
    delay(3000);
    digitalWrite(lock,LOW);
    digitalWrite(ledgr,LOW);    
  }
  else if(checkTag(tag)) {
    Serial.println("Access Denied");
    digitalWrite(ledred,HIGH);
    delay(1000);
    digitalWrite(ledred,LOW);
  }
  reset();
}

void printalltags() {
  NFCDATA printtag;
  int addr = 0;
  while(addr < address) {
    EEPROM.get(addr, printtag);
    Serial.print("Id no. = ");
    Serial.println(printtag.id);
    Serial.print("Tag Data : ");
    for(int i=0;i<tagSize;i++) {
      Serial.print(printtag.data[i] , HEX);
      Serial.print(" ");
    }
    Serial.println("");
    Serial.println("");
    addr = addr + sizeof(printtag);
  }
}

/* will read the tags from the rfid sensor and store it into the variable 'tag.data'
 *  it will create a loop of itself until a card is read i.e. when this function 
 *  is called a card must be kept on the sensor(mandatory).
 *  it gives us advantage over the previous method because u dont have to loop the 
 *  whole of 'loop' function over and over again.
 */
void sreadTag() {
  Serial.println("Place the tag to read");
start:
  if(rfid.isCard()) {
    rfid.readCardSerial();
    for(int i=0;i<tagSize;i++) {
      tag.data[i] = rfid.serNum[i]; 
    }
  Serial.println("Card Read");
  }
  else goto start;
}


/* compares the data of two tags that are passed to the function returns 1
 *  if the tags match otherwise returns 0
 */
boolean compareTag(byte arr1[],byte arr2[]) {
  int m,n,flag;
  flag = 0;
  m = sizeof(arr1)/sizeof(byte);
  n = sizeof(arr2)/sizeof(byte);
  if(m==n) {
    for(int i=0;i<tagSize;i++) {
      if(arr1[i] != arr2[i]) {
        flag = 1;
        return false;
        break;
      }
    }
    if(flag == 0) return true;
  }
  else return false;
}

/* adds a tag to the EEPROM after reading a tag with id
 *  does not add the tag data if the tag data matches with 
 *  the previously 
 */
void addTag() {
  int id = 0,check;
  if(checkTag(tag)) {
    startid:
    id = inputID();
    check = checkID(id);
    if(!(check > 0)) {
      tag.id = id;
    }
    else {
      Serial.print("ID already exists enter new ID");
      goto startid;
    }
    EEPROM.put(address, tag);
    address = address + sizeof(tag);
    EEPROM.write(address,0xFF);
    Serial.println("Tag data written");
  }
  else Serial.println("Tag already exists");
}

/*checks if the passed tag is present in the directory or not. Returns 1 if not previously added
 * else reurns 0 if tag already exists this is the case of you want to add a tag. otherwise you can 
 * use this function to check for authorization to the lab
 */
boolean checkTag(NFCDATA tag) {
  int checkAddr = 0;
  NFCDATA temptag;
  while(checkAddr < address) {
    EEPROM.get(checkAddr,temptag);
    if(compareTag(temptag.data,tag.data)) {
      return false;
      break;
    }
    checkAddr = checkAddr + sizeof(temptag);
  }
  return true;
}


/*removes a tag specified by id and leaves the memory location empty
 * the id no. of the empty id is stored so that if next time of someone tries to 
 * add a tag the empty location gets filled up first ( you have to write the code for that in the loop function)
 */
void removeTag(int id) {
  int lastAddr = getcurAddress();
  int addr = 0;
  NFCDATA lastTag,empty;
  lastAddr = lastAddr - sizeof(lastTag);
  addr = checkID(id);
  EEPROM.get(lastAddr,lastTag);
  if(lastTag.id == id) {
    EEPROM.put(addr,empty); 
  }
  else {
    EEPROM.put(addr,lastTag);
    EEPROM.put(lastAddr,empty);
  }
}


int inputID() {
  int id;
  Serial.println("Enter the ID : ");
  Serial.flush();
  start:
  if(Serial.available()) {
    id = Serial.parseInt();
    if(id > 0) return id;
    else goto start;
  }
  else goto start;
}

int getcurAddress() {
  int addr;
  byte value;
  for(addr = 0;addr < EEPROM.length();addr++) {
    value = EEPROM.read(addr);
    if(value == 0xFF) {
      return addr;
      break;
    }
  }
  return 0;
}

int checkID(int id) {
  int checkAddr = 0;
  NFCDATA temptag;
  while(checkAddr < address) {
    EEPROM.get(checkAddr,temptag);
    if(temptag.id == id) {
      return checkAddr;
      break;
    }
    checkAddr = checkAddr + sizeof(temptag);
  }
  Serial.println("ID not found");
}



