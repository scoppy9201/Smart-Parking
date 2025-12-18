// --- KHAI BÁO CÁC THƯ VIỆN CẦN THIẾT - Phần này vào libari tải về các bản đầu là đầu tiên hết nhé 
#include <Wire.h>               // Thư viện giao tiếp I2C (dùng cho màn hình LCD và module thời gian RTC)
#include <RTClib.h>             // Thư viện xử lý thời gian thực cho module DS1307 hoặc DS3231
#include <SPI.h>                // Thư viện giao tiếp chuẩn SPI (dùng cho module RFID)
#include <MFRC522.h>            // Thư viện điều khiển module đọc thẻ từ RFID RC522
#include <LiquidCrystal_I2C.h>  // Thư viện điều khiển màn hình LCD qua giao tiếp I2C
#include <Servo.h>              // Thư viện điều khiển động cơ Servo (đóng/mở barrier)
#include <EEPROM.h>             // Thư viện đọc/ghi bộ nhớ EEPROM nội của vi điều khiển

// Phần 1: Cấu hình hệ thống và định nghĩa chân kết nối 

// Cấu hình tham số hệ thống 
#define MASTER_UID        0x4B9C0705  // Mã UID của thẻ Admin (Thẻ chủ dùng để thêm/xóa VIP và Reset) - Tham số này cần thay đổi theo thẻ 
#define EEPROM_SIGNATURE  0xCAFE      // Chữ ký đặc biệt để kiểm tra xem bộ nhớ đã được khởi tạo chưa 
#define BLOCK_TIME_SEC 1800           // Thời gian 1 block tính phí 
#define FEE_PER_BLOCK 5000            // Phí cho 1 block thời gian (VNĐ)
#define MAX_FEE_PER_DAY 100000        // Mức phí trần trên ngày       

const int MAX_CARS      = 4;          // Tổng số chỗ đỗ xe tối đa của bãi
const int MAX_VIP_SIZE  = 2;          // Số lượng thẻ VIP tối đa được phép đăng ký   
const unsigned long BARRIER_DELAY = 3000; // Thời gian giữ barrier mở (3000ms = 3 giây)

// Định nghĩa chân kết nối (Pin Mapping)
#define PIN_RFID_SS              10            // Chân SS (SDA) của module RFID
#define PIN_RFID_RST             9             // Chân RST của module RFID
#define PIN_SERVO                8             // Chân tín hiệu điều khiển Servo
#define PIN_LED_GREEN            7             // Chân đèn LED Xanh (Báo thành công/Mở cổng)
#define PIN_LED_RED              6             // Chân đèn LED Đỏ (Báo lỗi/Thất bại)
#define PIN_BUZZER               5             // Chân còi chip (Buzzer) báo âm thanh
#define PIN_FLAME                4             // Chân cảm biến lửa (Báo cháy)
#define PIN_MQ2                  3             // Chân cảm biến khí Gas/Khói
#define PIN_RESET_BTN            A0            // Chân nút nhấn cứng (Dùng để Reset dữ liệu gốc)
#define CLOSE_ANGLE_SERVO        90            // Góc đóng của servo
#define OPEN_ANGLE_SERVO         0             // Góc mở của servo

// Khởi tạo các đối tượng điều khiển 
RTC_DS1307 rtc;                       // Đối tượng quản lý thời gian thực
MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST); // Đối tượng quản lý đầu đọc thẻ RFID
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Đối tượng màn hình LCD 
Servo barrierServo;                   // Đối tượng điều khiển thanh chắn barrier

//  Phần 2: Biến toàn cục (Lưu trữ trạng thái hệ thống)

uint32_t parkedUIDs[MAX_CARS];        // Mảng lưu UID của các xe đang gửi trong bãi
uint32_t parkedTimestamps[MAX_CARS];  // Mảng lưu thời gian (Unix time) lúc xe vào
int parkedCount = 0;                  // Biến đếm tổng số xe đang có trong bãi
int vipParkedCount = 0;               // Biến đếm số lượng xe VIP đang gửi

uint32_t vipList[MAX_VIP_SIZE];       // Mảng danh sách các thẻ VIP đã đăng ký
int currentVipCount = 0;              // Số lượng thẻ VIP hiện có trong danh sách

bool needSave = false;                // Cờ báo cầN EEPROM

bool isGasWarningActive = false;      // Cờ báo hiệu trạng thái cảnh báo khí Gas (True = Đang báo động)

