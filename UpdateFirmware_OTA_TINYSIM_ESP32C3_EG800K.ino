#include "Globals.h"
#include "Terminal.h"
#include <Update.h>

//
// ======================================================
// 1. CẤU HÌNH MẠNG & BIẾN TOÀN CỤC
// ======================================================
//

const char* apn      = "";
const char* gprsUser = "";
const char* gprsPass = "";

HardwareSerial SerialAT(1);
TinyGsm        modem(SerialAT);
TinyGsmClient  client(modem);

// URL mặc định (để trống: chỉ set khi dùng lệnh na,60)
String   g_fwHost = "";
uint16_t g_fwPort = 80;
String   g_fwPath = "";

// OTA login cố định
const char* OTA_USER = "nasa";
const char* OTA_PASS = "123456";

//
// ======================================================
// 2. SPIFFS: THÔNG TIN & TIỆN ÍCH CƠ BẢN
// ======================================================
//

SpiffsInfo getSpiffsInfo() {
  SpiffsInfo info{0, 0, 0, false};

  if (!SPIFFS.begin(true)) {
    return info;
  }

  info.total = SPIFFS.totalBytes();
  info.used  = SPIFFS.usedBytes();
  info.freeB = (info.total > info.used) ? (info.total - info.used) : 0;
  info.ok    = true;
  return info;
}

void printSpiffsInfo(const char* tag) {
  auto i = getSpiffsInfo();
  if (!i.ok) {
    Serial.println("[SPIFFS] mount thất bại");
    return;
  }

  Serial.printf("%s total=%u, used=%u, free=%u bytes\n",
                tag,
                (unsigned)i.total,
                (unsigned)i.used,
                (unsigned)i.freeB);
}

//
// ======================================================
// 3. SPIFFS: QUẢN LÝ FILE .BIN (XOÁ FILE CŨ, LIỆT KÊ, GIỚI HẠN SỐ LƯỢNG)
// ======================================================
//

// Xóa file .bin cũ nhất (theo getLastWrite)
bool deleteNearestBin() {
  File root = SPIFFS.open("/");
  if (!root) return false;

  String oldestFile = "";
  time_t oldestTime = LONG_MAX;

  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    String n = f.name();
    if (n.endsWith(".bin")) {
      time_t t = f.getLastWrite();
      if (t < oldestTime) {
        oldestTime = t;
        oldestFile = n;
      }
    }
  }

  if (oldestFile.length()) {
    String path1 = oldestFile;
    String path2 = oldestFile;
    if (!path1.startsWith("/")) path1 = "/" + path1;

    bool ok = SPIFFS.remove(path1);
    if (!ok) {
      ok = SPIFFS.remove(path2);  // thử luôn tên gốc
    }

    if (ok) {
      Serial.printf("[SPIFFS] Đã xóa file .bin cũ nhất: %s (dùng path: %s)\n",
                    oldestFile.c_str(),
                    ok ? path1.c_str() : path2.c_str());
      return true;
    }

    Serial.printf("[SPIFFS] Xóa thất bại: %s (path1=%s, path2=%s)\n",
                  oldestFile.c_str(),
                  path1.c_str(),
                  path2.c_str());
  }

  return false;
}

// In danh sách các file .bin trong SPIFFS
void printBinFiles(const char* tag) {
  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] mount thất bại");
    return;
  }

  File root = SPIFFS.open("/");
  if (!root) {
    Serial.println("[SPIFFS] mở root thất bại");
    return;
  }

  size_t cnt       = 0;
  size_t totalSize = 0;

  Serial.printf("%s Danh sách file .bin:\r\n", tag);

  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    String name = f.name();
    if (name.endsWith(".bin")) {
      size_t sz = f.size();
      Serial.printf("  - %s  (%u bytes)\r\n",
                    name.c_str(),
                    (unsigned)sz);
      cnt++;
      totalSize += sz;
    }
  }

  if (cnt == 0) {
    Serial.printf("%s (không có file .bin)\r\n", tag);
  } else {
    Serial.printf("%s Tổng: %u file, %u bytes\r\n",
                  tag,
                  (unsigned)cnt,
                  (unsigned)totalSize);
  }
}

