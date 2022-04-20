#include "esphome.h"
using namespace esphome;

#include <string>
#include "sdkconfig.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_hidd_prf_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include <stdlib.h>

static uint16_t hid_conn_id = 0;
static bool sec_conn = false;
#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
#define TAG "Esp32BleHidRemote"

static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
#pragma GCC diagnostic pop

class Esp32BleHidRemote : public PollingComponent, public api::CustomAPIDevice {
    private:
        static std::string name;
        static std::string manufacturer;
        static bool connected;
    public:
        binary_sensor::BinarySensor *connected_binarysensor;
        Esp32BleHidRemote(std::string p_name, std::string p_manufacturer);
        void setup(void) override;
        void update(void) override;
        void command(std::string command);
        uint8_t getKeyType(std::string key);
        void on_keycode(std::string key, bool value);
        static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);
        static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
        binary_sensor::BinarySensor *getConnectedSensor(void);
        void initializeBle(void);
        void initializeNvs(void);
};