// FSM - Trạng thái hệ thống 
enum SystemState {
    STATE_IDLE,        // Chờ quẹt thẻ
    STATE_ENTRY,       // Xe vào
    STATE_EXIT,        // Xe ra
    STATE_ADMIN,       // Chế độ quản trị
    STATE_EMERGENCY    // Cháy / Gas
};
SystemState currentState = STATE_IDLE;

// Phần 3: Câc hàm hỗ trợ hiẻn thị và điều khiển cơ bản 

// Hàm in số có 2 chữ số (ví dụ: 5 -> 05) để hiển thị giờ đẹp hơn
void printTwoDigits(int number) {
    if (number < 10) lcd.print("0");
    lcd.print(number);
}

// Hàm hiển thị thông báo lên màn hình LCD (có thể xóa màn hình và chờ)
void showMessage(const char* line1, const char* line2, int delayMs) {
    lcd.clear();                      // Xóa nội dung cũ
    lcd.setCursor(0, 0); lcd.print(line1); // In dòng 1
    lcd.setCursor(0, 1); lcd.print(line2); // In dòng 2
    if (delayMs > 0) delay(delayMs);  // Chờ một khoảng thời gian nếu cần
}

// Hàm tạo tiếng bíp từ còi chip
void beep(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(PIN_BUZZER, HIGH); delay(100); // Bật còi
        digitalWrite(PIN_BUZZER, LOW);  delay(100); // Tắt còi
    }
}

// Hàm điều khiển đóng mở cổng Barrier
void controlBarrier(bool success) {
    if (success) {
        // Trường hợp thành công: Mở cổng
        digitalWrite(PIN_LED_GREEN, HIGH); 
        digitalWrite(PIN_LED_RED, LOW);
        
        barrierServo.attach(PIN_SERVO); // Kết nối servo
        barrierServo.write(OPEN_ANGLE_SERVO);         // Góc 0 độ: Mở cổng lên
        delay(BARRIER_DELAY);           // Giữ cổng mở trong thời gian quy định
        barrierServo.write(CLOSE_ANGLE_SERVO);          // Góc 90 độ: Đóng cổng xuống
        // Sử dụng millis() -> tránh non-blocking 
        unsigned long t0 = millis();
        while (millis() - t0 < 3000){
            checkFireSafety();
        }
        barrierServo.detach();          // Ngắt kết nối để tránh rung servo
        
        digitalWrite(PIN_LED_GREEN, LOW);
    } else {
        // Trường hợp thất bại: Báo lỗi
        for (int i = 0; i < 3; i++) {
            digitalWrite(PIN_LED_RED, HIGH); beep(1); 
            digitalWrite(PIN_LED_RED, LOW); delay(100);
        }
    }
}

// Hàm cập nhật màn hình trạng thái chờ (Hiển thị số chỗ trống)
void updateIdleScreen() {
    lcd.clear();
    // Dòng 1: Hiển thị chỗ trống cho xe thường
    lcd.setCursor(0, 0); lcd.print("Slot Thuong:"); 
    lcd.print(parkedCount - vipParkedCount); 
    lcd.print("/"); 
    lcd.print(MAX_CARS - currentVipCount);
    
    // Dòng 2: Hiển thị trạng thái khu vực VIP
    lcd.setCursor(0, 1); lcd.print("Slot VIP   :"); 
    if (currentVipCount == 0) {
        lcd.print("0/0"); // Chưa có VIP nào đăng ký
    } else {
        lcd.print(vipParkedCount); lcd.print("/"); lcd.print(currentVipCount);
    }
}

//  PHẦN 4: QUẢN LÝ BỘ NHỚ EEPROM (Lưu dữ liệu khi mất điện)

// Hàm lưu toàn bộ dữ liệu quan trọng vào EEPROM
void saveData() {
    int addr = 0;
    // Sử dụng hàm put để tự động tính toán kích thước kiểu dữ liệu
    EEPROM.put(addr, EEPROM_SIGNATURE);   addr += sizeof(int); // Lưu chữ ký
    EEPROM.put(addr, parkedCount);        addr += sizeof(int); // Lưu số lượng xe
    EEPROM.put(addr, vipParkedCount);     addr += sizeof(int); // Lưu số xe VIP
    EEPROM.put(addr, parkedUIDs);         addr += sizeof(parkedUIDs); // Lưu mảng UID xe
    EEPROM.put(addr, parkedTimestamps);   addr += sizeof(parkedTimestamps); // Lưu mảng thời gian
    EEPROM.put(addr, currentVipCount);    addr += sizeof(int); // Lưu số lượng thẻ VIP
    EEPROM.put(addr, vipList);            // Lưu danh sách thẻ VIP
}

