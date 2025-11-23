# esp32-blufi-dh-overflow

Dự án nghiên cứu và phân tích lỗ hổng V3L (DH Buffer Overflow) trong ESP32 BLUFI. 
Đây là một phần của dự án học thuật về IoT Intrusion Detection.

## 📖 Mô tả

Dự án này tái tạo và phân tích lỗ hổng buffer overflow trong quá trình xử lý Diffie-Hellman key exchange của ESP32 BLUFI protocol. Lỗ hổng nằm trong file `blufi_security.c` khi xử lý DH parameters từ client.

## 🎯 Mục tiêu

- Tái tạo lỗ hổng DH Buffer Overflow
- Phân tích cơ chế hoạt động của lỗ hổng
- Phát triển công cụ khai thác (proof-of-concept)
- Đề xuất biện pháp khắc phục

## 📁 Cấu trúc dự án

```
esp32-blufi-dh-overflow/
├── blufi/                      # Thư mục chính chứa source code
│   ├── main/
│   │   ├── blufi_security.c    # File chứa lỗ hổng (DH key exchange)
│   │   ├── blufi_example_main.c # Main application
│   │   ├── blufi_init.c        # Bluetooth initialization
│   │   └── blufi_example.h     # Header file
│   ├── build_and_flash.ps1     # Script build/flash cho Windows
│   ├── build_and_flash.sh      # Script build/flash cho Linux/Mac
│   ├── exploit_dh_overflow.py  # Script khai thác lỗ hổng
│   ├── requirements.txt        # Python dependencies
│   ├── HUONG_DAN_BUILD_FLASH.md # Hướng dẫn build và flash
│   └── ATTACK_GUIDE.md         # Hướng dẫn tấn công
└── README.md                   # File này
```

## 🚀 Quick Start

### 1. Build và Flash Firmware

Xem hướng dẫn chi tiết: [HUONG_DAN_BUILD_FLASH.md](blufi/HUONG_DAN_BUILD_FLASH.md)

**Tóm tắt:**
```bash
cd blufi
idf.py set-target esp32s3  # hoặc esp32, esp32c3
idf.py build
idf.py -p COM3 flash monitor  # Windows
idf.py -p /dev/ttyUSB0 flash monitor  # Linux/Mac
```

### 2. Khai thác lỗ hổng

```bash
# Cài đặt dependencies
pip install -r blufi/requirements.txt

# Chạy exploit (thay địa chỉ MAC của ESP32)
python blufi/exploit_dh_overflow.py AA:BB:CC:DD:EE:FF

# Hoặc quét thiết bị
python blufi/exploit_dh_overflow.py scan
```

## 🔍 Lỗ hổng

### Vị trí
- **File**: `blufi/main/blufi_security.c`
- **Function**: `blufi_dh_negotiate_data_handler()`
- **Dòng**: 80-100

### Mô tả

Lỗ hổng xảy ra khi xử lý DH parameters:

1. **Không kiểm tra giới hạn `dh_param_len`**:
   - Giá trị `dh_param_len` được đọc từ data mà không validation
   - Có thể là giá trị rất lớn → gây malloc quá mức hoặc crash

2. **Buffer overflow trong `memcpy()`**:
   ```c
   memcpy(blufi_sec->dh_param, &data[1], blufi_sec->dh_param_len);
   ```
   - Không kiểm tra `len >= dh_param_len + 1`
   - Nếu `data` ngắn hơn `dh_param_len`, sẽ ghi ra ngoài buffer

### Tác động

- **DoS (Denial of Service)**: Crash ESP32
- **Memory Exhaustion**: Gây cạn kiệt bộ nhớ
- **Potential RCE**: Có thể khai thác để thực thi code tùy ý (tùy thuộc vào compiler và protections)

## 🛠️ Công cụ

### Build Scripts
- `build_and_flash.ps1` - Windows PowerShell script
- `build_and_flash.sh` - Linux/Mac bash script

### Exploit Script
- `exploit_dh_overflow.py` - Python script để khai thác lỗ hổng
  - Buffer overflow attack
  - Memory exhaustion attack
  - Custom attack options

## 📺 TFT Display Support

Dự án hỗ trợ hiển thị thông tin BLUFI lên màn hình TFT 2.8 inch (ILI9341):

- **BD ADDR**: Địa chỉ Bluetooth
- **VERSION**: Phiên bản BLUFI  
- **STATUS**: Trạng thái (Ready/Connected/Waiting)
- **BLE/WiFi Status**: Trạng thái kết nối

**Quick Start:**
```bash
# 1. Kết nối màn hình TFT với ESP32 (xem SETUP.md)
# 2. Enable display trong menuconfig
idf.py menuconfig
# Example Configuration -> TFT Display Configuration -> Enable TFT Display

# 3. Build và flash
idf.py build
idf.py -p COM3 flash monitor
```

**Xem hướng dẫn đầy đủ:** [blufi/SETUP.md](blufi/SETUP.md)

## 📚 Tài liệu

- [Hướng dẫn Cấu hình và Sử dụng](blufi/SETUP.md) - File hướng dẫn chính
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [BLUFI Protocol](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/blufi.html)

## ⚠️ Cảnh báo

**CHỈ SỬ DỤNG CHO MỤC ĐÍCH NGHIÊN CỨU VÀ HỌC TẬP**

- Không sử dụng để tấn công các thiết bị không được phép
- Chỉ test trên thiết bị của chính bạn
- Tuân thủ pháp luật về an ninh mạng

## 🔧 Yêu cầu

- ESP-IDF 4.4+
- Python 3.8+
- ESP32/ESP32-S3/ESP32-C3 (hỗ trợ Bluetooth)
- USB cable

## 📝 License

Dự án này chỉ dành cho mục đích nghiên cứu và học tập.

## 🙏 Credits

- ESP-IDF Team - ESP32 framework
- Espressif Systems - Hardware và firmware

## 📧 Liên hệ

Đây là dự án học thuật. Nếu có câu hỏi hoặc đóng góp, vui lòng tạo issue trên GitHub.
