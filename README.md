# ESP32 BLUFI - Lỗ Hổng DH Buffer Overflow (V3L)

## 📖 Mô tả

Dự án này là một phân tích về **lỗ hổng Buffer Overflow trong quá trình xử lý Diffie-Hellman key exchange** của ESP32 BLUFI protocol. Lỗ hổng này cho phép kẻ tấn công gây ra **Denial of Service (DoS)** hoặc tiềm ẩn **Remote Code Execution (RCE)** thông qua việc gửi các tham số DH không hợp lệ.

**Lỗ hổng:** V3L - DH Buffer Overflow  
**Mức độ nghiêm trọng:** Trung bình - Cao  
**Tác động:** DoS (chắc chắn), RCE (tiềm ẩn)  
**Vị trí:** `main/blufi_security.c` - Hàm `blufi_dh_negotiate_data_handler()`

---

## 📁 Cấu trúc Thư Mục

```
esp32-blufi-dh-overflow/
│
├── main/                           # Thư mục chứa source code chính
│   ├── blufi_example_main.c        # Main application - Xử lý sự kiện BLUFI
│   ├── blufi_example.h             # Header file - Khai báo functions và macros
│   ├── blufi_init.c                # Khởi tạo Bluetooth stack (Bluedroid/NimBLE)
│   ├── blufi_security.c            # ⚠️ CHỨA LỖ HỔNG - Xử lý DH key exchange
│   ├── CMakeLists.txt              # Cấu hình build cho component main
│   └── Kconfig.projbuild           # Cấu hình menuconfig cho project
│
├── build/                          # Thư mục build (tự động tạo)
│   └── ...                         # Files binary sau khi compile
│
├── CMakeLists.txt                  # Root CMakeLists - Cấu hình project
├── sdkconfig                       # File cấu hình ESP-IDF (tự động tạo)
├── sdkconfig.defaults              # Cấu hình mặc định chung
├── sdkconfig.defaults.esp32        # Cấu hình mặc định cho ESP32
├── sdkconfig.defaults.esp32c2      # Cấu hình mặc định cho ESP32-C2
├── sdkconfig.defaults.esp32c3      # Cấu hình mặc định cho ESP32-C3
└── sdkconfig.defaults.esp32s3      # Cấu hình mặc định cho ESP32-S3
```

### 📂 Chi Tiết Các File Chính

#### `main/blufi_security.c` - ⚠️ **FILE CHỨA LỖ HỔNG**

File này xử lý Diffie-Hellman key exchange trong quá trình bảo mật BLUFI:

- **Chức năng chính:**

  - Khởi tạo và quản lý cấu trúc bảo mật (`blufi_security`)
  - Xử lý tham số DH từ client (`blufi_dh_negotiate_data_handler`)
  - Mã hóa/giải mã AES (`blufi_aes_encrypt`, `blufi_aes_decrypt`)
  - Tính checksum CRC (`blufi_crc_checksum`)

- **Lỗ hổng:** Dòng 78-98 trong hàm `blufi_dh_negotiate_data_handler()`

#### `main/blufi_example_main.c`

File main application xử lý các sự kiện BLUFI:

- Khởi tạo WiFi và Bluetooth
- Xử lý các sự kiện BLE (connect/disconnect)
- Xử lý các sự kiện WiFi (connect/disconnect, get IP)
- Xử lý các sự kiện BLUFI (nhận SSID/password, cấu hình WiFi)

#### `main/blufi_init.c`

File khởi tạo Bluetooth stack:

- Hỗ trợ Bluedroid stack (ESP32 chuẩn)
- Hỗ trợ NimBLE stack (ESP32-C3/C6)
- Khởi tạo Bluetooth controller và host
- Đăng ký GAP/GATT callbacks

---

## 🔓 Mô Tả Chi Tiết Lỗ Hổng

### Vị Trí Lỗ Hổng

**File:** `main/blufi_security.c`  
**Hàm:** `blufi_dh_negotiate_data_handler()`  
**Dòng:** 78-98

### Các Lỗ Hổng Cụ Thể

#### 1. **Không Kiểm Tra Giới Hạn `dh_param_len` (Dòng 79)**

```c
case SEC_TYPE_DH_PARAM_LEN:
    blufi_sec->dh_param_len = ((data[1]<<8)|data[2]);  // ⚠️ Lỗ hổng: Không kiểm tra giới hạn
    // ...
    blufi_sec->dh_param = (uint8_t *)malloc(blufi_sec->dh_param_len);  // ⚠️ malloc với size không kiểm tra
```

**Vấn đề:**

