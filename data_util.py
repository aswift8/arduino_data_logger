from math import pi
import struct
import numpy as np
import os


csv_header = "timestamp,analog,btn_0,btn_1,acc_x,acc_y,acc_z,gyro_x,gyro_y,gyro_z"

# little-endian, u32 microseconds, u16 analog, 2 u8 buttons, 3 i16 accelerometer, 3 i16 gyro
data_length_bytes = 4 + 2 + 1 + 1 + 3*2 + 3*2
data_format       = "<LHBBhhhhhh"

# Constants for conversion to SI units
micros_to_s       = 1.0e-6                          # microseconds to seconds 
analog_to_voltage = 5.0 / 1023.0                    # 10-bit unsigned integer to voltage
acc_to_mps2       = 2.0 * 9.81 / 32768.0            # 16-bit signed integer to m/s²
gyro_to_radps     = 250.0 * pi / 180.0 / 32768.0    # 16-bit signed integer to rad/s
# For more information on data format, read Arduino sketch

"""
Convert data bytes to tuple of SI unit values
"""
def bytes_to_values(b):
    (micros, analog, b0, b1, ax, ay, az, gx, gy, gz) = struct.unpack(data_format, b)
    return (
        micros * micros_to_s,
        analog * analog_to_voltage,
        b0,
        b1,
        ax * acc_to_mps2,
        ay * acc_to_mps2,
        az * acc_to_mps2,
        gx * gyro_to_radps,
        gy * gyro_to_radps,
        gz * gyro_to_radps,
    )

"""
Convert tuple of values to csv row
"""
def values_to_csv(values):
    return ','.join([str(v) for v in values])

"""
Convert tuple of values to formatted string
"""
def values_to_str(values):
    return f"[{values[0]:8.4f}] a:{values[1]:4.2f}, b0:{values[2]:1.0f}, b1:{values[3]:1.0f}, acc:{values[4]:7.3f},{values[5]:7.3f},{values[6]:7.3f}, gyro:{values[7]:7.3f},{values[8]:7.3f},{values[9]:7.3f}"

"""
Convert filename.dat (binary) to filename.csv (text)
"""
def dat_to_csv(filename):
    filename_dat = f"{filename}.dat"
    filename_csv = f"{filename}.csv"
    with open(filename_dat, "rb") as dat:
        data = dat.read()
        data_element_count = int(len(data) / data_length_bytes)
        with open(filename_csv, "wt") as csv:
            csv.write(f"{csv_header}\n")
            for i in range(data_element_count):
                j1 = i * data_length_bytes
                j2 = j1 + data_length_bytes
                values = bytes_to_values(data[j1:j2])
                csv.write(f"{values_to_csv(values)}\n")
"""
Convert file.dat or file.csv to dictionary of numpy ndarrays
    Entry         Dimensions    Units
    "timestamp"   Nx1           s
    "analog"      Nx1           V
    "btn_0"       Nx1           0/1
    "btn_1"       Nx1           0/1
    "acc"         Nx3           m/s²
    "gyro"        Nx3           rad/s
"""
def read_data(filepath):
    data = {}
    name, extension = os.path.splitext(filepath)
    if extension == ".dat":
        with open(filepath, "rb") as f:
            data_bin = f.read()
            data_element_count = int(len(data_bin) / data_length_bytes)
            ts = np.ndarray((data_element_count,))
            vs = np.ndarray((data_element_count,))
            b0s = np.ndarray((data_element_count,))
            b1s = np.ndarray((data_element_count,))
            accs = np.ndarray((data_element_count,3))
            gyros = np.ndarray((data_element_count,3))
            for i in range(data_element_count):
                j1 = i * data_length_bytes
                j2 = j1 + data_length_bytes
                vals = bytes_to_values(data_bin[j1:j2])
                ts[i] = vals[0]
                vs[i] = vals[1]
                b0s[i] = vals[2]
                b1s[i] = vals[3]
                accs[i,:] = vals[4:7]
                gyros[i,:] = vals[7:10]
            data["timestamp"] = ts
            data["analog"] = vs
            data["btn_0"] = b0s
            data["btn_1"] = b1s
            data["acc"] = accs
            data["gyro"] = gyros
    elif extension == ".csv":
        with open(filepath, "rt") as f:
            f.readline()                    # ignore header
            lines = f.readlines()
            csv_data = np.loadtxt(lines, delimiter=',')
            data["timestamp"] = csv_data[:,0]
            data["analog"] = csv_data[:,1]
            data["btn_0"] = csv_data[:,2]
            data["btn_1"] = csv_data[:,3]
            data["acc"] = csv_data[:,4:7]
            data["gyro"] = csv_data[:,7:10]
    return data

