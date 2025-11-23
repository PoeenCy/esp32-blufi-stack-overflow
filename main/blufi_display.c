/*
 * BLUFI TFT Display Driver Implementation
 * Hỗ trợ ILI9341 2.8 inch TFT với SPI
 * Sử dụng driver đơn giản, dễ tích hợp
 */

#include "blufi_display.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>

#ifdef CONFIG_BLUFI_DISPLAY_ENABLE
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BLUFI_DISPLAY";

// Cấu hình pin
#define TFT_MOSI_PIN      CONFIG_BLUFI_DISPLAY_MOSI_PIN
#define TFT_SCLK_PIN      CONFIG_BLUFI_DISPLAY_SCLK_PIN
#define TFT_CS_PIN        CONFIG_BLUFI_DISPLAY_CS_PIN
#define TFT_DC_PIN        CONFIG_BLUFI_DISPLAY_DC_PIN
#define TFT_RST_PIN       CONFIG_BLUFI_DISPLAY_RST_PIN
#define TFT_BL_PIN        CONFIG_BLUFI_DISPLAY_BL_PIN

// SPI handle
static spi_device_handle_t spi_handle = NULL;
static bool display_initialized = false;

// Frame buffer đơn giản (có thể cải thiện sau)
static uint16_t *frame_buffer = NULL;

// Hàm helper
static void tft_write_cmd(uint8_t cmd) {
    gpio_set_level(TFT_DC_PIN, 0);  // Command mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_transmit(spi_handle, &t);
}

static void tft_write_data(uint8_t* data, int len) {
    gpio_set_level(TFT_DC_PIN, 1);  // Data mode
    if (len == 1) {
        spi_transaction_t t = {
            .length = 8,
            .tx_buffer = data,
        };
        spi_device_transmit(spi_handle, &t);
    } else {
        spi_transaction_t t = {
            .length = len * 8,
            .tx_buffer = data,
        };
        spi_device_transmit(spi_handle, &t);
    }
}

static void tft_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    tft_write_cmd(0x2A); // Column Address Set
    uint8_t data[4] = {
        (x1 >> 8) & 0xFF, x1 & 0xFF,
        (x2 >> 8) & 0xFF, x2 & 0xFF
    };
    tft_write_data(data, 4);
    
    tft_write_cmd(0x2B); // Row Address Set
    data[0] = (y1 >> 8) & 0xFF;
    data[1] = y1 & 0xFF;
    data[2] = (y2 >> 8) & 0xFF;
    data[3] = y2 & 0xFF;
    tft_write_data(data, 4);
    
    tft_write_cmd(0x2C); // Memory Write
}

static void ili9341_init_sequence(void) {
    // Reset
    gpio_set_level(TFT_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TFT_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Initialization commands
    tft_write_cmd(0x01); // Software Reset
    vTaskDelay(pdMS_TO_TICKS(150));

    tft_write_cmd(0xCF);
    uint8_t data1[] = {0x00, 0x83, 0x30};
    tft_write_data(data1, 3);

    tft_write_cmd(0xED);
    uint8_t data2[] = {0x64, 0x03, 0x12, 0x81};
    tft_write_data(data2, 4);

    tft_write_cmd(0xE8);
    uint8_t data3[] = {0x85, 0x01, 0x79};
    tft_write_data(data3, 3);

    tft_write_cmd(0xCB);
    uint8_t data4[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    tft_write_data(data4, 5);

    tft_write_cmd(0xF7);
    uint8_t data5[] = {0x20};
    tft_write_data(data5, 1);

    tft_write_cmd(0xEA);
    uint8_t data6[] = {0x00, 0x00};
    tft_write_data(data6, 2);

    tft_write_cmd(0xC0);
    uint8_t data7[] = {0x26};
    tft_write_data(data7, 1);

    tft_write_cmd(0xC1);
    uint8_t data8[] = {0x11};
    tft_write_data(data8, 1);

    tft_write_cmd(0xC5);
    uint8_t data9[] = {0x35, 0x3E};
    tft_write_data(data9, 2);

    tft_write_cmd(0xC7);
    uint8_t data10[] = {0xBE};
    tft_write_data(data10, 1);

    tft_write_cmd(0x36); // Memory Access Control
    uint8_t data11[] = {0x28};
    tft_write_data(data11, 1);

    tft_write_cmd(0x3A); // Pixel Format Set
    uint8_t data12[] = {0x55}; // 16-bit/pixel
    tft_write_data(data12, 1);

    tft_write_cmd(0xB1);
    uint8_t data13[] = {0x00, 0x1B};
    tft_write_data(data13, 2);

    tft_write_cmd(0xF2);
    uint8_t data14[] = {0x08};
    tft_write_data(data14, 1);

    tft_write_cmd(0x26);
    uint8_t data15[] = {0x01};
    tft_write_data(data15, 1);

    tft_write_cmd(0xE0); // Positive Gamma
    uint8_t data16[] = {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00};
    tft_write_data(data16, 15);

    tft_write_cmd(0xE1); // Negative Gamma
    uint8_t data17[] = {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F};
    tft_write_data(data17, 15);

    tft_write_cmd(0x11); // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(120));

    tft_write_cmd(0x29); // Display On
    vTaskDelay(pdMS_TO_TICKS(20));
}

esp_err_t blufi_display_init(void) {
    ESP_LOGI(TAG, "Initializing TFT Display");

    // Cấu hình GPIO
    gpio_reset_pin(TFT_DC_PIN);
    gpio_set_direction(TFT_DC_PIN, GPIO_MODE_OUTPUT);
    
    gpio_reset_pin(TFT_RST_PIN);
    gpio_set_direction(TFT_RST_PIN, GPIO_MODE_OUTPUT);
    
    if (TFT_BL_PIN >= 0) {
        gpio_reset_pin(TFT_BL_PIN);
        gpio_set_direction(TFT_BL_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(TFT_BL_PIN, 1); // Bật backlight
    }

    // Cấu hình SPI
    spi_bus_config_t buscfg = {
        .mosi_io_num = TFT_MOSI_PIN,
        .sclk_io_num = TFT_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2 + 8,
    };
    
    esp_err_t ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialization failed");
        return ret;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 26 * 1000 * 1000, // 26MHz
        .mode = 0,
        .spics_io_num = TFT_CS_PIN,
        .queue_size = 7,
        .flags = 0,
        .pre_cb = NULL,
    };
    
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed");
        return ret;
    }

    // Khởi tạo màn hình
    ili9341_init_sequence();

    display_initialized = true;
    ESP_LOGI(TAG, "TFT Display initialized successfully");
    return ESP_OK;
}

void blufi_display_clear(uint16_t color) {
    if (!display_initialized) return;
    
    tft_set_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
    
    uint8_t color_bytes[2] = {
        (color >> 8) & 0xFF,
        color & 0xFF
    };
    
    // Fill screen với color
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
        tft_write_data(color_bytes, 2);
    }
}

