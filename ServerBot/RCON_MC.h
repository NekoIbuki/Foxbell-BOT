#pragma once
#ifndef MINECRAFT_RCON_H
#define MINECRAFT_RCON_H

#ifdef MINECRAFTRCON_EXPORTS
#define MINECRAFTRCON_API __declspec(dllexport)
#else
#define MINECRAFTRCON_API __declspec(dllimport)
#endif

#include <string>

// RCON�ͻ��˾��
typedef void* HRCON;

#ifdef __cplusplus
extern "C" {
#endif

    // ����RCON�ͻ���ʵ��
    MINECRAFTRCON_API HRCON RconCreate();

    // ����RCON�ͻ���ʵ��
    MINECRAFTRCON_API void RconDestroy(HRCON hRcon);

    // ���ӵ�������
    MINECRAFTRCON_API bool RconConnect(HRCON hRcon, const char* host, int port);

    // �Ͽ�����
    MINECRAFTRCON_API void RconDisconnect(HRCON hRcon);

    // ��֤
    MINECRAFTRCON_API bool RconAuthenticate(HRCON hRcon, const char* password);

    // ִ������
    MINECRAFTRCON_API const char* RconExecuteCommand(HRCON hRcon, const char* command, bool isChinese);

    // ����������Ϣ
    MINECRAFTRCON_API bool RconSendChineseMessage(HRCON hRcon, const char* message);

    // ����Ƿ�������
    MINECRAFTRCON_API bool RconIsConnected(HRCON hRcon);

    // ����Ƿ�����֤
    MINECRAFTRCON_API bool RconIsAuthenticated(HRCON hRcon);

    // ��ȡ��������Ϣ
    MINECRAFTRCON_API const char* RconGetLastError(HRCON hRcon);

    // �ͷ��ַ����ڴ�
    MINECRAFTRCON_API void RconFreeString(const char* str);

#ifdef __cplusplus
}
#endif

#endif // MINECRAFT_RCON_H