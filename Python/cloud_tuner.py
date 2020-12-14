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
import sys

# Define useful parameters
window_width = 1000
window_height = 450
DELAY = 10

BLUE_COLOR = "#0492CF"
RED_COLOR_LIGHT = '#EE7E77'

tuning_param_list = ["MinSpeed", "MaxSpeed", "NewSpeedWeight", "InputExponent"]
display_vars_list = ["fSpeedRatio", "fRawSpeedRatio", "fNewSpeedRatio", "fCurSpeed", "fRawSpeed", "fPulsesSinceLastTick"]
num_color_grad_rows = 5

# Canvas layout vars
text_enty_label_height = 15
tuning_vars_x = 25
tuning_vars_y = 25
tuning_vars_vert_spacing = 40
colors_x = 25
colors_y = 310
colors_vert_spacing = 20
colors_horiz_spacing = 30
live_bars_x = 300
live_bars_y = 25
live_bars_width = 50
live_bars_height = 200
live_bars_horiz_spacing = 110
live_bats_label_vert_spacing = 20



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


    def make_text_entry(self, x, y, width=18):
        vcmd = (self.window.register(self.validate), 
                '%d', '%i', '%P', '%s', '%S', '%v', '%V', '%W')
        #e1 = tk.Entry(self.canvas, validate = 'key', validatecommand = vcmd) # Don't validate for now because it was being weird
        e1 = tk.Entry(self.canvas, width=width, justify='right')
        self.canvas.create_window((x, y), anchor=tk.NW, window = e1)
        e1.focus_set()
        return e1

    def make_color_grad_entry(self, x, y):
        entry_width = colors_horiz_spacing
        self.canvas.create_text(x, y, anchor=tk.NW, text="Color Gradient")
        self.canvas.create_text(x + entry_width*5 - 18, y, anchor=tk.NW, text="Current Values")
        self.canvas.create_text(x + entry_width*10 - 18, y, anchor=tk.NW, text="Saved Values")
        for row in range(num_color_grad_rows):
            y_pos = y + colors_vert_spacing * row + text_enty_label_height
            scale_entry = self.make_text_entry(x, y_pos, width=3)
            r_entry = self.make_text_entry(x + entry_width*1, y_pos, width=3)
            g_entry = self.make_text_entry(x + entry_width*2, y_pos, width=3)
            b_entry = self.make_text_entry(x + entry_width*3, y_pos, width=3)  

            if row == 0 or row == 4:
                scale_entry.config(state='disabled')
                
            self.color_entrys.append(scale_entry)
            self.color_entrys.append(r_entry)
            self.color_entrys.append(g_entry)
            self.color_entrys.append(b_entry)
            
            self.color_vals.append(self.canvas.create_text(x + entry_width*5, y_pos, anchor=tk.NE, fill=BLUE_COLOR, text="255"))
            self.color_vals.append(self.canvas.create_text(x + entry_width*6, y_pos, anchor=tk.NE, fill=BLUE_COLOR, text="255"))
            self.color_vals.append(self.canvas.create_text(x + entry_width*7, y_pos, anchor=tk.NE, fill=BLUE_COLOR, text="255"))
            self.color_vals.append(self.canvas.create_text(x + entry_width*8, y_pos, anchor=tk.NE, fill=BLUE_COLOR, text="255"))

            self.color_vals_saved.append(self.canvas.create_text(x + entry_width*10, y_pos, anchor=tk.NE, fill=RED_COLOR_LIGHT, text="255"))
            self.color_vals_saved.append(self.canvas.create_text(x + entry_width*11, y_pos, anchor=tk.NE, fill=RED_COLOR_LIGHT, text="255"))
            self.color_vals_saved.append(self.canvas.create_text(x + entry_width*12, y_pos, anchor=tk.NE, fill=RED_COLOR_LIGHT, text="255"))
            self.color_vals_saved.append(self.canvas.create_text(x + entry_width*13, y_pos, anchor=tk.NE, fill=RED_COLOR_LIGHT, text="255"))
            
    def init_screen(self):
        self.canvas.delete("all")
        self.begin_time = time.time()
        
        self.num_tuning_vars = len(tuning_param_list)
        self.text_inputs = []
        self.cur_tuning_vars = []
        self.tuning_vars_saved = []
        for i in range(self.num_tuning_vars):
            self.canvas.create_text(tuning_vars_x, i * tuning_vars_vert_spacing + tuning_vars_y, anchor=tk.NW, text = tuning_param_list[i])
            self.text_inputs.append(self.make_text_entry(tuning_vars_x, i * tuning_vars_vert_spacing + tuning_vars_y + text_enty_label_height))
            self.cur_tuning_vars.append(self.canvas.create_text(
                tuning_vars_x + 130,
                i * tuning_vars_vert_spacing + tuning_vars_y + text_enty_label_height,
                anchor=tk.NW,
                font="cmr 15 bold",
                fill=BLUE_COLOR,
                text="Panda",
            ))
            self.tuning_vars_saved.append(self.canvas.create_text(
                tuning_vars_x + 190,
                i * tuning_vars_vert_spacing + tuning_vars_y + text_enty_label_height,
                anchor=tk.NW,
                font="cmr 15 bold",
                fill=RED_COLOR_LIGHT,
                text="0.02",
            ))
            
        self.color_entrys = []
        self.color_vals = []
        self.color_vals_saved = []
        self.make_color_grad_entry(colors_x, colors_y)
        
        self.num_display_vars = len(display_vars_list)
        self.display_bars = []
        self.display_vals = []
        for i in range(self.num_display_vars):
            self.display_bars.append(self.canvas.create_rectangle(
                i * live_bars_horiz_spacing + live_bars_x, 
                live_bars_y,
                i * live_bars_horiz_spacing + live_bars_x + live_bars_width,
                live_bars_y + live_bars_height,
                fill=RED_COLOR_LIGHT, 
                outline=BLUE_COLOR,
            ))
            # Create outline for the first bars
            if i < 3:
                self.canvas.create_rectangle(
                    i * live_bars_horiz_spacing + live_bars_x, 
                    live_bars_y,
                    i * live_bars_horiz_spacing + live_bars_x + live_bars_width,
                    live_bars_y + live_bars_height,
                    fill="", 
                    outline=BLUE_COLOR,
                )

            self.canvas.create_text(
                i * live_bars_horiz_spacing + live_bars_x,
                live_bars_y + live_bars_height + live_bats_label_vert_spacing, 
                anchor=tk.NW, 
                text = display_vars_list[i]
            )
        
            self.display_vals.append(self.canvas.create_text(
                i * live_bars_horiz_spacing + live_bars_x,
                live_bars_y + live_bars_height + 2,
                anchor=tk.NW,
                font="cmr 15 bold",
                fill=BLUE_COLOR,
                text="",
            ))
            
        ## Create outline for the first bars
        #self.canvas.create_rectangle(
        #    live_bars_x, 
        #    live_bars_y,
        #    2 * live_bars_horiz_spacing + live_bars_x + live_bars_width,
        #    live_bars_y + live_bars_height,
        #    fill="", 
        #    outline=BLUE_COLOR,
        #)

        com_port_to_use = ""
        from serial.tools.list_ports import comports
        if comports:
            com_ports_list = list(comports())
            for port in com_ports_list:
                print("Found com port:", port.name)
                com_port_to_use = port.name
                
        if com_port_to_use != "":
            self.serial_port = serial.Serial(com_port_to_use, baudrate=9600)
        else:
            print("ERROR! Could not find a com port for cloud tuning!")
            print("Please attach a cloud node to this computer via USB.")
            sys.exit(0)
        
        # Request tuning vars
        self.serial_port.write(b'REQUEST_TUNING\n')
        self.serial_port.write(b'REQUEST_COLORS\n')
 

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
            
            # Fine grain debugging
            #c = self.serial_port.read()
            #print("got char:", c)
            
            # Less fine grain debugging
            if not(str_in.startswith("STATUS")):
                print("got line:", str_in, end='')
            
            # Look for new tuning vars
            if str_in.startswith("TUNING"):
                for i in range(self.num_tuning_vars):
                    val_string = self.read_val_string(str_in, tuning_param_list[i])
                    # TEMP_CL print(val_string)
                    self.text_inputs[i].delete(0, tk.END)
                    self.text_inputs[i].insert(0, val_string)
                    self.canvas.itemconfigure(self.cur_tuning_vars[i], text=val_string)
                    
            # Look for a new color gradient
            color_prefix = "COLORS - "
            if str_in.startswith(color_prefix):
                color_str = str_in[len(color_prefix):]
                colors = color_str.replace("\r\n","").split(',')
                for row in range(num_color_grad_rows):
                    for i in range(4):
                        color_entry_index = row * 4 + i
                        # TEMP_CL print(colors[color_entry_index])
                        
                        # Entry
                        self.color_entrys[color_entry_index].config(state='normal')
                        self.color_entrys[color_entry_index].delete(0, tk.END)
                        self.color_entrys[color_entry_index].insert(0, colors[color_entry_index])
                        if i == 0 and (row == 0 or row == 4):
                            self.color_entrys[color_entry_index].config(state='disabled')
                       
                        # Label
                        self.canvas.itemconfigure(self.color_vals[color_entry_index], text=colors[color_entry_index])
                   
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
                    
            try:
                color_string = "NEW_COLOR_GRAD PalHeat="
                for i in range(num_color_grad_rows*4):
                    entry_text = self.color_entrys[i].get()
                    color_val = int(entry_text)
                    color_string += str(color_val)
                    if i < num_color_grad_rows*4 - 1:
                        color_string += ","
                color_string += "\n"
                color_bytes = color_string.encode('UTF-8')
                print("Sending:", color_bytes)
                self.serial_port.write(color_bytes)
                
            except:
                print("Invalid color value!", entry_text)

            # Get the tuning vars back from the chip to show that it worked
            byte_written = self.serial_port.write(b'REQUEST_TUNING\n')
            byte_written = self.serial_port.write(b'REQUEST_COLORS\n')


game_instance = CloudTuner()
game_instance.update_loop()
tk.mainloop()
