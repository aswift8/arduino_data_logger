# Serial
import serial
import serial.tools.list_ports
# Gui
import tkinter as tk
from tkinter import scrolledtext
import threading
import queue
# Data
import data_util
# File naming
import time
import pathlib
import os


byte_message      = 0
byte_data_start   = 1
byte_data_element = 2
byte_data_end     = 3
byte_heartbeat    = 250
byte_error        = 255

connection_status_warning_time = 2


"""
Write binary data to file.
Initialize -> set suffix -> start -> write -> end
                  ^          ^        ^_|      |
                  |__________|_________________|
"""
class DataStreamDatWriter:
    def __init__(self):
        self.f = None
        self.fname = ""
        self.suffix = ""
        self.t_start = -1
        self.t_end = -1
        self.count = 0
    
    def set_file_suffix(self, suffix):
        self.suffix = suffix
    
    """
    Open file "directory/{timestamp}{suffix}.dat".
    """
    def start(self, directory):
        timestr = time.strftime("%Y%m%d-%H%M%S")
        self.fname = f"{timestr}{self.suffix}.dat"
        fpath = os.path.join(directory, self.fname)
        os.makedirs(os.path.dirname(fpath), exist_ok=True)
        self.f = open(fpath, "wb")
        self.element_count = 0
    
    """
    Write a single data entry to file.
    """
    def write(self, b):
        if self.f:
            self.f.write(b)
            # Update timestamp tracker and entry counter
            t = data_util.bytes_to_values(b)[0]
            if self.t_start == -1:
                self.t_start = t
            self.t_end = t
            self.count += 1
    
    """
    Close the current file.
    """
    def end(self):
        if self.f:
            self.f.close()
            self.f = None
            # Calculate average frequency
            time_range = self.t_end - self.t_start
            f = self.count / time_range
            print(f"Closed file '{self.fname}', {self.count} entries in {time_range:.2f}s, averaging {f:.0f}Hz")
            self.fname = ""
            self.t_start = -1
            self.t_end = -1
            self.count = 0


"""
Threaded serial communication
"""
class SerialMonitor:
    """
    Monitor initializer.
    Callbacks for:
        cb_msg(msg_string)          Message received
        cb_data(data_bytes)         Data element received
        cb_data_start()             Data stream start received
        cb_data_end()               Data stream end received
        cb_disconnect(string)       Connection lost
        cb_error()                  Device error
    """
    def __init__(self, cb_msg, cb_data, cb_data_start, cb_data_end, cb_disconnect, cb_heartbeat, cb_error):
        self.connected = False
        self.do_run = False
        self.cb_msg = cb_msg
        self.cb_data = cb_data
        self.cb_data_start = cb_data_start
        self.cb_data_end = cb_data_end
        self.cb_disconnect = cb_disconnect
        self.cb_heartbeat = cb_heartbeat
        self.cb_error = cb_error
        self.send_queue = queue.Queue()

    # Communication thread function
    def _loop(self, port, baud):
        try:
            with serial.Serial(port, baud, timeout=1) as s:
                #s.flush()
                print("Connection established")
                self.connected = True
                while self.do_run:
                    # READ
                    # Handle any received data
                    if s.in_waiting:
                        # First byte indicates what follows
                        b = s.read(1)
                        if len(b) == 0:                 # Timeout occurred; ignore
                            pass
                        elif b[0] == byte_message:      # Null-terminated c-style string
                            # Read string bytes
                            msg = s.read_until(b'\x00')
                            # Discard null and decode to string
                            self.cb_msg(msg[:-1].decode("utf-8"))
                        elif b[0] == byte_data_start:   # Start of data stream
                            self.cb_data_start()
                        elif b[0] == byte_data_element: # Data stream element
                            # Read element bytes
                            dat = s.read(data_util.data_length_bytes)
                            self.cb_data(dat)
                        elif b[0] == byte_data_end:
                            # End of data stream
                            self.cb_data_end()
                        elif b[0] == byte_heartbeat:
                            # Heartbeat
                            self.cb_heartbeat()
                        elif b[0] == byte_error:
                            self.cb_error()
                            break
                        else:
                            print(f"Unexpected initial byte: {b[0]}; disconnecting")
                            self.cb_disconnect("Unexpected data")
                            break
                    
                    # WRITE
                    # Send any queued commands
                    while not self.send_queue.empty():
                        msg = self.send_queue.get()
                        s.write(msg)
        except serial.SerialException:
            self.cb_disconnect("SerialException")
        except serial.SerialTimeoutException:
            self.cb_disconnect("SerialTimeoutException")
        except OSError:
            self.cb_disconnect("OSError")
            #except Exception as e:
            #print(f"Exception: {type(e)}")
            #self.cb_disconnect(str(type(e)))
        finally:
            self.connected = False
            print("Disconnected from device")
    
    """
    Connect to specified port. Begins internal communication thread.
    """
    def connect(self, port, baud):
        print(f"Connecting to {port}, baud:{baud}")
        self.do_run = True
        self.thr_loop = threading.Thread(target=self._loop, args=(port, baud))
        self.thr_loop.start()
    
    """
    Disconnect from current port, returning when communication thread is joined. 
    """
    def disconnect(self):
        self.do_run = False
        if self.is_connected():
            self.thr_loop.join()
            print("Serial communication loop exited")
    
    """
    Send a string of characters to the device, ending in newline.
    Message is placed in a queue, consumed by communication thread.
    """
    def send_msg(self, msg):
        self.send_queue.put((f"{msg}\n").encode("utf-8"))
    
    """
    Check current connection status.
    """
    def is_connected(self):
        return self.connected


