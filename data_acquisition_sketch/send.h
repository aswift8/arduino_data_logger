#ifndef __SEND_H__
#define __SEND_H__

#include <stdarg.h>   // for variadic functions
#include "data.h"     // for struct Data


// Send error string then hang
#define ERROR             { SendLine("---ERROR---"); Serial.write(BYTE_ERROR); while (1) ; }

// Communication format
#define BYTE_MESSAGE      byte(0)
#define BYTE_DATA_START   byte(1)
#define BYTE_DATA_ELEMENT byte(2)
#define BYTE_DATA_END     byte(3)
#define BYTE_HEARTBEAT    byte(250)
#define BYTE_ERROR        byte(255)


template<typename T>
void _VSend(T t) {
  Serial.print(t);
}
template<typename T, typename... Rest>
void _VSend(T t, Rest... rest) {
  _VSend(t);
  _VSend(rest...);
}

// Send a message (variadic)
template<typename... Args>
void Send(Args... args) {
  Serial.write(BYTE_MESSAGE);
  _VSend(args...);
  Serial.write(0);
}
// Send a message and append newline (variadic)
template<typename... Args>
void SendLine(Args... args) {
  Serial.write(BYTE_MESSAGE);
  _VSend(args...);
  Serial.write("\n");
  Serial.write(0);
}

// Send a message containing a hexadecimal error code
void SendErrorCode(uint8_t error_code) {
  Serial.write(BYTE_MESSAGE);
  Serial.print("  Error: 0x");
  Serial.print(error_code, HEX);
  Serial.write('\n');
  Serial.write(0);
}

// Send BYTE_DATA_START
void SendDataStart() {
  Serial.write(BYTE_DATA_START);
}
// Send a single data element
void SendData(const struct Data &dat) {
  Serial.write(BYTE_DATA_ELEMENT);
  Serial.write((uint8_t*)&dat, sizeof(struct Data));
}
// Send BYTE_DATA_END
void SendDataEnd() {
  Serial.write(BYTE_DATA_END);
}

void SendHeartbeat() {
  Serial.write(BYTE_HEARTBEAT);
}

#endif