#define MINECRAFTRCON_EXPORTS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include "RCON_MC.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <winsock2.h>
#include <windows.h>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

class MinecraftRconImpl {
private:
    SOCKET sock;
    int requestId;
    bool authenticated;
    bool connected;
    std::string lastError;

    // UTF-8 转 GBK
    std::string UTF8ToGBK(const std::string& utf8Str) {
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
        if (wideSize == 0) return utf8Str;

        std::wstring wideStr(wideSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wideStr[0], wideSize);

        int gbkSize = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (gbkSize == 0) return utf8Str;

        std::string gbkStr(gbkSize, 0);
        WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &gbkStr[0], gbkSize, nullptr, nullptr);

        return gbkStr;
    }

    // GBK 转 UTF-8
    std::string GBKToUTF8(const std::string& gbkStr) {
        int wideSize = MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, nullptr, 0);
        if (wideSize == 0) return gbkStr;

        std::wstring wideStr(wideSize, 0);
        MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, &wideStr[0], wideSize);

        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size == 0) return gbkStr;

        std::string utf8Str(utf8Size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &utf8Str[0], utf8Size, nullptr, nullptr);

        return utf8Str;
    }

    std::vector<char> buildPacket(int type, const std::string& payload) {
        int packetSize = 14 + static_cast<int>(payload.size());
        std::vector<char> packet(packetSize, 0);

        *reinterpret_cast<int32_t*>(packet.data()) = packetSize - 4;
        *reinterpret_cast<int32_t*>(packet.data() + 4) = requestId;
        *reinterpret_cast<int32_t*>(packet.data() + 8) = type;

        if (!payload.empty()) {
            memcpy(packet.data() + 12, payload.c_str(), payload.size());
        }

        return packet;
    }

    bool sendPacket(const std::vector<char>& packet) {
        return send(sock, packet.data(), static_cast<int>(packet.size()), 0) != SOCKET_ERROR;
    }

    std::vector<char> receivePacket() {
        int32_t size;
        if (recv(sock, reinterpret_cast<char*>(&size), 4, 0) != 4) {
            lastError = "Failed to receive packet size";
            return {};
        }

        std::vector<char> packet(size + 4);
        memcpy(packet.data(), &size, 4);

        int totalReceived = 4;
        while (totalReceived < size + 4) {
            int bytesReceived = recv(sock, packet.data() + totalReceived, size + 4 - totalReceived, 0);
            if (bytesReceived <= 0) {
                lastError = "Failed to receive complete packet";
                return {};
            }
            totalReceived += bytesReceived;
        }

        return packet;
    }

    bool parsePacket(const std::vector<char>& packet, int32_t& outId, int32_t& outType, std::string& outPayload) {
        if (packet.size() < 12) {
            lastError = "Packet too small";
            return false;
        }

        outId = *reinterpret_cast<const int32_t*>(packet.data() + 4);
        outType = *reinterpret_cast<const int32_t*>(packet.data() + 8);

        if (packet.size() > 12) {
            outPayload.assign(packet.data() + 12, packet.size() - 14);
        }
        else {
            outPayload.clear();
        }

        return true;
    }

