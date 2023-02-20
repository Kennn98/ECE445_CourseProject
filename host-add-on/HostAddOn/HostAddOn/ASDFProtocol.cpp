// ASDF Protocol over Serial Communication with Arduino

#include "ASDFProtocol.h"
#include <windows.h>
#include <stdio.h>
#include <string>
#define VERBOSE
#include "debug.h"

// Serial session handler
static HANDLE Serial;
static DCB dcb;
static bool Serial_Initialized = false;

// initialize serial connection
int asdf_init_serial(const char* PORT_NAME = NULL, unsigned long BAUD_RATE = 0) {
	// static fields to store port name and baud rate for reset
	static char port_name[16] = { 0 };
	static unsigned long baud_rate = { 0 };

	// init static fields if params are not NULL/0
	if (PORT_NAME != NULL)
		if (strncmp(port_name, PORT_NAME, sizeof(port_name)) != 0)
			strcpy_s(port_name, PORT_NAME);
	if (BAUD_RATE != 0)
		if (baud_rate != BAUD_RATE)
			baud_rate = BAUD_RATE;

	Serial = CreateFileA(port_name,			// port name "\\\\.\\COM24"
		GENERIC_READ | GENERIC_WRITE,		// Read/Write
		0,									// No Sharing
		NULL,								// No Security
		OPEN_EXISTING,						// Open existing port only
		0,									// Non Overlapped I/O
		NULL);								// Null for Comm Devices

	if (Serial == INVALID_HANDLE_VALUE) {
		Err("Error in opening serial port\n");
		return -1;
	}

	if (!GetCommState(Serial, &dcb)) {
		Err("Get DCB Failed.\n");
		return -1;
	}

	dcb.BaudRate = baud_rate;

	if (!SetCommState(Serial, &dcb)) {
		Err("Set DCB Failed.\n");
		return -1;
	}

	Log("Open serial port successful\n");
	Serial_Initialized = true;

	// wait for serial ready
	Sleep(200);

	return 0;
}

// close the serial port
void asdf_close_serial() {
	CloseHandle(Serial);		//Closing the Serial Port
	Serial_Initialized = false;
	Log("Close serial port successful\n");
}

// flush serial receive buffer
int asdf_flush_receive_buffer() {
	return PurgeComm(Serial, PURGE_RXCLEAR);
}

// return # of bytes in the receive buffer
unsigned int asdf_available() {
	// Device Errors
	DWORD commErrors;
	// Device status
	COMSTAT commStatus;
	// Read status
	ClearCommError(Serial, &commErrors, &commStatus);
	// Return the number of pending bytes
	return commStatus.cbInQue;
}

// write to serial port
int asdf_serial_write(void* buffer, unsigned int size, unsigned long* size_written) {
	return WriteFile(Serial, buffer, size, size_written, NULL);
}

// read from serial port; block until get @size bytes; @size must be >0!!!
int asdf_serial_read(void* buffer, unsigned int size, unsigned long* size_read) {
	while (asdf_available() < size) {}	// hang over until have enough bytes to read
	return ReadFile(Serial, buffer, size, size_read, NULL);
}

// read all bytes remaining in the receive buffer
int asdf_serial_read_remaining(void* buffer, unsigned int size, unsigned long* size_read) {
	unsigned int truncated_size = asdf_available() >= size ? size: asdf_available();
	return ReadFile(Serial, buffer, truncated_size, size_read, NULL);
}

// Send an ASDF packet to the device without capturing the return packet
static int asdf_send_no_recv(ASDFPacket& asdf_pkt) {
	if (!Serial_Initialized) {
		Err("Serial Port not initialized.\n");
		return -1;
	}

	char write_buf[16];
	unsigned int write_size = 0;

	// set command code
	write_buf[write_size++] = asdf_pkt.code & 0xFF;

	// set data
	for (unsigned int i = write_size; i < asdf_pkt.data_size && i < 16; i++)
		write_buf[i] = asdf_pkt.data[i - write_size];
	write_size += asdf_pkt.data_size;

	// send packet
	unsigned long size_written = 0;
	asdf_serial_write((void*)write_buf, write_size, &size_written);
	if (size_written != write_size) {
		Err("Serial Write size mismatch.\n");
		return -1;
	}

	return 0;
}


// Send an ASDF packet to the device. Will return only when it gets a response from the device.
int asdf_send(ASDFPacket& asdf_pkt, ASDFPacket& pkt_recvd) {
	if (!Serial_Initialized) {
		Err("Serial Port not initialized.\n");
		return -1;
	}
	
	// input sanity check
	if (pkt_recvd.data_size > 15) {
		Err("expected receive size overflow: %d\n", pkt_recvd.data_size);
		return -1;
	}

	// send packet
	if (asdf_send_no_recv(asdf_pkt) != 0)
		return -1;

	// read packet
	unsigned char read_buf[16];
	unsigned long size_read = 0;
	asdf_serial_read((void*)read_buf, pkt_recvd.data_size + 1, &size_read);
	if (size_read == 0) {
		Err("Serial read size is 0.\n");
		return -1;
	}
	if (size_read - 1 != pkt_recvd.data_size) {
		Err("Serial read size mismatch: Expected: %u; Received: %u\n", pkt_recvd.data_size, size_read - 1);
	}
	if (read_buf[0] != pkt_recvd.code) {
		Err("Received wrong response code: Expected: %u, Received: %u\n", pkt_recvd.code, read_buf[0]);
		return -1;
	}

	// parse packet
	pkt_recvd.code = read_buf[0];
	for (unsigned int i = 0; i < pkt_recvd.data_size && i < size_read - 1; i++)
		pkt_recvd.data[i] = read_buf[i + 1];
	pkt_recvd.data_size = size_read - 1;

	return 0;
}