// Giới hạn số lượng file .bin, nếu vượt thì xóa file cũ nhất
void cleanOldBinFiles(int maxFiles) {
  if (maxFiles <= 0) return;

  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] mount thất bại");
    return;
  }

  // Đếm số file .bin hiện có
  int  count = 0;
  File root  = SPIFFS.open("/");
  if (!root) return;

  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    if (String(f.name()).endsWith(".bin")) {
      count++;
    }
  }

  // Nếu vượt quá → xoá file cũ nhất, giảm dần đến khi <= maxFiles
  while (count > maxFiles) {
    Serial.printf("[SPIFFS] Đang có %d file .bin (giới hạn %d). Tiến hành xoá file cũ nhất...\n",
                  count,
                  maxFiles);

    if (!deleteNearestBin()) {
      Serial.println("[SPIFFS] Lỗi: không xoá được file cũ nhất!");
      break;
    }

    count--;
  }
}

//
// ======================================================
// 4. HTTP (TinyGSM): LẤY CONTENT-LENGTH TỪ HEADER
// ======================================================
//

// Gửi GET 1 lần để đọc Content-Length, sau đó đóng kết nối
int fetchContentLengthViaGETHeader() {
  if (!client.connect(g_fwHost.c_str(), g_fwPort, 30)) {
    Serial.println("[NET] client.connect FAIL");
    return -1;
  }

  String req = String("GET ") + g_fwPath + " HTTP/1.1\r\n" +
               "Host: " + g_fwHost + "\r\n" +
               "Connection: close\r\n\r\n";
  client.print(req);

  int contentLen = -1;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Content-Length:")) {
      line.trim();
      int idx = line.indexOf(':');
      if (idx >= 0) {
        contentLen = line.substring(idx + 1).toInt();
      }
    }
    if (line == "\r") break;  // hết header
  }

  client.stop();
  return contentLen;
}

//
// ======================================================
// 5. DOWNLOAD FILE .BIN từ HTTP VỀ SPIFFS (.part → rename)
// ======================================================
//

