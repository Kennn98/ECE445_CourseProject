// SimConnect Thread

/*
NOTE: the PMDG 777 doesn't send the SDK data by default.
You will need to add these lines to <FSX>\PMDG\PMDG 777X\777X_Options.ini:

[SDK]
EnableDataBroadcast=1

to enable the data sending from the 777X.
*/

#include "ThrottleControl.h"
#include "PMDG_777X_SDK.h"
#include "SharedStruct.h"

//#define VERBOSE
#include "debug.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "SimConnect.h"
#include <strsafe.h>
#include <atomic>

static bool    quit = false;
static HANDLE  hSimConnect = NULL;

// notification group IDs
enum GROUP_ID{
	GROUP_BUTTONS,	// button input from device
	//GROUP_AT,		// A/T status
	GROUP_ENG		// engine
};

// input group IDs
enum INPUT_ID {
	INPUT_BUTTONS	// button input from device
};

// client event IDs
enum EVENT_ID {
	// system events
    EVENT_SIM,
	EVENT_PAUSE,
	EVENT_AIRCRAFT_LOADED,

	EVENT_AT_DISENGAGE_1,
	EVENT_AT_DISENGAGE_2,
	EVENT_TOGGLE_TOGA,
	EVENT_SET_SPEED_BRAKE,

	EVENT_REV_THRUST_1,
	EVENT_REV_THRUST_2,
	EVENT_FWD_THRUST_1,
	EVENT_FWD_THRUST_2
};

// event string names
static const char* EVENT_NAME_AT_DISENGAGE[THROTTLE_NUM] = {
	"#70134",
	"#70138"
};
static const char* EVENT_NAME_TOGGLE_TOGA = "AUTO_THROTTLE_TO_GA";
static const char* EVENT_NAME_AIRCRAFT_LOADED = "AircraftLoaded";

// reverse/forward thrust
static const char* EVENT_NAME_REV_THRUST[THROTTLE_NUM] = {
	"#70131",	// EVT_CONTROL_STAND_REV_THRUST1_LEVER
	"#70135"	// EVT_CONTROL_STAND_REV_THRUST2_LEVER
};
static const char* EVENT_NAME_FWD_THRUST[THROTTLE_NUM] = {
	"#70133",	// EVT_CONTROL_STAND_FWD_THRUST1_LEVER
	"#70137"	// EVT_CONTROL_STAND_FWD_THRUST2_LEVER
};

// speed brake
static const char* EVENT_NAME_SET_SPEED_BRAKE = "AXIS_SPOILER_SET"; //"#70130";

// client data type IDs
enum DATA_DEFINE_ID {
    DEFINITION_THROTTLE_1,
	DEFINITION_THROTTLE_2
};

// client data request IDs
enum DATA_REQUEST_ID {
	REQUEST_THROTTLE_1,
	REQUEST_THROTTLE_2,
	REQUEST_AIR_PATH,
	REQUEST_PMDG_777_DATA	// used for controlling speed brake
};

/* simulation variables for SimConnect_AddToDataDefinition() */

// engine throttle lever position (double) [0, 100]
static const char* SIM_VAR_ENG_THROTTLE_LEVER_POS[THROTTLE_NUM] = {
	"GENERAL ENG THROTTLE LEVER POSITION:1",
	"GENERAL ENG THROTTLE LEVER POSITION:2"
};

// all data used between SCThread and P3D
struct ThrottleQuadrantData 
{
	double throttle_level[THROTTLE_NUM] = { 0 };	// 0.0 - 100.0
	int speed_brake = -16383;		// -16383 to 16383
	bool button_status[BUTTON_NUM] = { false };
	bool is_AT_engaged = false;
	bool reverse_thrust = false;
};

// global variables
static ThrottleQuadrantData tc;
static bool sim_paused = true;
static bool sim_start = false;
static bool aircraft_loaded = false;	// PMDG specific flag

