/*

Arduino data logger

A sketch to record timestamped sensor data from an analog input, 2 push-buttons, and an IMU,
 with the binary data stored in a SD card.

Interact with Arduino using various commands, enabling user to read live sensor data, record
 data to SD file, transfer data from SD through serial (slow), and delete files on SD.

Supports MPU6050 IMU on I2C pins, and SD on SPI pins.

Check sections [WIRING], [DATA], [COMMANDS] for more information.



[WIRING]

[    Nano    ]       [   Device  ]
PIN_ANALOG  A0 <---> analog sensor
PIN_BTN_0   D2 <---> button
PIN_BTN_1   D3 <---> button
PIN_SD_CS  D10 <---> SD CS           ┐
SPI MOSI   D11 <---> SD MOSI         │ SD card reader
SPI MISO   D12 <---> SD MISO         │ (SPI)
SPI SCK    D13 <---> SD SCK          ┘
I2C SDA     A4 <---> IMU SDA         ┐ IMU - MPU6050
I2C SCL     A5 <---> IMU SCL         ┘ (I2C)

Other microcontrollers may use different SPI & I2C pins
 - wire to these and change PIN_SD_CS accordingly
Buttons are configured with internal pullup resistor
 - wire buttons from pin to ground
 - data inverts signal so pushed is 1, open is 0
Ensure MPU6050 is on GY-521 module (or equivalent) to prevent damage
 - Nano is 5V logic, MPU6050 3.3V, GY-521 allows 3.3-5V
 - connect to 5V VCC



[DATA]

Messages from the microcontroller are prefixed with a single byte, informing the
 connected device what should follow.

[  Prefix Byte  ]   [         Data          ]   [             Description             ]
     BYTE_MESSAGE   zero-terminated string      ASCII c-style string
  BYTE_DATA_START              ---              Indicates start of data stream
BYTE_DATA_ELEMENT   sizeof(struct Data) bytes   Single element of data
    BYTE_DATA_END              ---              Indicates end of data stream
       BYTE_ERROR              ---              Error occurred, device restart required

Specifics of data struct, including conversion to SI units, are in data.h file.
Prefix bytes are defined in send.h file.



[COMMANDS]

Commands to the microcontroller are a newline-ended  string. Some command strings
 include a filename string before the newline character, separated by a space.

[   Command    ]   [                         Description                         ]
  START            Start writing sensor data to SD file (higher frequency)
   TEST            Start sending live sensor data through serial (lower frequency)
   STOP            End either of the above states
   LIST            Send message listing SD file names
   READ filename   Send all data from SD filename through serial
    DEL filename   Delete filename from SD
DEL_ALL            Delete all files and directories on SD

Note:
  START and TEST will continue until a new command is received.
  TEST and READ send BYTE_DATA_START when starting, and BYTE_DATA_END when
   finished. This can be used for opening & closing files on the receiving end.
  READ cannot be interrupted once started. The device will be unresponsive until
   BYTE_DATA_END has been sent.
  READ may take a substantial amount of time to finish. Consider moving SD card to
   receiving device and manually transferring files.

*/


#include "send.h"
#include "data.h"
#include "fast_mpu6050.h" // for IMU
#include <SdFat.h>        // for SD cards


// CS pin on SD reader
#define PIN_SD_CS     10
// SPI driver frequency (MHz)
#define SD_CLOCK_MHZ  50

// Sensor pins
#define PIN_ANALOG    A0
#define PIN_BTN_0     2
#define PIN_BTN_1     3


// Supports FAT16/32 (SD cards up to 32GB)
typedef SdFat32 Sd_;
typedef FatFile SdFile_;


Sd_ sd;
SdFile_ current_file;
char file_name_buf[13] = {0};   // Convenience buffer for getting SD card filenames (DOS 8.3 format, up to 12 characters)


/***********************
        COMMANDS
 ***********************/

enum Command {
  E_CMD_INVALID = -1,
  E_CMD_STOP,             // Stop current task and wait for new command
  E_CMD_DATA_GATHER,      // Read sensors and write to file on SD card
  E_CMD_DATA_SEND_LIVE,   // Read sensors and send through serial
  E_CMD_LIST_FILES,       // List all files on SD card
  E_CMD_DATA_READ,        // Read data from file on SD card and send through serial
  E_CMD_DELETE_FILE,      // Delete file on SD card
  E_CMD_DELETE_ALL,       // Delete files and directories on SD card
};