"""
Provide UI for SerialMonitor, and integrate DataStreamDatWriter with some callbacks.
"""
class App:
    """
    Begin app.
    """
    def __init__(self):
        self.do_display_data = True
        
        self.sm = SerialMonitor(self.on_serial_msg, self.on_serial_data, self.on_serial_data_start, self.on_serial_data_end, self.on_serial_disconnect, self.on_serial_heartbeat, self.on_serial_error)
        self.dscw = DataStreamDatWriter()
        
        # --- Create UI ---
        root = tk.Tk()
        self.root = root
        root.protocol("WM_DELETE_WINDOW", self.on_exit)
        root.title("Serial Monitor")
        f = tk.Frame(root)
        f.grid()
        row = 0
        
        # Connection
        self.port_sv = tk.StringVar(root)
        self.port_om = tk.OptionMenu(f, self.port_sv, "")
        self.refresh_devices()
        self.port_om.grid(row=row, column=0, columnspan=3, sticky="ew")
        row += 1
        tk.Button(f, text="Connect", command=self.connect_to_device).grid(row=row, column=0, sticky="ew")
        tk.Button(f, text="Refresh", command=self.refresh_devices).grid(row=row, column=1, sticky="ew")
        tk.Button(f, text="Disconnect", command=self.disconnect_from_device).grid(row=row, column=2, sticky="ew")
        # Connection status
        row += 1
        tk.Label(f, text="  Connection Life:", anchor="w").grid(row=row, column=0, columnspan=2, sticky="ew")
        self.heartbeat_sv = tk.StringVar(root)
        self.heartbeat_l = tk.Label(f, textvariable=self.heartbeat_sv)
        self.heartbeat_l.grid(row=row, column=2, sticky="ew")
        self.data_time_last = time.time()
        
        # Manual commands
        row += 2
        tk.Label(f, text="  Command:", anchor="w").grid(column=0, row=row, sticky="ew")
        self.send_e = tk.Entry(f, width=16)
        self.send_e.grid(column=1, row=row, columnspan=2, sticky="ew")
        self.send_e.bind("<Return>", self.send_manual_command)
        
        # Data commands
        row += 2
        tk.Button(f, text="Start", command=lambda:self.send_command("START")).grid(row=row, column=0, sticky="ew")
        tk.Button(f, text="Stop", command=lambda:self.send_command("STOP")).grid(row=row, column=1, sticky="ew")
        tk.Button(f, text="Test", command=self.send_command_test).grid(row=row, column=2, sticky="ew")
        
        # File commands
        row += 2
        tk.Button(f, text="List", command=lambda:self.send_command("LIST")).grid(row=row, column=0, sticky="ew")
        tk.Button(f, text="Read", command=self.send_command_read).grid(row=row, column=1, sticky="ew")
        tk.Button(f, text="Delete", command=self.send_command_delete).grid(row=row, column=2, sticky="ew")
        row += 1
        tk.Label(f, text="  SD File:", anchor="w").grid(row=row, column=0, sticky="ew")
        self.file_e = tk.Entry(f, width=16)
        self.file_e.grid(column=1, row=row, columnspan=2, sticky="ew")
        row += 1
        tk.Label(f, text="  Output Dir:", anchor="w").grid(row=row, column=0, sticky="ew")
        self.dir_e = tk.Entry(f, width=16)
        self.dir_e.grid(column=1, row=row, columnspan=2, sticky="ew")
        self.dir_e.insert(0, "data/")
        
        # Make rows and columns uniform
        element_rows = row
        for i in range(3):              # Make first 3 columns equal width
            f.grid_columnconfigure(i, weight=1, uniform="column")
        for i in range(element_rows):   # Make rows equal height
            f.grid_rowconfigure(i, weight=0, uniform="row")
        f.grid_rowconfigure(element_rows + 1, weight=1)
        
        # Message output console
        self.console_st = scrolledtext.ScrolledText(f, width=90, height=32, state="disabled")
        self.console_st.grid(row=0, column=3, rowspan=element_rows+2)
        self.display_queue = queue.Queue()
        
        
        # --- Run UI ---
        try:
            self.refresh_ui()       # Calls itself with delay
            root.mainloop()
        except KeyboardInterrupt:
            self.sm.disconnect()
    
    # Self-invoking function to update UI
    def refresh_ui(self):
        # Update console
        if not self.display_queue.empty():
            self.console_st.configure(state="normal")
            while not self.display_queue.empty():
                msg = self.display_queue.get()
                self.console_st.insert(tk.INSERT, f"{msg}")
            self.console_st.configure(state="disabled")
            self.console_st.yview(tk.END)
        # Update heartbeat monitor
        if self.sm.is_connected():
            time_since_data = time.time() - self.data_time_last
            if time_since_data > connection_status_warning_time:
                self.heartbeat_l.config(fg='red')
            else:
                self.heartbeat_l.config(fg='green')
            self.heartbeat_sv.set(f"{time_since_data:.2f}")
        else:
            self.heartbeat_sv.set("n/a")
            self.heartbeat_l.config(fg='red')
        # Refresh again after delay
        self.root.after(50, self.refresh_ui)
    
    # Invoked when UI closed
    def on_exit(self):
        self.sm.disconnect()
        self.root.destroy()
    
    # Callback for Refresh
    def refresh_devices(self):
        ports = serial.tools.list_ports.comports()
        # Filter out virtual terminals
        valid_ports = [p.device for p in ports if p.name[0:4] != "ttyS"]
        if len(valid_ports) == 0:
            valid_ports.append("No devices detected")
        self.port_sv.set(valid_ports[0])
        # Remove previous options, add new options
        self.port_om["menu"].delete(0, tk.END)
        for p in valid_ports:
            self.port_om["menu"].add_command(label=p, command=tk._setit(self.port_sv, p))
    
    # Callback for Connect
    def connect_to_device(self):
        self.any_data()
        self.sm.disconnect()
        p = self.port_sv.get()
        self.sm.connect(p, 1000000)
        self.console_st.configure(state="normal")
        self.console_st.delete("1.0", tk.END)
        self.console_st.insert(tk.END, f"=== CONNECTING TO '{p}' ===\n")
        self.console_st.configure(state="disabled")
    
    # Callback for Disconnect
    def disconnect_from_device(self):
        if self.sm.is_connected():
            self.sm.disconnect()
            self.display_queue.put("=== DISCONNECTED ===\n")
            self.dscw.end()
    
    # Sends command message to SerialMonitor
    def send_command(self, command):
        self.sm.send_msg(command)
    
    # Callback when <Return> is pressed in command text entry
    def send_manual_command(self, event):
        # Get command and clear widget
        command = self.send_e.get()
        self.send_e.delete(0, tk.END)
        self.send_command(command)
    
    # Send TEST command, and set DataStreamDatWriter suffix
    def send_command_test(self):
        self.dscw.set_file_suffix("-test")
        self.send_command("TEST")
    
    # Send READ file command, set DataStreamDatWriter suffix, suppress console data display
    def send_command_read(self):
        file = self.file_e.get()
        self.dscw.set_file_suffix(f"-{pathlib.Path(file).stem}") # remove file extension
        self.do_display_data = False
        self.send_command(f"READ {file}")
    
    # Send DEL file command
    def send_command_delete(self):
        file = self.file_e.get()
        self.send_command(f"DEL {file}")
    
    # Callback for message
    def on_serial_msg(self, msg):
        self.display_queue.put(msg)
        self.any_data()
    
    # Callback for data stream start - start DataStreamDatWriter
    def on_serial_data_start(self):
        self.display_queue.put("=== DATA START ===\n")
        self.dscw.start(self.dir_e.get())
        self.any_data()
    
    # Callback for data element - write to DataStreamDatWriter, and to console if not suppressed
    def on_serial_data(self, data):
        if self.do_display_data:
            vals = data_util.bytes_to_values(data)
            vals_pretty = data_util.values_to_str(vals)
            self.display_queue.put(f"{vals_pretty}\n")
        self.dscw.write(data)
        self.any_data()
    
    # Callback for data stream end - end DataStreamDatWriter and remove console data display suppression
    def on_serial_data_end(self):
        self.display_queue.put("=== DATA END ===\n")
        self.dscw.end()
        self.do_display_data = True
        self.any_data()
    
    # Callback for disconnection, display reason
    def on_serial_disconnect(self, disconnect_str):
        self.display_queue.put(f"=== DISCONNECTED [{disconnect_str}] ===\n")
        self.dscw.end()
    
    # Callback for heartbeat
    def on_serial_heartbeat(self):
        self.any_data()
    
    # Callback for device error
    def on_serial_error(self):
        self.display_queue.put("=== INTERNAL ERROR ===\n")
        self.dscw.end()
    
    # Called when any data is received
    def any_data(self):
        self.data_time_last = time.time()


if __name__ == "__main__":
    App()

