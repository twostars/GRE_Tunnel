#pragma once

class GRE
{
	SOCKET s{};
	sockaddr_in RecvAddr{};
	char* sendBuf;

public:
	GRE(const char* _server_ip, const char* _bind_ip, uint32_t mtu);
	[[noreturn]] void receiver(HMODULE wintun);
	void sender(char* packet, unsigned int size);
};