int StrToCommand(const String &str) {
  if      (str.equals("STOP"))      return E_CMD_STOP;
  else if (str.equals("START"))     return E_CMD_DATA_GATHER;
  else if (str.equals("TEST"))      return E_CMD_DATA_SEND_LIVE;
  else if (str.equals("LIST"))      return E_CMD_LIST_FILES;
  else if (str.startsWith("READ ")) return E_CMD_DATA_READ;
  else if (str.startsWith("DEL "))  return E_CMD_DELETE_FILE;
  else if (str.equals("DEL_ALL"))   return E_CMD_DELETE_ALL;
  else                              return E_CMD_INVALID;
}

void ListCommands() {
  SendLine(F("Available commands:"));
  SendLine(F("  STOP           -  stop current task"                ));
  SendLine(F("  START          -  write sensor data to a new file"  ));
  SendLine(F("  TEST           -  send live sensor readings"        ));
  SendLine(F("  LIST           -  list available files"             ));
  SendLine(F("  READ filename  -  send data from specified file"    ));
  SendLine(F("  DEL filename   -  delete specified file"            ));
  SendLine(F("  DEL_ALL        -  delete all files and directories" ));
}


/***********************
      PROGRAM STATE
 ***********************/

enum State {
  E_STATE_WAITING,
  E_STATE_GATHERING,
  E_STATE_SENDING_LIVE,
};

enum State current_state = E_STATE_WAITING;


/***********************
        IO DATA
 ***********************/

struct Data current_data;


/***********************
          SETUP
 ***********************/

void setup() {
  // Connect
  Serial.begin(1000000);
  while (!Serial) ;

  SendLine(F("Initializing ADC"));
  // ADC setup
  // Set prescaler factor to 8 to speed up ADC conversion (http://microelex.blogspot.com/p/2.html)
  ADCSRA = (ADCSRA & 0b11111000) | 0b00000011;

  SendLine(F("Initializing button inputs"));
  // Pin setup
  pinMode(PIN_BTN_0, INPUT_PULLUP);
  pinMode(PIN_BTN_1, INPUT_PULLUP);

  SendLine(F("Initializing SD reader"));
  // SD card setup
  if (!sd.begin(PIN_SD_CS, SD_SCK_MHZ(SD_CLOCK_MHZ))) {
    SendLine(F("Failed to initialize SD"));
    ERROR
  }

  SendLine(F("Initializing IMU"));
  SetupMPU6050();

  SendLine(F("Done!"));

  ListCommands();

  StartState(current_state);
}

void loop() {
  HandleCommand();
  HandleState();
}


/***********************
        COMMANDS
 ***********************/

// Match serial input to command, then executes command.
void HandleCommand() {
  if (!Serial.available()) return;

  String str = Serial.readStringUntil('\n');
  SendLine(F("> "), str);     // Make it clear what the received command is
  enum Command cmd = (enum Command)StrToCommand(str);
  switch (cmd) {
  case E_CMD_INVALID:
    StopState();
    SendLine(F("Invalid command string: '"), str, F("'"));
    ListCommands();
    StartState(E_STATE_WAITING);
    break;
  case E_CMD_STOP:
    StopState();
    StartState(E_STATE_WAITING);
    break;
  case E_CMD_DATA_GATHER:
    StopState();
    StartState(E_STATE_GATHERING);
    break;
  case E_CMD_DATA_READ:
    StopState();
    str.remove(0, 5);   // remove prefix "READ "
    str.trim();
    TryReadData(str);
    StartState(E_STATE_WAITING);
    break;
  case E_CMD_DATA_SEND_LIVE:
    StopState();
    StartState(E_STATE_SENDING_LIVE);
    break;
  case E_CMD_LIST_FILES:
    StopState();
    ListFiles();
    StartState(E_STATE_WAITING);
    break;
  case E_CMD_DELETE_FILE:
    StopState();
    str.remove(0, 4);   // remove prefix "DEL "
    str.trim();
    TryDeleteFile(str);
    StartState(E_STATE_WAITING);
    break;
  case E_CMD_DELETE_ALL:
    StopState();
    DeleteAll();
    StartState(E_STATE_WAITING);
    break;
  default:
    SendLine(F("Unimplemented command: '"), str, F("'"));
    ERROR
  }
}


/***********************
      STATE MACHINE
 ***********************/