// ASDF Command Sender and Response Handler

int cmd_reset() {
	Log("Sending CMD_RESET\n");
	
	// craft ASDFPacket
	ASDFPacket pkt = {
		CMD_RESET,
		{ 0 },
		0
	};

	Log("Resetting Serial Connection...\n");

	// send ASDFPacket
	if (asdf_send_no_recv(pkt) != 0)
		return -1;

	asdf_close_serial();
	Sleep(MAX_DEVICE_RESET_MS);
	asdf_init_serial();

	// read ASDF_RESET packet
	unsigned char code;
	unsigned long size_read;
	asdf_serial_read(&code, 1, &size_read);
	if (code != ASDF_RESET) {
		Err("ASDF_RESET mismatch upon reset. Received: %d\n", code);
		return -1;
	}

	Log("Device Reset Complete.\n");

	return 0;
}

int cmd_poll(unsigned char* lever_pos, unsigned char* btn_status) {
	LogV("Sending CMD_POLL: ");

	// craft ASDFPackets
	ASDFPacket pkt = {
		CMD_POLL,
		{ 0 },
		0
	};

	ASDFPacket recv_pkt = {
		ASDF_POLL_OK,
		{ 0 },
		4
	};

	// send ASDFPacket
	if (asdf_send(pkt, recv_pkt) != 0) {
		Err("ASDFPacket send Error: CMD_POLL\n");
		return -1;
	}

	*btn_status = recv_pkt.data[0];		// button status
	lever_pos[0] = recv_pkt.data[1];	// speed brake
	lever_pos[1] = recv_pkt.data[2];	// throttle 1
	lever_pos[2] = recv_pkt.data[3];	// throttle 2

	LogV("%u %u %u %u\n", *btn_status, lever_pos[0], lever_pos[1], lever_pos[2]);
	
	//for (int i = 0; i < 8; i++)
	//	LogV("%u ", recv_pkt.data[i]);
	//LogV("\n");

	return 0;
}

int cmd_lvr_rels() {
	LogV("Sending CMD_LVR_RELS\n");

	// craft ASDFPackets
	ASDFPacket pkt = {
		CMD_LVR_RELS,
		{ 0 },
		0
	};

	ASDFPacket recv_pkt = {
		ASDF_LVR_RELS_RESP,
		{ 0 },
		0
	};

	// send ASDFPacket
	if (asdf_send(pkt, recv_pkt) != 0) {
		Err("ASDFPacket send Error: CMD_LVR_RELS\n");
		return -1;
	}

	return 0;
}

// reserved for debug
int cmd_asdf() {
	LogV("Sending CMD_ASDF\n");

	// craft ASDFPackets
	ASDFPacket pkt = {
		CMD_ASDF,
		{ 0 },
		0
	};

	ASDFPacket recv_pkt = {
		ASDF_ACK,
		{ 0 },
		0
	};

	// send ASDFPacket
	if (asdf_send(pkt, recv_pkt) != 0) {
		Err("ASDFPacket send Error: CMD_ASDF\n");
		return -1;
	}

	return 0;
}

// @bitmask to set levers = (speed brake, throttle 1, throttle 2)
int cmd_lvr_set(unsigned char bitmask, unsigned char* values) {
	static constexpr unsigned char LVR_MASK = CMD_LVR_SET_SPDBR | CMD_LVR_SET_TR1 | CMD_LVR_SET_TR2;

	LogV("Sending CMD_LVR_SET: %u %u %u %u\n", bitmask, values[0], values[1], values[2]);

	// set command lever bitmask
	unsigned char cmd = CMD_LVR_SET_EMPTY | ((bitmask << 4) & LVR_MASK);

	// craft ASDFPacket
	ASDFPacket pkt = {
		cmd,
		{ 0 },
		0
	};

	switch (bitmask) {
		case 0b000:
			break;

		case 0b001:
		case 0b010:
		case 0b100:
			pkt.data_size = 1;
			pkt.data[0] = values[0] & 0x7F;
			break;
		
		case 0b011:
		case 0b101:
		case 0b110:
			pkt.data[0] = values[0] & 0x7F;
			pkt.data[1] = values[1] & 0x7F;
			pkt.data_size = 2;
			break;

		case 0b111:
			pkt.data[0] = values[0] & 0x7F;
			pkt.data[1] = values[1] & 0x7F;
			pkt.data[2] = values[2] & 0x7F;
			pkt.data_size = 3;
			break;

		default:
			Err("Unrecognized bitmask: %u\n", bitmask);
			break;
	}

	ASDFPacket recv_pkt = {
		ASDF_ACK,
		{ 0 },
		0
	};

	// send ASDFPacket
	if (asdf_send(pkt, recv_pkt) != 0) {
		Err("ASDFPacket send Error: CMD_LVR_SET\n");
		return -1;
	}

	return 0;
}