- `dh_param_len` được đọc trực tiếp từ `data[1]` và `data[2]` (2 bytes = 0-65535)
- Không có validation để kiểm tra giá trị hợp lệ (0 < dh_param_len < MAX_VALUE)
- Có thể gây `malloc()` với size cực lớn → **Memory Exhaustion** → DoS

**Kiểu tấn công:** Memory Exhaustion Attack

#### 2. **Buffer Overflow trong `memcpy()` (Dòng 98)**

```c
case SEC_TYPE_DH_PARAM_DATA:{
    // ...
    memcpy(blufi_sec->dh_param, &data[1], blufi_sec->dh_param_len);  // ⚠️ Lỗ hổng: Không kiểm tra len thực tế
```

**Vấn đề:**

- `memcpy()` sử dụng `dh_param_len` để copy dữ liệu
- Không kiểm tra độ dài thực tế của `data` (`len` parameter)
- Nếu `data` ngắn hơn `dh_param_len`, sẽ gây **Buffer Overflow** → Ghi đè memory

**Kiểu tấn công:** Buffer Overflow Attack

#### 3. **Thiếu Validation Độ Dài Dữ Liệu**

```c
void blufi_dh_negotiate_data_handler(uint8_t *data, int len, ...)
{
    // ⚠️ Không kiểm tra len >= 3 cho SEC_TYPE_DH_PARAM_LEN
    // ⚠️ Không kiểm tra len >= dh_param_len + 1 cho SEC_TYPE_DH_PARAM_DATA
}
```

**Vấn đề:**

- Hàm nhận `len` nhưng không sử dụng để validate
- Không đảm bảo `data` có đủ dữ liệu trước khi xử lý

---

## 🎯 Kiểu Tấn Công

### 1. **Memory Exhaustion Attack**

**Cách thực hiện:**

- Gửi `SEC_TYPE_DH_PARAM_LEN` với `dh_param_len = 65535` (hoặc giá trị lớn bất kỳ)
- ESP32 sẽ cố gắng `malloc(65535)` → Hết bộ nhớ → Panic → Reboot

**Tác động:**

- ✅ DoS (Denial of Service) - Chắc chắn
- ESP32 crash và reboot liên tục
- Mất kết nối WiFi/BLE

**Ví dụ:**

```
Payload: [0x00, 0xFF, 0xFF]  // SEC_TYPE_DH_PARAM_LEN với length = 65535
Kết quả: malloc(65535) → Hết bộ nhớ → Panic → Reboot
```

### 2. **Buffer Overflow Attack**

**Cách thực hiện:**

1. Gửi `SEC_TYPE_DH_PARAM_LEN` với `dh_param_len = 1024`
2. Gửi `SEC_TYPE_DH_PARAM_DATA` với `data` chỉ có 10 bytes
3. `memcpy()` sẽ copy 1024 bytes từ buffer 10 bytes → Buffer overflow

**Tác động:**

- ✅ Buffer Overflow - Ghi đè memory
- ✅ DoS - Crash ngay lập tức
- ⚠️ RCE (Remote Code Execution) - Tiềm ẩn (phụ thuộc compiler protections)

**Ví dụ:**

```
Bước 1: [0x00, 0x04, 0x00]  // dh_param_len = 1024
Bước 2: [0x01, 0x41, 0x41, ...]  // 10 bytes data, nhưng memcpy copy 1024 bytes
Kết quả: Buffer overflow → Ghi đè memory → Crash
```

### 3. **Heap Corruption Attack**

**Cách thực hiện:**

- Kết hợp cả hai kiểu tấn công trên
- Gây heap corruption bằng cách ghi đè các cấu trúc heap

**Tác động:**

- ✅ Heap Corruption
- ✅ Crash sau một khoảng thời gian
- ⚠️ RCE - Tiềm ẩn (phụ thuộc heap layout)

---

## 🛡️ Cách Khắc Phục

### 1. **Thêm Validation cho `dh_param_len`**

**Vị trí:** `main/blufi_security.c` - Dòng 78-89

**Code trước khi sửa:**

```c
case SEC_TYPE_DH_PARAM_LEN:
    blufi_sec->dh_param_len = ((data[1]<<8)|data[2]);
    // ...
    blufi_sec->dh_param = (uint8_t *)malloc(blufi_sec->dh_param_len);
```

**Code sau khi sửa:**