#define sim_running ((!sim_paused) && sim_start && aircraft_loaded)

// copy over data from shared struct if AT disengaged;
// copy data to shared struct if AT engaged.
static void syncDataWithSharedStruct(ThrottleQuadrantData& tc, volatile SharedStruct& st) {
	// always forward button status from st to tc
	for (unsigned int i = 0; i < BUTTON_NUM; i++)
		tc.button_status[i] = st.button_status[i];

	// always forward AT status from tc to st
	st.is_AT_engaged = tc.is_AT_engaged;
	LogV("SCThread: AT_engaged: %u\n", tc.is_AT_engaged);

	// always forward speed brake lever position from st to tc [0,100] -> [-16383,16383]
	tc.speed_brake = -16383 + (int)st.speed_brake * (16383 * 2) / 100;

	if (tc.is_AT_engaged) {		// st <- tc
		for (unsigned int i = 0; i < THROTTLE_NUM; i++)
			st.throttle_level[i] = tc.throttle_level[i];
		//st.speed_brake = tc.speed_brake * 100.0 / ((unsigned int)0x03FFF);
		LogV("SCThread: Sync to device\n");
	} else {	// tc <- st
		for (unsigned int i = 0; i < THROTTLE_NUM; i++)
			tc.throttle_level[i] = st.throttle_level[i];
		//tc.speed_brake = (unsigned int)(st.speed_brake * ((unsigned int)0x03FFF) / 100);
		LogV("SCThread: Sync from device\n");
	}
}

// set speed brake and throttle request data frequency
static HRESULT setRequestLeverFrequency(const SIMCONNECT_PERIOD period) {
	HRESULT hr;

	// throttle 1
	hr = SimConnect_RequestDataOnSimObject(hSimConnect,
		REQUEST_THROTTLE_1,
		DEFINITION_THROTTLE_1,
		SIMCONNECT_OBJECT_ID_USER,
		period,
		SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);

	// throttle 2
	hr = SimConnect_RequestDataOnSimObject(hSimConnect,
		REQUEST_THROTTLE_2,
		DEFINITION_THROTTLE_2,
		SIMCONNECT_OBJECT_ID_USER,
		period,
		SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);

	return hr;
}

// set A/T and TOGA status request data frequency
static HRESULT setRequestATFrequency(const SIMCONNECT_PERIOD period) {
	HRESULT hr = S_OK;

	// not needed

	return hr;
}

// set request all data frequency
static HRESULT setRequestDataFromAircraft(const SIMCONNECT_PERIOD period) {
	HRESULT hr;

	hr = setRequestLeverFrequency(period);
	hr = setRequestATFrequency(period);

	return hr;
}

