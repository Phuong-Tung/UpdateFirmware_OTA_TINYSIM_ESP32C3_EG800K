#include "Terminal.h"

// printBinFiles() nằm trong .ino
extern void printBinFiles(const char* tag);

// ---------------------------------------------
// URL parser: http://host[:port]/path
// ---------------------------------------------
bool parseHttpUrl(const String& url, String& host, uint16_t& port, String& path) {
  host = "";
  path = "";
  port = 80;

  String u = url;
  u.trim();

  const String prefix = "http://";
  if (!u.startsWith(prefix)) return false;
  u.remove(0, prefix.length());

  int slash = u.indexOf('/');
  if (slash < 0) return false;

  String hostPort = u.substring(0, slash);
  path = u.substring(slash); // include leading '/'

  int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    host = hostPort.substring(0, colon);
    long pv = hostPort.substring(colon + 1).toInt();
    if (pv <= 0 || pv > 65535) return false;
    port = (uint16_t)pv;
  } else {
    host = hostPort;
  }

  return host.length() && path.length();
}

// ---------------------------------------------
// Serial terminal task: only command "na,60"
//   - na,60                       -> show current link
//   - na,60,http://host[:port]/path -> set & download immediately
// ---------------------------------------------
void serialCmdTask(void *pv) {
  String buf;

  for (;;) {
    while (Serial.available()) {
      char c = Serial.read();

      if (c == '\n' || c == '\r') {
        buf.trim();
          // Bỏ ngoặc nếu gõ dạng (fwupdate,....)
        if (buf.startsWith("(") && buf.endsWith(")")) {
          buf = buf.substring(1, buf.length() - 1);
          buf.trim();
        }

        if (buf.startsWith("fwupdate")) {
          // fwupdate,fwhost,OTA_user,OTA_password,file.bin
          String parts[5];
          int idx = 0;
          int start = 0;

          while (idx < 5) {
            int comma = buf.indexOf(',', start);
            if (comma < 0) {
              parts[idx++] = buf.substring(start);
              break;
            } else {
              parts[idx++] = buf.substring(start, comma);
              start = comma + 1;
            }
          }

          // Cần đủ 5 phần
          if (idx < 5) {
            Serial.println("[FWUPDATE] Sai cú pháp. Dùng: (fwupdate,host,user,pass,file.bin)");
          } else {
            String cmd  = parts[0]; cmd.trim();
            String host = parts[1]; host.trim();
            String user = parts[2]; user.trim();
            String pass = parts[3]; pass.trim();
            String file = parts[4]; file.trim();

            if (!cmd.equalsIgnoreCase("fwupdate")) {
              Serial.println("[FWUPDATE] Lệnh không hợp lệ");
            } else {
              // Kiểm tra user/pass
              if (user != OTA_USER || pass != OTA_PASS) {
                Serial.println("[FWUPDATE] Sai OTA user/password!");
              } else {
                Serial.printf("[FWUPDATE] Host: %s, file: %s -> bắt đầu OTA...\r\n",
                              host.c_str(), file.c_str());

                bool ok = applyOtaFromSpiffs(file);
                Serial.printf("[FWUPDATE] Kết quả: %s\r\n", ok ? "THÀNH CÔNG" : "THẤT BẠI");
                }
              }
            }       
          }
          
          else if (buf.startsWith("na,60")) {
          // tìm 2 dấu phẩy đầu: "na" , "60" , "<URL>"
          int p1 = buf.indexOf(',');
          int p2 = (p1 >= 0) ? buf.indexOf(',', p1 + 1) : -1;

          if (p2 > 0 && (p2 + 1) < (int)buf.length()) {
            String url = buf.substring(p2 + 1);
            url.trim();

            String host, path; uint16_t port = 80;
            if (parseHttpUrl(url, host, port, path)) {
              g_fwHost = host;
              g_fwPort = port;
              g_fwPath = path;

              Serial.printf("[NA,60] Cập nhật link: http://%s:%u%s\r\n",
                            g_fwHost.c_str(), g_fwPort, g_fwPath.c_str());

              bool ok = downloadToSpiffsWithProgress();
            } else {
              Serial.println("[NA,60] URL không hợp lệ! Ví dụ: na,60,http://45.117.176.252/NASA_MOTO.V1.bin");
            }
          } else {
            Serial.println("[NA,60] Dùng: na,60,http://host[:port]/path");
          }
        }

        // --------- Lệnh na,61: Xóa toàn bộ file .bin ----------
        else if (buf.startsWith("na,61")) {
            Serial.println("[NA,61] Xóa toàn bộ file .bin trong SPIFFS...");
        
            File root = SPIFFS.open("/");
            if (!root) {
                Serial.println("[NA,61] Lỗi SPIFFS!");
            } else {
                int count = 0;
                for (File f = root.openNextFile(); f; f = root.openNextFile()) {
                    String name = f.name();
                    if (name.endsWith(".bin")) {
                        String path = name;
                        if (!path.startsWith("/")) path = "/" + path;
        
                        if (SPIFFS.remove(path)) {
                            Serial.printf("[NA,61] Đã xóa: %s\n", path.c_str());
                            count++;
                        } else {
                            Serial.printf("[NA,61] Không thể xóa: %s\n", path.c_str());
                        }
                    }
                }
                Serial.printf("[NA,61] Đã xóa tổng cộng %d file .bin\n", count);
            }
        }
        
        
        // --------- Lệnh na,62: Liệt kê file .bin + thông tin dung lượng ----------
        else if (buf.startsWith("na,62")) {
            Serial.println("[NA,62] Kiểm tra SPIFFS và danh sách file .bin\n");
        
            // --- Thông tin SPIFFS ---
            SpiffsInfo info = getSpiffsInfo();
            if (!info.ok) {
                Serial.println("[NA,62] SPIFFS ERROR! Không đọc được thông tin.");
            } else {
                float usedPct = (info.used * 100.0f) / info.total;
        
                Serial.printf("[SPIFFS] Tổng: %u bytes\n",   (unsigned)info.total);
                Serial.printf("[SPIFFS] Đã dùng: %u bytes\n", (unsigned)info.used);
                Serial.printf("[SPIFFS] Còn trống: %u bytes\n", (unsigned)info.freeB);
                Serial.printf("[SPIFFS] Mức sử dụng: %.2f%%\n\n", usedPct);
            }
        
            // --- Liệt kê file .bin trong SPIFFS ---
            Serial.println("[NA,62] Danh sách file .bin:");
        
            File root = SPIFFS.open("/");
            if (!root) {
                Serial.println("  (Không mở được root SPIFFS)");
            } else {
                int count = 0;
                size_t totalSize = 0;
        
                for (File f = root.openNextFile(); f; f = root.openNextFile()) {
                    String name = f.name();
                    if (name.endsWith(".bin")) {
                        unsigned fileSize = f.size();
                        totalSize += fileSize;
                        count++;
        
                        Serial.printf("  - %s  (%u bytes)\n",
                                      name.c_str(),
                                      fileSize);
                    }
                }
        
                if (count == 0) {
                    Serial.println("  (Không có file .bin nào)");
                } else {
                    Serial.printf("\n[NA,62] Tổng số file .bin: %d\n", count);
                    Serial.printf("[NA,62] Tổng dung lượng .bin: %u bytes\n", (unsigned)totalSize);
                }
            }
        }  
        
        else if (buf.length() > 0) {
          Serial.printf("[TERMINAL] Không nhận diện: %s\r\n", buf.c_str());
        }

        buf = ""; // reset
      } else {
        buf += c;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