bool downloadToSpiffsWithProgress() {
  Serial.println("[SPIFFS] Bắt đầu tải về SPIFFS qua TinyGSM...");
  cleanOldBinFiles(1);   // Giữ tối đa 1 file .bin trước khi tải file mới

  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] mount thất bại");
    return false;
  }

  printSpiffsInfo("[SPIFFS]");

  // Chuẩn bị modem
  SerialAT.begin(115200, SERIAL_8N1, 20, 21);
  delay(3000);

  if (!modem.restart()) {
    Serial.println("[NET] modem.restart FAIL");
    return false;
  }
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("[NET] gprsConnect FAIL");
    return false;
  }

  // 1) Lấy Content-Length
  int contentLen = fetchContentLengthViaGETHeader();
  if (contentLen <= 0) {
    Serial.println("[HTTP] Không đọc được Content-Length (server không gửi). Sẽ cố tải nhưng % tiến độ có thể không chính xác.");
  } else {
    Serial.printf("[HTTP] Content-Length: %d bytes\n", contentLen);
  }

  // 2) Đảm bảo đủ dung lượng: nếu thiếu thì xóa file .bin cũ nhất 1 lần, vẫn thiếu => hủy
  {
    auto i = getSpiffsInfo();
    if (!i.ok) {
      Serial.println("[SPIFFS] mount fail");
      modem.gprsDisconnect();
      return false;
    }

    if (contentLen > 0 && (size_t)contentLen > i.freeB) {
      Serial.printf("[SPIFFS] KHÔNG ĐỦ DUNG LƯỢNG (need=%d, free=%u). Thử xóa file .bin cũ nhất...\n",
                    contentLen,
                    (unsigned)i.freeB);

      if (!deleteNearestBin()) {
        Serial.println("[SPIFFS] Không có hoặc xóa .bin cũ thất bại. Hủy tải.");
        modem.gprsDisconnect();
        return false;
      }

      // kiểm tra lại
      i = getSpiffsInfo();
      if (!i.ok || (contentLen > 0 && (size_t)contentLen > i.freeB)) {
        Serial.printf("[SPIFFS] Sau khi xóa vẫn không đủ (need=%d, free=%u). Hủy tải.\n",
                      contentLen,
                      (unsigned)i.freeB);
        modem.gprsDisconnect();
        return false;
      }

      Serial.println("[SPIFFS] Dung lượng đã đủ, tiếp tục tải...");
    }
  }

  // 3) Kết nối lại để thực sự tải body
  if (!client.connect(g_fwHost.c_str(), g_fwPort, 30)) {
    Serial.println("[NET] client.connect FAIL");
    modem.gprsDisconnect();
    return false;
  }

  // Gửi GET (lần 2) để lấy body và bỏ header
  {
    String req = String("GET ") + g_fwPath + " HTTP/1.1\r\n" +
                 "Host: " + g_fwHost + "\r\n" +
                 "Connection: close\r\n\r\n";
    client.print(req);

    // bỏ header
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }
  }

  // 4) Ghi SPIFFS: lưu tên file từ URL path (ngắn), ghi vào .part
  String fileName   = g_fwPath;
  int    slashIndex = fileName.lastIndexOf('/');
  if (slashIndex >= 0) {
    fileName = fileName.substring(slashIndex + 1);
  }

  String spiffsPath = "/" + fileName;
  String tempPath   = spiffsPath + ".part";

  Serial.printf("[SPIFFS] Tải vào file tạm: %s\n", tempPath.c_str());

  // Nếu còn .part cũ thì xóa
  if (SPIFFS.exists(tempPath)) {
    SPIFFS.remove(tempPath);
  }

  // Mở file TẠM để ghi
  File f = SPIFFS.open(tempPath, FILE_WRITE);
  if (!f) {
    Serial.println("[SPIFFS] mở file tạm ghi thất bại");
    client.stop();
    modem.gprsDisconnect();
    return false;
  }

  uint8_t        buf[1024];
  size_t         totalWritten = 0;
  unsigned long  lastPrint    = millis();
  int            lastPct      = -1;

  while (client.connected() || client.available()) {
    int len = client.read(buf, sizeof(buf));
    if (len < 0) {
      Serial.println("[NET] read error");
      break;
    } else if (len == 0) {
      delay(10);
      continue;
    }

    size_t written = f.write(buf, len);
    if (written != (size_t)len) {
      Serial.printf("[SPIFFS] write mismatch %d/%d (có thể HẾT DUNG LƯỢNG)\n",
                    (int)written,
                    len);
      f.close();
      SPIFFS.remove(tempPath);  // chỉ xóa file tạm
      client.stop();
      modem.gprsDisconnect();
      return false;
    }

    totalWritten += written;

    // In tiến độ theo % nếu biết contentLen
    if (contentLen > 0) {
      int pct = (int)((totalWritten * 100ULL) /
                      (unsigned long long)contentLen);
      if (pct != lastPct && (millis() - lastPrint > 300)) {
        Serial.printf("[SPIFFS] Đã tải %u/%d bytes (%d%%)\n",
                      (unsigned)totalWritten,
                      contentLen,
                      pct);
        lastPct   = pct;
        lastPrint = millis();
      }
    } else {
      // fallback: in mỗi ~1s nếu không biết size
      if (millis() - lastPrint > 1000) {
        Serial.printf("[SPIFFS] Đã tải %u bytes\n",
                      (unsigned)totalWritten);
        lastPrint = millis();
      }
    }
  }

  f.flush();
  f.close();
  client.stop();
  modem.gprsDisconnect();

  // Nếu server có Content-Length mà size thực khác -> XÓA FILE TẠM, coi như FAIL
  if (contentLen > 0 && (int)totalWritten != contentLen) {
    Serial.printf("[SPIFFS] INCOMPLETE: size thực %u != Content-Length %d -> XÓA FILE TẠM\n",
                  (unsigned)totalWritten,
                  contentLen);
    SPIFFS.remove(tempPath);
    printSpiffsInfo("[SPIFFS][AFTER]");
    
    return false;
  }

  // OK đủ dữ liệu: nếu file đích đã tồn tại -> xóa, rồi đổi tên .part -> file thật
  if (SPIFFS.exists(spiffsPath)) {
    Serial.printf("[SPIFFS] Tồn tại sẵn %s -> sẽ ghi đè\n",
                  spiffsPath.c_str());
    SPIFFS.remove(spiffsPath);
  }

  if (!SPIFFS.rename(tempPath, spiffsPath)) {
    Serial.println("[SPIFFS] rename .part -> file thật thất bại");
    SPIFFS.remove(tempPath);
    return false;
  }

  Serial.printf("[SPIFFS] Hoàn tất: %u bytes, lưu tại %s\n",
                (unsigned)totalWritten,
                spiffsPath.c_str());

  Serial.println("\n================== [OTA DOWNLOAD COMPLETE] ==================");
  Serial.printf("File saved : %s\n", spiffsPath.c_str());
  Serial.printf("Size       : %u bytes\n", (unsigned)totalWritten);
  
  auto i = getSpiffsInfo();
  Serial.printf("SPIFFS     : total=%u | used=%u | free=%u bytes\n",
                (unsigned)i.total, (unsigned)i.used, (unsigned)i.freeB);
  
  Serial.println("\n🔍 Keeping only the newest .bin (limit = 1)");
  int beforeCount = 0;
  
  // Đếm trước
  {
    File r = SPIFFS.open("/");
    for (File f = r.openNextFile(); f; f = r.openNextFile()) {
      if (String(f.name()).endsWith(".bin")) beforeCount++;
    }
  }
  
  // Dọn file cũ
  if (beforeCount > 1) {
    Serial.println("🗑 Removing old .bin files...");
    cleanOldBinFiles(1);
  } else {
    Serial.println("✔ Already only one .bin, nothing to delete.");
  }
  
  // In lại danh sách file .bin còn lại
  Serial.println("\n📦 Remaining .bin files:");
  printBinFiles("   •");
  
  Serial.println("==============================================================\n");
  return true;
}

