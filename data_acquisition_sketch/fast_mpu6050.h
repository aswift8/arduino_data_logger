#ifndef __FAST_MPU6050_H__
#define __FAST_MPU6050_H__

/*

Information on the MPU6050 registers is available here:
 https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf
Page numbers refer to the above document.

*******************************************
NOTE: sensor calibration is NOT implemented
*******************************************

Several libraries exist for interfacing with MPU6050 chip, but they obfuscate some important
 details. This code uses the Arduino Wire.h library to interface with the MPU6050 using I2C,
 prioritising high-frequency readings.

MPU6050 is a 3.3V chip, but often comes on a GY-521 breakout board which includes a level
 shifter, so this may not be an issue.

Accelerometer and gyro readings are 16-bit signed integers, the value mapping to ± Full Scale
 Range. This code uses the default ranges for both, ±2g and ±250°/s, respectively. These are
 the smallest ranges, but highest precision. The ranges can be increased by 2x, 4x, or 8x by
 writing to registers 0x1C (ACCEL_CONFIG - pg15) and 0x1B (GYRO_CONFIG - pg14).

Reading conversion formulae for default ranges are available in data.h, while formulae for
 other ranges can be derived from information in pages 29-30.

*/

#include <Wire.h>   // for I2C communication with IMU
#include "data.h"   // for struct Data

// I2C clock speed
#define I2C_CLOCK_HZ              1000000UL   // seems to be fastest I2C can run with Nano
// I2C address
#define MPU6050_ADDR              byte(0x68)  // default I2C address for MPU6050  (pg45)
// Register addresses
#define MPU6050_SIGNAL_PATH_RESET byte(0x68)  // signal path reset                (pg37)
#define MPU6050_PWR_MGMT_1        byte(0x6B)  // power management                 (pg40)
#define MPU6050_DATA_OUT          byte(0x3B)  // start of data registers          (pg29-31)

void SetupMPU6050(void) {
  // IMU setup
  Wire.begin(MPU6050_ADDR);
  Wire.setClock(I2C_CLOCK_HZ);
  // Perform reset
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_PWR_MGMT_1);
  Wire.write(0b10000000);                     // set DEVICE_RESET bit to 1
  Wire.endTransmission(false);
  // Wait for reset to complete (DEVICE_RESET bit changes to 0)
  bool has_reset = false;
  while (!has_reset) {
    Wire.write(MPU6050_PWR_MGMT_1);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_ADDR, (uint8_t)1);
    has_reset = !(Wire.read() & 0b10000000);
  }
  delay(100);
  // Reset signal path (probably unnecessary - MPU6050 doesn't have SPI interface)
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_SIGNAL_PATH_RESET);
  Wire.write(0b00000111);         // set reset bits for GYRO_RESET, ACCEL_RESET, TEMP_RESET
  Wire.endTransmission(false);
  delay(100);
  // Disable sleep mode, set internal 8MHz oscillator as clock source (less stable but higher frequency?)
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_PWR_MGMT_1);
  Wire.write(0b00000000);
  Wire.endTransmission(true);
}

void FillDataMPU6050(struct Data *dat) {
  // Read sensor data
  //  Each sensor axis outputs 16-bit signed integer corresponding to value over full scale range
  //  As sensor registers are located adjacent to each other, it is fastest to request and read all bytes
  //  Accelerometer x-, y-, z-axes, temperature sensor, gyro x-, y-, z-axes
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_DATA_OUT);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, (uint8_t)14);        // request total of 14 bytes
  dat->lin_acc_x = (Wire.read() << 8) | Wire.read();  // accelerometer x-axis high & low bytes
  dat->lin_acc_y = (Wire.read() << 8) | Wire.read();  // accelerometer y-axis high & low bytes
  dat->lin_acc_z = (Wire.read() << 8) | Wire.read();  // accelerometer z-axis high & low bytes
  Wire.read(); Wire.read();                           // temperature high & low bytes (ignored)
  dat->rot_vel_x = (Wire.read() << 8) | Wire.read();  // gyro x-axis high & low bytes
  dat->rot_vel_y = (Wire.read() << 8) | Wire.read();  // gyro y-axis high & low bytes
  dat->rot_vel_z = (Wire.read() << 8) | Wire.read();  // gyro z-axis high & low bytes
}

#endif