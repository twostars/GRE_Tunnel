#include "pch.h"
#include "wintun_helper.h"
#include "GRE.h"
#include "wintun.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

bool reset_adapter = false;
std::unique_ptr<GRE> gre;
WINTUN_ADAPTER_HANDLE Adapter;
WINTUN_SESSION_HANDLE Session;

void configure_logging() {
	el::Configurations conf;
	conf.setToDefault();

	conf.set(el::Level::Global, el::ConfigurationType::ToFile, "true");
	conf.set(el::Level::Global, el::ConfigurationType::Filename, "./gre.log");
	conf.setGlobally(el::ConfigurationType::Format, "%datetime{%d/%M/%Y %H:%m:%s} [%level] %msg");

	el::Loggers::reconfigureLogger("default", conf);
}

void make_minidump(EXCEPTION_POINTERS* e)
{
	auto hDbgHelp = LoadLibraryA("dbghelp");
	if (hDbgHelp == nullptr)
		return;
	auto pMiniDumpWriteDump = (decltype(&MiniDumpWriteDump))GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
	if (pMiniDumpWriteDump == nullptr)
		return;

	char name[MAX_PATH];
	{
		auto nameEnd = name + GetModuleFileNameA(GetModuleHandleA(0), name, MAX_PATH);
		SYSTEMTIME t;
		GetSystemTime(&t);
		wsprintfA(nameEnd - strlen(".exe"),
			"_%4d%02d%02d_%02d%02d%02d.dmp",
			t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	}

	auto hFile = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
	exceptionInfo.ThreadId = GetCurrentThreadId();
	exceptionInfo.ExceptionPointers = e;
	exceptionInfo.ClientPointers = FALSE;

	auto dumped = pMiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		hFile,
		MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
		e ? &exceptionInfo : nullptr,
		nullptr,
		nullptr);

	CloseHandle(hFile);
	return;
}

LONG CALLBACK unhandled_handler(EXCEPTION_POINTERS* e)
{
	make_minidump(e);
	return EXCEPTION_CONTINUE_SEARCH;
}

INITIALIZE_EASYLOGGINGPP
int main(int argc, char* argv[])
{
	SetUnhandledExceptionFilter(unhandled_handler);
	std::cout << "GRE Tunnel for Windows" << std::endl << std::endl;
	if (argc < 5 || argc > 9) {
		std::cerr << "At least 2 arguments must be given" << std::endl <<
			"Arguments: gre_tunnel.exe GRE_BIND_IP GRE_SERVER INTERFACE_IP GATEWAY_IP [CIDR (30)] [ADAPTER_NAME] [ADDITIONAL PUBLIC IP] [MTU]" << std::endl;
		return 0;
	}
	configure_logging();

	LOG(DEBUG) << "Starting GRE Application";

	const char* gre_bind_ip = argv[1];
	const char* server_ip = argv[2];

	const char* bind_ip = argv[3];
	const char* gateway_ip = argv[4];
	std::wstring adapter_name = L"GRE_Tunnel";
	int cidr = 30;
	const char* additional_ip = "";
	uint32_t mtu = 1476;

	// Default CIDR is 30
	if (argc >= 6) {
		cidr = atoi(argv[5]);
		if (cidr == 0) LOG(FATAL) << "CIDR " << cidr << " is invalid.";
	}

	if (argc >= 7)
		adapter_name = GetWC(argv[6]);

	if (argc >= 8)
		additional_ip = argv[7];

	if (argc >= 9)
		mtu = (uint32_t) atoi(argv[8]);

	std::cout
		<< "My IP on the GRE network: " << bind_ip << "/" << cidr << std::endl
		<< "GRE server IP           : " << server_ip << std::endl
		<< "GRE server IP (Gateway) : " << gateway_ip << std::endl
		<< "GRE binding IP          : " << gre_bind_ip << std::endl
		<< "Additional IP           : " << additional_ip << std::endl
		<< "MTU                     : " << mtu << std::endl;
	std::wcout
		<< L"Adapter name            : " << adapter_name << std::endl << std::endl;

	if (!validateIpAddress(bind_ip) || !validateIpAddress(gre_bind_ip) || !validateIpAddress(server_ip)
		|| !validateIpAddress(gateway_ip)) {
		LOG(FATAL) << "Failed to parse IPv4";
	}

	HMODULE MWintun = wt_LoadModule();
	if (!InitializeWintun(MWintun)) {
		LOG(FATAL) << "Failed to initialize WinTun: " << GetLastError() << std::endl << "Please make sure that wintun.dll exists";
		return 0;
	}

	Adapter = WintunCreateAdapter(adapter_name.c_str(), L"GRE_Tunnel", NULL);
	if (Adapter == nullptr)
	{
		LOG(FATAL) << "Failed to create Wintun adapter: " << GetLastError();
		return 0;
	}

	SetConsoleCtrlHandler(exit_handler, TRUE);

	MIB_UNICASTIPADDRESS_ROW AddressRow;
	InitializeUnicastIpAddressEntry(&AddressRow);
	WintunGetAdapterLUID(Adapter, &AddressRow.InterfaceLuid);
	AddressRow.Address.Ipv4.sin_family = AF_INET;
	inet_pton(AF_INET, bind_ip, &AddressRow.Address.Ipv4.sin_addr.S_un.S_addr);

	AddressRow.OnLinkPrefixLength = cidr;
	AddressRow.DadState = IpDadStatePreferred;
	auto LastError = CreateUnicastIpAddressEntry(&AddressRow);

	if (LastError != ERROR_SUCCESS && LastError != ERROR_OBJECT_ALREADY_EXISTS)
	{
		LOG(FATAL) << "Failed to assign IP: " << LastError;
		WintunCloseAdapter(Adapter);
		return 0;
	}

	Session = WintunStartSession(Adapter, WINTUN_MAX_RING_CAPACITY);
	if (Session == nullptr)
	{
		LOG(FATAL) << "Failed to create adapter";
		WintunCloseAdapter(Adapter);
		return 0;
	}

	char comm[256];
	sprintf_s(comm, 256, "netsh interface ip set address name=\"%ls\" static %s 255.255.255.252 %s 300", adapter_name.c_str(), bind_ip, gateway_ip);
	LOG(INFO) << comm;
	system(comm);
	memset(comm, 0, 256);
	sprintf_s(comm, 256, "netsh interface ip set subinterface \"%ls\" mtu=%u store=persistent", adapter_name.c_str(), mtu);
	LOG(INFO) << comm;
	system(comm);

	if (argc >= 8)
	{
		memset(comm, 0, 256);
		sprintf_s(comm, 256, "netsh interface ipv4 add address \"%ls\" %s 255.255.255.255", adapter_name.c_str(), additional_ip);
		LOG(INFO) << comm;
		system(comm);
	}

	gre = std::make_unique<GRE>(server_ip, gre_bind_ip, mtu);

	std::thread t1(wintun_receive_loop);
	gre->receiver(MWintun);
}
