# ESP32-BLUFI-STACK-OVERFLOW: PHÂN TÍCH VÀ KHAI THÁC LỖ HỔNG TRÀN BỘ ĐỆM (WXR) TRÊN GIAO THỨC BLUFI

## 1. Giới thiệu Đồ án

Đồ án này là một nghiên cứu chuyên sâu về an toàn thông tin hệ thống nhúng, tập trung vào việc phân tích và tái hiện lỗ hổng tràn bộ đệm nghiêm trọng trong giao thức cấu hình không dây BluFi của Espressif Systems.

Lỗ hổng này, được định danh là **NCC-BluFi-Ref-WXR**, tồn tại trong mã nguồn tham chiếu của ESP-IDF (phiên bản v5.0.7 trở về trước) và cho phép một kẻ tấn công trong phạm vi Bluetooth Low Energy (BLE) thực hiện các cuộc tấn công Từ chối Dịch vụ (DoS) hoặc tiềm ẩn nguy cơ Thực thi Mã từ xa (RCE) mà không cần xác thực.

Mục tiêu của Đồ án là:
1.  Tái hiện thành công kịch bản tấn công trên phần cứng ESP32 thực tế.
2.  Phân tích nguyên nhân gốc rễ ở mức mã nguồn C.
3.  Đề xuất và kiểm chứng một bản vá lỗi tối ưu dựa trên nguyên tắc Quản lý Khối Bộ nhớ Thô.

## 2. Mô tả Kỹ thuật Lỗ hổng (NCC-BluFi-Ref-WXR)

| Thuộc tính | Chi tiết |
| :--- | :--- |
| **Giao thức bị ảnh hưởng** | BluFi (Bluetooth Wi-Fi Provisioning) |
| **Bản chất** | Buffer Overflow (Global/Heap-based) |
| **Nguyên nhân gốc** | Lỗi logic khi xử lý sự kiện `ESP\_BLUFI\_EVENT\_RECV\_STA\_SSID` và `ESP\_BLUFI\_EVENT\_RECV\_STA\_PASSWD` Cụ thể, hàm `strncpy` được gọi với tham số độ dài lấy trực tiếp từ dữ liệu đầu vào `param->sta\_ssid.ssid\_len`) thay vì kích thước an toàn của bộ đệm đích `sizeof(sta\_config.sta.ssid)`). |
| **Tác động** | Ghi đè lên bộ nhớ cấu hình, phá hủy Heap Metadata, gây `Từ chối Dịch vụ (DoS)` hoặc `Thực thi Mã từ xa (RCE)`. |
| **Yêu cầu tấn công** | Kẻ tấn công phải ở trong phạm vi sóng BLE (khoảng 10-30m) và không cần xác thực (Unauthenticated Pairing). |

## 3. Cấu trúc Thư mục Đồ án

Cấu trúc thư mục được tổ chức theo tiêu chuẩn của ESP-IDF và bổ sung thêm thư mục `attack` để chứa các công cụ PoC (Proof-of-Concept).

```
esp32-blufi-dh-overflow/
│
├── main/ # Thư mục chứa Source Code chính của Firmware
│ ├── blufi_example_main.c # Main application - Chứa hàm callback lỗi (VULNERABLE)
│ ├── blufi_security.c # Xử lý DH key exchange (Chứa lỗi V3L khác)
│ └── ... # Các file build/config khác của ESP-IDF
│
├── build/ # Thư mục build (Tự động tạo)
├── attack/ # THƯ MỤC CÔNG CỤ TẤN CÔNG (PoC)
│ └── attack.py # Script Python khai thác lỗ hổng WXR (DOS/RCE Payload)
│
├── CMakeLists.txt # Root CMakeLists - Cấu hình project
└── sdkconfig* # Files cấu hình ESP-IDF
```

## 4. Hướng dẫn Thực hiện Tấn công (PoC)

Thí nghiệm được thực hiện trên phiên bản firmware **ESP-IDF v5.0.7** (hoặc phiên bản chưa vá lỗi tương đương).

