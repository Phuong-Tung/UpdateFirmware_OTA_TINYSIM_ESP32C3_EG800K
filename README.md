Linh kiện
ESP32-C3 
Module SIM EG800K

RX - 20
TX - 21

Chức năng gồm có 4 lệnh
- na,60,file.bin
=> Dùng để tải file .bin mong muốn về SPIFFS của ESP32-C3

Những chức năng phụ kèm theo

. Giới hạn 1 file .bin nếu tải file thứ 2 thì sẽ xóa file .bin cũ nhất để bảo tồn ổn định dung lượng không bị ngắt quảng khi so sánh dung lượng
. Nếu trong quá trình file .bin quá cao thì sẽ xóa luôn file đầu tiên
Ví dụ: File đầu tiên 600Kb bộ nhớ tổng 1200Kb. File muốn tải là 700Kb thì lập tức sẽ xóa file đầu tiên để đủ bộ nhớ cho file tải mong muốn
. Khi 2 file cùng tên thì file .bin tải sẽ lưu thành .part trước nếu tải 100% thì sẽ gán đè lên file .bin cùng tên hiện có, còn không thì sẽ tự xóa

- na,61
=> Dùng để xóa toàn bộ các file .bin có trong bộ nhớ SPIFFS

- na,62
=> Kiểm tra dung lượng tổng, đã dùng, còn trống và tên các file .bin hiện tại đang nằm trong SPIFFS

- (fwupdate,45.117.176.252,nasa,123456,file.bin)
=> Dùng để lấy file .bin nằm trong bộ nhớ SPIFFS Update OTA cho ESP32-C3 thừa hưởng chức năng của file .bin đó

Ảnh minh họa lệnh na,61
<img width="718" height="174" alt="image" src="https://github.com/user-attachments/assets/9bfab75e-8ebd-408a-a7f0-c2087b2f1654" />





Ảnh minh hoạt lệnh na,62
<img width="625" height="311" alt="image" src="https://github.com/user-attachments/assets/3be2ad6e-104e-4528-b05a-4a90b3ef59c0" />





Ảnh minh họa lệnh na,60,file.bin Tải lần đầu
<img width="663" height="609" alt="image" src="https://github.com/user-attachments/assets/2d323f8a-6a7c-410b-b18b-9ce5b4209ba4" />
<img width="690" height="337" alt="image" src="https://github.com/user-attachments/assets/6506c259-eab8-4479-8a54-8f8ffeee2b89" />





Ảnh minh hoạt lệnh na,60,file.bin Tải lần hai để xóa chỉ giữ 1 file.bin
<img width="656" height="462" alt="image" src="https://github.com/user-attachments/assets/8b1e0c6e-4d90-4215-9ae9-463e6eb0e979" />

<img width="748" height="370" alt="image" src="https://github.com/user-attachments/assets/ac630b39-2118-4102-ad5f-62518dfa7db3" />





Ảnh lưu đồ giải thuật hoạt động của Terminal
<img width="531" height="632" alt="image" src="https://github.com/user-attachments/assets/a5ed8176-bbda-4955-9628-e510d8ebcf47" />
