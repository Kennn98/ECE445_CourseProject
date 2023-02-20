#include "SharedStruct.h"

#include <iostream>

using namespace std;

/* map from ASDF byte range to SimConnect throttle level [0,127] -> [0,100] */
double asdf2sc(unsigned char val) {
	return ((double)val) * 100 / 127;
}

/* map from SimConnect throttle level to ASDF byte range [0,100] -> [0,127] */
unsigned char sc2asdf(double val) {
	return (unsigned char)(val * 127 / 100);
}

/* prints out @st to stdout */
void printSharedStruct(volatile SharedStruct& st) {
	cout << endl;

	cout << "speed_brake: " << st.speed_brake << endl;

	cout << "throttle_level: ";
	for (unsigned int i = 0; i < THROTTLE_NUM; i++)
		cout << st.throttle_level[i] << " ";
	cout << endl;

	cout << "button_status: ";
	for (unsigned int i = 0; i < BUTTON_NUM; i++)
		cout << st.button_status[i] << " ";
	cout << endl;

	cout << "is_AT_engaged: " << st.is_AT_engaged << endl;

	cout << "quit: " << st.quit << endl;
}