# SMART PARKING SYSTEM – SC9021
**Hệ thống bãi đỗ xe thông minh sử dụng Arduino UNO**
## 📌 1. Giới thiệu chung
Dự án **Smart Parking System (SC9021)** là một hệ thống bãi đỗ xe thông minh được xây dựng nhằm:
- Quản lý xe **vào / ra tự động bằng thẻ RFID**
- Phân biệt **xe thường – xe VIP**
- Tính **phí gửi xe tự động theo thời gian**
- Đảm bảo **an toàn cháy nổ (cảm biến lửa & khí gas)**
- Lưu dữ liệu ngay cả khi **mất điện (EEPROM)**
Hệ thống phù hợp cho:
- Bãi giữ xe trường học
- Chung cư
- Văn phòng, tòa nhà nhỏ
- Đồ án học phần / đồ án tốt nghiệp Arduino – IoT
##  2. Chức năng chính
### Quản lý thẻ RFID
- Nhận diện thẻ RFID RC522
- Phân quyền:
  - **Admin (Master Card)**: quản trị hệ thống
  - **VIP**: miễn phí gửi xe
  - **Thường**: tính phí theo thời gian
### Điều khiển cổng tự động
- Servo mở cổng khi xe vào / ra
- Đóng cổng sau khi hoàn tất
### Tính phí gửi xe
- Miễn phí **5 phút đầu**
- Tính phí theo **block thời gian**
- Có **giới hạn trần phí / ngày**
### An toàn cháy nổ
- Cảm biến lửa (Flame sensor) – ưu tiên cao nhất
- Cảm biến khí gas MQ-2
- Tự động mở cổng thoát hiểm
- Buzzer + LED cảnh báo
### Lưu dữ liệu thông minh
- Lưu xe đang gửi vào EEPROM
- Giảm số lần ghi EEPROM để tăng tuổi thọ bộ nhớ
## 3. Phần cứng sử dụng
| STT | Linh kiện |
|----|-----------|
| 1 | Arduino UNO R3 |
| 2 | Module RFID RC522 |
| 3 | Thẻ RFID |
| 4 | Màn hình LCD I2C 16x2 |
| 5 | Servo điều khiển barrier |
| 6 | Module RTC (DS1307 / DS3231) |
| 7 | Cảm biến lửa |
| 8 | Cảm biến khí gas MQ-2 |
| 9 | Buzzer |
| 10 | LED cảnh báo |
## 4. Thư viện sử dụng
Các thư viện Arduino cần thiết:
- `RTClib` – xử lý thời gian thực
- `MFRC522` – đọc thẻ RFID
- `LiquidCrystal_I2C` – LCD I2C
- `Wire` – giao tiếp I2C
- `SPI` – giao tiếp SPI
- `Adafruit BusIO` – phụ trợ cho RTClib
> 📁 Thư viện đã được đính kèm trong thư mục  
> `SC9021 Source_code / SC9021_library`
## 🗂️ 5. Cấu trúc thư mục
Smart-Parking/
│
├── SC9021 Source_code/
│ ├── SC9021_Source/
│ │ └── BLKLab_Code_DIY_Bai_Do_Xe_Thong_Minh/
│ │ └── SCP201_Code_Bai_Do_Xe_Thong_Minh.ino
│ └── SC9021_library/
│ ├── RTClib.zip
│ ├── MFRC522.zip
│ ├── LiquidCrystal_I2C.zip
│ └── ...
│
├── SC9021 Hướng dẫn/
├── SC9021 Giới Thiệu Chung/
└── README.md
yaml
Sao chép mã
## 6. Cấu hình quan trọng trong code
```cpp
#define MASTER_UID        0x4B9C0705   // Thẻ Admin
#define BLOCK_TIME_SEC    1800         // 30 phút / block
#define FEE_PER_BLOCK    5000         // 5.000 VND / block
#define MAX_FEE_PER_DAY  50000        // Trần phí / ngày
7. Hướng dẫn nạp code
Cài Arduino IDE
Chọn Board:
Arduino UNO
Cài các thư viện cần thiết
Mở file:
Sao chép mã
SCP201_Code_Bai_Do_Xe_Thong_Minh.ino
Kết nối Arduino → Upload
8. Nguyên lý hoạt động
Quẹt thẻ RFID
Hệ thống kiểm tra:
Thẻ Admin / VIP / Thường
Mở cổng nếu hợp lệ
Ghi thời gian vào RTC
Khi xe ra:
Tính thời gian gửi
Tính phí (nếu có)
Hiển thị LCD
Luôn ưu tiên kiểm tra cháy & khí gas
9. Hướng phát triển
Kết nối WiFi / ESP32
App quản lý bãi xe
Thanh toán QR / ví điện tử
Camera nhận diện biển số
Lưu dữ liệu Cloud
10. Tác giả
DEV Bùi Mạnh Hưng
Source: Banlinhkien.com
Năm: 2025
11. Ghi chú
Dự án phục vụ mục đích học tập – nghiên cứu – trình diễn.
Có thể mở rộng cho ứng dụng thực tế.
Nếu thấy dự án hữu ích, hãy cho repo một STAR! ⭐
