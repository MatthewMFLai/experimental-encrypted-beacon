/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 *
 * @defgroup ble_sdk_app_template_main main.c
 * @{
 * @ingroup ble_sdk_app_template
 * @brief Template project main file.
 *
 * This file contains a template for creating a new application. It has the code necessary to wakeup
 * from button, advertise, get a connection restart advertising on disconnect and if no new
 * connection created go back to system-off mode.
 * It can easily be used as a starting point for creating a new application, the comments identified
 * with 'YOUR_JOB' indicates where and how you can customize.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "app_uart.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_nus.h"
#include "ble_conn_params.h"
#include "boards.h"
#include "softdevice_handler.h"
#include "app_timer.h"
#include "device_manager.h"
#include "pstorage.h"
#include "app_trace.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#include "sensorsim.h"
#include "nrf_gpio.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "nrf_delay.h"
#include "SEGGER_RTT.h"

#include "atcmd.h"
#include "radio_notify.h"
#include "secure_scan.h"
#include "pstore.h"
#include "config_hdlr.h"
#include "uart_reply.h"

#define UART_TX_BUF_SIZE        256                             /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE        256                             /**< UART RX buffer size. */

#define IS_SRVC_CHANGED_CHARACT_PRESENT  1                                          /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define CENTRAL_LINK_COUNT               0                                          /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT            1                                          /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define DEVICE_NAME                      "S-Beacon"                                 /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */
//#define APP_ADV_INTERVAL                 300                                        /**< The advertising interval (in units of 0.625 ms. This value corresponds to 25 ms). */
//#define APP_ADV_TIMEOUT_IN_SECONDS       20                                        /**< The advertising timeout in units of seconds. */
#define APP_ADV_INTERVAL                 300                                        /**< The advertising interval (in units of 0.625 ms. This value corresponds to 100 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS       0                                        /**< The advertising timeout in units of seconds. */
#define APP_ADV_NUS_TIMEOUT_IN_SECONDS   60                                        /**< The advertising timeout in units of seconds. */

#define APP_TIMER_PRESCALER              0                                          /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE          4                                          /**< Size of timer operation queues. */

#define MIN_CONN_INTERVAL                MSEC_TO_UNITS(100, UNIT_1_25_MS)           /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL                MSEC_TO_UNITS(200, UNIT_1_25_MS)           /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                    0                                          /**< Slave latency. */
#define CONN_SUP_TIMEOUT                 MSEC_TO_UNITS(4000, UNIT_10_MS)            /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY    APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER)/**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT     3                                          /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                   1                                          /**< Perform bonding. */
#define SEC_PARAM_MITM                   0                                          /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                   0                                          /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS               0                                          /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES        BLE_GAP_IO_CAPS_NONE                       /**< No I/O capabilities. */
#define SEC_PARAM_OOB                    0                                          /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE           7                                          /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE           16                                         /**< Maximum encryption key size. */

#define APP_BEACON_INFO_LENGTH          0x17                              /**< Total length of information advertised by the Beacon. */
#define APP_ADV_DATA_LENGTH             0x15                              /**< Length of manufacturer specific data in the advertisement. */
#define APP_DEVICE_TYPE                 0x02                              /**< 0x02 refers to Beacon. */
#define APP_MEASURED_RSSI               0xC3                              /**< The Beacon's measured RSSI at 1 meter distance in dBm. */
#define APP_COMPANY_IDENTIFIER          0x0059                            /**< Company identifier for Nordic Semiconductor ASA. as per www.bluetooth.org. */
#define APP_MAJOR_VALUE                 0x01, 0x02                        /**< Major value used to identify Beacons. */ 
#define APP_MINOR_VALUE                 0x03, 0x04                        /**< Minor value used to identify Beacons. */ 
#define APP_BEACON_UUID                 0x00, 0x00, 0x00, 0x00, \
                                        0x00, 0x00, 0x00, 0x00, \
                                        0x00, 0x00, 0x00, 0x00, \
                                        0x00, 0x00, 0x00, 0x00            /**< Proprietary UUID for Beacon. */
										
#define APP_AES_LENGTH          0x10                              /**< Total length for AES encryption. */										
										
#define DEAD_BEEF                        0xDEADBEEF                                 /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

static dm_application_instance_t         m_app_handle;                              /**< Application identifier allocated by device manager */

static ble_nus_t                        m_nus;                                      /**< Structure to identify the Nordic UART Service. */
static uint16_t                         m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current connection. */

static ble_uuid_t                       m_adv_uuids[] = {{BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}};  /**< Universally unique service identifier. */

static uint8_t m_beacon_info[APP_BEACON_INFO_LENGTH] =                    /**< Information advertised by the Beacon. */
{
    APP_DEVICE_TYPE,     // Manufacturer specific information. Specifies the device type in this 
                         // implementation. 
    APP_ADV_DATA_LENGTH, // Manufacturer specific information. Specifies the length of the 
                         // manufacturer specific data in this implementation.
    APP_BEACON_UUID,     // 128 bit UUID value. 
    APP_MAJOR_VALUE,     // Major arbitrary value that can be used to distinguish between Beacons. 
    APP_MINOR_VALUE,     // Minor arbitrary value that can be used to distinguish between Beacons. 
    APP_MEASURED_RSSI    // Manufacturer specific information. The Beacon's measured TX power in 
                         // this implementation. 
};

static uint8_t m_aes128_key[APP_AES_LENGTH] =
{
	0
};

static uint8_t m_beacon_uuid[APP_AES_LENGTH] =
{
	0
};
static uint8_t m_adv_reinit = 0;
static uint32_t m_counter_ticks;
static uint32_t m_fast_adv_interval;
static char m_atcmd_resp_str[PSTORE_MAX_BLOCK + 1];
static uint8_t m_ble_data_src[APP_ATCMD_MAX_DATA_LEN] = {0};
static void advertising_reinit(void);
static void advertising_init(void);
                                   
/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
}


