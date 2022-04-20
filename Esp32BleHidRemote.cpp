#include "Esp32BleHidRemote.h"
#include "hidd_le_prf_int.h"
#include "esp_hidd_prf_api.h"
#include "ble_hid_remote.h"
#include "hid_keymap.h"

using namespace esphome;

std::string Esp32BleHidRemote::name = "HIDDevice";
std::string Esp32BleHidRemote::manufacturer = "ESPHome";
bool Esp32BleHidRemote::connected = false;

Esp32BleHidRemote::Esp32BleHidRemote(std::string p_name, std::string p_manufacturer) : PollingComponent(1000){
    Esp32BleHidRemote::name = "test";
    Esp32BleHidRemote::manufacturer = "test";
    this->connected_binarysensor = new binary_sensor::BinarySensor();
};


void Esp32BleHidRemote::setup(void){
    this->connected_binarysensor->publish_initial_state(connected);
    register_service(&Esp32BleHidRemote::command, "remote_key_command",{"keycode"});
    register_service(&Esp32BleHidRemote::on_keycode, "remote_key",{"keycode","value"});
    initializeNvs();
    initializeBle();
};

void Esp32BleHidRemote::update(void){
    this->connected_binarysensor->publish_state(connected);
};

#define KEYTYPE_INVALID -1
#define KEYTYPE_KEYBOARD 0
#define KEYTYPE_CONSUMER 1

uint8_t Esp32BleHidRemote::getKeyType(std::string key){
    if(strstr(key.c_str(),"HID_KEY")){
        return KEYTYPE_KEYBOARD;
    } else if(strstr(key.c_str(),"HID_CONSUMER")){
        return KEYTYPE_CONSUMER;
    }
    return KEYTYPE_INVALID;
}

void Esp32BleHidRemote::on_keycode(std::string key, bool value){
    if(getKeyType(key) == KEYTYPE_KEYBOARD){
        keyboard_cmd_t cmd_val = get_keyboard_cmd(key.c_str());
        if(cmd_val == 0){
            ESP_LOGW(TAG,"Not a valid keycode");
        } else {
            if(value){
                esp_hidd_send_keyboard_value(hid_conn_id,0, &cmd_val, 1);
            } else {
                esp_hidd_send_keyboard_value(hid_conn_id,0, &cmd_val, 0);
            }
            
            ESP_LOGI(TAG,"Send keyboard key %s (%d), %d", key.c_str(), cmd_val, value);                    
        }
    } else if(getKeyType(key) == KEYTYPE_CONSUMER){
        consumer_cmd_t cmd_val = get_consumer_cmd(key.c_str());
        if(cmd_val == 0){
            ESP_LOGW(TAG,"Not a valid consumer command");
        } else {
            esp_hidd_send_consumer_value(hid_conn_id, cmd_val, value);
            ESP_LOGI(TAG,"Send consumer command %s (%d), %d", key.c_str(), cmd_val, value);
        }
    } else {
        ESP_LOGW(TAG,"Not a valid command");
    }
}





void Esp32BleHidRemote::command(std::string command){
    on_keycode(command, true);
    on_keycode(command, false);

};

binary_sensor::BinarySensor *Esp32BleHidRemote::getConnectedSensor(void){
    return this->connected_binarysensor;
}

void Esp32BleHidRemote::initializeNvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void Esp32BleHidRemote::initializeBle(void){
    esp_err_t ret;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed\n", __func__);
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed\n", __func__);
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluedroid failed\n", __func__);
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluedroid failed\n", __func__);
        return;
    }

    if((ret = esp_hidd_profile_init()) != ESP_OK) {
        ESP_LOGE(TAG, "%s init bluedroid failed\n", __func__);
    }

    ///register the callback function to the gap module
    esp_ble_gap_register_callback(&Esp32BleHidRemote::gap_event_handler);
    esp_hidd_register_callbacks(&Esp32BleHidRemote::hidd_event_callback);

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
     esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
}

void Esp32BleHidRemote::hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch(event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                //esp_bd_addr_t rand_addr = {0x04,0x11,0x11,0x11,0x11,0x05};
                esp_ble_gap_set_device_name(Esp32BleHidRemote::name.c_str());
                esp_ble_gap_config_adv_data(&hidd_adv_data);

            }
            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH:
	     break;
		case ESP_HIDD_EVENT_BLE_CONNECT: {
            ESP_LOGI(TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
            Esp32BleHidRemote::connected = true;
            hid_conn_id = param->connect.conn_id;
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            sec_conn = false;
            ESP_LOGI(TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
            Esp32BleHidRemote::connected = false;
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
            ESP_LOGI(TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
            ESP_LOG_BUFFER_HEX(TAG, param->vendor_write.data, param->vendor_write.length);
        }
        default:
            break;
    }
    return;
}

void Esp32BleHidRemote::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
     case ESP_GAP_BLE_SEC_REQ_EVT:
        for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
             ESP_LOGD(TAG, "%x:",param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
	 break;
     case ESP_GAP_BLE_AUTH_CMPL_EVT:
        sec_conn = true;
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if(!param->ble_security.auth_cmpl.success) {
            ESP_LOGE(TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    default:
        break;
    }
}