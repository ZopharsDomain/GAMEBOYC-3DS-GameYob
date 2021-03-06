#pragma once

#include "types.h"

bool systemInit(int argc, char* argv[]);
void systemExit();
bool systemIsRunning();
void systemRequestExit();

const std::string systemIniPath();
const std::string systemDefaultBorderPath();
const std::string systemDefaultRomPath();

void systemPrintDebug(const char* fmt, ...);

bool systemGetIRState();
void systemSetIRState(bool state);

const std::string systemGetIP();
int systemSocketListen(u16 port);
FILE* systemSocketAccept(int listeningSocket, std::string* acceptedIp);
FILE* systemSocketConnect(const std::string ipAddress, u16 port);

void systemSetRumble(bool rumble);

u32* systemGetCameraImage();

void systemPrintImage(bool appending, u8* buf, int size, u8 palette);