/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    
    // Declearing parameter structs. Try to go to the struct definitions to get
	// more information about what parameters they contain
    ble_gap_conn_params_t   gap_conn_params; 	// Struct to store GAP connection parameters like max min connection interval etc
    ble_gap_conn_sec_mode_t sec_mode;			// Struct to store security parameters 

    // A simple macro that sets the Security Mode and Level bits in sec_mode
	// to require no protection (open link)
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    // Store the device name and security mode in the SoftDevice. Our name is defined to "HelloWorld" in the beginning of this file
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code); // Check for errors

    // Always initialize all fields in structs to zero or you might get unexpected behaviour
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    // Populate the GAP connection parameter struct
    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    // Set GAP Peripheral Preferred Connection Parameters
    // The device use these prefered values when negotiating connection terms with another device
    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
                                          
    // Set appearence										  
	sd_ble_gap_appearance_set(0);
	APP_ERROR_CHECK(err_code);// Check for errors 	
                                          
}


/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

	// test code
	advertising_reinit();
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
	
    // Prepare wakeup buttons.
    //err_code = bsp_btn_ble_sleep_mode_prepare();
    //APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    //err_code = sd_power_system_off();
    //APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;
    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
			
        case BLE_ADV_EVT_IDLE:
			advertising_init();
			err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
			APP_ERROR_CHECK(err_code);
			m_adv_reinit = 1;

            break;
			
        default:
            break;
    }
}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
            {
        case BLE_GAP_EVT_CONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    dm_ble_evt_handler(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    bsp_btn_ble_on_ble_evt(p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
	ble_nus_on_ble_evt(&m_nus, p_ble_evt);
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in] sys_evt  System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
    ble_advertising_on_sys_evt(sys_evt);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;
    
    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;
    
    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);
    
    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);
    
    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);
    
    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:

			//advertising_reinit();
            err_code = ble_advertising_restart_without_whitelist();
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        default:
            break;
    }
}