// Hàm tải dữ liệu từ EEPROM khi khởi động lại
void loadData() {
    int addr = 0, signature;
    EEPROM.get(addr, signature); addr += sizeof(int);
    
    // Kiểm tra chữ ký: Nếu khác nhau tức là hệ thống mới lắp hoặc chưa có dữ liệu -> Reset về 0
    if (signature != EEPROM_SIGNATURE) {
        parkedCount = 0; vipParkedCount = 0; currentVipCount = 0;
        memset(parkedUIDs, 0, sizeof(parkedUIDs));
        memset(parkedTimestamps, 0, sizeof(parkedTimestamps));
        memset(vipList, 0, sizeof(vipList));
        saveData(); // Ghi đè dữ liệu trắng vào
    } else {
        // Nếu chữ ký đúng, tiến hành đọc dữ liệu cũ ra
        EEPROM.get(addr, parkedCount);        addr += sizeof(int);
        EEPROM.get(addr, vipParkedCount);     addr += sizeof(int);
        EEPROM.get(addr, parkedUIDs);         addr += sizeof(parkedUIDs);
        EEPROM.get(addr, parkedTimestamps);   addr += sizeof(parkedTimestamps);
        EEPROM.get(addr, currentVipCount);    addr += sizeof(int);
        EEPROM.get(addr, vipList);
        
        // Kiểm tra an toàn: Nếu dữ liệu đọc ra bị sai lệch logic , reset lại danh sách VIP
        if (currentVipCount < 0 || currentVipCount > MAX_VIP_SIZE || currentVipCount > MAX_CARS) {
            currentVipCount = 0; memset(vipList, 0, sizeof(vipList));
        }
    }
}

// Phần 5: Logic nghiệp vụ & quản trị 

// Kiểm tra xem một UID có phải là thẻ VIP hay không
bool isVipCard(uint32_t uid) {
    if (uid == MASTER_UID) return true; // Thẻ Admin cũng được coi là quyền cao nhất
    for (int i = 0; i < MAX_VIP_SIZE; i++) {
        if (vipList[i] == uid) return true;
    }
    return false;
}

bool isButtonPressed() {
    static unsigned long lastPress = 0;
    if (digitalRead(PIN_RESET_BTN) == LOW) {
        if (millis() - lastPress > 300) { // debounce
            lastPress = millis();
            return true;
        }
    }
    return false;
}


// Kiểm tra điều kiện có thể thêm VIP mới không
// Trả về: 0 (OK); 1 (Đầy bộ nhớ VIP); 2 (Không còn slot trong bãi)
int checkVipAddStatus() {
    if (currentVipCount >= MAX_VIP_SIZE) return 1; // Lỗi: Đã đủ số lượng VIP tối đa
    // Logic: Nếu thêm 1 VIP thì tổng số xe hiện tại + số VIP mới phải nhỏ hơn tổng chỗ của bãi
    // Ngăn chặn việc xe thường đang chiếm chỗ khiến VIP mới đăng ký không có chỗ đỗ
    if ((parkedCount - vipParkedCount) + (currentVipCount + 1) > MAX_CARS) return 2; 
    return 0;
}

// Hàm kiểm tra xe có đang gửi không 
// Mục đích: Ngăn chặn việc thay đổi quyền (Thêm/Xóa VIP) khi xe đang nằm trong bãi
bool isCarCurrentlyParked(uint32_t checkUID) {
    for (int i = 0; i < MAX_CARS; i++) {
        // Chỉ kiểm tra những vị trí có dữ liệu (UID khác 0)
        if (parkedUIDs[i] != 0 && parkedUIDs[i] == checkUID) {
            return true; // Tìm thấy xe trong bãi
        }
    }
    return false; // Không tìm thấy
}

