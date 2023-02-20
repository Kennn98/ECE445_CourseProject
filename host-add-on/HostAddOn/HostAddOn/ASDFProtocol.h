#pragma once

// ASDF Protocol over Serial Communication with Arduino

#include <Windows.h>

// command codes
#define CMD_RESET	 (0x80)
#define CMD_POLL	 (0x81)
#define CMD_LVR_RELS (0x83)
#define CMD_ASDF	 (0xFF)

// CMD_LVR_SET command list
// cmd[6:4] => 000, 001, 010, 011, 100, 101, 110, 111
constexpr unsigned int CMD_LVR_SET_LIST[] = {0x82, 0x92, 0xA2, 0xB2, 0xC2, 0xD2, 0xE2, 0xF2};

#define CMD_LVR_SET_EMPTY	(0x82)

// bitmask to set levers (speed brake, throttle 1, throttle 2)
#define CMD_LVR_SET_SPDBR	(1 << 6)
#define CMD_LVR_SET_TR1		(1 << 5)
#define CMD_LVR_SET_TR2		(1 << 4)

// response codes
#define ASDF_ERROR		(0xFF)
#define	ASDF_ACK		(0x00)
#define	ASDF_RESET		(0x01)
#define	ASDF_POLL_OK	(0x02)

// ASDF_LVR_RELS responses
#define ASDF_LVR_RELS_PILOT	(0x03)
#define ASDF_LVR_RELS_RESP	(0x83)

// max time to wait for device reset until reconnecting Serial (ms)
#define MAX_DEVICE_RESET_MS	(3000)

// return status of button @i, given bitmap @btmp
#define getButtonStatus(btmp, i)	((btmp) & (0x1 << i))


// ASDF packet struct
struct ASDFPacket {
	unsigned char code;		// command/response code; or expected response code
	unsigned char data[8];	// use maximum 8 bytes of data for now should be sufficient
	unsigned int data_size;		// size of data array to be sent; or expected size of received data array
};


// ASDF Serial Functions

/* initialize serial connection */
int asdf_init_serial(const char* PORT_NAME, unsigned long BAUD_RATE);

/* close the serial port */
void asdf_close_serial();

/* write to serial port */
int asdf_serial_write(void* buffer, unsigned int size, unsigned long* size_written);

/* read from serial port */
int asdf_serial_read(void* buffer, unsigned int size, unsigned long* size_read);

/* read all bytes remaining in the receive buffer */
int asdf_serial_read_remaining(void* buffer, unsigned int size, unsigned long* size_read);

/* flush serial receive buffer; return 0 if failed, nonzero if success */
int asdf_flush_receive_buffer();

/* return # of bytes in the receive buffer */
unsigned int asdf_available();

/**	
 *	@asdf_pkt: packet to be sent
 *	@pkt_recvd: the received asdf packet
 *
 *	Send an ASDF packet to the device. Will return only when it gets a response from the device.
 **/ 
int asdf_send(ASDFPacket& asdf_pkt, ASDFPacket& pkt_recvd);


// ASDF Command Sender and Response Handler

int cmd_reset();
int cmd_poll(unsigned char* lever_pos, unsigned char* btn_status);
int cmd_lvr_rels();
int cmd_asdf();		// reserved for debug

// @bitmask to set levers = (speed brake, throttle 1, throttle 2)
int cmd_lvr_set(unsigned char bitmask, unsigned char* values);