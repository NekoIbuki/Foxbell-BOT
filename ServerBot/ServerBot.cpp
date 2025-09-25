#include "Server_Bot.h"

// ============================================================================
// 配置结构体
// ============================================================================

struct BotConfig {
    std::string targetGroupId = "Group";
    std::string botQQ = "Bot";
    std::string logPath = "\\latest.log";
    std::string opsPath = "\\ops.json";
    std::string rconHost = "127.0.0.1";
    int rconPort = 25575;
    std::string rconPassword = "Password";
    std::string napcatHost = "127.0.0.1";
    int napcatPort = 3000;

    // 保存配置到文件
    bool saveToFile(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        file << "target_group_id=" << targetGroupId << "\n";
        file << "bot_qq=" << botQQ << "\n";
        file << "log_path=" << logPath << "\n";
        file << "ops_path=" << opsPath << "\n";
        file << "rcon_host=" << rconHost << "\n";
        file << "rcon_port=" << rconPort << "\n";
        file << "rcon_password=" << rconPassword << "\n";
        file << "napcat_host=" << napcatHost << "\n";
        file << "napcat_port=" << napcatPort << "\n";

        file.close();
        return true;
    }

    // 从文件加载配置
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // 去除首尾空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (key == "targetGroupId") targetGroupId = value;
            else if (key == "botQQ") botQQ = value;
            else if (key == "logPath") logPath = value;
            else if (key == "opsPath") opsPath = value;
            else if (key == "rconHost") rconHost = value;
            else if (key == "rconPort") rconPort = std::stoi(value);
            else if (key == "rconPassword") rconPassword = value;
            else if (key == "napcatHost") napcatHost = value;
            else if (key == "napcatPort") napcatPort = std::stoi(value);
        }

        file.close();
        return true;
    }

    // 创建默认配置文件
    bool createDefaultConfig(const std::string& filename) {
        BotConfig defaultConfig;
        return defaultConfig.saveToFile(filename);
    }
};

// 全局配置对象
BotConfig g_config;


// 全局变量定义
const std::string HIDE_PREFIX = "hide:";
std::atomic<bool> g_running{ true };
std::queue<std::string> g_messageQueue;
std::mutex g_queueMutex;
std::atomic<bool> g_napcatRunning{ false };
std::string g_targetGroupId;  // 将在main函数中从配置初始化
HANDLE g_napcatProcess = nullptr;
HANDLE g_napcatOutputRead = nullptr;
std::thread g_napcatMonitorThread;

// 使用配置中的路径
std::string LogPath;  // 将在main函数中从配置初始化
std::string OpsPath;  // 将在main函数中从配置初始化
const std::string BIND_DATA_FILE = "mc_bind_data.txt";
const std::string POSITION_CONFIG_FILE = "mc_listener.cfg";
const std::string CONFIG_FILE = "fox_config.cfg";

// RCON相关
HRCON g_hRcon = nullptr;
std::mutex g_rconMutex;
std::string g_rconHost;      // 将从配置初始化
int g_rconPort;              // 将从配置初始化
std::string g_rconPassword;  // 将从配置初始化
std::string g_botQQ;         // 将从配置初始化

// 登录绑定相关
std::unordered_map<std::string, VerificationCode> g_verificationCodes;
std::unordered_map<std::string, BindInfo> g_bindInfos;
std::unordered_map<std::string, std::string> g_tempPasswordHashes;
std::mutex g_bindMutex;
std::mutex g_tempPasswordMutex;
const int VERIFICATION_CODE_EXPIRE_MINUTES = 8;

// OP相关
std::unordered_map<std::string, int> g_opList;
std::mutex g_opMutex;

// ============================================================================
// 配置文件功能函数
// ============================================================================

bool loadConfigFromFile() {
    std::ifstream configFile(CONFIG_FILE);
    if (!configFile.is_open()) {
        smartPrint("[Error] 无法打开配置文件: " + CONFIG_FILE + "\n", "program", RED);
        smartPrint("[Info] 将使用默认配置并创建配置文件\n", "program", YELLOW);
        return false;
    }

    std::string line;
    int lineNum = 0;

    while (std::getline(configFile, line)) {
        lineNum++;
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 清理行内容
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            smartPrint("[Error] 配置文件第 " + std::to_string(lineNum) + " 行格式错误: " + line + "\n", "program", YELLOW);
            continue;
        }

        std::string key = line.substr(0, equalsPos);
        std::string value = line.substr(equalsPos + 1);

        // 清理key和value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // 去除值的引号（如果有）
        if (value.length() >= 2 && value[0] == '"' && value[value.length() - 1] == '"') {
            value = value.substr(1, value.length() - 2);
        }

        // 根据key设置配置值
        if (key == "target_group_id") {
            g_config.targetGroupId = value;
        }
        else if (key == "bot_qq") {
            g_config.botQQ = value;
        }
        else if (key == "log_path") {
            g_config.logPath = value;
        }
        else if (key == "ops_path") {
            g_config.opsPath = value;
        }
        else if (key == "rcon_host") {
            g_config.rconHost = value;
        }
        else if (key == "rcon_port") {
            try {
                g_config.rconPort = std::stoi(value);
            }
            catch (...) {
                smartPrint("[Error] RCON端口格式错误: " + value + "\n", "program", RED);
            }
        }
        else if (key == "rcon_password") {
            g_config.rconPassword = value;
        }
        else if (key == "napcat_host") {
            g_config.napcatHost = value;
        }
        else if (key == "napcat_port") {
            try {
                g_config.napcatPort = std::stoi(value);
            }
            catch (...) {
                smartPrint("[Error] NapCat端口格式错误: " + value + "\n", "program", RED);
            }
        }
        else {
            smartPrint("[Error] 未知配置项: " + key + "\n", "program", YELLOW);
        }
    }

    configFile.close();
    smartPrint("[Info] 配置文件加载完成\n", "program", GREEN);
    return true;
}

void createDefaultConfig() {
    std::ofstream configFile(CONFIG_FILE);
    if (!configFile.is_open()) {
        smartPrint("[Error] 无法创建配置文件: " + CONFIG_FILE + "\n", "program", RED);
        return;
    }

    configFile << "# Minecraft服务器Bot配置文件\n";
    configFile << "# 修改后重启程序生效\n\n";

    configFile << "# 目标QQ群号\n";
    configFile << "target_group_id = \" \"\n\n";

    configFile << "# Bot的QQ号\n";
    configFile << "bot_qq = \" \"\n\n";

    configFile << "# 服务器日志文件路径\n";
    configFile << "log_path = \" \"\n\n";

    configFile << "# OP列表文件路径\n";
    configFile << "ops_path = \" \"\n\n";

    configFile << "# RCON连接设置\n";
    configFile << "rcon_host = \"127.0.0.1\"\n";
    configFile << "rcon_port = 25575\n";
    configFile << "rcon_password = \" \"\n\n";

    configFile << "# NapCat连接设置\n";
    configFile << "napcat_host = \"127.0.0.1\"\n";
    configFile << "napcat_port = 3000\n";

    configFile.close();
    smartPrint("[Info] 已创建默认配置文件: " + CONFIG_FILE + "\n", "program", GREEN);
}

void initializeConfig() {
    // 设置默认值
    g_config.targetGroupId = "Group";
    g_config.botQQ = "BOT";
    g_config.logPath = "\\latest.log";
    g_config.opsPath = "\\ops.json";
    g_config.rconHost = "127.0.0.1";
    g_config.rconPort = 25575;
    g_config.rconPassword = "Password";
    g_config.napcatHost = "127.0.0.1";
    g_config.napcatPort = 3000;

    // 尝试加载配置文件
    if (!loadConfigFromFile()) {
        // 如果加载失败，创建默认配置文件
        createDefaultConfig();
    }

    // 更新全局变量
    g_targetGroupId = g_config.targetGroupId;
    g_botQQ = g_config.botQQ;
}

// ============================================================================
// 控制台相关函数
// ============================================================================

void setConsoleColor(ConsoleColor color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void resetConsoleColor() {
    setConsoleColor(WHITE);
}

void smartPrint(const std::string& message, const std::string& source, ConsoleColor color) {
    if (message.empty()) return;

    // 添加长度检查，防止过长的字符串
    if (message.length() > 10000) {
        std::cout << "[ERROR] Message too long: " << message.length() << " characters\n";
        return;
    }

    std::string output = message;
    if ((source == "program" || source == "napcat") && isUtf8(message)) {
        output = utf8ToGbk(message);

        // 检查转换后的字符串
        if (output.length() > 10000) {
            std::cout << "[ERROR] Converted message too long\n";
            return;
        }
    }

    setConsoleColor(color);
    std::cout << output;
    resetConsoleColor();

    // 强制刷新输出缓冲区
    std::cout.flush();
}

// ============================================================================
// 编码处理函数
// ============================================================================

bool isUtf8(const std::string& str) {
    int i = 0;
    int len = str.length();
    while (i < len) {
        unsigned char c = str[i];
        if (c <= 0x7F) i++;
        else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= len || (str[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        }
        else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= len || (str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80) return false;
            i += 3;
        }
        else return false;
    }
    return true;
}

std::string utf8ToGbk(const std::string& utf8Str) {
    if (utf8Str.empty()) return "";

    // 第一步：UTF-8 转 WideChar（不使用-1长度，避免依赖null终止符）
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8Str.length(), nullptr, 0);
    if (wideSize == 0) return utf8Str;

    std::wstring wideStr(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8Str.length(), &wideStr[0], wideSize);

    // 第二步：WideChar 转 GBK（不使用-1长度）
    int gbkSize = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), wideSize, nullptr, 0, nullptr, nullptr);
    if (gbkSize == 0) return utf8Str;

    std::string gbkStr(gbkSize, 0);
    WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), wideSize, &gbkStr[0], gbkSize, nullptr, nullptr);

    return gbkStr;
}

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

