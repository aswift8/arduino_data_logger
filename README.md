# Data Logger

An Arduino sketch with accompanying Python scripts written for high frequency (kHz) data logging.

Details for sensors and wiring are in Arduino sketch.


### Prerequisite Libraries

Arduino
- [`SdFat`](https://github.com/greiman/SdFat)

Python
- [`PySerial`](https://pyserial.readthedocs.io/en/latest/pyserial.html)
- [`numpy`](https://numpy.org/)
- [`matplotlib`](https://matplotlib.org/) (used in `data_plot.py` demo)


### User Instructions

Upload `data_acquisition_sketch.ino` to Arduino using Arduino IDE.

Run `python monitor.py` to start serial monitor gui.

Use `[Connect]` to connect to Arduino, `[Refresh]` to refresh dropdown of connected devices.

Use `[Start]` and `[Stop]` to begin and end sensor data recording to SD.

Use `[Read]` to transfer binary data from SD (slow), or manually move the SD card and transfer files (irritating).

Use `[Test]` to output realtime sensor readings to gui console, useful for ensuring sensors are working as expected.

Use functions in `data_util.py` to convert binary data to other forms (csv file, numpy ndarray).

Run `python data_plot.py` to display data from binary or csv file using matplotlib. 
