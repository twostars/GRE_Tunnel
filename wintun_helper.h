#pragma once

extern WINTUN_CREATE_ADAPTER_FUNC* WintunCreateAdapter;
extern WINTUN_CLOSE_ADAPTER_FUNC* WintunCloseAdapter;
extern WINTUN_OPEN_ADAPTER_FUNC* WintunOpenAdapter;
extern WINTUN_GET_ADAPTER_LUID_FUNC* WintunGetAdapterLUID;
extern WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC* WintunGetRunningDriverVersion;
extern WINTUN_DELETE_DRIVER_FUNC* WintunDeleteDriver;
extern WINTUN_SET_LOGGER_FUNC* WintunSetLogger;
extern WINTUN_START_SESSION_FUNC* WintunStartSession;
extern WINTUN_END_SESSION_FUNC* WintunEndSession;
extern WINTUN_GET_READ_WAIT_EVENT_FUNC* WintunGetReadWaitEvent;
extern WINTUN_RECEIVE_PACKET_FUNC* WintunReceivePacket;
extern WINTUN_RELEASE_RECEIVE_PACKET_FUNC* WintunReleaseReceivePacket;
extern WINTUN_ALLOCATE_SEND_PACKET_FUNC* WintunAllocateSendPacket;
extern WINTUN_SEND_PACKET_FUNC* WintunSendPacket;

HMODULE wt_LoadModule();
HMODULE InitializeWintun(HMODULE _Wintun);

std::wstring GetWC(const char* c);
bool validateIpAddress(const char* ipAddress);
