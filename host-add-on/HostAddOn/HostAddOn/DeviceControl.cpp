// device control thread

#include "DeviceControl.h"
#include "ASDFProtocol.h"
#include "SharedStruct.h"
#include "debug.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdio.h>


using namespace std;

static const char* PORT_NAME = "\\\\.\\COM6";
static const unsigned long BAUD_RATE = CBR_115200;

// reset device when an unexpected device-side error happens
static void reset_device() {
	asdf_close_serial();
	Sleep(MAX_DEVICE_RESET_MS);
	asdf_init_serial(PORT_NAME, BAUD_RATE);
	cmd_reset();
}

unsigned int __stdcall TQThread(void* data) {
	volatile SharedStruct& sharedst = *((SharedStruct*) data);
	
	if (asdf_init_serial(PORT_NAME, BAUD_RATE)) {
		Err("TQThread: Serial Init Failed. Quit.\n");
		return -1;
	}

	bool is_lever_released = true;
	bool init_done = false;

	if (asdf_flush_receive_buffer()) {
		Log("TQThread: Init Flush Serial Receive Buffer.\n");
	} else {
		Err("TQThread: Fail to flush serial receive buffer. Quit.\n");
		return -1;
	}

	// reset device and read initial ASDF_RESET
	{
		cmd_reset();
		//unsigned char code;
		//unsigned long size_read;
		//asdf_serial_read(&code, 1, &size_read);
		//if (code != ASDF_RESET) {
		//	printf("TQThread: ASDF_RESET mismatch upon init. Received: %d\n", code);
		//	return -1;
		//}
		//
		//printf("TQThread: Get ASDF_RESET response.\n");
		init_done = true;
	}

	Log("TQThread: Done DeviceControl Thread Initialization!\n");

	while (sharedst.quit == false) {
		unsigned char throttle_level[3];	// [0,1,2] = [speed brake, throttle 1, throttle 2]
		unsigned char button_status;

		// read throttle levels and button status from device
		if (cmd_poll(throttle_level, &button_status) != 0) {
			LogV("TQThread: Poll Result: Throttle = %u %u %u; Button = %u",	\
				throttle_level[0], throttle_level[1], throttle_level[2], button_status);
			reset_device();	// try to reset device upon error
			continue;		// goto next iteration and repoll
		}

		// update button status in shared structure
		sharedst.button_status[BUTTON_TOGA] = getButtonStatus(button_status, BUTTON_TOGA);
		sharedst.button_status[BUTTON_AT_DISENGAGE] = getButtonStatus(button_status, BUTTON_AT_DISENGAGE);

		// update speed brake lever position in shared structure
		sharedst.speed_brake = asdf2sc(throttle_level[0]);

		if (sharedst.is_AT_engaged) {
		// A/T engaged; get throttle levels from sharedst and send to device
			//throttle_level[0] = (unsigned char)(sharedst.speed_brake * 128 / 100);
			throttle_level[1] = sc2asdf(sharedst.throttle_level[THROTTLE_LEFT]);
			throttle_level[2] = sc2asdf(sharedst.throttle_level[THROTTLE_RIGHT]);
			if (cmd_lvr_set(0b011, throttle_level + 1) != 0) {
				reset_device();	// try to reset device upon error
				continue;		// goto next iteration and repoll
			}

			// set lever release flag
			if (is_lever_released) {
				is_lever_released = false;
				Log("TQThread: Lever Locked.\n");
			}
		} else {
		// A/T not engaged; update throttle levels in shared structure
			if (is_lever_released == false) {
				// send lever release command if not released
				if (cmd_lvr_rels() != 0) {
					reset_device();	// try to reset device upon error
					continue;		// goto next iteration and repoll
				}

				is_lever_released = true;
				Log("TQThread: Lever Released.\n");
			}
		
			// update throttle levels in shared structure
			sharedst.throttle_level[THROTTLE_LEFT] = asdf2sc(throttle_level[1]);
			sharedst.throttle_level[THROTTLE_RIGHT] = asdf2sc(throttle_level[2]);
		}
	}

	asdf_close_serial();

	Log("TQThread: Quit.\n");
	return 0;
}