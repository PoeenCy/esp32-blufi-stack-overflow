/*
 * BLUFI TFT Display Driver
 * Hỗ trợ màn hình TFT 2.8 inch (ILI9341)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Màu sắc (RGB565)
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_ORANGE      0xFDA0
#define COLOR_GRAY        0x7BEF
#define COLOR_DARKGRAY    0x4208

// Cấu hình màn hình (có thể thay đổi trong menuconfig)
#define TFT_WIDTH         240
#define TFT_HEIGHT        320

/**
 * @brief Khởi tạo màn hình TFT
 * @return ESP_OK nếu thành công
 */
esp_err_t blufi_display_init(void);

/**
 * @brief Xóa màn hình với màu nền
 * @param color Màu nền (RGB565)
 */
void blufi_display_clear(uint16_t color);

/**
 * @brief Hiển thị text tại vị trí
 * @param x Tọa độ X
 * @param y Tọa độ Y
 * @param text Chuỗi text
 * @param color Màu chữ
 * @param bg_color Màu nền
 * @param size Kích thước chữ (1-4)
 */
void blufi_display_text(int x, int y, const char* text, uint16_t color, uint16_t bg_color, int size);

/**
 * @brief Vẽ đường kẻ ngang
 * @param x Tọa độ X bắt đầu
 * @param y Tọa độ Y
 * @param width Độ dài
 * @param color Màu
 */
void blufi_display_line_h(int x, int y, int width, uint16_t color);

/**
 * @brief Hiển thị thông tin BLUFI
 * @param bd_addr Địa chỉ Bluetooth (6 bytes)
 * @param version Phiên bản BLUFI
 * @param status Trạng thái ("Init", "Ready", "Connected", etc.)
 */
void blufi_display_show_info(const uint8_t* bd_addr, uint16_t version, const char* status);

/**
 * @brief Cập nhật trạng thái kết nối BLE
 * @param connected true nếu đã kết nối
 * @param mac_address Địa chỉ MAC của client (có thể NULL)
 */
void blufi_display_update_ble_status(bool connected, const uint8_t* mac_address);

/**
 * @brief Cập nhật trạng thái WiFi
 * @param ssid SSID WiFi
 * @param connected true nếu đã kết nối
 * @param ip_addr Địa chỉ IP (có thể NULL)
 */
void blufi_display_update_wifi_status(const char* ssid, bool connected, const char* ip_addr);

#ifdef __cplusplus
}
#endif