```c
#define MAX_DH_PARAM_LEN 512  // Giới hạn hợp lý cho DH parameters

case SEC_TYPE_DH_PARAM_LEN:
    // Kiểm tra độ dài input
    if (len < 3) {
        BLUFI_ERROR("Invalid data length for DH_PARAM_LEN\n");
        btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
        return;
    }

    blufi_sec->dh_param_len = ((data[1]<<8)|data[2]);

    // Kiểm tra giới hạn
    if (blufi_sec->dh_param_len <= 0 || blufi_sec->dh_param_len > MAX_DH_PARAM_LEN) {
        BLUFI_ERROR("Invalid dh_param_len: %d (max: %d)\n", blufi_sec->dh_param_len, MAX_DH_PARAM_LEN);
        btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
        return;
    }

    if (blufi_sec->dh_param) {
        free(blufi_sec->dh_param);
        blufi_sec->dh_param = NULL;
    }

    blufi_sec->dh_param = (uint8_t *)malloc(blufi_sec->dh_param_len);
    if (blufi_sec->dh_param == NULL) {
        btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
        BLUFI_ERROR("%s, malloc failed\n", __func__);
        return;
    }
```

### 2. **Thêm Validation cho `memcpy()`**

**Vị trí:** `main/blufi_security.c` - Dòng 91-98

**Code trước khi sửa:**

```c
case SEC_TYPE_DH_PARAM_DATA:{
    // ...
    memcpy(blufi_sec->dh_param, &data[1], blufi_sec->dh_param_len);
```

**Code sau khi sửa:**

```c
case SEC_TYPE_DH_PARAM_DATA:{
    if (blufi_sec->dh_param == NULL) {
        BLUFI_ERROR("%s, blufi_sec->dh_param == NULL\n", __func__);
        btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
        return;
    }

    // ⭐ QUAN TRỌNG: Kiểm tra độ dài thực tế của data
    if (len < blufi_sec->dh_param_len + 1) {
        BLUFI_ERROR("Invalid data length: %d < %d (dh_param_len + 1)\n",
                   len, blufi_sec->dh_param_len + 1);
        btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
        return;
    }

    uint8_t *param = blufi_sec->dh_param;
    memcpy(blufi_sec->dh_param, &data[1], blufi_sec->dh_param_len);
    // ...
```

### 3. **Thêm Validation Tổng Quát ở Đầu Hàm**

**Vị trí:** `main/blufi_security.c` - Đầu hàm `blufi_dh_negotiate_data_handler()`

**Code thêm:**

```c
void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free)
{
    // Kiểm tra input hợp lệ
    if (data == NULL || len < 1) {
        BLUFI_ERROR("Invalid input: data=%p, len=%d\n", data, len);
        btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
        return;
    }

    int ret;
    uint8_t type = data[0];

    // ...
```

### 4. **Sử dụng `memcpy_s()` hoặc `strncpy_s()` (Nếu có)**

**Code khuyến nghị:**

```c
// Thay vì memcpy() không an toàn
// memcpy(blufi_sec->dh_param, &data[1], blufi_sec->dh_param_len);

// Sử dụng memcpy có kiểm tra
size_t copy_len = (len - 1) < blufi_sec->dh_param_len ? (len - 1) : blufi_sec->dh_param_len;
memcpy(blufi_sec->dh_param, &data[1], copy_len);
if (copy_len < blufi_sec->dh_param_len) {
    // Zero padding nếu cần
    memset(blufi_sec->dh_param + copy_len, 0, blufi_sec->dh_param_len - copy_len);
}
```

---

## 🚀 Build và Flash

### Yêu Cầu

- **ESP-IDF** (phiên bản 4.4 trở lên)
- **Python 3.8+**
- **ESP32/ESP32-S3/ESP32-C3** (hỗ trợ Bluetooth)
- **USB cable**

### Các Bước

1. **Thiết lập target:**

```bash
idf.py set-target esp32s3  # hoặc esp32, esp32c3
```

2. **Build:**

```bash
idf.py build
```

3. **Flash và Monitor:**

```bash
idf.py -p COM3 flash monitor  # Windows
idf.py -p /dev/ttyUSB0 flash monitor  # Linux/Mac
```

---

## ⚠️ Cảnh Báo

**CHỈ SỬ DỤNG CHO MỤC ĐÍCH NGHIÊN CỨU VÀ HỌC TẬP**

- Không sử dụng để tấn công các thiết bị không được phép
- Chỉ test trên thiết bị của chính bạn
- Tuân thủ pháp luật về an ninh mạng

**Lưu ý:**

- Lỗ hổng này có thể gây crash/reboot ESP32
- Không có thiệt hại phần cứng vĩnh viễn
- Reset ESP32 để phục hồi

---

## 📚 Tài Liệu Tham Khảo

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [BLUFI Protocol Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/blufi.html)
- [Diffie-Hellman Key Exchange](https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange)
- [Buffer Overflow Attack](https://owasp.org/www-community/vulnerabilities/Buffer_Overflow)

---

## 📝 License

Dự án này chỉ dành cho mục đích nghiên cứu và học tập.