// send data to p3d if AT disengaged
static HRESULT setDataOnAircraft() {
	HRESULT hr = NULL;
	
	// toga button
	if (tc.button_status[BUTTON_TOGA] && tc.is_AT_engaged == false) {
		hr = SimConnect_TransmitClientEvent(hSimConnect, 
			SIMCONNECT_OBJECT_ID_USER,
			EVENT_TOGGLE_TOGA, 
			1, 
			SIMCONNECT_GROUP_PRIORITY_HIGHEST, 
			SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
		hr = setRequestLeverFrequency(SIMCONNECT_PERIOD_SIM_FRAME);
		Log("SCThread: TOGA Button.\n");
	}

	// A/T disengage button
	if (tc.button_status[BUTTON_AT_DISENGAGE] && tc.is_AT_engaged == true) {
		// trigger button input by simulating mouse click
		hr = SimConnect_TransmitClientEvent(hSimConnect, 
			SIMCONNECT_OBJECT_ID_USER,
			EVENT_AT_DISENGAGE_1,
			MOUSE_FLAG_LEFTSINGLE,
			SIMCONNECT_GROUP_PRIORITY_HIGHEST, 
			SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
		hr = SimConnect_TransmitClientEvent(hSimConnect,
			SIMCONNECT_OBJECT_ID_USER,
			EVENT_AT_DISENGAGE_1,
			MOUSE_FLAG_LEFTRELEASE,
			SIMCONNECT_GROUP_PRIORITY_HIGHEST,
			SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
		hr = setRequestLeverFrequency(SIMCONNECT_PERIOD_NEVER);
		Log("SCThread: A/T Disengage Button.\n");
	}
	
	// speed brake
	hr = SimConnect_TransmitClientEvent(hSimConnect,
		SIMCONNECT_OBJECT_ID_USER,
		EVENT_SET_SPEED_BRAKE,
		tc.speed_brake,
		SIMCONNECT_GROUP_PRIORITY_HIGHEST,
		SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

	LogV("SCThread: Set Spolier to: %u\n", tc.speed_brake);

	// do not send lever data if A/T engaged
	if (tc.is_AT_engaged)
		if (hr == NULL)
			return S_OK;
		else
			return hr;

	// throttle 1
	hr = SimConnect_SetDataOnSimObject(hSimConnect,
		DEFINITION_THROTTLE_1,
		SIMCONNECT_OBJECT_ID_USER,
		0,
		0,
		sizeof(tc.throttle_level[0]),
		&(tc.throttle_level[0]));

	LogV("SCThread: Set Throttle 0 to: %2.1f\n", tc.throttle_level[0]);

	// throttle 2
	hr = SimConnect_SetDataOnSimObject(hSimConnect,
		DEFINITION_THROTTLE_2,
		SIMCONNECT_OBJECT_ID_USER,
		0,
		0,
		sizeof(tc.throttle_level[1]),
		&(tc.throttle_level[1]));

	LogV("SCThread: Set Throttle 1 to: %2.1f\n", tc.throttle_level[1]);

	return hr;
}

// set up all client events in SimConnect
static HRESULT initClientEvents() {
	HRESULT hr;
	
	// map events
	hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_TOGGLE_TOGA, EVENT_NAME_TOGGLE_TOGA);
	hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_AT_DISENGAGE_1, EVENT_NAME_AT_DISENGAGE[0]);
	hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_AT_DISENGAGE_2, EVENT_NAME_AT_DISENGAGE[1]);
	hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_REV_THRUST_1, EVENT_NAME_REV_THRUST[0]);
	hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_REV_THRUST_2, EVENT_NAME_REV_THRUST[1]);
	hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_FWD_THRUST_1, EVENT_NAME_FWD_THRUST[0]);
	hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_FWD_THRUST_2, EVENT_NAME_FWD_THRUST[1]);
	hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_SET_SPEED_BRAKE, EVENT_NAME_SET_SPEED_BRAKE);

	// Sign up for notifications
	hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_BUTTONS, EVENT_TOGGLE_TOGA, false);	
	hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_BUTTONS, EVENT_AT_DISENGAGE_1, false);
	hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_BUTTONS, EVENT_AT_DISENGAGE_2, false);
	hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_ENG, EVENT_REV_THRUST_1, false);
	hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_ENG, EVENT_REV_THRUST_2, false);
	hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_ENG, EVENT_FWD_THRUST_1, false);
	hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_ENG, EVENT_FWD_THRUST_2, false);
	hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_BUTTONS, EVENT_SET_SPEED_BRAKE, false);

	// Set a high priority for the group
	hr = SimConnect_SetNotificationGroupPriority(hSimConnect, GROUP_BUTTONS, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
	//hr = SimConnect_SetNotificationGroupPriority(hSimConnect, GROUP_AT, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
	hr = SimConnect_SetNotificationGroupPriority(hSimConnect, GROUP_ENG, SIMCONNECT_GROUP_PRIORITY_HIGHEST);

	return hr;
}

