#pragma once
#include <Arduino.h>

// Quan trọng: định nghĩa modem trước khi include TinyGSM
#ifndef TINY_GSM_MODEM_BG96
#define TINY_GSM_MODEM_BG96
#endif
#include <TinyGsmClient.h>
#include <FS.h>
#include <SPIFFS.h>

// Cấu hình GPRS (định nghĩa thật ở .ino)
extern const char* apn;
extern const char* gprsUser;
extern const char* gprsPass;

// Modem & TCP Client (định nghĩa thật ở .ino)
extern HardwareSerial SerialAT;
extern TinyGsm        modem;
extern TinyGsmClient  client;

// Link OTA động (định nghĩa thật ở .ino)
extern String   g_fwHost;
extern uint16_t g_fwPort;
extern String   g_fwPath;

// SPIFFS helpers
struct SpiffsInfo {
  size_t total;
  size_t used;
  size_t freeB;
  bool   ok;
};

SpiffsInfo getSpiffsInfo();
void       printSpiffsInfo(const char* tag = "[SPIFFS]");
bool       deleteNearestBin();                         // xóa file .bin cũ nhất
bool       downloadToSpiffsWithProgress();             // dùng g_fwHost/g_fwPort/g_fwPath
void       printBinFiles(const char* tag = "[SPIFFS][BIN]"); // in danh sách .bin

// OTA từ SPIFFS
extern const char* OTA_USER;
extern const char* OTA_PASS;
bool               applyOtaFromSpiffs(const String& fileName);

// Terminal task
void serialCmdTask(void *pv);