// ============================================================================
// 数据存储相关函数
// ============================================================================

void saveBindDataToFile() {
    std::lock_guard<std::mutex> lock(g_bindMutex);

    std::ofstream file(BIND_DATA_FILE);
    if (!file.is_open()) {
        smartPrint("[Error] 无法打开绑定数据文件进行保存: " + BIND_DATA_FILE + "\n", "program", RED);
        return;
    }

    for (const auto& pair : g_bindInfos) {
        file << pair.second.toString() << "\n";
    }

    file.close();
    smartPrint("[Info] 绑定数据已保存到文件\n", "program", GREEN);
}

void loadBindDataFromFile() {
    std::lock_guard<std::mutex> lock(g_bindMutex);

    std::ifstream file(BIND_DATA_FILE);
    if (!file.is_open()) {
        smartPrint("[Warn] 绑定数据文件不存在，将创建新文件\n", "program", YELLOW);
        return;
    }

    g_bindInfos.clear();
    std::string line;
    int loadedCount = 0;

    while (std::getline(file, line)) {
        if (!line.empty()) {
            try {
                BindInfo info = BindInfo::fromString(line);
                if (!info.qqId.empty() && !info.mcName.empty()) {
                    g_bindInfos[info.mcName] = info;
                    loadedCount++;
                }
            }
            catch (const std::exception& e) {
                smartPrint("[Error] 解析绑定数据行失败: " + line + "\n", "program", RED);
            }
        }
    }

    file.close();
    smartPrint("[Info] 从文件加载了 " + std::to_string(loadedCount) + " 条绑定记录\n", "program", GREEN);
}

std::streampos loadFilePosition(const std::string& logPath) {
    std::ifstream configFile(POSITION_CONFIG_FILE);
    if (configFile.is_open()) {
        std::string savedPath;
        std::string positionStr;
        std::getline(configFile, savedPath);
        if (std::getline(configFile, positionStr)) {
            try {
                std::streampos savedPosition = static_cast<std::streampos>(std::stoll(positionStr));
                if (savedPath == logPath) {
                    smartPrint("[Info] 从配置文件恢复读取位置: " + std::to_string(savedPosition) + "\n", "program", CYAN);
                    return savedPosition;
                }
            }
            catch (...) {}
        }
    }
    return 0;
}

void saveFilePosition(const std::string& logPath, std::streampos position) {
    std::ofstream configFile(POSITION_CONFIG_FILE);
    if (configFile.is_open()) {
        configFile << logPath << "\n" << position;
        smartPrint("[Info] 保存读取位置到 " + POSITION_CONFIG_FILE + ": " + std::to_string(position) + "\n", "program", CYAN);
    }
    else {
        smartPrint("[Error] 无法打开位置配置文件: " + POSITION_CONFIG_FILE + "\n", "program", RED);
    }
}

// ============================================================================
// 工具函数
// ============================================================================

std::string customHash(const std::string& input) {
    unsigned long hash = 5381;
    for (char c : input) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    return std::to_string(hash);
}

std::string generateVerificationCode(int length) {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);

    std::string code;
    for (int i = 0; i < length; ++i) {
        code += chars[dis(gen)];
    }
    return code;
}

bool isCodeExpired(const VerificationCode& vc) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - vc.createTime);
    return duration.count() >= VERIFICATION_CODE_EXPIRE_MINUTES;
}

void cleanupExpiredCodes() {
    std::lock_guard<std::mutex> lock(g_bindMutex);
    for (auto it = g_verificationCodes.begin(); it != g_verificationCodes.end(); ) {
        if (isCodeExpired(it->second)) {
            it = g_verificationCodes.erase(it);
        }
        else {
            ++it;
        }
    }
}

std::string extractJsonValue(const std::string& json, const std::string& key) {
    size_t keyPos = json.find("\"" + key + "\":");
    if (keyPos == std::string::npos) return "";

    size_t valueStart = json.find(":", keyPos) + 1;
    size_t valueEnd = std::string::npos;

    // 跳过空白字符
    valueStart = json.find_first_not_of(" \t\r\n", valueStart);
    if (valueStart == std::string::npos) return "";

    if (json[valueStart] == '\"') {
        valueStart++;
        valueEnd = json.find("\"", valueStart);
    }
    else {
        valueEnd = json.find_first_of(",}", valueStart);
    }

    if (valueEnd == std::string::npos) return "";

    std::string value = json.substr(valueStart, valueEnd - valueStart);

    // 清理值
    value.erase(0, value.find_first_not_of(" \t\r\n"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);

    return value;
}

// ============================================================================
// RCON功能函数
// ============================================================================

bool initRcon() {
    std::lock_guard<std::mutex> lock(g_rconMutex);

    if (g_hRcon) {
        RconDestroy(g_hRcon);
        g_hRcon = nullptr;
    }

    g_hRcon = RconCreate();
    if (!g_hRcon) {
        smartPrint("[Error] 创建RCON句柄失败\n", "program", RED);
        return false;
    }

    // 使用配置中的RCON设置
    smartPrint("尝试连接到RCON服务器: " + g_config.rconHost + ":" + std::to_string(g_config.rconPort) + "\n", "program", YELLOW);

    if (!RconConnect(g_hRcon, g_config.rconHost.c_str(), g_config.rconPort)) {
        smartPrint("[Error] 连接RCON服务器失败\n", "program", RED);
        RconDestroy(g_hRcon);
        g_hRcon = nullptr;
        return false;
    }

    smartPrint("[Info] RCON连接成功\n", "program", GREEN);
    smartPrint("尝试认证...\n", "program", YELLOW);

    // 使用配置中的密码
    if (!RconAuthenticate(g_hRcon, g_config.rconPassword.c_str())) {
        smartPrint("[Error] RCON认证失败\n", "program", RED);
        RconDestroy(g_hRcon);
        g_hRcon = nullptr;
        return false;
    }

    smartPrint("[Info] RCON认证成功\n", "program", GREEN);
    return true;
}

bool sendRconCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(g_rconMutex);

    if (!g_hRcon && !initRcon()) {
        smartPrint("[Error] RCON连接未初始化\n", "program", RED);
        return false;
    }

    try {
        if (RconSendChineseMessage(g_hRcon, command.c_str())) {
            smartPrint("[Info] RCON消息发送成功: " + command + "\n", "program", GREEN);
            return true;
        }
        else {
            smartPrint("[Error] RCON消息发送失败: " + command + "\n", "program", RED);
            return initRcon() && RconSendChineseMessage(g_hRcon, command.c_str());
        }
    }
    catch (const std::exception& e) {
        smartPrint("[Error] RCON发送异常: " + std::string(e.what()) + "\n", "program", RED);
        return false;
    }
}

bool sendRunRconCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(g_rconMutex);

    if (!g_hRcon && !initRcon()) {
        smartPrint("[Error] RCON连接未初始化\n", "program", RED);
        return false;
    }

    try {
        if (RconExecuteCommand(g_hRcon, command.c_str(), true)) {
            smartPrint("[Info] RCON命令发送成功: " + command + "\n", "program", GREEN);
            return true;
        }
        else {
            smartPrint("[Error] RCON命令发送失败: " + command + "\n", "program", RED);
            return initRcon() && RconExecuteCommand(g_hRcon, command.c_str(), true);
        }
    }
    catch (const std::exception& e) {
        smartPrint("[Error] RCON发送异常: " + std::string(e.what()) + "\n", "program", RED);
        return false;
    }
}

// ============================================================================
// QQ消息功能函数
// ============================================================================

std::string escapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.length());

    for (char c : input) {
        switch (c) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            // 对于中文字符，直接保留
            output += c;
            break;
        }
    }
    return output;
}

