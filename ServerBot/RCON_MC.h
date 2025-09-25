#pragma once
#ifndef MINECRAFT_RCON_H
#define MINECRAFT_RCON_H

#ifdef MINECRAFTRCON_EXPORTS
#define MINECRAFTRCON_API __declspec(dllexport)
#else
#define MINECRAFTRCON_API __declspec(dllimport)
#endif

#include <string>

// RCON客户端句柄
typedef void* HRCON;

#ifdef __cplusplus
extern "C" {
#endif

    // 创建RCON客户端实例
    MINECRAFTRCON_API HRCON RconCreate();

    // 销毁RCON客户端实例
    MINECRAFTRCON_API void RconDestroy(HRCON hRcon);

    // 连接到服务器
    MINECRAFTRCON_API bool RconConnect(HRCON hRcon, const char* host, int port);

    // 断开连接
    MINECRAFTRCON_API void RconDisconnect(HRCON hRcon);

    // 认证
    MINECRAFTRCON_API bool RconAuthenticate(HRCON hRcon, const char* password);

    // 执行命令
    MINECRAFTRCON_API const char* RconExecuteCommand(HRCON hRcon, const char* command, bool isChinese);

    // 发送中文消息
    MINECRAFTRCON_API bool RconSendChineseMessage(HRCON hRcon, const char* message);

    // 检查是否已连接
    MINECRAFTRCON_API bool RconIsConnected(HRCON hRcon);

    // 检查是否已认证
    MINECRAFTRCON_API bool RconIsAuthenticated(HRCON hRcon);

    // 获取最后错误信息
    MINECRAFTRCON_API const char* RconGetLastError(HRCON hRcon);

    // 释放字符串内存
    MINECRAFTRCON_API void RconFreeString(const char* str);

#ifdef __cplusplus
}
#endif

#endif // MINECRAFT_RCON_H