// Hàm xử lý thêm thẻ vip an toàn 
void tryAddVip(uint32_t newUID) {
    // Bước 1: Kiểm tra xe đang gửi -> Cấm đổi trạng thái để tránh lỗi tính tiền
    if (isCarCurrentlyParked(newUID)) {
        showMessage("LOI: XE DANG GUI", "RA KHOI BAI TRUOC",0);
        beep(3);
        return;
    }

    // Bước 2: Kiểm tra các điều kiện logic khác (đầy bộ nhớ, hết chỗ)
    int status = checkVipAddStatus();
    if (status == 0) {
        vipList[currentVipCount] = newUID; // Thêm vào danh sách
        currentVipCount++;
        needSave = true; // Lưu ngay vào EEPROM
        digitalWrite(PIN_LED_GREEN, HIGH); beep(1);
        showMessage("DA THEM VIP", "THANH CONG", 1000); 
        digitalWrite(PIN_LED_GREEN, LOW);  
    } else {
        // Báo lỗi tương ứng
        if (status == 1) showMessage("LOI: BO NHO FULL", "GIOI HAN VIP!", 0);
        else if (status == 2) showMessage("LOI: KHONG THEM", "XE THUONG CHIEM!", 0);
        beep(3);
    }
}

// Hàm xử lý xóa thẻ vip an toàn 
void tryRemoveVip(int indexToRemove, uint32_t uidToRemove) {
    // Bước 1: Kiểm tra xe đang gửi -> Cấm xóa VIP khi xe chưa ra bãi
    if (isCarCurrentlyParked(uidToRemove)) {
        showMessage("LOI: XE DANG GUI", "RA KHOI BAI TRUOC", 2000);
        beep(3);
        return;
    }

    // Bước 2: Xóa khỏi mảng và dồn danh sách lên (thuật toán xóa phần tử mảng)
    vipList[indexToRemove] = 0; 
    for (int j = indexToRemove; j < MAX_VIP_SIZE - 1; j++) vipList[j] = vipList[j+1];
    vipList[MAX_VIP_SIZE-1] = 0; // Xóa phần tử cuối cùng
    currentVipCount--;
    
    needSave = true; // Cập nhật EEPROM
    digitalWrite(PIN_LED_RED, HIGH); beep(1); 
    showMessage("DA XOA VIP", "THANH CONG", 1000);
    digitalWrite(PIN_LED_RED, LOW);   
}

// Chế độ quản trị viên (Admin mode)
void handleMasterMode() {
    showMessage("ADMIN MODE", "NUT: RESET ALL", 0);
    beep(3);

    while (true) {
        // Luôn ưu tiên an toàn
        checkFireSafety();

        // 1 Reset hệ thống 
        if (isButtonPressed()) {
            showMessage("DANG RESET...", " ", 1000);

            parkedCount = 0;
            vipParkedCount = 0;
            currentVipCount = 0;

            memset(parkedUIDs, 0, sizeof(parkedUIDs));
            memset(parkedTimestamps, 0, sizeof(parkedTimestamps));
            memset(vipList, 0, sizeof(vipList));

            saveData();

            showMessage("DA XOA DU LIEU", "HE THONG SACH SE", 1000);
            return;
        }

        // 2. Quẹt thẻ quản lý 
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

            uint32_t newUID = 0;
            for (byte i = 0; i < mfrc522.uid.size; i++) {
                newUID = (newUID << 8) | mfrc522.uid.uidByte[i];
            }

            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();

            // Quẹt lại thẻ Admin → Thoát
            if (newUID == MASTER_UID) {
                showMessage("ADMIN MODE", "KET THUC!", 0);
                digitalWrite(PIN_LED_RED, HIGH);
                beep(3);
                digitalWrite(PIN_LED_RED, LOW);
                return;
            }

            // Kiểm tra VIP
            int foundIndex = -1;
            for (int i = 0; i < MAX_VIP_SIZE; i++) {
                if (vipList[i] == newUID) {
                    foundIndex = i;
                    break;
                }
            }

            if (foundIndex != -1) {
                tryRemoveVip(foundIndex, newUID);
            } else {
                tryAddVip(newUID);
            }

            showMessage("ADMIN MODE", "NUT: RESET ALL", 0);
        }
    }
}

// Phần 6: An toàn cháy nổ và quy trình đỗ xe 