// set up all data definitions in SimConnect
static HRESULT initDataDefinitions() {
	HRESULT hr;

	// throttle control 1
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_THROTTLE_1,
		SIM_VAR_ENG_THROTTLE_LEVER_POS[THROTTLE_LEFT], "percent");

	// throttle control 2
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_THROTTLE_2,
		SIM_VAR_ENG_THROTTLE_LEVER_POS[THROTTLE_RIGHT], "percent");

	// PMDG 777 specific
	hr = SimConnect_MapClientDataNameToID(hSimConnect, PMDG_777X_DATA_NAME, PMDG_777X_DATA_ID);
	hr = SimConnect_AddToClientDataDefinition(hSimConnect, PMDG_777X_DATA_DEFINITION, 0, sizeof(PMDG_777X_Data), 0, 0);
	hr = SimConnect_RequestClientData(hSimConnect, PMDG_777X_DATA_ID, REQUEST_PMDG_777_DATA, PMDG_777X_DATA_DEFINITION,
		SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0);

	return hr;
}

static void CALLBACK MyDispatchProcTC(SIMCONNECT_RECV* pData, DWORD cbData, void *pContext) {
    HRESULT hr;
    
    switch(pData->dwID)
    {
        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
        {
            SIMCONNECT_RECV_SIMOBJECT_DATA *pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*) pData;
            
            switch(pObjData->dwRequestID) {
                case REQUEST_THROTTLE_1:
                {
					if (tc.is_AT_engaged) {
						tc.throttle_level[0] = *((double*)&pObjData->dwData);
						LogV("SCThread: REQUEST_THROTTLE_1 received, throttle = %2.1f\n", *((double*)&pObjData->dwData));
					}
					break;
                }
				
				case REQUEST_THROTTLE_2:
				{
					if (tc.is_AT_engaged) {
						tc.throttle_level[1] = *((double*)&pObjData->dwData);
						LogV("SCThread: REQUEST_THROTTLE_2 received, throttle = %2.1f\n", *((double*)&pObjData->dwData));
					}
					break;
				}

                default:
                   break;
            }
            break;
        }

		case SIMCONNECT_RECV_ID_CLIENT_DATA:
		{
			SIMCONNECT_RECV_CLIENT_DATA* pObjData = (SIMCONNECT_RECV_CLIENT_DATA*)pData;
			
			switch (pObjData->dwRequestID) {
				case REQUEST_PMDG_777_DATA:
				{
					PMDG_777X_Data* pS = (PMDG_777X_Data*)&pObjData->dwData;
					//if (tc.reverse_thrust)
					//	tc.speed_brake = pS->FCTL_Speedbrake_Lever;
					
					if (pS->MCP_AT_Sw_Pushed)
						tc.is_AT_engaged = true;

					if (pS->MCP_annunAT)
						tc.is_AT_engaged = true;
					else
						tc.is_AT_engaged = false;

					LogV("SCThread: AT: %d", tc.is_AT_engaged);

					break;
				}

				default:
					break;
			}
			break;
		}

        case SIMCONNECT_RECV_ID_EVENT:
        {
            SIMCONNECT_RECV_EVENT *evt = (SIMCONNECT_RECV_EVENT*)pData;

            switch(evt->uEventID) {
		        case EVENT_SIM:
                {
					if (evt->dwData) {
						hr = setRequestATFrequency(SIMCONNECT_PERIOD_SIM_FRAME);
						hr = setRequestLeverFrequency(SIMCONNECT_PERIOD_ONCE);
						hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_AIR_PATH, EVENT_NAME_AIRCRAFT_LOADED);
						sim_start = true;
						Log("SCThread: Sim Starts.\n");
					} else {
						sim_start = false;
						Log("SCThread: Sim Stops.\n");
					}
					break;
				}
			    
				case EVENT_PAUSE:
				{
					if (evt->dwData) {
						sim_paused = true;
						Log("SCThread: Simulation Paused.\n");
					} else {
						sim_paused = false;
						Log("SCThread: Simulation Resumed.\n");
					}
					break;
				}
				
				case EVENT_TOGGLE_TOGA:
				{
					tc.is_AT_engaged = true;
					Log("SCThread: EVENT_TOGGLE_TOGA\n");
					break;
				}

				//case EVENT_AUTO_THROTTLE_ARM:
				//{
				//	tc.is_AT_engaged = evt->dwData;
				//	Log("SCThread: EVENT_AUTO_THROTTLE_ARM: %u\n", evt->dwData);
				//	break;
				//}

				case EVENT_REV_THRUST_1:
				case EVENT_REV_THRUST_2:
				{
					tc.reverse_thrust = true;
					Log("SCThread: Reverse Thrust Active.\n");
					break;
				}

				case EVENT_FWD_THRUST_1:
				case EVENT_FWD_THRUST_2:
				{
					tc.reverse_thrust = false;
					Log("SCThread: Forward Thrust.\n");
					break;
				}

				case EVENT_AT_DISENGAGE_1:
				case EVENT_AT_DISENGAGE_2:
				{
					tc.is_AT_engaged = false;
					Log("SCThread: EVENT_AT_DISENGAGE_X.\n");
					break;
				}

                default:
                    break;
            }
            break;
        }

		case SIMCONNECT_RECV_ID_SYSTEM_STATE: // Track aircraft changes
		{
			SIMCONNECT_RECV_SYSTEM_STATE *evt = (SIMCONNECT_RECV_SYSTEM_STATE*)pData;
			if (evt->dwRequestID == REQUEST_AIR_PATH)
			{
				if (strstr(evt->szString, "PMDG 777") != NULL) {
					aircraft_loaded = true;
					Log("SCThread: Aircraft Loaded.\n");
				} else {
					aircraft_loaded = false;
					//Err("SCThread: Aircraft is not PMDG 777: Name=%s\n", evt->szString);
				}
			}
			break;
		}

        case SIMCONNECT_RECV_ID_QUIT:
        {
            quit = true;
			Log("SCThread: Quit = 1\n");
            break;
        }

		case SIMCONNECT_RECV_ID_EXCEPTION:
		{
			SIMCONNECT_RECV_EXCEPTION* except = (SIMCONNECT_RECV_EXCEPTION*)pData;
			Err("SCThread: Exception ID: %d\n", except->dwException);
			break;
		}

        default:
            LogV("SCThread: Received:%d\n", pData->dwID);
            break;
    }
}