void blufi_display_text(int x, int y, const char* text, uint16_t color, uint16_t bg_color, int size) {
    // Implementation đơn giản - chỉ hiển thị text cơ bản
    // Có thể cải thiện sau với font bitmap
    (void)x; (void)y; (void)text; (void)color; (void)bg_color; (void)size;
    // TODO: Implement với font bitmap
}

void blufi_display_line_h(int x, int y, int width, uint16_t color) {
    if (!display_initialized) return;
    
    tft_set_window(x, y, x + width - 1, y);
    uint8_t color_bytes[2] = {(color >> 8) & 0xFF, color & 0xFF};
    
    for (int i = 0; i < width; i++) {
        tft_write_data(color_bytes, 2);
    }
}

void blufi_display_show_info(const uint8_t* bd_addr, uint16_t version, const char* status) {
    if (!display_initialized) return;
    
    // Xóa màn hình
    blufi_display_clear(COLOR_BLACK);
    
    // Hiển thị thông tin cơ bản (sẽ cải thiện với font sau)
    ESP_LOGI(TAG, "Displaying BLUFI info: Version=%04X, Status=%s", version, status);
    if (bd_addr) {
        ESP_LOGI(TAG, "BD ADDR: %02X:%02X:%02X:%02X:%02X:%02X",
                 bd_addr[0], bd_addr[1], bd_addr[2],
                 bd_addr[3], bd_addr[4], bd_addr[5]);
    }
}

void blufi_display_update_ble_status(bool connected, const uint8_t* mac_address) {
    if (!display_initialized) return;
    
    ESP_LOGI(TAG, "BLE Status: %s", connected ? "Connected" : "Disconnected");
    if (connected && mac_address) {
        ESP_LOGI(TAG, "Client MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_address[0], mac_address[1], mac_address[2],
                 mac_address[3], mac_address[4], mac_address[5]);
    }
    // TODO: Update display với text rendering
}

void blufi_display_update_wifi_status(const char* ssid, bool connected, const char* ip_addr) {
    if (!display_initialized) return;
    
    ESP_LOGI(TAG, "WiFi Status: %s", connected ? "Connected" : "Disconnected");
    if (ssid) {
        ESP_LOGI(TAG, "SSID: %s", ssid);
    }
    if (connected && ip_addr) {
        ESP_LOGI(TAG, "IP Address: %s", ip_addr);
    }
    // TODO: Update display với text rendering
}

#else
// Nếu không enable display
esp_err_t blufi_display_init(void) {
    return ESP_OK;
}

void blufi_display_clear(uint16_t color) {
    (void)color;
}

void blufi_display_text(int x, int y, const char* text, uint16_t color, uint16_t bg_color, int size) {
    (void)x; (void)y; (void)text; (void)color; (void)bg_color; (void)size;
}

void blufi_display_line_h(int x, int y, int width, uint16_t color) {
    (void)x; (void)y; (void)width; (void)color;
}

void blufi_display_show_info(const uint8_t* bd_addr, uint16_t version, const char* status) {
    (void)bd_addr; (void)version; (void)status;
}

void blufi_display_update_ble_status(bool connected, const uint8_t* mac_address) {
    (void)connected; (void)mac_address;
}

void blufi_display_update_wifi_status(const char* ssid, bool connected, const char* ip_addr) {
    (void)ssid; (void)connected; (void)ip_addr;
}
#endif // CONFIG_BLUFI_DISPLAY_ENABLE