// Perform appropriate action depending on current state
void HandleState() {
  switch (current_state) {
  case E_STATE_WAITING:
    break;
  case E_STATE_GATHERING:
    GetData(&current_data);
    StoreData(current_data, current_file);
    break;
  case E_STATE_SENDING_LIVE:
    GetData(&current_data);
    SendData(current_data);
    break;
  }
}

// Called once when ending current state
void StopState() {
  switch (current_state) {
  case E_STATE_WAITING:
    break;
  case E_STATE_GATHERING:
    current_file.getName(file_name_buf, sizeof(file_name_buf));
    current_file.close();
    SendLine(F("Closed '"), file_name_buf, F("'"));
    break;
  case E_STATE_SENDING_LIVE:
    SendDataEnd();
    break;
  }
}

// Called once when starting new state
void StartState(int new_state) {
  current_state = (enum State)new_state;
  switch (current_state) {
  case E_STATE_WAITING:
    break;
  case E_STATE_GATHERING:
    OpenNewFile();
    break;
  case E_STATE_SENDING_LIVE:
    SendDataStart();
    break;
  }
}


/***********************
      READ SENSORS
 ***********************/

void GetData(struct Data *dat) {
  dat->micros = micros();
  dat->analog = analogRead(PIN_ANALOG);
  dat->btn_0 = 1 - digitalRead(PIN_BTN_0);  // Internal pullup resistors, so open button reads high - invert this
  dat->btn_1 = 1 - digitalRead(PIN_BTN_1);  // Internal pullup resistors, so open button reads high - invert this
  FillDataMPU6050(dat);
}


/***********************
       SD CARD IO
 ***********************/

// Store readings to file
void StoreData(const struct Data &dat, SdFile_ &file) {
  if (!file.write((uint8_t*)&dat, sizeof(struct Data))) {
    SendLine(F("Failed to store data to SD card"));
    SendErrorCode(file.getWriteError());
    ERROR
  }
}

// Send names of all available files through serial
void ListFiles() {
  SdFile_ root = sd.open("/");
  SdFile_ f;
  SendLine(F("[name]  [size (bytes)]"));
  while (f.openNext(&root)) {
    f.getName(file_name_buf, sizeof(file_name_buf));
    if (f.isFile()) {
      SendLine(F("  "), file_name_buf, F("  "), f.fileSize());
    } else if (f.isDir()) {
      SendLine(F("  "), file_name_buf, F("/"));
    }
    f.close();
  }
  root.close();
}

// Open new file on SD
void OpenNewFile() {
  uint16_t i = 0;
  while (1) {
    snprintf(file_name_buf, sizeof(file_name_buf), "dat%u.dat", i);
    if (!sd.exists(file_name_buf)) {
      current_file = sd.open(file_name_buf, O_RDWR | O_CREAT);
      if (!sd.exists(file_name_buf)) {
        SendLine(F("Failed to create file '"), file_name_buf, F("'"));
        ERROR
      }
      SendLine(F("Created file '"), file_name_buf, F("'"));
      break;
    }
    i++;
  }
}

// Read and send all data from file
void TryReadData(const String &filename) {
  if (sd.exists(filename)) {
    SendDataStart();
    SdFile_ f = sd.open(filename);
    while (f.read((void*)&current_data, sizeof(struct Data)) > 0) {
      SendData(current_data);
    }
    f.close();
    SendDataEnd();
  } else {
    SendLine(F("File '"), filename, F("' not found"));
  }
}

// Delete file from SD
void TryDeleteFile(const String &filename) {
  if (sd.remove(filename)) {
    SendLine(F("Deleted file '"), filename, F("'"));
  } else {
    SendLine(F("Failed to delete file '"), filename, F("'"));
  }
}

// Delete all files and directories from SD
void DeleteAll() {
  SdFile_ root = sd.open("/");
  root.rewind();
  SdFile_ f;
  while (f.openNext(&root)) {
    f.getName(file_name_buf, sizeof(file_name_buf));
    Send(F("Trying to delete '"), file_name_buf, F("' ... "));

    if (f.isDir()) {
      f.rmdir();    // Will fail if directory contains files
    } else {
      f.close();
      root.remove(file_name_buf);
    }

    if (root.exists(file_name_buf)) {
      SendLine(F("Failed"));
      f.close();
    } else {
      SendLine(F("Succeeded"));
      root.rewind();
    }
  }
  SendLine(F("Done!"));
  root.close();
}
