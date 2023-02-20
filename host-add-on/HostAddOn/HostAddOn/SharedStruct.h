#pragma once

// shared data structure between SCThread and TQThread

#include <atomic>

#define THROTTLE_NUM 2
#define BUTTON_NUM 2

/* 
 * This structure is shared between TQThread and SCThread.
 * @speed_brake can only be set by TQThread.
 * @throttle_level can only be set by:
 *		SCThread,	if @is_AT_engaged == true;
 *		TQThread,	if @is_AT_engaged == false.
 * @button_status can only be set by TQThread.
 * @is_AT_engaged can only be set by SCThread.
 * @quit can only be set by SCThread.
 */
struct SharedStruct {
	std::atomic<double> speed_brake = 0;	// speed brake level (0-100, percent)
	std::atomic<double> throttle_level[THROTTLE_NUM] = { 0 };	// throttle levels; see throttle_idx_t
	std::atomic<bool> button_status[BUTTON_NUM] = { false };	// button status; see button_idx_t
	std::atomic<bool> is_AT_engaged = false;		// true => A/T engaged; false => A/T disengaged
	std::atomic<bool> quit = false;		// quit add-on
};

enum throttle_idx_t {
	THROTTLE_LEFT = 0,
	THROTTLE_RIGHT = 1
};

enum button_idx_t {
	BUTTON_TOGA = 0,
	BUTTON_AT_DISENGAGE = 1
};

/* map from ASDF byte range to SimConnect throttle level [0,127] -> [0,100] */
double asdf2sc(unsigned char val);

/* map from SimConnect throttle level to ASDF byte range [0,100] -> [0,127] */
unsigned char sc2asdf(double val);

/* prints out @st to stdout */
void printSharedStruct(volatile SharedStruct& st);