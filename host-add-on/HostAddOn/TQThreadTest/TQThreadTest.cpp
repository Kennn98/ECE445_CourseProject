// TQThreadTest.cpp : Test Program to launch TQThread in a standalone program.

#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <iostream>
#include <chrono>

#include "SharedStruct.h"
#include "DeviceControl.h"
#include "ASDFProtocol.h"

#include "debug.h"

using namespace std;

#define TEST_HEADER	do {	\
	Log("\nStarting Test: " __FUNCTION__ "\n");	\
} while (0)

#define TEST_PASS do {	\
	Log("Test Passed!\n");	\
} while (0)

#define TEST_FAIL do {	\
	Err("Test Failed!");	\
} while (0)

static const char* PORT_NAME = "\\\\.\\COM6";
static const unsigned long BAUD_RATE = CBR_115200;

SharedStruct sharedst;

static unsigned int __stdcall TQThreadSnooper(void* data) {
	Log("TQThreadSnooper Thread Starts.\n");

	volatile SharedStruct& sharedst = *((SharedStruct*)data);

	while (sharedst.quit == false) {
		printSharedStruct(sharedst);
		Sleep(2000);
	}

	Log("TQThreadSnooper Thread Ends.\n");
	return 0;
}

static void TQThreadTest() {
	TEST_HEADER;

	HANDLE tqthread, debug_printer;
	tqthread = (HANDLE)_beginthreadex(0, 0, TQThread, &sharedst, 0, 0);
	debug_printer = (HANDLE)_beginthreadex(0, 0, TQThreadSnooper, &sharedst, 0, 0);

	Log("HostAddOn Main Thread: TQThread start.\n");
	WaitForSingleObject(tqthread, INFINITE);
	WaitForSingleObject(debug_printer, INFINITE);

	CloseHandle(tqthread);
	CloseHandle(debug_printer);

	Log("HostAddOn Main Thread: TQThread quit.\n");
}

static void test_CMD_ASDF() {
	TEST_HEADER;

	Log("CMD_ASDF Response: %d\n", cmd_asdf());
}

static void test_CMD_RESET() {
	TEST_HEADER;

	Log("CMD_RESET Response: %d\n", cmd_reset());
}

static void test_CMD_POLL() {
	TEST_HEADER;

	unsigned char lever_pos[3];
	unsigned char btn_status;
	Log("CMD_POLL Response: %d\n", cmd_poll(lever_pos, &btn_status));
	// TODO: check poll results?
}

static void test_CMD_LVR_RELS() {
	TEST_HEADER;

	Log("CMD_LVR_RELS Response: %d\n", cmd_lvr_rels());
}

static void test_CMD_LVR_SET() {
	TEST_HEADER;

	unsigned char values[3] = {10, 20, 30};

	// loop through all possible bitmasks
	for (unsigned char bitmask = 0; bitmask <= 0b111; bitmask++) {
		Log("CMD_LVR_SET Response: %d\n", cmd_lvr_set(bitmask, values));
	}
}

// test all ASDF commands
static void testASDFCommands() {
	TEST_HEADER;

	asdf_init_serial(PORT_NAME, BAUD_RATE);

	unsigned char garbage;
	unsigned long gbg_size_read;
	asdf_serial_read_remaining(&garbage, 1, &gbg_size_read);

	test_CMD_RESET();
	test_CMD_ASDF();
	test_CMD_POLL();
	test_CMD_LVR_SET();

	asdf_close_serial();
}

// recommand undefining DEBUG flag for perf tests
static void PollTest(unsigned int num_tests) {
	TEST_HEADER;

	unsigned char throttle_level[3];	// [0,1,2] = [speed brake, throttle 1, throttle 2]
	unsigned char button_status;

	asdf_init_serial(PORT_NAME, BAUD_RATE);
	
	unsigned char garbage;
	unsigned long gbg_size_read;
	asdf_serial_read_remaining(&garbage, 1, & gbg_size_read);

	// start perf timer
	auto start = chrono::steady_clock::now();

	for (unsigned int i = 0; i < num_tests; i++) {
		// read throttle levels and button status from device
		cmd_poll(throttle_level, &button_status);

		// update button status in shared structure
		sharedst.button_status[BUTTON_TOGA] = getButtonStatus(button_status, BUTTON_TOGA);
		sharedst.button_status[BUTTON_AT_DISENGAGE] = getButtonStatus(button_status, BUTTON_AT_DISENGAGE);
	}

	// end perf timer
	auto end = chrono::steady_clock::now();
	chrono::duration<double> elapsed_sec = end - start;

	asdf_close_serial();
	
	// print stats
	cout << "Poll Rate: " << (double)num_tests / elapsed_sec.count() << " polls/sec" << endl;
}

int main() {
	//PollTest(4096);
	TQThreadTest();
	//testASDFCommands();

	system("pause");
	return 0;
}