/**@brief Function for handling the Device Manager events.
 *
 * @param[in] p_evt  Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const * p_handle,
                                           dm_event_t const  * p_event,
                                           ret_code_t        event_result)
{
    APP_ERROR_CHECK(event_result);

#ifdef BLE_DFU_APP_SUPPORT
    if (p_event->event_id == DM_EVT_LINK_SECURED)
    {
        app_context_load(p_handle);
    }
#endif // BLE_DFU_APP_SUPPORT

    return NRF_SUCCESS;
}


/**@brief Function for the Device Manager initialization.
 *
 * @param[in] erase_bonds  Indicates whether bonding information should be cleared from
 *                         persistent storage during initialization of the Device Manager.
 */
static void device_manager_init(bool erase_bonds)
{
    uint32_t               err_code;
    dm_init_param_t        init_param = {.clear_persistent_data = erase_bonds};
    dm_application_param_t register_param;

    // Initialize persistent storage module.
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    err_code = dm_init(&init_param);
    APP_ERROR_CHECK(err_code);

    memset(&register_param.sec_param, 0, sizeof(ble_gap_sec_params_t));

    register_param.sec_param.bond         = SEC_PARAM_BOND;
    register_param.sec_param.mitm         = SEC_PARAM_MITM;
    register_param.sec_param.lesc         = SEC_PARAM_LESC;
    register_param.sec_param.keypress     = SEC_PARAM_KEYPRESS;
    register_param.sec_param.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    register_param.sec_param.oob          = SEC_PARAM_OOB;
    register_param.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    register_param.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    register_param.evt_handler            = device_manager_evt_handler;
    register_param.service_type           = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void nus_advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    ble_advdata_t scanrsp;

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = false;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    memset(&scanrsp, 0, sizeof(scanrsp));
    scanrsp.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    scanrsp.uuids_complete.p_uuids  = m_adv_uuids;

    ble_adv_modes_config_t options = {0};
    options.ble_adv_fast_enabled  = BLE_ADV_FAST_ENABLED;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_NUS_TIMEOUT_IN_SECONDS;

    err_code = ble_advertising_init(&advdata, &scanrsp, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;  // Struct containing advertising parameters
	uint8_t       flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
	
    ble_advdata_manuf_data_t        manuf_data; // Variable to hold manufacturer specific data
    //uint8_t data[]                      = "SomeData!"; // Our data to adverise
    manuf_data.company_identifier       = 0x0059; // Nordics company ID
    //manuf_data.data.p_data              = data;     
    //manuf_data.data.size                = sizeof(data);

	encrypt_128bit_uuid(m_beacon_uuid, m_aes128_key, &m_beacon_info[2], 0);
	manuf_data.data.p_data = (uint8_t *) m_beacon_info;
    manuf_data.data.size   = APP_BEACON_INFO_LENGTH;
	
    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));

    //advdata.name_type               = BLE_ADVDATA_SHORT_NAME;
    //advdata.short_name_len          = 3;
    //advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    advdata.name_type             = BLE_ADVDATA_NO_NAME;
    advdata.flags                 = flags;	
	
    advdata.p_manuf_specific_data   = &manuf_data;
    //int8_t tx_power                 = -4;
    //advdata.p_tx_power_level        = &tx_power;
    //advdata.include_appearance      = true;

    // Prepare the scan response Manufacturer specific data packet
    /*ble_advdata_manuf_data_t                manuf_data_response;
    uint8_t                                 data_response[] = "Many_bytes_of_data"; // Remember there is a 0 terminator at the end of string
    manuf_data_response.company_identifier       = 0x0059;               
    manuf_data_response.data.p_data              = data_response;        
    manuf_data_response.data.size                = sizeof(data_response);

    ble_advdata_t   advdata_response;// Declare and populate a scan response packet

    // Always initialize all fields in structs to zero or you might get unexpected behaviour
    memset(&advdata_response, 0, sizeof(advdata_response));
    // Populate the scan response packet
    advdata_response.name_type               = BLE_ADVDATA_NO_NAME; 
    advdata_response.p_manuf_specific_data   = &manuf_data_response;
	*/

    ble_adv_modes_config_t options = {0};
    options.ble_adv_fast_enabled  = BLE_ADV_FAST_ENABLED;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;

    //err_code = ble_advertising_init(&advdata, &advdata_response, &options, on_adv_evt, NULL);
	err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_reinit(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;  // Struct containing advertising parameters
	uint32_t company_id = 0x0059;
	uint8_t  flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

    ble_advdata_manuf_data_t        manuf_data; // Variable to hold manufacturer specific data
    //uint8_t data[]                      = "DataSome!"; // Our data to adverise
    manuf_data.company_identifier       = company_id; // Nordics company ID
    //manuf_data.data.p_data              = data;     
    //manuf_data.data.size                = sizeof(data);
	encrypt_128bit_uuid(m_beacon_uuid, m_aes128_key, &m_beacon_info[2], m_counter_ticks);
	memcpy(&m_beacon_info[18], &m_counter_ticks, sizeof(m_counter_ticks));
	m_counter_ticks++;
	manuf_data.data.p_data = (uint8_t *) m_beacon_info;
    manuf_data.data.size   = APP_BEACON_INFO_LENGTH;
	
    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));

    //advdata.name_type               = BLE_ADVDATA_SHORT_NAME;
    //advdata.short_name_len          = 3;
    //advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.name_type             = BLE_ADVDATA_NO_NAME;
    advdata.flags                 = flags;
	
    advdata.p_manuf_specific_data   = &manuf_data;
    //int8_t tx_power                 = -4;
    //advdata.p_tx_power_level        = &tx_power;
    //advdata.include_appearance      = true;

    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);
}

