import serial
import time
import os
import sys

# command codes
CMD_RESET	 = (0x80)
CMD_POLL	 = (0x81)
CMD_LVR_RELS = (0x83)
CMD_ASDF	 = (0xFF)

# CMD_LVR_SET command list
# cmd[6:4] => 000, 001, 010, 011, 100, 101, 110, 111
CMD_LVR_SET_MAP = {
    "000": 0x82,    # unused
    "001": 0x92,
    "010": 0xA2,
    "011": 0xB2,
    "100": 0xC2,
    "101": 0xD2,
    "110": 0xE2,
    "111": 0xF2
}

# response codes
ASDF_ERROR	 =	(0xFF)
ASDF_ACK	 =	(0x00)
ASDF_RESET	 =	(0x01)
ASDF_POLL_OK =	(0x02)

# ASDF_LVR_RELS responses
ASDF_LVR_RELS_PILOT	= (0x03)
ASDF_LVR_RELS_RESP	= (0x83)

str2cmd_map = {
    "reset": CMD_RESET,
    "poll": CMD_POLL,
    "lvrrels": CMD_LVR_RELS,
    "asdf": CMD_ASDF,
    "lvrset": CMD_LVR_SET_MAP
}

resp2str_map = {
    ASDF_ERROR: "error",
    ASDF_ACK: "ack'd",
    ASDF_RESET: "reset done",
    ASDF_POLL_OK: "poll ok",
    ASDF_LVR_RELS_PILOT: "pilot release lever",
    ASDF_LVR_RELS_RESP: "release lever done"
}

def readCmd(n):
    if n == 0:
        data = arduino.read(arduino.in_waiting)
    else:
        data = arduino.read(n)
    return data

def writeCmd(data): # data should be a byte
    length = arduino.write(bytes([data]))
    print("write size: ", length)
    return length

# serial_port = input("Input Serial Port Name: ")
# baudrate = int(input("Input Baud Rate: "))

print("opening serial port")
arduino = serial.Serial(port = sys.argv[1], baudrate = int(sys.argv[2]), timeout = 0.1)
time.sleep(1)

print("starting debug session")

def wait_for_serial(n = 1):
    global arduino
    while (arduino.in_waiting < n):
        print(arduino.in_waiting)
        time.sleep(1)

while True:
    # read any redundant bytes
    print("redundant: ", readCmd(0))

    cmd = input("Provide command: ")

    if cmd == "quit":
        break

    print("Command: ", str2cmd_map[cmd])

    if cmd == "reset" : # CMD_RESET
        writeCmd(str2cmd_map[cmd])
        arduino.close()
        time.sleep(3)
        arduino.open()
        while True:
            #wait_for_serial()
            rdy = int.from_bytes(readCmd(1), "big")
            #print(resp2str_map[rdy])
            if rdy == ASDF_RESET:
                print("Device reset done")
                break

    if cmd == "poll": # CMD_POLL
        t = time.time()
        writeCmd(str2cmd_map[cmd])
        #wait_for_serial()
        acquired = readCmd(5)
        acq_array = list(acquired)
        print(acq_array)
        print("rate =", 1 / (time.time() - t), "polls/sec") # Calculate poll rate
    
    if cmd == "lvrrels": # CMD_LVR_RELS
        writeCmd(str2cmd_map[cmd])
        #wait_for_serial()
        # time.sleep(0.05)
        print(readCmd(1))

    if cmd == "lvrset": # CMD_LVR_SET
        bitmask = input("Input Lever Bitmask ({speed brake, thrust 1, thrust 2}): ")
        lvr_vals = input("Input Lever Values: ").strip().split(" ")
        lvr_vals = list(map(lambda x : int(x), lvr_vals))
        print("Lever Values: ", lvr_vals)
        print("Command: ", str2cmd_map[cmd][bitmask])
        if bitmask == "000":
            pass
        writeCmd(str2cmd_map[cmd][bitmask])
        for val in lvr_vals:
            writeCmd(val)

        #wait_for_serial()
        print(readCmd(1))

    if cmd == "asdf":
        writeCmd(str2cmd_map[cmd])
        #wait_for_serial()
        print(readCmd(1))


print("closing debug session")
arduino.close()