public:
    MinecraftRconImpl() : sock(INVALID_SOCKET), requestId(1), authenticated(false), connected(false) {}
    ~MinecraftRconImpl() { disconnect(); }

    bool connect(const std::string& host, int port) {
        lastError.clear();

        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            lastError = "WSAStartup failed";
            return false;
        }

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            lastError = "Socket creation failed: " + std::to_string(WSAGetLastError());
            return false;
        }

        int timeout = 5000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = inet_addr(host.c_str());

        if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
            lastError = "Invalid address: " + host;
            closesocket(sock);
            return false;
        }

        if (::connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            lastError = "Connection failed: " + std::to_string(WSAGetLastError());
            closesocket(sock);
            return false;
        }

        connected = true;
        return true;
    }

    void disconnect() {
        if (connected) {
            closesocket(sock);
            WSACleanup();
            connected = false;
            authenticated = false;
        }
    }

    bool authenticate(const std::string& password) {
        lastError.clear();
        if (!connected) {
            lastError = "Not connected to server";
            return false;
        }

        auto authPacket = buildPacket(3, password);
        if (!sendPacket(authPacket)) {
            lastError = "Failed to send authentication packet";
            return false;
        }

        auto response = receivePacket();
        if (response.empty()) {
            lastError = "No authentication response";
            return false;
        }

        int32_t responseId, responseType;
        std::string responsePayload;
        if (!parsePacket(response, responseId, responseType, responsePayload)) {
            lastError = "Failed to parse authentication response";
            return false;
        }

        if (responseId == requestId) {
            authenticated = true;
            requestId++;
            return true;
        }
        else if (responseId == -1) {
            lastError = "Authentication failed: Invalid password";
        }
        else {
            lastError = "Authentication failed: Unexpected response ID " + std::to_string(responseId);
        }

        return false;
    }

    std::string executeCommand(const std::string& command, bool isChinese) {
        lastError.clear();
        if (!connected || !authenticated) {
            lastError = "Not connected or authenticated";
            return "";
        }

        std::string processedCommand = command;
        if (isChinese) {
            processedCommand = GBKToUTF8(command);
        }

        auto commandPacket = buildPacket(2, processedCommand);
        if (!sendPacket(commandPacket)) {
            lastError = "Send failed";
            return "";
        }

        auto response = receivePacket();
        if (response.empty()) {
            lastError = "No response from server";
            return "";
        }

        int32_t responseId, responseType;
        std::string responsePayload;
        if (!parsePacket(response, responseId, responseType, responsePayload)) {
            lastError = "Parse failed";
            return "";
        }

        if (responseType == 0) {
            requestId++;
            try {
                return UTF8ToGBK(responsePayload);
            }
            catch (...) {
                return responsePayload;
            }
        }

        lastError = "Unexpected response type: " + std::to_string(responseType);
        return "";
    }

    bool sendChineseMessage(const std::string& message) {
        std::string command = "say " + message;
        std::string response = executeCommand(command, true);
        return !response.empty();
    }

    bool isConnected() const { return connected; }
    bool isAuthenticated() const { return authenticated; }
    std::string getLastError() const { return lastError; }
};

// C接口实现
extern "C" {

    MINECRAFTRCON_API HRCON RconCreate() {
        return new MinecraftRconImpl();
    }

    MINECRAFTRCON_API void RconDestroy(HRCON hRcon) {
        if (hRcon) {
            delete static_cast<MinecraftRconImpl*>(hRcon);
        }
    }

    MINECRAFTRCON_API bool RconConnect(HRCON hRcon, const char* host, int port) {
        if (!hRcon) return false;
        return static_cast<MinecraftRconImpl*>(hRcon)->connect(host, port);
    }

    MINECRAFTRCON_API void RconDisconnect(HRCON hRcon) {
        if (hRcon) {
            static_cast<MinecraftRconImpl*>(hRcon)->disconnect();
        }
    }

    MINECRAFTRCON_API bool RconAuthenticate(HRCON hRcon, const char* password) {
        if (!hRcon) return false;
        return static_cast<MinecraftRconImpl*>(hRcon)->authenticate(password);
    }

    MINECRAFTRCON_API const char* RconExecuteCommand(HRCON hRcon, const char* command, bool isChinese) {
        if (!hRcon) return nullptr;
        std::string result = static_cast<MinecraftRconImpl*>(hRcon)->executeCommand(command, isChinese);
        if (result.empty()) return nullptr;

        char* cstr = new char[result.size() + 1];
        strcpy(cstr, result.c_str());
        return cstr;
    }

    MINECRAFTRCON_API bool RconSendChineseMessage(HRCON hRcon, const char* message) {
        if (!hRcon) return false;
        return static_cast<MinecraftRconImpl*>(hRcon)->sendChineseMessage(message);
    }

    MINECRAFTRCON_API bool RconIsConnected(HRCON hRcon) {
        if (!hRcon) return false;
        return static_cast<MinecraftRconImpl*>(hRcon)->isConnected();
    }

    MINECRAFTRCON_API bool RconIsAuthenticated(HRCON hRcon) {
        if (!hRcon) return false;
        return static_cast<MinecraftRconImpl*>(hRcon)->isAuthenticated();
    }

    MINECRAFTRCON_API const char* RconGetLastError(HRCON hRcon) {
        if (!hRcon) return "Invalid handle";
        std::string error = static_cast<MinecraftRconImpl*>(hRcon)->getLastError();
        if (error.empty()) return nullptr;

        char* cstr = new char[error.size() + 1];
        strcpy(cstr, error.c_str());
        return cstr;
    }

    MINECRAFTRCON_API void RconFreeString(const char* str) {
        if (str) {
            delete[] str;
        }
    }
}