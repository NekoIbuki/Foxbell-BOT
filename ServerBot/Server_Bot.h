#pragma once
#define WIN32_LEAN_AND_MEAN
#ifndef SERVER_BOT_H
#define SERVER_BOT_H

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>

#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>
#include <queue>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <random>
#include <iomanip>
#include <functional>

#include "RCON_MC.h"


#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "RCON_Link.lib")

// ȫ�ֱ�������
extern const std::string HIDE_PREFIX;
extern std::atomic<bool> g_running;
extern std::queue<std::string> g_messageQueue;
extern std::mutex g_queueMutex;
extern std::atomic<bool> g_napcatRunning;
extern std::string g_targetGroupId;
extern HANDLE g_napcatProcess;
extern HANDLE g_napcatOutputRead;
extern std::thread g_napcatMonitorThread;

extern std::string LogPath;
extern std::string OpsPath;
extern const std::string CONFIG_FILE;
extern const std::string BIND_DATA_FILE;

// RCON���
extern HRCON g_hRcon;
extern std::mutex g_rconMutex;
extern std::string g_rconHost;
extern int g_rconPort;
extern std::string g_rconPassword;
extern std::string g_botQQ;

// ����̨��ɫö��
enum ConsoleColor {
    BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3, RED = 4,
    MAGENTA = 5, YELLOW = 6, WHITE = 7, GRAY = 8,
    BRIGHT_BLUE = 9, BRIGHT_GREEN = 10, BRIGHT_CYAN = 11,
    BRIGHT_RED = 12, BRIGHT_MAGENTA = 13, BRIGHT_YELLOW = 14, BRIGHT_WHITE = 15
};

// ��֤��ṹ
struct VerificationCode {
    std::string code;
    std::string mcName;
    std::chrono::steady_clock::time_point createTime;

    // Ĭ�Ϲ��캯��
    VerificationCode() : createTime(std::chrono::steady_clock::now()) {}

    // �������캯��
    VerificationCode(const VerificationCode& other)
        : code(other.code), mcName(other.mcName), createTime(other.createTime) {
    }

    // ��ֵ�����
    VerificationCode& operator=(const VerificationCode& other) {
        if (this != &other) {
            code = other.code;
            mcName = other.mcName;
            createTime = other.createTime;
        }
        return *this;
    }
};

// ����Ϣ�ṹ
struct BindInfo {
    std::string qqId;
    std::string mcName;
    std::string passwordHash;
    std::chrono::steady_clock::time_point bindTime;

    // Ĭ�Ϲ��캯��
    BindInfo() : bindTime(std::chrono::steady_clock::now()) {}

    // �������캯��
    BindInfo(const BindInfo& other)
        : qqId(other.qqId), mcName(other.mcName),
        passwordHash(other.passwordHash), bindTime(other.bindTime) {
    }

    // ��ֵ�����
    BindInfo& operator=(const BindInfo& other) {
        if (this != &other) {
            qqId = other.qqId;
            mcName = other.mcName;
            passwordHash = other.passwordHash;
            bindTime = other.bindTime;
        }
        return *this;
    }

    // ���л�Ϊ�ַ���
    std::string toString() const {
        auto duration = bindTime.time_since_epoch();
        auto count = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        std::stringstream ss;
        ss << qqId << "|" << mcName << "|" << passwordHash << "|" << count;
        return ss.str();
    }

    // ���ַ�������
    static BindInfo fromString(const std::string& str) {
        BindInfo info;
        std::stringstream ss(str);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(ss, token, '|')) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 4) {
            info.qqId = tokens[0];
            info.mcName = tokens[1];
            info.passwordHash = tokens[2];
            try {
                long long count = std::stoll(tokens[3]);
                auto duration = std::chrono::seconds(count);
                info.bindTime = std::chrono::steady_clock::time_point(duration);
            }
            catch (...) {
                info.bindTime = std::chrono::steady_clock::now();
            }
        }
        return info;
    }
};

// OPȨ�޽ṹ
struct OpInfo {
    std::string uuid;
    std::string name;
    int level;
    bool bypassesPlayerLimit;
};

// ȫ�ֱ�������
extern const std::string HIDE_PREFIX;
extern std::atomic<bool> g_running;
extern std::queue<std::string> g_messageQueue;
extern std::mutex g_queueMutex;
extern std::atomic<bool> g_napcatRunning;
extern std::string g_targetGroupId;
extern HANDLE g_napcatProcess;
extern HANDLE g_napcatOutputRead;
extern std::thread g_napcatMonitorThread;

extern std::string LogPath;
extern std::string OpsPath;
extern const std::string CONFIG_FILE;
extern const std::string BIND_DATA_FILE;

// RCON���
extern HRCON g_hRcon;
extern std::mutex g_rconMutex;
extern std::string g_rconHost;
extern int g_rconPort;
extern std::string g_rconPassword;
extern std::string g_botQQ;

// ��¼�����
extern std::unordered_map<std::string, VerificationCode> g_verificationCodes;
extern std::unordered_map<std::string, BindInfo> g_bindInfos;
extern std::unordered_map<std::string, std::string> g_tempPasswordHashes;
extern std::mutex g_bindMutex;
extern std::mutex g_tempPasswordMutex;
extern const int VERIFICATION_CODE_EXPIRE_MINUTES;

// OP���
extern std::unordered_map<std::string, int> g_opList;
extern std::mutex g_opMutex;

// ��������
// ����̨���
void setConsoleColor(ConsoleColor color);
void resetConsoleColor();
void smartPrint(const std::string& message, const std::string& source = "program", ConsoleColor color = WHITE);

// ���봦��
bool isUtf8(const std::string& str);
std::string utf8ToGbk(const std::string& utf8Str);

// ���ݴ洢���
void saveBindDataToFile();
void loadBindDataFromFile();
std::streampos loadFilePosition(const std::string& logPath);
void saveFilePosition(const std::string& logPath, std::streampos position);

// ���ߺ���
std::string customHash(const std::string& input);
std::string generateVerificationCode(int length = 6);
bool isCodeExpired(const VerificationCode& vc);
void cleanupExpiredCodes();
std::string extractJsonValue(const std::string& json, const std::string& key);

// RCON����
bool initRcon();
bool sendRconCommand(const std::string& command);
bool sendRunRconCommand(const std::string& command);

// QQ��Ϣ����
bool sendPrivateMessage(const std::string& userId, const std::string& message);
bool sendGroupMessage(const std::string& groupId, const std::string& message);
void addMessageToQueue(const std::string& message);

// ��¼�󶨹���
void handleLoginCommand(const std::string& qqId, const std::string& message);
void handleValidationCommand(const std::string& qqId, const std::string& message);
void handleBindQueryCommand(const std::string& playerName);

// OPȨ�޹���
void loadOpList();
int getOpLevel(const std::string& playerName);
bool checkCommandPermission(int opLevel, const std::string& command);
std::string getCommandPermissionInfo(const std::string& command);
void handleCmdCommand(const std::string& senderId, const std::string& message, bool isPrivate = false);

// NapCat����
bool startNapCatProcess();
void processNapCatLine(const std::string& line);
void monitorNapCatOutput();
void shutdownNapCat();

// ��Ϣת������
void forwardToMinecraft(const std::string& senderName, const std::string& senderId, const std::string& message);
void monitorMinecraftLog(const std::string& logPath);
void messageSender(const std::string& groupId);

// ��Ϸ�����
void handleTeleportCommand(const std::string& playerName, const std::string& message);
void handleGameModeCommand(const std::string& playerName, const std::string& message);
void handleMCValidationCommand(const std::string& playerName, const std::string& message);

#endif