#include <windows.h>
#include <process.h>
#include <tchar.h>
#include <stdio.h>

#include "ThrottleControl.h"
#include "DeviceControl.h"
#include "ASDFProtocol.h"
#include "SharedStruct.h"
#include "debug.h"

SharedStruct sharedst;

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	HANDLE myHandle[2];	// 0 is SCThread, 1 is TQThread
	myHandle[0] = (HANDLE) _beginthreadex(0, 0, SCThread, &sharedst, 0, 0);
	myHandle[1] = (HANDLE) _beginthreadex(0, 0, TQThread, &sharedst, 0, 0);

	Log("HostAddOn Main Thread: TQThread and SCThread start.\n");
	WaitForMultipleObjects(2, myHandle, true, INFINITE);

	CloseHandle(myHandle[0]);
	CloseHandle(myHandle[1]);

	Log("HostAddOn Main Thread: TQThread and SCThread quit.\n");
	system("pause");

	return 0;
}
