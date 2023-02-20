import serial
import time
# import numpy as np
import os

def readCmd (n) :
    if n == 0:
        data = arduino.read(arduino.in_waiting)
    else :
        data = arduino.read(n)
    return data

def writeCmd (data) :
    length = arduino.write(bytes(data, "UTF-8"))
    return length

arduino = serial.Serial(port = 'COM8', baudrate = 115200, timeout = 0.1)
time.sleep(1)

# List of valid CMD_LVR_SET commands
CMD_LVR_SET_list = ["130", "146", "162", "178", "194", "210", "226", "242"]

while True :
    cmd = input("Provide command: ")
    # cmd = "129"
    if cmd == "128" : # CMD_RESET
        writeCmd(cmd)
        time.sleep(1)
        arduino.close()
        arduino.open()
        while True :
            rdy = readCmd(0)
            if rdy == b'DONE':
                print("Device reset done")
                break

    if cmd == "129" : # CMD_POLL
        t = time.time()
        writeCmd(cmd)
        while(arduino.in_waiting < 14) : # Wait for buffer to fill. Avoid the first empty cycle
            pass
        acquired = readCmd(14)
        acq_array = list(acquired)
        parsed = []
        i = 2
        if acq_array[0] != 2 and acq_array[1] != 0 :
            acq_array.append(acq_array.pop(0)) # Addresses bit misalignment after reset caused by unknown reasons
        while i < len(acq_array) - 2:
            parsed.append(acq_array[i] + (acq_array[i+1] << 8))
            i += 2
        print(acq_array)
        print(parsed)
        print("rate = " + str(1/(time.time()-t))) # Calculate poll rate
    
    if cmd == "131" : # CMD_LVR_RELS
        writeCmd(cmd)
        time.sleep(0.05)
        print(readCmd(0))

    # CMD_LVR_SET
    if cmd in CMD_LVR_SET_list :
        writeCmd(cmd)
    # Unused
    # if cmd == "255" :
    #     pass