bool sendPrivateMessage(const std::string& userId, const std::string& message) {
    // 转义消息内容
    std::string escapedMessage = GBKToUTF8(message);

    HINTERNET hSession = WinHttpOpen(L"MC Bot", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) {
        smartPrint("[Error] WinHttpOpen失败\n", "program", RED);
        return false;
    }

    // 使用配置中的napcatHost和napcatPort
    HINTERNET hConnect = WinHttpConnect(hSession,
        std::wstring(g_config.napcatHost.begin(), g_config.napcatHost.end()).c_str(),
        g_config.napcatPort, 0);
    if (!hConnect) {
        smartPrint("[Error] WinHttpConnect失败\n", "program", RED);
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/send_private_msg", nullptr, nullptr, nullptr, 0);
    if (!hRequest) {
        smartPrint("[Error] WinHttpOpenRequest失败\n", "program", RED);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // 修复JSON格式：使用数组格式的message
    std::string jsonData = "{\"user_id\":" + userId + ",\"message\":[{\"type\":\"text\",\"data\":{\"text\":\"" + escapedMessage + "\"}}]}";

    // 设置正确的HTTP头
    LPCWSTR headers = L"Content-Type: application/json; charset=utf-8";

    // 发送请求
    BOOL success = WinHttpSendRequest(hRequest, headers, wcslen(headers),
        (LPVOID)jsonData.c_str(), jsonData.size(),
        jsonData.size(), 0);

    if (success) {
        success = WinHttpReceiveResponse(hRequest, nullptr);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (success) {
        smartPrint("[Info] 私聊消息发送成功: " + message + "\n", "program", GREEN);
    }
    else {
        smartPrint("[Error] 私聊消息发送失败: " + message + "\n", "program", RED);
    }

    return success;
}

bool sendGroupMessage(const std::string& groupId, const std::string& message) {
    if (message.empty()) return false;

    //std::string escapedMessage = GBKToUTF8(message);
    std::string escapedMessage = escapeJsonString(message);

    HINTERNET hSession = WinHttpOpen(L"MC Bot", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;

    // 使用配置中的napcatHost和napcatPort
    HINTERNET hConnect = WinHttpConnect(hSession,
        std::wstring(g_config.napcatHost.begin(), g_config.napcatHost.end()).c_str(),
        g_config.napcatPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/send_group_msg", nullptr, nullptr, nullptr, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // 修复JSON格式：使用数组格式的message
    std::string jsonData = "{\"group_id\":" + groupId + ",\"message\":[{\"type\":\"text\",\"data\":{\"text\":\"" + escapedMessage + "\"}}]}";
    LPCWSTR headers = L"Content-Type: application/json; charset=utf-8";

    BOOL success = WinHttpSendRequest(hRequest, headers, wcslen(headers),
        (LPVOID)jsonData.c_str(), jsonData.size(),
        jsonData.size(), 0);
    if (success) {
        success = WinHttpReceiveResponse(hRequest, nullptr);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (success) {
        smartPrint("[Info] 群聊消息发送成功: " + message + "\n", "program", GREEN);
    }
    else {
        smartPrint("[Error] 群聊消息发送失败: " + message + "\n", "program", RED);
    }

    return success;
}
bool sendToGroupMessage(const std::string& groupId, const std::string& message) {
    if (message.empty()) return false;

    std::string escapedMessage = GBKToUTF8(message);
    //std::string escapedMessage = escapeJsonString(message);

    HINTERNET hSession = WinHttpOpen(L"MC Bot", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;

    // 使用配置中的napcatHost和napcatPort
    HINTERNET hConnect = WinHttpConnect(hSession,
        std::wstring(g_config.napcatHost.begin(), g_config.napcatHost.end()).c_str(),
        g_config.napcatPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/send_group_msg", nullptr, nullptr, nullptr, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // 修复JSON格式：使用数组格式的message
    std::string jsonData = "{\"group_id\":" + groupId + ",\"message\":[{\"type\":\"text\",\"data\":{\"text\":\"" + escapedMessage + "\"}}]}";
    LPCWSTR headers = L"Content-Type: application/json; charset=utf-8";

    BOOL success = WinHttpSendRequest(hRequest, headers, wcslen(headers),
        (LPVOID)jsonData.c_str(), jsonData.size(),
        jsonData.size(), 0);
    if (success) {
        success = WinHttpReceiveResponse(hRequest, nullptr);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (success) {
        smartPrint("[Info] 群聊消息发送成功: " + message + "\n", "program", GREEN);
    }
    else {
        smartPrint("[Error] 群聊消息发送失败: " + message + "\n", "program", RED);
    }

    return success;
}

void addMessageToQueue(const std::string& message) {
    if (message.empty()) return;
    std::lock_guard<std::mutex> lock(g_queueMutex);
    g_messageQueue.push(message);
    smartPrint("[Info] 添加到发送队列: " + message + "\n", "program", CYAN);
}

// ============================================================================
// 登录绑定功能函数
// ============================================================================

void handleLoginCommand(const std::string& qqId, const std::string& message) {
    smartPrint("[Info] 处理登录命令 - QQ: " + qqId + " 消息: " + message + "\n", "program", CYAN);

    if (message.find("login:") == 0) {
        std::string rest = message.substr(6);
        rest.erase(0, rest.find_first_not_of(" "));

        size_t spacePos = rest.find(' ');
        if (spacePos == std::string::npos) {
            sendPrivateMessage(qqId, "格式错误: login: <MC名称> <密码>");
            return;
        }

        std::string mcName = rest.substr(0, spacePos);
        std::string password = rest.substr(spacePos + 1);
        password.erase(0, password.find_first_not_of(" "));
        password.erase(password.find_last_not_of(" ") + 1);

        if (mcName.empty() || password.empty()) {
            sendPrivateMessage(qqId, "格式错误: login: <MC名称> <密码>");
            return;
        }

        // 检查该QQ是否已经绑定了其他MC账号
        {
            std::lock_guard<std::mutex> lock(g_bindMutex);
            for (const auto& bind : g_bindInfos) {
                if (bind.second.qqId == qqId) {
                    if (bind.second.mcName != mcName) {
                        sendPrivateMessage(qqId, "该QQ已绑定MC账号: " + bind.second.mcName + "，请先解绑后再绑定新账号");
                        return;
                    }
                    // 如果是同一个账号，继续验证密码
                    break;
                }
            }
        }

        std::string passwordHash = customHash(password);
        smartPrint("[Info] 登录请求 - MC名: " + mcName + " 密码哈希: " + passwordHash + "\n", "program", GREEN);

        std::lock_guard<std::mutex> lock(g_bindMutex);

        auto bindIt = g_bindInfos.find(mcName);
        if (bindIt != g_bindInfos.end()) {
            if (bindIt->second.passwordHash == passwordHash) {
                smartPrint("[Info] 密码验证成功，自动登录\n", "program", GREEN);
                sendPrivateMessage(qqId, "登录成功！您的MC名称: " + mcName);
                sendRunRconCommand("tell " + mcName + " 登录成功！");
                return;
            }
            else {
                smartPrint("[Error] 密码验证失败\n", "program", RED);
                sendPrivateMessage(qqId, "密码错误，请检查后重试");
                return;
            }
        }

        // 检查该QQ是否已经绑定了其他MC账号（再次检查，防止竞态条件）
        for (const auto& bind : g_bindInfos) {
            if (bind.second.qqId == qqId) {
                sendPrivateMessage(qqId, "QQ 已绑定 MC 账号: " + bind.second.mcName + "，请先解绑新账户");
                return;
            }
        }

        std::string code = generateVerificationCode();
        VerificationCode vc;
        vc.code = code;
        vc.mcName = mcName;
        vc.createTime = std::chrono::steady_clock::now();

        g_verificationCodes[qqId] = vc;

        {
            std::lock_guard<std::mutex> tempLock(g_tempPasswordMutex);
            g_tempPasswordHashes[qqId] = passwordHash;
        }

        smartPrint("[Info] 新绑定 - 生成验证码: " + code + "\n", "program", BRIGHT_GREEN);
        sendRunRconCommand("tell " + mcName + " 请在游戏内输入: vali: " + code);
        sendPrivateMessage(qqId, "请在游戏中输入: vali: " + code + " (有效时间8分钟)");
    }
}

// 添加解绑功能函数
void handleUnbindCommand(const std::string& qqId, const std::string& message) {
    smartPrint("[Info] 处理解绑命令 - QQ: " + qqId + " 消息: " + message + "\n", "program", CYAN);

    if (message.find("unbind:") == 0) {
        std::string rest = message.substr(7);
        rest.erase(0, rest.find_first_not_of(" "));

        size_t spacePos = rest.find(' ');
        if (spacePos == std::string::npos) {
            sendPrivateMessage(qqId, "格式错误: unbind: <MC名称> <密码>");
            return;
        }

        std::string mcName = rest.substr(0, spacePos);
        std::string password = rest.substr(spacePos + 1);
        password.erase(0, password.find_first_not_of(" "));
        password.erase(password.find_last_not_of(" ") + 1);

        if (mcName.empty() || password.empty()) {
            sendPrivateMessage(qqId, "格式错误: unbind: <MC名称> <密码>");
            return;
        }

        std::string passwordHash = customHash(password);
        smartPrint("[Info] 解绑请求 - MC名: " + mcName + "\n", "program", YELLOW);

        std::lock_guard<std::mutex> lock(g_bindMutex);

        // 查找绑定信息
        auto bindIt = g_bindInfos.find(mcName);
        if (bindIt == g_bindInfos.end()) {
            sendPrivateMessage(qqId, "未找到 MC 账号的绑定信息");
            return;
        }

        // 验证QQ号是否匹配
        if (bindIt->second.qqId != qqId) {
            sendPrivateMessage(qqId, "此 MC 账号未绑定当前 QQ");
            return;
        }

        // 验证密码
        if (bindIt->second.passwordHash != passwordHash) {
            sendPrivateMessage(qqId, "密码错误");
            return;
        }

        // 执行解绑
        g_bindInfos.erase(bindIt);
        saveBindDataToFile();

        smartPrint("[Info] 解绑成功 - QQ: " + qqId + " MC: " + mcName + "\n", "program", GREEN);
        sendPrivateMessage(qqId, "解绑成功！已解除QQ与MC账号 " + mcName + " 的绑定");
        sendRunRconCommand("tell " + mcName + " QQ绑定已解除");
    }
}

// 添加查询绑定功能函数
void handleQueryBindCommand(const std::string& qqId) {
    smartPrint("[Info] 处理查询绑定命令 - QQ: " + qqId + "\n", "program", CYAN);

    std::lock_guard<std::mutex> lock(g_bindMutex);

    std::string boundMcName;
    for (const auto& bind : g_bindInfos) {
        if (bind.second.qqId == qqId) {
            boundMcName = bind.first;
            break;
        }
    }

    if (!boundMcName.empty()) {
        sendPrivateMessage(qqId, "该账户已绑定: " + boundMcName);
        smartPrint("[Info] 查询绑定 - QQ: " + qqId + " -> MC: " + boundMcName + "\n", "program", GREEN);
    }
    else {
        sendPrivateMessage(qqId, "该账户未绑定任何账号");
        smartPrint("[Info] 查询绑定 - QQ: " + qqId + " 未绑定任何账号\n", "program", YELLOW);
    }
}

void handleValidationCommand(const std::string& qqId, const std::string& message) {
    try {
        smartPrint("[Info] 处理验证命令 - QQ: " + qqId + " 消息: " + message + "\n", "program", CYAN);

        if (qqId.empty() || message.find("vali:") != 0) {
            return;
        }

        std::string inputCode = message.substr(5);
        size_t start = inputCode.find_first_not_of(" \t\r\n");
        size_t end = inputCode.find_last_not_of(" \t\r\n");

        if (start == std::string::npos) {
            sendPrivateMessage(qqId, "验证码不能为空");
            return;
        }

        inputCode = inputCode.substr(start, end - start + 1);
        smartPrint("[Info] 输入验证码: '" + inputCode + "'\n", "program", YELLOW);

        // 一次性获取所有需要的数据
        std::string storedCode;
        std::string storedMcName;
        std::string passwordHash;
        bool codeExpired = false;
        bool codeExists = false;

        {
            std::lock_guard<std::mutex> lock(g_bindMutex);
            auto it = g_verificationCodes.find(qqId);
            if (it != g_verificationCodes.end()) {
                storedCode = it->second.code;
                storedMcName = it->second.mcName;
                codeExists = true;
                codeExpired = isCodeExpired(it->second);
            }
        }

        if (!codeExists) {
            smartPrint("[Error] 验证码不存在\n", "program", RED);
            sendPrivateMessage(qqId, "验证码不存在");
            return;
        }

        if (codeExpired) {
            smartPrint("[Warn] 验证码已过期\n", "program", RED);
            {
                std::lock_guard<std::mutex> lock(g_bindMutex);
                g_verificationCodes.erase(qqId);
            }
            sendPrivateMessage(qqId, "验证码超时");
            return;
        }

        smartPrint("[Info] 存储的验证码: '" + storedCode + "'\n", "program", WHITE);

        // 在验证码匹配成功后的部分添加检查
        if (inputCode == storedCode) {
            smartPrint("[Info] 验证码匹配!\n", "program", GREEN);

            // 检查该QQ是否已经绑定了其他MC账号
            {
                std::lock_guard<std::mutex> lock(g_bindMutex);
                for (const auto& bind : g_bindInfos) {
                    if (bind.second.qqId == qqId) {
                        smartPrint("[Warn] QQ已绑定其他MC账号: " + bind.second.mcName + "\n", "program", RED);
                        sendPrivateMessage(qqId, "该账户已绑定: " + bind.second.mcName + " Fail");

                        // 清理验证码和临时密码
                        g_verificationCodes.erase(qqId);
                        {
                            std::lock_guard<std::mutex> tempLock(g_tempPasswordMutex);
                            g_tempPasswordHashes.erase(qqId);
                        }
                        return;
                    }
                }
            }

            // 获取临时存储的密码哈希
            std::string passwordHash;
            {
                std::lock_guard<std::mutex> tempLock(g_tempPasswordMutex);
                auto it = g_tempPasswordHashes.find(qqId);
                if (it != g_tempPasswordHashes.end()) {
                    passwordHash = it->second;
                    g_tempPasswordHashes.erase(it);
                }
            }

            if (passwordHash.empty()) {
                passwordHash = customHash("default_password");
            }

            {
                std::lock_guard<std::mutex> lock(g_bindMutex);

                BindInfo bindInfo;
                bindInfo.qqId = qqId;
                bindInfo.mcName = storedMcName;
                bindInfo.passwordHash = passwordHash;
                bindInfo.bindTime = std::chrono::steady_clock::now();

                g_bindInfos.emplace(storedMcName, bindInfo);
                g_verificationCodes.erase(qqId);
            }

            // 保存到文件
            saveBindDataToFile();

            smartPrint("[Info] 绑定完成\n", "program", BRIGHT_GREEN);
            sendPrivateMessage(qqId, "绑定成功!" + storedMcName);
            sendRunRconCommand("tell " + storedMcName + " 绑定成功！");
        }
        else {
            smartPrint("[Error] 验证码不匹配\n", "program", RED);
            sendPrivateMessage(qqId, "验证码不匹配");
        }

    }
    catch (const std::exception& e) {
        smartPrint("[Warn] 验证异常: " + std::string(e.what()) + "\n", "program", RED);
    }
}

void handleBindQueryCommand(const std::string& playerName) {
    std::lock_guard<std::mutex> lock(g_bindMutex);

    auto it = g_bindInfos.find(playerName);
    if (it != g_bindInfos.end()) {
        std::string verificationCode = generateVerificationCode();

        // 更新验证码
        VerificationCode vc;
        vc.code = verificationCode;
        vc.mcName = playerName;
        vc.createTime = std::chrono::steady_clock::now();
        g_verificationCodes[it->second.qqId] = vc;

        // 发送私聊验证码
        sendPrivateMessage(it->second.qqId, "您的验证码为:" + verificationCode + " (有效时间: 8分钟)");
        sendRunRconCommand("tell " + playerName + " 验证码已发送到绑定QQ，请查收");

        smartPrint("[Info] 为已绑定玩家 " + playerName + " 发送验证码到QQ: " + it->second.qqId + "\n", "program", GREEN);
    }
    else {
        sendRunRconCommand("tell " + playerName + " 该账号未绑定QQ");
        smartPrint("[Warn] 玩家 " + playerName + " 尝试查询绑定但未绑定QQ\n", "program", YELLOW);
    }
}

// ============================================================================
// OP权限功能函数
// ============================================================================

void loadOpList() {
    std::lock_guard<std::mutex> lock(g_opMutex);
    g_opList.clear();

    try {
        std::ifstream file(g_config.opsPath);  // 使用配置的路径
        if (!file.is_open()) {
            smartPrint("[Info] 无法打开ops.json文件: " + g_config.opsPath + "\n", "program", RED);
            return;
        }

        // 读取整个文件
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonContent = buffer.str();
        file.close();

        // 清理JSON内容
        jsonContent.erase(std::remove(jsonContent.begin(), jsonContent.end(), '\n'), jsonContent.end());
        jsonContent.erase(std::remove(jsonContent.begin(), jsonContent.end(), '\r'), jsonContent.end());
        jsonContent.erase(std::remove(jsonContent.begin(), jsonContent.end(), '\t'), jsonContent.end());

        smartPrint("[Info] 读取JSON内容: " + jsonContent.substr(0, 100) + "...\n", "program", WHITE);

        // 检查是否是数组格式
        if (jsonContent[0] == '[') {
            // 数组格式：[{...}, {...}]
            size_t pos = 0;
            while (pos < jsonContent.length()) {
                size_t objStart = jsonContent.find('{', pos);
                if (objStart == std::string::npos) break;

                size_t objEnd = jsonContent.find('}', objStart);
                if (objEnd == std::string::npos) break;

                std::string objStr = jsonContent.substr(objStart, objEnd - objStart + 1);

                // 提取name和level
                std::string name = extractJsonValue(objStr, "name");
                std::string levelStr = extractJsonValue(objStr, "level");

                // 双重清理确保无空格
                name.erase(0, name.find_first_not_of(" \t\r\n"));
                name.erase(name.find_last_not_of(" \t\r\n") + 1);
                levelStr.erase(0, levelStr.find_first_not_of(" \t\r\n"));
                levelStr.erase(levelStr.find_last_not_of(" \t\r\n") + 1);

                if (!name.empty() && !levelStr.empty()) {
                    try {
                        int level = std::stoi(levelStr);
                        g_opList[name] = level;
                        smartPrint("[Info] 加载OP: '" + name + "' 等级: " + std::to_string(level) + "\n", "program", GREEN);
                    }
                    catch (...) {
                        smartPrint("[Error] 解析OP等级失败: " + levelStr + "\n", "program", RED);
                    }
                }

                pos = objEnd + 1;
            }
        }
        else if (jsonContent[0] == '{') {
            // 单个对象格式
            std::string name = extractJsonValue(jsonContent, "name");
            std::string levelStr = extractJsonValue(jsonContent, "level");

            name.erase(0, name.find_first_not_of(" \t\r\n"));
            name.erase(name.find_last_not_of(" \t\r\n") + 1);
            levelStr.erase(0, levelStr.find_first_not_of(" \t\r\n"));
            levelStr.erase(levelStr.find_last_not_of(" \t\r\n") + 1);

            if (!name.empty() && !levelStr.empty()) {
                try {
                    int level = std::stoi(levelStr);
                    g_opList[name] = level;
                    smartPrint("[Info] 加载OP: '" + name + "' 等级: " + std::to_string(level) + "\n", "program", GREEN);
                }
                catch (...) {
                    smartPrint("[Error] 解析OP等级失败: " + levelStr + "\n", "program", RED);
                }
            }
        }

        smartPrint("[Info] 已加载 " + std::to_string(g_opList.size()) + " 个OP\n", "program", GREEN);

        // 调试输出所有OP
        for (const auto& op : g_opList) {
            smartPrint("   - '" + op.first + "' (等级 " + std::to_string(op.second) + ")\n", "program", WHITE);
        }
    }
    catch (const std::exception& e) {
        smartPrint("[Error] 解析ops.json失败: " + std::string(e.what()) + "\n", "program", RED);
    }
}

int getOpLevel(const std::string& playerName) {
    std::lock_guard<std::mutex> lock(g_opMutex);

    // 首先尝试精确匹配
    auto it = g_opList.find(playerName);
    if (it != g_opList.end()) {
        smartPrint("[Info] 匹配OP: " + playerName + " -> 等级 " + std::to_string(it->second) + "\n", "program", GREEN);
        return it->second;
    }

    // 如果精确匹配失败，尝试不区分大小写匹配
    std::string lowerPlayerName = playerName;
    std::transform(lowerPlayerName.begin(), lowerPlayerName.end(), lowerPlayerName.begin(), ::tolower);

    for (const auto& op : g_opList) {
        std::string lowerOpName = op.first;
        std::transform(lowerOpName.begin(), lowerOpName.end(), lowerOpName.begin(), ::tolower);

        if (lowerOpName == lowerPlayerName) {
            smartPrint("[Info] OP名称大小写匹配: 输入='" + playerName + "' 文件='" + op.first + "' -> 等级 " + std::to_string(op.second) + "\n", "program", YELLOW);
            return op.second;
        }
    }

    // 添加调试信息，显示所有OP名字
    smartPrint("[Error] 未找到OP: " + playerName + "\n", "program", RED);
    smartPrint("[Info] 当前OP列表:\n", "program", WHITE);
    for (const auto& op : g_opList) {
        smartPrint("   - '" + op.first + "' (等级 " + std::to_string(op.second) + ")\n", "program", WHITE);
    }

    return 0;
}

bool checkCommandPermission(int opLevel, const std::string& command) {
    if (opLevel <= 0) return false;

    // 提取基础命令（去除参数）
    std::string baseCommand = command;
    size_t spacePos = command.find(' ');
    if (spacePos != std::string::npos) {
        baseCommand = command.substr(0, spacePos);
    }

    // 转换为小写便于比较
    std::transform(baseCommand.begin(), baseCommand.end(), baseCommand.begin(), ::tolower);

    // 等级1命令：基础命令
    std::vector<std::string> level1Commands = {
        "help", "list", "me", "msg", "tell", "w", "seed", "trigger", "vote"
    };

    // 等级2命令：需要2级权限（但不能执行危险命令）
    std::vector<std::string> level2Commands = {
        "clear", "give", "tp", "teleport", "gamemode", "time", "weather",
        "effect", "enchant", "xp", "kill", "spawnpoint", "setworldspawn"
    };

    // 等级3命令：需要3级权限（服务器管理命令）
    std::vector<std::string> level3Commands = {
        "ban", "pardon", "ban-ip", "pardon-ip", "kick", "op", "deop",
        "reload", "save-all", "save-off", "save-on", "whitelist"
    };

    // 等级4命令：需要4级权限（最高权限命令）
    std::vector<std::string> level4Commands = {
        "stop", "restart", "banlist", "publish", "debug"
    };

    // 检查命令权限
    for (const auto& cmd : level1Commands) {
        if (baseCommand == cmd) {
            return opLevel >= 1;
        }
    }

    for (const auto& cmd : level2Commands) {
        if (baseCommand == cmd) {
            return opLevel >= 2;
        }
    }

    for (const auto& cmd : level3Commands) {
        if (baseCommand == cmd) {
            // op和deop命令需要4级权限
            if ((baseCommand == "op" || baseCommand == "deop") && opLevel < 4) {
                return false;
            }
            return opLevel >= 3;
        }
    }

    for (const auto& cmd : level4Commands) {
        if (baseCommand == cmd) {
            return opLevel >= 4;
        }
    }

    // 未知命令默认需要2级权限
    return opLevel >= 2;
}

std::string getCommandPermissionInfo(const std::string& command) {
    std::string baseCommand = command;
    size_t spacePos = command.find(' ');
    if (spacePos != std::string::npos) {
        baseCommand = command.substr(0, spacePos);
    }
    std::transform(baseCommand.begin(), baseCommand.end(), baseCommand.begin(), ::tolower);

    if (baseCommand == "stop" || baseCommand == "restart") return "需要4级OP权限";
    if (baseCommand == "reload") return "需要3级OP权限";
    if (baseCommand == "op" || baseCommand == "deop") return "需要4级OP权限";
    if (baseCommand == "ban" || baseCommand == "kick") return "需要3级OP权限";
    return "需要2级OP权限";
}

void handleCmdCommand(const std::string& senderId, const std::string& message, bool isPrivate) {
    smartPrint("[Case] 处理CMD命令 - QQ: " + senderId + " 消息: " + message + "\n", "program", CYAN);

    if (message.find("cmd:") == 0) {
        std::string command = message.substr(4);
        command.erase(0, command.find_first_not_of(" "));

        if (command.empty()) {
            if (isPrivate) {

                sendPrivateMessage(senderId, "命令不能为空");
            }
            else {
                sendToGroupMessage(g_targetGroupId, "命令不能为空");
            }
            return;
        }

        // 查找对应的MC玩家名
        std::string mcName;
        {
            std::lock_guard<std::mutex> lock(g_bindMutex);
            for (const auto& bind : g_bindInfos) {
                smartPrint("[Info] 检查绑定: QQ=" + bind.second.qqId + " MC=" + bind.first + "\n", "program", WHITE);
                if (bind.second.qqId == senderId) {
                    mcName = bind.first;
                    smartPrint("[Info] 找到绑定: QQ " + senderId + " -> MC " + mcName + "\n", "program", GREEN);
                    break;
                }
            }

            if (mcName.empty()) {
                smartPrint("[Error] 未找到QQ " + senderId + " 的绑定信息\n", "program", RED);
            }
        }

        if (mcName.empty()) {
            std::string reply = "请先在 QQ 私信中输入: login: <MC Name> <password> 绑定mc账号后使用此功能";
            if (isPrivate) {
                sendPrivateMessage(senderId, reply);
            }
            else {
                sendToGroupMessage(g_targetGroupId, reply);
            }
            return;
        }

        // 检查OP权限
        smartPrint("[Info] 检查OP权限: MC玩家=" + mcName + "\n", "program", WHITE);
        int opLevel = getOpLevel(mcName);
        smartPrint("[Info] OP等级检测结果: " + mcName + " -> 等级 " + std::to_string(opLevel) + "\n", "program", WHITE);

        if (opLevel <= 0) {
            std::string reply = "您不是服务器 OP,无法执行命令.请联系您的管理员以添加 OP 权限. 您的mc名称: " + mcName;
            smartPrint("[Error] OP权限检查失败: " + mcName + " 不是OP\n", "program", RED);
            if (isPrivate) {
                sendPrivateMessage(senderId, reply);
            }
            else {
                sendToGroupMessage(g_targetGroupId, reply);
            }
            return;
        }

        // 检查命令权限 - 修复这里的逻辑错误！
        if (!checkCommandPermission(opLevel, command)) {
            std::string permissionInfo = getCommandPermissionInfo(command);
            std::string reply = "权限不足!当前 OP 等级: " + std::to_string(opLevel) + "，" + permissionInfo;

            smartPrint("[Error] 命令权限检查失败: " + command + " 需要更高权限\n", "program", RED);

            if (isPrivate) {
                sendPrivateMessage(senderId, reply);
            }
            else {
                sendToGroupMessage(g_targetGroupId, reply);
            }
            return;
        }

        // 特殊检查：op/deop命令需要4级权限
        std::string baseCommand = command;
        size_t spacePos = command.find(' ');
        if (spacePos != std::string::npos) {
            baseCommand = command.substr(0, spacePos);
        }
        std::transform(baseCommand.begin(), baseCommand.end(), baseCommand.begin(), ::tolower);

        if ((baseCommand == "op" || baseCommand == "deop") && opLevel < 4) {
            std::string reply = "op/deop 该命令需要 4 级 OP 权限";
            smartPrint("[Error] 特殊权限检查失败: " + command + " 需要4级OP\n", "program", RED);
            if (isPrivate) {
                sendPrivateMessage(senderId, reply);
            }
            else {
                sendToGroupMessage(g_targetGroupId, reply);
            }
            return;
        }

        // 执行命令
        smartPrint("[Case] 执行CMD命令: " + command + " (执行者: " + mcName + " OP等级: " + std::to_string(opLevel) + ")\n", "program", GREEN);

        if (sendRunRconCommand(command)) {
            std::string successMsg = "成功执行: " + command;
            smartPrint("[Case] 命令执行成功\n", "program", GREEN);
            if (isPrivate) {
                sendPrivateMessage(senderId, successMsg);
            }
            else {
                sendToGroupMessage(g_targetGroupId, successMsg);
            }
        }
        else {
            std::string failMsg = "执行失败n: " + command;
            smartPrint("[Error] 命令执行失败\n", "program", RED);
            if (isPrivate) {
                sendPrivateMessage(senderId, failMsg);
            }
            else {
                sendToGroupMessage(g_targetGroupId, failMsg);
            }
        }
    }
}

// ============================================================================
// NapCat功能函数
// ============================================================================

bool startNapCatProcess() {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hOutputReadTmp, hOutputWrite;

    if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0) ||
        !DuplicateHandle(GetCurrentProcess(), hOutputReadTmp, GetCurrentProcess(),
            &g_napcatOutputRead, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        smartPrint("[Error] 创建管道失败\n", "program", RED);
        return false;
    }
    CloseHandle(hOutputReadTmp);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hOutputWrite;
    si.hStdError = hOutputWrite;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(nullptr, (LPSTR)"NapCatWinBootMain.exe", nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        g_napcatProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(hOutputWrite);
        g_napcatRunning = true;
        smartPrint("[Info] NapCat进程启动成功\n", "program", GREEN);
        return true;
    }
    else {
        smartPrint("[Error] 启动NapCat进程失败\n", "program", RED);
        CloseHandle(hOutputWrite);
        CloseHandle(g_napcatOutputRead);
        return false;
    }
}

void processNapCatLine(const std::string& line) {
    smartPrint("[NAP] " + line + "\n", "napcat", GRAY);

    // 移除ANSI颜色代码
    std::string cleanLine = line;
    std::regex ansiPattern(R"(\[\d+m)");
    cleanLine = std::regex_replace(cleanLine, ansiPattern, "");
    cleanLine.erase(std::remove(cleanLine.begin(), cleanLine.end(), '\x1b'), cleanLine.end());

    // 检查是否是Rcon发送的消息（避免循环）
    if (cleanLine.find("[Rcon]") != std::string::npos) {
        smartPrint("[Info] 忽略Rcon发送的消息（避免循环）\n", "program", YELLOW);
        return;
    }

    // 转换字体编码
    std::string convertedLine;
    if (isUtf8(cleanLine)) {
        convertedLine = utf8ToGbk(cleanLine);
    }
    else {
        convertedLine = cleanLine;
    }

    // 清理字符串
    convertedLine.erase(std::remove_if(convertedLine.begin(), convertedLine.end(),
        [](char c) { return static_cast<unsigned char>(c) < 32; }), convertedLine.end());

    while (!convertedLine.empty() &&
        (convertedLine.back() == ' ' ||
            convertedLine.back() == '\r' ||
            convertedLine.back() == '\n' ||
            static_cast<unsigned char>(convertedLine.back()) < 32)) {
        convertedLine.pop_back();
    }

    // 忽略自己发送的消息
    if (convertedLine.find(g_botQQ) != std::string::npos && cleanLine.find("发送 ->") != std::string::npos) {
        smartPrint("[Info] 忽略bot自己发送的消息\n", "program", YELLOW);
        return;
    }

    // 检查是否是群聊消息
    if (convertedLine.find("接收 <- 群聊") == std::string::npos &&
        convertedLine.find("接收 <- 私聊") == std::string::npos) {
        return;
    }

    //私聊消息处理
    if (convertedLine.find("接收 <- 私聊") != std::string::npos) {
        smartPrint("[Info] 检测到私聊消息\n", "program", BRIGHT_CYAN);

        // 解析私聊消息格式
        std::regex privatePattern(R"(接收 <- 私聊 \((\d+)\)\s*(.+))");
        std::smatch privateMatches;

        if (std::regex_search(convertedLine, privateMatches, privatePattern) && privateMatches.size() >= 3) {
            std::string senderId = privateMatches[1].str();
            std::string message = privateMatches[2].str();

            // 处理登录/绑定命令
            if (message.find("login:") == 0) {
                handleLoginCommand(senderId, message);
            }
            // 处理验证命令
            else if (message.find("vali:") == 0) {
                handleValidationCommand(senderId, message);
            }
            // 处理解绑命令
            else if (message.find("unbind:") == 0) {
                handleUnbindCommand(senderId, message);
            }
            // 处理查询绑定命令
            else if (message.find("query") == 0 || message == "查询绑定") {
                handleQueryBindCommand(senderId);
            }
            // 处理命令执行
            else if (message.find("cmd:") == 0) {
                handleCmdCommand(senderId, message, true);
            }
            // 帮助信息
            else if (message.find("help") == 0 || message == "帮助") {
                

                std::string helpMsg = "Command:\n"
                    "login: <MC名称> <绑定密码> - 绑定MC账号\n"
                    "unbind: <MC名称> <绑定密码> - 解绑MC账号\n"
                    "query - 查询绑定信息\n"
                    "cmd: <MC指令> - 执行mc命令(需要op权限)\n"
                    "vali: <验证码> - 在游戏中输入来验证验证码";
                
                sendPrivateMessage(senderId, helpMsg);
            }
        }
        return;
    }

    smartPrint("[Info] 检测到群聊消息\n", "program", BRIGHT_CYAN);

    // 检查是否是目标群组
    if (convertedLine.find(g_targetGroupId) == std::string::npos) {
        smartPrint("[Info] 忽略非目标群消息\n", "program", YELLOW);
        return;
    }

    // 解析消息格式
    std::regex pattern(R"(接收 <- 群聊 \[([^\]]+?)\((\d+)\)\] \[([^\]]+?)\((\d+)\)\]\s*(.+))");
    std::smatch matches;

    if (std::regex_search(convertedLine, matches, pattern) && matches.size() >= 6) {
        std::string groupName = matches[1].str();
        std::string groupId = matches[2].str();
        std::string senderName = matches[3].str();
        std::string senderId = matches[4].str();
        std::string message = matches[5].str();

        // 清理提取的值
        groupId.erase(std::remove(groupId.begin(), groupId.end(), ' '), groupId.end());
        senderId.erase(std::remove(senderId.begin(), senderId.end(), ' '), senderId.end());
        message.erase(std::remove_if(message.begin(), message.end(),
            [](char c) { return static_cast<unsigned char>(c) < 32; }), message.end());

        // 移除消息末尾的特殊字符
        while (!message.empty() &&
            (message.back() == ' ' ||
                message.back() == '\r' ||
                message.back() == '\n' ||
                static_cast<unsigned char>(message.back()) < 32)) {
            message.pop_back();
        }

        // 检查hide前缀（QQ->MC方向）
        if (message.find(HIDE_PREFIX) == 0) {
            smartPrint("[Hide] 忽略隐藏消息（QQ->MC）: " + message + "\n", "program", YELLOW);
            return;
        }
        if (message.find("cmd:") == 0) {
            handleCmdCommand(senderId, message, false);
            return;
        }
        else if (message.find("query") == 0 || message == "查询绑定") {
            handleQueryBindCommand(senderId);
            sendToGroupMessage(g_targetGroupId, "已发送,请在狐务器娘私信查看");
			return;
        }
        else if (message.find("help") == 0 || message == "帮助") {
            std::string helpMsg = "Command:\n"
                "login: <MC名称> <绑定密码> - 绑定MC账号\n"
                "unbind: <MC名称> <绑定密码> - 解绑MC账号\n"
                "query - 查询绑定信息\n"
                "cmd: <MC指令> - 执行mc命令(需要op权限)\n"
                "vali: <验证码> - 在游戏中输入来验证验证码";
            sendToGroupMessage(g_targetGroupId, helpMsg);
        }

        smartPrint("[Info] 解析结果:\n", "program", BRIGHT_GREEN);
        smartPrint("   发送者: " + senderName + "\n", "program", WHITE);
        smartPrint("   发送者QQ: " + senderId + "\n", "program", WHITE);
        smartPrint("   消息内容: " + message + "\n", "program", WHITE);

        std::string mcMessage = "<" + senderName + "(" + senderId + ")> " + message;

        // 再次清理最终消息
        /*mcMessage.erase(std::remove_if(mcMessage.begin(), mcMessage.end(),
            [](char c) { return static_cast<unsigned char>(c) < 32; }), mcMessage.end());*/

        forwardToMinecraft(senderName, senderId, message);
        smartPrint("[Info] 消息已转发到MC: " + mcMessage + "\n", "program", GREEN);


    }
}

void monitorNapCatOutput() {
    char buffer[4096];
    DWORD bytesRead;
    std::string partialLine;

    smartPrint("开始监控NapCat输出...\n", "program", YELLOW);

    while (g_napcatRunning) {
        if (ReadFile(g_napcatOutputRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::istringstream stream(partialLine + buffer);
            partialLine.clear();

            std::string line;
            while (std::getline(stream, line)) {
                if (stream.eof()) partialLine = line;
                else processNapCatLine(line);
            }
        }
        Sleep(100);
    }
}

void shutdownNapCat() {
    smartPrint("[Stop] 关闭NapCat...\n", "program", YELLOW);

    system("taskkill /f /im QQ.exe >nul 2>&1");

    if (g_napcatProcess) {
        TerminateProcess(g_napcatProcess, 0);
        CloseHandle(g_napcatProcess);
        g_napcatProcess = nullptr;
    }

    if (g_napcatOutputRead) {
        CloseHandle(g_napcatOutputRead);
        g_napcatOutputRead = nullptr;
    }

    system("taskkill /f /im QQ.exe >nul 2>&1");
}

// ============================================================================
// 消息转发功能函数
// ============================================================================

void forwardToMinecraft(const std::string& senderName, const std::string& senderId, const std::string& message) {

    // 检查隐藏前缀
    if (message.find(HIDE_PREFIX) == 0) {
        smartPrint("[Hide] 忽略隐藏消息: " + message + "\n", "program", YELLOW);
        return;
    }
    if (message.find("vali:") == 0) {
        smartPrint("[Hide] 忽略隐藏消息: " + message + "\n", "program", YELLOW);
        return;
    }

    std::string cleanMessage = message;

    // 去除开头的 []<> 符号
    if (cleanMessage.find("[]<> ") == 0) {
        cleanMessage = cleanMessage.substr(5);
    }

    // 去除其他可能的格式问题
    size_t pos = cleanMessage.find_first_not_of(" []<>");
    if (pos != std::string::npos && pos > 0) {
        cleanMessage = cleanMessage.substr(pos);
    }

    // 去除首尾空格
    cleanMessage.erase(0, cleanMessage.find_first_not_of(" "));
    cleanMessage.erase(cleanMessage.find_last_not_of(" ") + 1);

    // 构造MC命令
    std::string mcCommand;
    if (senderName.empty() || senderId.empty()) {
        mcCommand = "<" + cleanMessage;
    }
    else {
        mcCommand = "<" + senderName + "(" + senderId + ")> " + cleanMessage;
    }

	std::string mcCmd = GBKToUTF8(mcCommand);

    smartPrint("[Info] 发送RCON命令: " + mcCommand + "\n", "program", YELLOW);

    if (!sendRconCommand(mcCommand)) {
        smartPrint("[Error] 消息转发失败\n", "program", RED);
    }
}

void monitorMinecraftLog(const std::string& logPath) {
    // 使用配置的日志路径
    std::string actualLogPath = logPath.empty() ? g_config.logPath : logPath;
    std::ifstream logFile;
    std::string line;
    std::streampos fileSize = loadFilePosition(actualLogPath);

    smartPrint("开始监听MC日志: " + actualLogPath + "\n", "program", YELLOW);

    while (g_running) {
        logFile.open(actualLogPath);  // 使用 actualLogPath
        if (!logFile.is_open()) {
            smartPrint("[Error] 无法打开日志文件: " + actualLogPath + "\n", "program", RED);
            Sleep(5000);
            continue;
        }

        logFile.seekg(0, std::ios::end);
        std::streampos currentSize = logFile.tellg();

        if (currentSize < fileSize) {
            smartPrint("[Info] 检测到日志文件被重置\n", "program", YELLOW);
            fileSize = 0;
            saveFilePosition(actualLogPath, 0);
        }

        if (currentSize > fileSize) {
            logFile.seekg(fileSize);

            while (std::getline(logFile, line) && g_running) {
                // 忽略Rcon发送的消息
                if (line.find("[Rcon]") != std::string::npos) {
                    continue;
                }

                // 检测玩家聊天消息
                if (line.find("<") != std::string::npos && line.find(">") != std::string::npos) {
                    size_t start = line.find("<");
                    size_t end = line.find(">");
                    if (start != std::string::npos && end != std::string::npos && end > start) {
                        std::string playerName = line.substr(start + 1, end - start - 1);
                        std::string message = line.substr(end + 1);

                        // 清理消息内容，去除首尾空格
                        message.erase(0, message.find_first_not_of(" "));
                        message.erase(message.find_last_not_of(" ") + 1);

                        // 改进的hide前缀检测
                        if (message.find(HIDE_PREFIX) == 0) {
                            smartPrint("[Hide] 忽略隐藏消息（MC->QQ）: " + message + "\n", "program", YELLOW);
                            continue;
                        }

                        std::string chatContent = "<" + playerName + ">" + message;
                        smartPrint("[Chat] 检测到玩家聊天: " + chatContent + "\n", "program", BRIGHT_CYAN);
                        addMessageToQueue(chatContent);

                        if (message.find("tp:") == 0) {
                            handleTeleportCommand(playerName, message);
                        }
                        else if (message.find("mode:") == 0) {
                            handleGameModeCommand(playerName, message);
                        }
                        else if (message.find("vali:") == 0) {
                            handleMCValidationCommand(playerName, message);
                        }
                    }
                }
                // 检测玩家加入游戏
                else if (line.find("加入了游戏") != std::string::npos) {
                    size_t pos = line.find("加入了游戏");
                    if (pos != std::string::npos) {
                        std::string playerName = line.substr(0, pos);
                        playerName.erase(playerName.find_last_not_of(" \t\n\r\f\v") + 1);
                        std::string joinMessage = playerName + "加入了游戏";
                        smartPrint("[Case] 玩家加入: " + joinMessage + "\n", "program", GREEN);
                        addMessageToQueue("[MC事件] " + joinMessage);
                    }
                }
                // 检测玩家退出游戏
                else if (line.find("left the game") != std::string::npos) {
                    size_t pos = line.find("left the game");
                    if (pos != std::string::npos) {
                        std::string playerName = line.substr(0, pos);
                        playerName.erase(playerName.find_last_not_of(" \t\n\r\f\v") + 1);
                        std::string leaveMessage = playerName + "退出了游戏";
                        smartPrint("[Case] 玩家退出: " + leaveMessage + "\n", "program", YELLOW);
                        addMessageToQueue("[MC事件] " + leaveMessage);
                    }
                }
            }
            fileSize = currentSize;
            saveFilePosition(actualLogPath, fileSize);
        }

        logFile.close();
        Sleep(1000);
    }

    saveFilePosition(actualLogPath, fileSize);
}

void messageSender(const std::string& groupId) {
    smartPrint("🚀 消息发送线程启动\n", "program", YELLOW);

    while (g_running) {
        std::string message;
        {
            std::lock_guard<std::mutex> lock(g_queueMutex);
            if (!g_messageQueue.empty()) {
                message = g_messageQueue.front();
                g_messageQueue.pop();
            }
        }

        if (!message.empty()) {
            // 检查是否是隐藏消息
            if (message.find(HIDE_PREFIX) == 0) {
                smartPrint("[Hide] 忽略隐藏消息: " + message + "\n", "program", YELLOW);
                continue;
            }
            if (message.find("vali:") == 0) {
                smartPrint("[Hide] 忽略隐藏消息: " + message + "\n", "program", YELLOW);
                continue;
            }

            // 检查是否是Rcon消息（避免循环）
            if (message.find("[Rcon]") != std::string::npos ||
                message.find("[MC事件]") == 0) {
                smartPrint("[Hide] 忽略Rcon/事件消息: " + message + "\n", "program", YELLOW);
                continue;
            }

            if (sendGroupMessage(groupId, message)) {
                smartPrint("[Info] 发送到QQ成功: " + message + "\n", "program", GREEN);
            }
            else {
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_messageQueue.push(message);
                Sleep(2000);
            }
        }
        Sleep(100);
    }
}

// ============================================================================
// 游戏命令功能函数
// ============================================================================

void handleTeleportCommand(const std::string& playerName, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_bindMutex);

    // 检查是否已绑定
    auto it = g_bindInfos.find(playerName);
    if (it == g_bindInfos.end()) {
        sendRunRconCommand("tell " + playerName + " 请先绑定QQ账号才能使用此功能");
        return;
    }

    std::regex tpPattern(R"(tp:\s*(\w+))");
    std::smatch matches;

    // 修正：添加模式参数
    if (std::regex_search(message, matches, tpPattern) && matches.size() >= 2) {
        std::string targetPlayer = matches[1].str();
        sendRunRconCommand("tp " + playerName + " " + targetPlayer);
    }
}

void handleGameModeCommand(const std::string& playerName, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_bindMutex);

    auto it = g_bindInfos.find(playerName);
    if (it == g_bindInfos.end()) {
        sendRunRconCommand("tell " + playerName + " 请先绑定QQ账号才能使用此功能");
        return;
    }

    std::regex modePattern(R"(mode:\s*([0-3]))");
    std::smatch matches;

    if (std::regex_search(message, matches, modePattern) && matches.size() >= 2) {
        std::string modeNum = matches[1].str();
        std::string modeName;

        // 将数字转换为对应的游戏模式名称
        if (modeNum == "0") {
            modeName = "survival";
        }
        else if (modeNum == "1") {
            modeName = "creative";
        }
        else if (modeNum == "2") {
            modeName = "adventure";
        }
        else if (modeNum == "3") {
            modeName = "spectator";
        }
        else {
            sendRunRconCommand("tell " + playerName + " 模式错误，请输入0-3之间的数字");
            return;
        }

        std::string command = "gamemode " + modeName + " " + playerName;
        smartPrint("[Case] 切换游戏模式: " + playerName + " -> " + modeName + "\n", "program", GREEN);
        sendRunRconCommand(command);
    }
    else {
        sendRconCommand("tell " + playerName + " 格式错误，正确格式: mode: 0-3");
    }
}

void handleMCValidationCommand(const std::string& playerName, const std::string& message) {
    smartPrint("[Case] 处理MC验证命令 - 玩家: " + playerName + " 消息: " + message + "\n", "program", CYAN);

    if (message.find("vali:") == 0) {
        std::string inputCode = message.substr(5);
        inputCode.erase(0, inputCode.find_first_not_of(" "));
        inputCode.erase(inputCode.find_last_not_of(" ") + 1);

        if (inputCode.empty()) {
            sendRunRconCommand("tell " + playerName + " 验证码不能为空");
            return;
        }

        smartPrint("[Case] MC玩家输入验证码: " + inputCode + "\n", "program", YELLOW);

        std::string foundQqId;
        {
            std::lock_guard<std::mutex> lock(g_bindMutex);
            for (const auto& vc : g_verificationCodes) {
                if (vc.second.mcName == playerName) {
                    foundQqId = vc.first;
                    smartPrint("[Info] 找到匹配 - QQ: " + foundQqId + " MC: " + playerName + "\n", "program", GREEN);
                    break;
                }
            }
        }  // 锁在这里释放

        if (!foundQqId.empty()) {
            smartPrint("[Info] 转发到QQ: " + foundQqId + "\n", "program", CYAN);
            // 现在调用handleValidationCommand时锁已经释放
            handleValidationCommand(foundQqId, "vali: " + inputCode);
        }
        else {
            smartPrint("[Error] 未找到绑定请求\n", "program", RED);
            sendRunRconCommand("tell " + playerName + " 未找到绑定请求");
        }
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    
    try {
        // 加载配置文件
        if (!g_config.loadFromFile(CONFIG_FILE)) {
            smartPrint("[Warn] 配置文件不存在，创建默认配置文件...\n", "program", YELLOW);
            if (g_config.createDefaultConfig(CONFIG_FILE)) {
                smartPrint("已创建默认配置文件: " + CONFIG_FILE + "\n", "program", GREEN);
                smartPrint("请编辑配置文件后重新启动程序\n", "program", YELLOW);
                return 0;
            }
            else {
                smartPrint("[Warn] 创建配置文件失败，使用默认配置\n", "program", RED);
            }
        }
        else {
            smartPrint("配置文件加载成功: " + CONFIG_FILE + "\n", "program", GREEN);
        }

        // 初始化全局变量（从配置）
        g_targetGroupId = g_config.targetGroupId;
        g_botQQ = g_config.botQQ;
        LogPath = g_config.logPath;
        OpsPath = g_config.opsPath;
        g_rconHost = g_config.rconHost;
        g_rconPort = g_config.rconPort;
        g_rconPassword = g_config.rconPassword;
        // 初始化配置
        initializeConfig();

        // 设置控制台字体
        CONSOLE_FONT_INFOEX fontInfo = { sizeof(CONSOLE_FONT_INFOEX) };
        GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &fontInfo);
        wcscpy_s(fontInfo.FaceName, L"微软雅黑");
        SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &fontInfo);

        // 显示配置信息
        smartPrint("Minecraft服务器Bot\n", "program", BRIGHT_WHITE);
        smartPrint("配置文件: " + CONFIG_FILE + "\n", "program", WHITE);
        smartPrint("RCON: " + g_config.rconHost + ":" + std::to_string(g_config.rconPort) + "\n", "program", WHITE);
        smartPrint("NapCat: " + g_config.napcatHost + ":" + std::to_string(g_config.napcatPort) + "\n", "program", WHITE);
        smartPrint("日志路径: " + g_config.logPath + "\n", "program", WHITE);
        smartPrint("OPS路径: " + g_config.opsPath + "\n", "program", WHITE);
        smartPrint("Bot QQ: " + g_config.botQQ + "\n", "program", WHITE);
        smartPrint("监听群: " + g_config.targetGroupId + "\n", "program", WHITE);

        // 加载数据
        loadBindDataFromFile();
        loadOpList();

        smartPrint("当前已绑定账号数量: " + std::to_string(g_bindInfos.size()) + "\n", "program", WHITE);
        smartPrint("当前OP数量: " + std::to_string(g_opList.size()) + "\n", "program", WHITE);
        smartPrint("按回车键启动服务...\n", "program", BRIGHT_YELLOW);

        std::cin.get();

        // 启动服务
        if (startNapCatProcess()) {
            g_napcatMonitorThread = std::thread(monitorNapCatOutput);
        }

        initRcon();

        // 启动线程 - 使用配置的日志路径
        std::thread logThread(monitorMinecraftLog, g_config.logPath);
        std::thread senderThread(messageSender, g_config.targetGroupId);

        std::thread cleanupThread([]() {
            int counter = 0;
            while (g_running) {
                cleanupExpiredCodes();
                counter++;
                if (counter >= 5) {
                    loadOpList();
                    counter = 0;
                }
                std::this_thread::sleep_for(std::chrono::minutes(1));
            }
            });

        std::thread saveThread([]() {
            while (g_running) {
                std::this_thread::sleep_for(std::chrono::minutes(5));
                saveBindDataToFile();
            }
            });

        
        addMessageToQueue("Foxbell runs successfully");
        forwardToMinecraft("", "", "狐务器娘Foxbell已部署,很高兴为您服务");

        smartPrint("[Start] 服务已启动\n", "program", BRIGHT_GREEN);
        std::cin.get();

        // 关闭服务
        shutdownNapCat();
        g_running = false;
        g_napcatRunning = false;

        smartPrint("程序退出，保存绑定数据...\n", "program", YELLOW);
        saveBindDataToFile();

        // 等待线程结束
        if (g_napcatMonitorThread.joinable()) g_napcatMonitorThread.join();
        if (senderThread.joinable()) senderThread.join();
        if (logThread.joinable()) logThread.join();
        if (cleanupThread.joinable()) cleanupThread.join();
        if (saveThread.joinable()) saveThread.join();

        if (g_hRcon) {
            RconDestroy(g_hRcon);
            g_hRcon = nullptr;
        }
    }
    catch (const std::exception& e) {
        smartPrint("[Error] 主程序异常: " + std::string(e.what()) + "\n", "program", RED);
    }
    catch (...) {
        smartPrint("[Error] 主程序未知异常\n", "program", RED);
    }

    smartPrint("所有服务已停止\n", "program", BRIGHT_WHITE);
	system("pause");
    return 0;
}