// Hàm kiểm tra an toàn (Cháy và Khí Gas) - Được gọi liên tục trong vòng lặp
void checkFireSafety() {
    static unsigned long lastBlink = 0;
    static bool blinkState = false;

    unsigned long now = millis();

    // 1. Phát hiện cháy => ưu tiên cao nhất 
    if (digitalRead(PIN_FLAME) == LOW) {

        if (currentState != STATE_EMERGENCY) {
            currentState = STATE_EMERGENCY;

            barrierServo.attach(PIN_SERVO);
            barrierServo.write(OPEN_ANGLE_SERVO); // Mở cổng thoát hiểm

            lcd.clear();
            lcd.setCursor(0, 0); lcd.print("!!! CHAY !!!");
            lcd.setCursor(0, 1); lcd.print("MO CONG THOAT HIEM");
        }

        // Nhấp nháy còi + LED mỗi 100ms (KHÔNG BLOCK)
        if (now - lastBlink >= 100) {
            lastBlink = now;
            blinkState = !blinkState;

            digitalWrite(PIN_BUZZER, blinkState);
            digitalWrite(PIN_LED_RED, blinkState);
        }

        return; // Chặn mọi xử lý khác
    }

    // 2. Hết cháy => phục hồi 
    if (currentState == STATE_EMERGENCY) {
        barrierServo.write(CLOSE_ANGLE_SERVO);
        barrierServo.detach();

        digitalWrite(PIN_BUZZER, LOW);
        digitalWrite(PIN_LED_RED, LOW);

        currentState = STATE_IDLE;
        isGasWarningActive = false;
        updateIdleScreen();
        return;
    }

    // 3. Cảnh báo khí gas 
    if (digitalRead(PIN_MQ2) == LOW) {
        isGasWarningActive = true;

        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("CANH BAO GAS");
        lcd.setCursor(0, 1); lcd.print("NGUY HIEM!");

        // Nhấp nháy chậm hơn cháy
        if (now - lastBlink >= 300) {
            lastBlink = now;
            blinkState = !blinkState;

            digitalWrite(PIN_BUZZER, blinkState);
            digitalWrite(PIN_LED_RED, blinkState);
        }
    }
    // 4. Hết gas
    else if (isGasWarningActive) {
        isGasWarningActive = false;

        digitalWrite(PIN_BUZZER, LOW);
        digitalWrite(PIN_LED_RED, LOW);

        updateIdleScreen();
    }
}

// Hàm đọc thẻ RFID và trả về UID (trả về 0 nếu không có thẻ)
uint32_t getRFID() {
    // Không có thẻ mới
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return 0;
    }
    uint32_t uid = 0;
    // Duyệt đúng số byte UID (4 / 7 / 10 byte)
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        uid = (uid << 8) | mfrc522.uid.uidByte[i];
    }
    // Kết thúc giao tiếp với thẻ
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return uid;
}


// HÀM XỬ LÝ CHÍNH: QUẢN LÝ XE VÀO / RA
void handleParking(uint32_t rfid) {
    beep(1); // Báo đã nhận thẻ

    // 1. Thẻ admin vào chế độ quản trị 
    if (rfid == MASTER_UID) {
        currentState = STATE_ADMIN;
        handleMasterMode();      // Xử lý thêm/xóa VIP, reset hệ thống
        currentState = STATE_IDLE;
        updateIdleScreen();
        return;
    }

    // 2. Lấy thời gian hiện tại từ RTC 
    DateTime now = rtc.now();
    uint32_t currentUnix = now.unixtime();

    // 3. Kiểm tra xe đã có trong bãi chữa 
    int index = -1;
    for (int i = 0; i < MAX_CARS; i++) {
        if (parkedUIDs[i] == rfid) {
            index = i;
            break;
        }
    }

    bool isVIP = isVipCard(rfid); // Kiểm tra thẻ VIP

    // 4. Trường hợp xe vào (chưa có trong bãi)
    if (index == -1) {
        currentState = STATE_ENTRY;
        bool allowEntry = false;

        // Kiểm tra còn chỗ không 
        if (isVIP) {
            // Xe VIP: chỉ cần tổng xe < MAX_CARS
            if (parkedCount < MAX_CARS) allowEntry = true;
        } else {
            // Xe thường: không được chiếm slot VIP
            if ((parkedCount - vipParkedCount) < (MAX_CARS - currentVipCount)
                && parkedCount < MAX_CARS) {
                allowEntry = true;
            }
        }

        // Nếu hết chỗ 
        if (!allowEntry) {
            showMessage("!!! HET CHO", "VUI LONG CHO", 0);
            controlBarrier(false);
            currentState = STATE_IDLE;
            updateIdleScreen();
            return;
        }

        // Lưu xe vào slot trống 
        for (int i = 0; i < MAX_CARS; i++) {
            if (parkedUIDs[i] == 0) {
                parkedUIDs[i] = rfid;
                parkedTimestamps[i] = currentUnix;
                parkedCount++;
                if (isVIP) vipParkedCount++;

                needSave = true; // Lưu EEPROM

                // Hiển thị thông tin vào bãi 
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(isVIP ? "VIP (VAO)" : "XE THUONG (VAO)");

                lcd.setCursor(0, 1);
                printTwoDigits(now.hour()); lcd.print(":");
                printTwoDigits(now.minute());
                lcd.print(" ");
                printTwoDigits(now.day()); lcd.print("/");
                printTwoDigits(now.month());

                controlBarrier(true); // Mở cổng
                currentState = STATE_IDLE;
                updateIdleScreen();
                return;
            }
        }
    }

    // Trường hợp xe ra đã có trong bãi 
    else {
        currentState = STATE_EXIT;
        // Tính thời gian gửi xe theo giây 
        unsigned long duration = currentUnix - parkedTimestamps[index];
        unsigned long blocks = 0;
        unsigned long fee = 0;
        
        // Chi tính tiền với xe thường 
        if (!isVIP) {
            // Miễn phí 5 phút đầu
            if (duration >= 300) {
                // Tính số block (làm tròn lên)
                blocks = (duration + BLOCK_TIME_SEC - 1) / BLOCK_TIME_SEC;
                // Tính phí
                fee = blocks * FEE_PER_BLOCK;
                // Giới hạn phí tối đa / ngày
                if (fee > MAX_FEE_PER_DAY) {
                    fee = MAX_FEE_PER_DAY;
                }
            }
        }

        // Xóa dữ liệu xe khỏi hệ thống 
        parkedUIDs[index] = 0;
        parkedTimestamps[index] = 0;
        parkedCount--;
        if (isVIP) vipParkedCount--;

        needSave = true;

        // Hiển thị thông tin xe ra 
        lcd.clear();

        if (isVIP) {
            // Xe VIP: miễn phí
            lcd.setCursor(0, 0);
            lcd.print("VIP: MIEN PHI");

            lcd.setCursor(0, 1);
            lcd.print("TG: ");
            lcd.print(duration / 60);
            lcd.print(" phut");
        } else {
            // Xe thường
            lcd.setCursor(0, 0);
            lcd.print("TG: ");
            lcd.print(duration / 60);
            lcd.print(" phut");

            lcd.setCursor(0, 1);
            lcd.print("PHI: ");
            lcd.print(fee);
            lcd.print(" VND");
        }

        controlBarrier(true); // Mở cổng cho xe ra
        currentState = STATE_IDLE;
        updateIdleScreen();
    }
}