static void ThrottleControl(volatile SharedStruct& sharedst) {
    HRESULT hr;
	
	if (SUCCEEDED(SimConnect_Open(&hSimConnect, "Throttle Control", NULL, 0, 0, 0)))
	{
		Log("\nSCThread: Connected to Prepar3D!\n");

		// set up all data definitions in SimConnect
		hr = initDataDefinitions();

		// Request system events/states
		hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM, "Sim");
		hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_PAUSE, "Pause");
		//hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_AIR_PATH, EVENT_NAME_AIRCRAFT_LOADED);

		// init button events
		hr = initClientEvents();

		// hook keyboard events; only used for debug purposes
		//hr = setupKeyboardEvents();

		Log("SCThread: Done SimConnect Thread Initialization!\n");

        while(quit == false) {
			if (sim_running) {
				syncDataWithSharedStruct(tc, sharedst);
				setDataOnAircraft();
			}
			SimConnect_CallDispatch(hSimConnect, MyDispatchProcTC, NULL);
            Sleep(1);
		} 

        hr = SimConnect_Close(hSimConnect);
	} else {
		Err("\nSCThread: Error on SimConnect_Open().\n");
	}
}

unsigned int __stdcall SCThread(void* data) {
	volatile SharedStruct& sharedst = *((SharedStruct*) data);

	ThrottleControl(sharedst);

	sharedst.quit = true;

	Log("SCThread: Quit.\n");
	return 0;
}