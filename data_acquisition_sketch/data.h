#ifndef __DATA_H__
#define __DATA_H__

/*

Conversion to SI units

Data        SI      Conversion Formula                  Notes
time        s       micros  * 1e-6                      time since microcontroller started
analog      V       analog  * 5 / 1023                  10-bit unsigned integer mapped to [0,5]V
lin_acc_v   m/s²    lin_acc_v * 2 * 9.81 / 2^15         16-bit signed integer mapped to ±2g, converted to m/s²
rot_vel_v   rad/s   rot_vel_v * 250 * pi / 180 / 2^15   16-bit signed integer mapped to ±250°/s, converted to rad/s
btn_0       -       -                                   1 pressed, 0 released
btn_1       -       -                                   1 pressed, 0 released

*/

struct Data {
  uint32_t micros;
  uint16_t analog;
  uint8_t btn_0, btn_1;
  int16_t lin_acc_x, lin_acc_y, lin_acc_z;
  int16_t rot_vel_x, rot_vel_y, rot_vel_z;
};

#endif