//  PHẦN 7: KHỞI TẠO (SETUP) VÀ VÒNG LẶP CHÍNH (LOOP)
void setup() {
    Wire.begin(); // Khởi động giao tiếp I2C
    SPI.begin();  // Khởi động giao tiếp SPI
    
    // Thiết lập chế độ cho các chân IO
    pinMode(PIN_RESET_BTN, INPUT_PULLUP); // Nút nhấn dùng trở kéo lên nội bộ
    pinMode(PIN_FLAME, INPUT);            // Cảm biến là đầu vào
    pinMode(PIN_MQ2, INPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);       // Đèn/Còi là đầu ra
    pinMode(PIN_LED_RED, OUTPUT); 
    pinMode(PIN_BUZZER, OUTPUT); 

    // Khởi động màn hình LCD
    lcd.init(); lcd.backlight(); 
    showMessage("HE THONG GIU XE", "--BANLINHKIEN--", 1000);
    
    // Khởi động module thời gian thực RTC
    rtc.begin(); 
    if (!rtc.isrunning()) {
        rtc.adjust(DateTime(__DATE__, __TIME__)); // Cập nhật giờ từ máy tính nếu RTC chưa chạy
    }

    mfrc522.PCD_Init(); // Khởi động module RFID
    
    // Đặt trạng thái ban đầu cho Servo (Đóng cổng)
    barrierServo.attach(PIN_SERVO); 
    barrierServo.write(CLOSE_ANGLE_SERVO); 
    delay(500); 
    barrierServo.detach();

    loadData(); // Tải dữ liệu cũ từ EEPROM lên
    updateIdleScreen(); // Hiển thị màn hình chờ
}

void loop() {
    // Ưu tiên an toàn 
    checkFireSafety();

    // Nếu đang khẩn cấp → chặn toàn bộ xử lý khác
    if (currentState == STATE_EMERGENCY) {
        return;
    }

    // Xử lý RFID khi hệ thống rảnh 
    if (currentState == STATE_IDLE && !isGasWarningActive) {
        uint32_t uid = getRFID();
        if (uid != 0) {
            handleParking(uid);
        }
    }

    // Ghi EEPROM tập trung 
    if (needSave) {
        saveData();
        needSave = false;
    }
}