static void execute_atcmd(uint16_t index, uint8_t *data_array, char *p_resp_str)
{
	uint16_t param_size;
	char datastr[16] = {0};
	
	memset(p_resp_str, 0, PSTORE_MAX_BLOCK + 1);
	// Execute AT command.
	switch (atcmd_parse(index, (char *)data_array)) {
		case APP_ATCMD_ACT_CONFIG_GET :
			memcpy(p_resp_str, atcmd_get_ok(), strlen(atcmd_get_ok()));
			break;
			
		case APP_ATCMD_ACT_CONFIG_SET :
			memcpy(p_resp_str, atcmd_get_ok(), strlen(atcmd_get_ok()));
			break;
			
		case APP_ATCMD_ACT_CONFIG_GET_VER :
			if (!config_hdlr_get_string("vers", &param_size, datastr))
			{
				strcpy(datastr, "NUL");
			}
			memcpy(p_resp_str, datastr, strlen(datastr));
			break;
			
		default :
			memcpy(p_resp_str, atcmd_get_nack(), strlen(atcmd_get_nack()));
			break;
	}
}

/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to 
 *          a string. The string will be be sent over BLE when the last character received was a 
 *          'new line' i.e '\n' (hex 0x0D) or if the string has reached a length of 
 *          @ref NUS_MAX_DATA_LENGTH.
 */
