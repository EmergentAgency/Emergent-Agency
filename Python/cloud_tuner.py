# main5.py
#
# Author: Christopher Linder
# Created: 2020

import tkinter as tk
from tkinter import Canvas, Frame, BOTH
import random
import time
import numpy as np
import serial

# Define useful parameters
window_width = 600
window_height = 400
DELAY = 10

BLUE_COLOR = "#0492CF"
RED_COLOR_LIGHT = '#EE7E77'

tuning_param_list = ["MinSpeed", "MaxSpeed", "NewSpeedWeight", "InputExponent"]
display_vars_list = ["fRawSpeed", "fCurSpeed", "fNewSpeedRatio", "fSpeedRatio"]



class CloudTuner:
    # ------------------------------------------------------------------
    # Initialization Functions:
    # ------------------------------------------------------------------
    def __init__(self):
        self.window = tk.Tk()
        self.window.title("Cloud Tuner")
        self.canvas = Canvas(self.window, width=window_width, height=window_height)
        self.canvas.pack()
        # Input from user in form of clicks and keyboard
        self.window.bind("<Key>", self.key_input)
        self.window.bind("<Button-1>", self.mouse_input)
        self.init_screen()
        
        
    def validate(self, action, index, value_if_allowed,
                       prior_value, text, validation_type, trigger_type, widget_name):
        if value_if_allowed:
            try:
                float(value_if_allowed)
                return True
            except ValueError:
                return False
        else:
            return False


    def make_text_entry(self, x, y):
        vcmd = (self.window.register(self.validate), 
                '%d', '%i', '%P', '%s', '%S', '%v', '%V', '%W')
        #e1 = tk.Entry(self.canvas, validate = 'key', validatecommand = vcmd)
        e1 = tk.Entry(self.canvas) # Don't valid for now because it was being weird
        self.canvas.create_window((x, y), anchor=tk.NW, window = e1)
        e1.focus_set()
        return e1
        
            
    def init_screen(self):
        self.canvas.delete("all")
        self.begin_time = time.time()
        
        self.num_tuning_vars = len(tuning_param_list)
        self.text_inputs = []
        self.cur_tuning_vars = []
        for i in range(self.num_tuning_vars):
            self.canvas.create_text(25, i * 40 + 25, anchor=tk.NW, text = tuning_param_list[i])
            self.text_inputs.append(self.make_text_entry(25, i * 40 + 40))
            self.cur_tuning_vars.append(self.canvas.create_text(
                155,
                i * 40 + 40,
                anchor=tk.NW,
                font="cmr 15 bold",
                fill=BLUE_COLOR,
                text="Panda",
            ))
        
        self.num_display_vars = len(display_vars_list)
        self.display_bars = []
        self.display_vals = []
        for i in range(self.num_display_vars):
            self.display_bars.append(self.canvas.create_rectangle(
                i * 100 + 200, 300, i * 100 + 250, 350, fill=RED_COLOR_LIGHT, outline=BLUE_COLOR,
            ))
            self.canvas.create_text(i * 100 + 200, 350 + 20, anchor=tk.NW, text = display_vars_list[i])
        
            self.display_vals.append(self.canvas.create_text(
                i * 100 + 200,
                350 + 2,
                anchor=tk.NW,
                font="cmr 15 bold",
                fill=BLUE_COLOR,
                text="",
            ))

        from serial.tools.list_ports import comports
        if comports:
            com_ports_list = list(comports())
            for port in com_ports_list:
                print("TEMP_CL - port=", port)
                
        # TEMP_CL - fix this!
        self.serial_port = serial.Serial('COM14', baudrate=9600)
        
        # Request tuning vars
        self.serial_port.write(b'REQUEST_TUNING\n')
        
        
    def read_val_string(self, str_in, match_string):
        match_string_length = len(match_string)
        val_start = str_in.find(match_string) + match_string_length + 1
        val_end = str_in.find(" ", val_start)
        val_string = str_in[val_start:val_end]
        
        return val_string


    def update_loop(self):
        # Init the status string
        latest_status = ""
        
        # Get most recent data
        while self.serial_port.in_waiting:
            str_in = self.serial_port.readline().decode('UTF-8')
            print("got line:", str_in, end='')
            #c = self.serial_port.read()
            #print("got char:", c)
            
            # Look for new tuning vars
            tuning_vars = str_in.startswith("TUNING")
            if str_in.startswith("TUNING"):
                for i in range(self.num_tuning_vars):
                    val_string = self.read_val_string(str_in, tuning_param_list[i])
                    # TEMP_CL print(val_string)
                    self.text_inputs[i].delete(0, tk.END)
                    self.text_inputs[i].insert(0, val_string)
                    self.canvas.itemconfigure(self.cur_tuning_vars[i], text=val_string)
                    
             # Look for new status      
            elif str_in.startswith("STATUS"):
                latest_status = str_in
        
        # If we got new status, update based on it
        if latest_status != "":
            for i in range(self.num_display_vars):
                val_string = self.read_val_string(latest_status, display_vars_list[i])
                val = float(val_string)
                
                x0, y0, x1, y1 = self.canvas.coords(self.display_bars[i])
                self.canvas.coords(self.display_bars[i], x0, y1 - 200 * val, x1, y1)
                
                self.canvas.itemconfigure(self.display_vals[i], text=val_string)
        
        # Keep loop going
        self.window.after(DELAY, self.update_loop)


    def mouse_input(self, event):
        print("mouse_input")
        byte_written = self.serial_port.write(b'REQUEST_TUNING\n')
        self.serial_port.flush()
        print("byte_written=", byte_written)


    def key_input(self, event):
        # If the user pressed enter, send all tuning vars
        if event.keysym == "Return":
            for i in range(self.num_tuning_vars):
                entry_text = self.text_inputs[i].get()
                try:
                    text_val = float(entry_text)
                    tuning_string = "NEW_TUNING " + tuning_param_list[i] + "=" + str(text_val) + "\n"
                    tuning_bytes = tuning_string.encode('UTF-8')
                    print("Sending:", tuning_bytes)
                    self.serial_port.write(tuning_bytes)
                except:
                    print("Invalid float value!", entry_text)

            # Get the tuning vars back from the chip to show that it worked
            byte_written = self.serial_port.write(b'REQUEST_TUNING\n')


game_instance = CloudTuner()
game_instance.update_loop()
tk.mainloop()