### 4.1. Chuẩn bị Môi trường

1.  **Phần cứng Mục tiêu:** ESP32 DevKit V1 (hoặc ESP32-S3 DevKit).
2.  **Phần mềm Attacker:** Python 3.x trên Linux/macOS/Windows (cần hỗ trợ Bluetooth BLE).
3.  **Cài đặt thư viện Python:**
    ```bash
    pip install bleak
    ```

### 4.2. Khai thác Lỗ hổng WXR (DoS/RCE)

1.  **Biên dịch và Nạp Firmware Lỗi:**
    *   Sử dụng ESP-IDF v5.0.7 để biên dịch và nạp firmware từ thư mục `main/` lên ESP32.
2.  **Giám sát Log:**
    *   Mở Serial Monitor (ví dụ: PuTTY hoặc `idf.py monitor`) để giám sát log của ESP32.
3.  **Thực thi Tấn công:**
    *   Chạy script tấn công:
        ```bash
        python attack/attack.py
        ```
    *   Script sẽ tự động quét, kết nối và gửi Payload tràn bộ đệm.
4.  **Quan sát Kết quả:**
    *   Log Serial của ESP32 sẽ hiển thị các thông báo lỗi:
        *   `wifi:Config authmode threshold is invalid, 1094795585`
        *   `assert failed: heap\_caps\_free...`
        *   `Backtrace: 0x400...` (CPU Panic / Guru Meditation Error).
    *   Thiết bị sẽ tự động khởi động lại, chứng minh cuộc tấn công DoS thành công.

## 5. Áp dụng Bản vá Tối ưu (Secure Patch)

Để sửa lỗi triệt để, nhóm khuyến nghị áp dụng mô hình quản lý bộ nhớ thô.

**Hành động:** Thay thế logic xử lý trong `case ESP\_BLUFI\_EVENT\_RECV\_STA\_SSID` và các case tương tự bằng hàm Wrapper an toàn, đảm bảo `memset` (vệ sinh), `memcpy` (sao chép an toàn) và gán `\0` thủ công.

**Lợi ích của Bản vá Tối ưu:** Ngăn chặn tuyệt đối `Buffer Overflow` và `Buffer Over-read` mà vẫn đảm bảo tính đúng đắn của dữ liệu.

---

## 📋 Thông tin Dự án

| Thông tin | Chi tiết |
|-----------|---------|
| **Tác giả** | 👨‍💼 Trần Thanh Nhã, 👨‍💼 Trần Hữu Nhan |
| **Loại Đồ án** | 🔒 Đồ án môn học Bảo mật mạng |
| **Mục đích** | 📚 Giáo dục & Nghiên cứu |
| **Năm thực hiện** | 2025 |

### 🛡️ Copyright © 2025
**Được thực hiện bởi:**
- 👤 **Trần Thanh Nhã**
- 👤 **Trần Hữu Nhân** 

*Tất cả quyền được bảo vệ. Dự án này được chia sẻ với mục đích giáo dục và nghiên cứu an toàn thông tin.*

---

## ⚠️ Tuyên bố Trách nhiệm

Dự án này được cung cấp **"AS IS"** cho mục đích **giáo dục và nghiên cứu chỉ dành cho các chuyên gia bảo mật**. 

**Người sử dụng chịu trách nhiệm hoàn toàn về:**
- ✅ Việc lấy phép từ chủ sở hữu thiết bị trước khi kiểm thử
- ✅ Tuân thủ pháp luật địa phương về an toàn thông tin
- ✅ Không sử dụng dụng cụ này cho mục đích trái phép

**Các tác giả không chịu trách nhiệm về bất kỳ thiệt hại nào phát sinh từ việc sử dụng sai mục đích Đồ án này.**

---
*Đồ án được thực hiện nhằm mục đích nghiên cứu và giáo dục. Không khuyến khích sử dụng cho mục đích trái phép.*