void uart_event_handle(app_uart_evt_t * p_event)
{
	static uint8_t data_array[APP_ATCMD_MAX_DATA_LEN];
    static uint16_t index = 0;
	
    switch (p_event->evt_type)
    {
        /**@snippet [Handling data from UART] */ 
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;
            if ((data_array[index - 1] == '\r') ||
				(index >= (APP_ATCMD_MAX_DATA_LEN)))
				//(index >= (BLE_NUS_MAX_DATA_LEN)))
            {
                /*while (ble_nus_c_string_send(&m_ble_nus_c, data_array, index) != NRF_SUCCESS)
                {
                    // repeat until sent.
                }*/
				
				// Execute AT command.
				execute_atcmd(index, data_array, m_atcmd_resp_str);
				m_atcmd_resp_str[strlen(m_atcmd_resp_str)] = '\n';
				uart_reply_string(m_atcmd_resp_str);
                index = 0;
            }
            break;
        /**@snippet [Handling data from UART] */ 
        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}

/**@brief Function for initializing the UART.
 */
static void uart_init(void)
{
    uint32_t err_code;

    const app_uart_comm_params_t comm_params =
      {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud57600
      };

    APP_UART_FIFO_INIT(&comm_params,
                        UART_RX_BUF_SIZE,
                        UART_TX_BUF_SIZE,
                        uart_event_handle,
                        APP_IRQ_PRIORITY_LOW,
                        err_code);

    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS,
                                 APP_TIMER_TICKS(100, APP_TIMER_PRESCALER), 
                                 bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}

/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

/**@brief Software interrupt 1 IRQ Handler, handles radio notification interrupts.
 */
void SWI1_IRQHandler(bool radio_evt)
{
	static uint8_t loop = 10;
    if (radio_evt)
    {
        nrf_gpio_pin_toggle(BSP_LED_2); //Toggle the status of the LED on each radio notification event
		if (!loop)
		{
			loop = 10;
			if (m_adv_reinit)
			{
				advertising_reinit();
			}
		}
		else
			loop--;
    }
}

/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_nus    Nordic UART Service structure.
 * @param[in] p_data   Data to be send to UART module.
 * @param[in] length   Length of the data.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_t * p_nus, uint8_t * p_data, uint16_t length)
{
	uint32_t err_code;
	uint16_t cur_len = strlen((char *)m_ble_data_src);
	
	// Execute AT command.
	memcpy(m_ble_data_src + cur_len, p_data, length);
	length += cur_len;
	// Need to have the '\r' or ';' as the terminator!
	if (m_ble_data_src[length-1] == ';')
		m_ble_data_src[length-1] = '\r';
	
	if (m_ble_data_src[length-1] != '\r')
		return;
	
	execute_atcmd(length, m_ble_data_src, m_atcmd_resp_str);
	err_code = ble_nus_string_send(&m_nus, (uint8_t *)m_atcmd_resp_str, strlen(m_atcmd_resp_str));
	if (err_code != NRF_ERROR_INVALID_STATE)
	{
		APP_ERROR_CHECK(err_code);
	}
	// Clear the ble command buffer.
	memset(m_ble_data_src, 0, APP_ATCMD_MAX_DATA_LEN);			
}


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t       err_code;
    ble_nus_init_t nus_init;
    
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;
    
    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for application main entry.
 */
int main(void)
{
	uint8_t config_data_raw[PSTORE_MAX_BLOCK] = {0};
	uint16_t config_size;
    uint32_t err_code;
    bool erase_bonds;
	uint16_t param_size;

    // Initialize.
    timers_init();
    buttons_leds_init(&erase_bonds);
    ble_stack_init();
    device_manager_init(erase_bonds);
    gap_params_init();
    conn_params_init();
	services_init();
    nus_advertising_init();
	
    // Start execution.
    //err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    //APP_ERROR_CHECK(err_code);
	
	// Matt: our code
	// Get config data from internal flash.
	uart_init();
	sscan_init();
	config_hdlr_init();
	pstore_init();
	
	config_size = pstore_get(config_data_raw);
	config_hdlr_parse(config_size, config_data_raw);
	
	// Set scan parameters
	config_hdlr_get_longword("be06", &m_fast_adv_interval);

	if (config_hdlr_get_bcd("be02", &param_size, (char *)m_beacon_uuid))
		sscan_set_device_uuid(0, m_beacon_uuid);
	if (config_hdlr_get_bcd("be05", &param_size, (char *)m_aes128_key))
		sscan_set_encryption_key(0, m_aes128_key);
	
	// Radio notification
	err_code = radio_notification_init(6, NRF_RADIO_NOTIFICATION_TYPE_INT_ON_INACTIVE, NRF_RADIO_NOTIFICATION_DISTANCE_800US);
    APP_ERROR_CHECK(err_code);

	// Use random number for counter_tick initial value.
	// Add some spin loop delay for the random numbers to get "ready".
	nrf_delay_ms(10);
	uint8_t num_rand_bytes_available;
	err_code = sd_rand_application_bytes_available_get(&num_rand_bytes_available);
	APP_ERROR_CHECK(err_code);
	
	if (num_rand_bytes_available >= 4)
	{
		err_code = sd_rand_application_vector_get((uint8_t *)&m_counter_ticks, 4);
		APP_ERROR_CHECK(err_code);
	}
	else
		m_counter_ticks = 0;
	
    // Start execution.
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
	
    // Enter main loop.
    for (;;)
    {
        power_manage();
    }
}

/**
 * @}
 */