//
// ======================================================
// 6. OTA TRỰC TIẾP TỪ FILE .BIN TRONG SPIFFS
// ======================================================
//

bool applyOtaFromSpiffs(const String& fileName) {
  if (!SPIFFS.begin(true)) {
    Serial.println("[OTA] SPIFFS mount thất bại");
    return false;
  }

  String path = fileName;
  path.trim();
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  File f = SPIFFS.open(path, FILE_READ);
  if (!f) {
    Serial.printf("[OTA] Không mở được file: %s\n", path.c_str());
    return false;
  }

  size_t fsize = f.size();
  if (fsize == 0) {
    Serial.printf("[OTA] File rỗng: %s\n", path.c_str());
    f.close();
    return false;
  }

  Serial.printf("[OTA] Bắt đầu update từ %s (%u bytes)\n",
                path.c_str(),
                (unsigned)fsize);

  if (!Update.begin(fsize)) {  // dùng phân vùng app hiện tại
    Serial.printf("[OTA] Update.begin thất bại, error=%d\n",
                  Update.getError());
    f.close();
    return false;
  }

  const size_t BUF_SIZE = 1024;
  uint8_t      buf[BUF_SIZE];
  size_t       writtenTotal = 0;

  while (f.available()) {
    size_t len = f.read(buf, BUF_SIZE);
    if (len == 0) continue;

    size_t written = Update.write(buf, len);
    if (written != len) {
      Serial.printf("[OTA] Ghi lỗi: %u/%u, error=%d\n",
                    (unsigned)written,
                    (unsigned)len,
                    Update.getError());
      f.close();
      Update.abort();
      return false;
    }
    writtenTotal += written;
  }

  f.close();

  if (!Update.end()) {
    Serial.printf("[OTA] Update.end thất bại, error=%d\n",
                  Update.getError());
    return false;
  }

  if (!Update.isFinished()) {
    Serial.println("[OTA] Update chưa hoàn tất!");
    return false;
  }

  Serial.printf("[OTA] Update OK, đã ghi %u bytes. Khởi động lại...\n",
                (unsigned)writtenTotal);
  delay(500);
  ESP.restart();

  return true;  // thường sẽ không tới đây vì đã restart
}

//
// ======================================================
// 7. FREERTOS TASK
// ======================================================
//

// Task mẫu: chỉ in thông tin SPIFFS định kỳ
static void SpiffsLoadTask(void* pv) {
  printSpiffsInfo("[SPIFFS][BOOT]");

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

void setup() {
  Serial.begin(115200);

  xTaskCreatePinnedToCore(SpiffsLoadTask, "SpiffsLoadTask", 8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(serialCmdTask, "serialCmdTask", 4096, nullptr, 1, nullptr, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
