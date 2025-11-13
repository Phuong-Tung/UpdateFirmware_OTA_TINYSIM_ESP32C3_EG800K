#pragma once
#include "Globals.h"

// Parse URL dạng http://host[:port]/path
bool parseHttpUrl(const String& url, String& host, uint16_t& port, String& path);

// Serial task: chỉ lệnh na,60
void serialCmdTask(void *pv);
