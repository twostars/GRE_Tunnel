#include "pch.h"
#include "wintun_helper.h"

WINTUN_CREATE_ADAPTER_FUNC* WintunCreateAdapter = nullptr;
WINTUN_CLOSE_ADAPTER_FUNC* WintunCloseAdapter = nullptr;
WINTUN_OPEN_ADAPTER_FUNC* WintunOpenAdapter = nullptr;
WINTUN_GET_ADAPTER_LUID_FUNC* WintunGetAdapterLUID = nullptr;
WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC* WintunGetRunningDriverVersion = nullptr;
WINTUN_DELETE_DRIVER_FUNC* WintunDeleteDriver = nullptr;
WINTUN_SET_LOGGER_FUNC* WintunSetLogger = nullptr;
WINTUN_START_SESSION_FUNC* WintunStartSession = nullptr;
WINTUN_END_SESSION_FUNC* WintunEndSession = nullptr;
WINTUN_GET_READ_WAIT_EVENT_FUNC* WintunGetReadWaitEvent = nullptr;
WINTUN_RECEIVE_PACKET_FUNC* WintunReceivePacket = nullptr;
WINTUN_RELEASE_RECEIVE_PACKET_FUNC* WintunReleaseReceivePacket = nullptr;
WINTUN_ALLOCATE_SEND_PACKET_FUNC* WintunAllocateSendPacket = nullptr;
WINTUN_SEND_PACKET_FUNC* WintunSendPacket = nullptr;

HMODULE wt_LoadModule()
{
	return LoadLibraryExW(L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
}

HMODULE InitializeWintun(HMODULE _Wintun)
{
	HMODULE Wintun = _Wintun;
	if (!Wintun)
		return NULL;
#define X(Name) ((*(FARPROC *)&Name = GetProcAddress(Wintun, #Name)) == NULL)
	if (X(WintunCreateAdapter) || X(WintunCloseAdapter) || X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
		X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) || X(WintunSetLogger) || X(WintunStartSession) ||
		X(WintunEndSession) || X(WintunGetReadWaitEvent) || X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
		X(WintunAllocateSendPacket) || X(WintunSendPacket))
#undef X
	{
		DWORD LastError = GetLastError();
		FreeLibrary(Wintun);
		SetLastError(LastError);
		return NULL;
	}
	return Wintun;
}

// This function use new, free the memory afterwards
// It converts to a const char* to wchar_t* for use in the Wintun adapter name
const wchar_t* GetWC(const char* c)
{
	const size_t cSize = strlen(c) + 1;
	wchar_t* wc = new wchar_t[cSize];

	size_t outS;
	mbstowcs_s(&outS, wc, cSize, c, cSize - 1);

	return wc;
}

bool validateIpAddress(const char* ipAddress)
{
	struct sockaddr_in sa;
	int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
	return result != 0;
}
