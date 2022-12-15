/* Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"


/* Azure Provisioning/IoT Hub library includes */
#include "azure_iot_hub_client.h"
#include "azure_iot_hub_client_properties.h"

#include "sample_azure_iot_pnp_data_if.h"

/* Azure Device Update */
#include <azure/iot/az_iot_adu_client.h>

#include "sensor_manager.h"
#include "azure_iot_freertos_esp32_sensors_data.h"
/*-----------------------------------------------------------*/

#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR     1

#if CONFIG_SAMPLE_IOT_WIFI_SCAN_METHOD_FAST
    #define SAMPLE_IOT_WIFI_SCAN_METHOD    WIFI_FAST_SCAN
#elif CONFIG_SAMPLE_IOT_WIFI_SCAN_METHOD_ALL_CHANNEL
    #define SAMPLE_IOT_WIFI_SCAN_METHOD    WIFI_ALL_CHANNEL_SCAN
#endif

#if CONFIG_SAMPLE_IOT_WIFI_CONNECT_AP_BY_SIGNAL
    #define SAMPLE_IOT_WIFI_CONNECT_AP_SORT_METHOD    WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_SAMPLE_IOT_WIFI_CONNECT_AP_BY_SECURITY
    #define SAMPLE_IOT_WIFI_CONNECT_AP_SORT_METHOD    WIFI_CONNECT_AP_BY_SECURITY
#endif

#if CONFIG_SAMPLE_IOT_WIFI_AUTH_OPEN
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_OPEN
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WEP
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WEP
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA2_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA2_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA_WPA2_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA2_ENTERPRISE
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA2_ENTERPRISE
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA3_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA3_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WPA2_WPA3_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_SAMPLE_IOT_WIFI_AUTH_WAPI_PSK
    #define SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD    WIFI_AUTH_WAPI_PSK
#endif /* if CONFIG_SAMPLE_IOT_WIFI_AUTH_OPEN */

#define INDEFINITE_TIME                                 ( ( time_t ) -1 )

#define SNTP_SERVER_FQDN                                "pool.ntp.org"

// #define OLED_SPLASH_MESSAGE                        "ESP32 ADU v" democonfigADU_UPDATE_VERSION
#define OLED_SPLASH_MESSAGE                        "OTA v" democonfigADU_UPDATE_VERSION
/*-----------------------------------------------------------*/

static const char * TAG = "sample_azureiotkit";

static bool xTimeInitialized = false;

static xSemaphoreHandle xSemphGetIpAddrs;
static esp_ip4_addr_t xIpAddress;


/*-----------------------------------------------------------*/
// NVS Storage for device specific data.
/*-----------------------------------------------------------*/

static char *m_wifi_ssid_string;
static char *m_wifi_pw_string;

static char *m_iot_fqdn_string;
static char *m_iot_device_id_string;
static char *m_iot_device_nr_string;
static char *m_iot_device_key_string;

/*-----------------------------------------------------------*/

extern void vStartDemoTask( char* iot_fqdn_string, char* iot_device_id_string,  char* iot_device_key_string );

/*-----------------------------------------------------------*/

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created within common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
static bool prvIsOurNetif( const char * pcPrefix,
                           esp_netif_t * pxNetif )
{
    return strncmp( pcPrefix, esp_netif_get_desc( pxNetif ), strlen( pcPrefix ) - 1 ) == 0;
}
/*-----------------------------------------------------------*/

static void prvOnGotIpAddress( void * pvArg,
                               esp_event_base_t xEventBase,
                               int32_t lEventId,
                               void * pvEventData )
{
    ip_event_got_ip_t * pxEvent = ( ip_event_got_ip_t * ) pvEventData;

    if( !prvIsOurNetif( TAG, pxEvent->esp_netif ) )
    {
        ESP_LOGW( TAG, "Got IPv4 from another interface \"%s\": ignored",
                  esp_netif_get_desc( pxEvent->esp_netif ) );
        return;
    }

    ESP_LOGI( TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR,
              esp_netif_get_desc( pxEvent->esp_netif ), IP2STR( &pxEvent->ip_info.ip ) );
    memcpy( &xIpAddress, &pxEvent->ip_info.ip, sizeof( xIpAddress ) );
    xSemaphoreGive( xSemphGetIpAddrs );
}
/*-----------------------------------------------------------*/

static void prvOnWifiDisconnect( void * pvArg,
                                 esp_event_base_t xEventBase,
                                 int32_t lEventId,
                                 void * pvEventData )
{
    ESP_LOGI( TAG, "Wi-Fi disconnected, trying to reconnect..." );
    esp_err_t xError = esp_wifi_connect();

    if( xError == ESP_ERR_WIFI_NOT_STARTED )
    {
        ESP_LOGE( TAG, "Failed connecting to Wi-Fi" );
        return;
    }

    ESP_ERROR_CHECK( xError );
}
/*-----------------------------------------------------------*/

static esp_netif_t * prvGetExampleNetifFromDesc( const char * pcDesc )
{
    esp_netif_t * pxNetif = NULL;
    char * pcExpectedDesc;

    asprintf( &pcExpectedDesc, "%s: %s", TAG, pcDesc );

    while( ( pxNetif = esp_netif_next( pxNetif ) ) != NULL )
    {
        if( strcmp( esp_netif_get_desc( pxNetif ), pcExpectedDesc ) == 0 )
        {
            break;
        }
    }

    free( pcExpectedDesc );
    return pxNetif;
}
/*-----------------------------------------------------------*/

static esp_netif_t * prvWifiStart( void )
{
    char * pcDesc;
    wifi_init_config_t xWifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init( &xWifiInitConfig ) );

    esp_netif_inherent_config_t xEspNetifConfig = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    /* Prefix the interface description with the module TAG */
    /* Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask) */
    asprintf( &pcDesc, "%s: %s", TAG, xEspNetifConfig.if_desc );
    xEspNetifConfig.if_desc = pcDesc;
    xEspNetifConfig.route_prio = 128;
    esp_netif_t * netif = esp_netif_create_wifi( WIFI_IF_STA, &xEspNetifConfig );
    free( pcDesc );
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT,
                                                 WIFI_EVENT_STA_DISCONNECTED, &prvOnWifiDisconnect, NULL ) );
    ESP_ERROR_CHECK( esp_event_handler_register( IP_EVENT,
                                                 IP_EVENT_STA_GOT_IP, &prvOnGotIpAddress, NULL ) );
    #ifdef CONFIG_EXAMPLE_CONNECT_IPV6
        ESP_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT,
                                                     WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, netif ) );
        ESP_ERROR_CHECK( esp_event_handler_register( IP_EVENT,
                                                     IP_EVENT_GOT_IP6, &prvOnGotIpAddressv6, NULL ) );
    #endif

    ESP_ERROR_CHECK( esp_wifi_set_storage( WIFI_STORAGE_RAM ) );



    // Use SSID and password stored in NVS.
    wifi_config_t xWifiConfig =
    {
        .sta                    =
        {
            .scan_method        = SAMPLE_IOT_WIFI_SCAN_METHOD,
            .sort_method        = SAMPLE_IOT_WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi     = CONFIG_SAMPLE_IOT_WIFI_SCAN_RSSI_THRESHOLD,
            .threshold.authmode = SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    strcpy(&xWifiConfig.sta.ssid, m_wifi_ssid_string);
    strcpy(&xWifiConfig.sta.password, m_wifi_pw_string);



    // wifi_config_t xWifiConfig =
    // {
    //     .sta                    =
    //     {
    //         .ssid               = CONFIG_SAMPLE_IOT_WIFI_SSID,
    //         .password           = CONFIG_SAMPLE_IOT_WIFI_PASSWORD,
    //         .scan_method        = SAMPLE_IOT_WIFI_SCAN_METHOD,
    //         .sort_method        = SAMPLE_IOT_WIFI_CONNECT_AP_SORT_METHOD,
    //         .threshold.rssi     = CONFIG_SAMPLE_IOT_WIFI_SCAN_RSSI_THRESHOLD,
    //         .threshold.authmode = SAMPLE_IOT_WIFI_SCAN_AUTH_MODE_THRESHOLD,
    //     },
    // };


    ESP_LOGI( TAG, "Connecting to %s...", xWifiConfig.sta.ssid );
    ESP_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
    ESP_ERROR_CHECK( esp_wifi_set_config( WIFI_IF_STA, &xWifiConfig ) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    esp_wifi_connect();
    return netif;
}
/*-----------------------------------------------------------*/

static void prvWifiStop( void )
{
    esp_netif_t * pxWifiNetif = prvGetExampleNetifFromDesc( "sta" );

    ESP_ERROR_CHECK( esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &prvOnWifiDisconnect ) );
    ESP_ERROR_CHECK( esp_event_handler_unregister( IP_EVENT, IP_EVENT_STA_GOT_IP, &prvOnGotIpAddress ) );
    #ifdef CONFIG_EXAMPLE_CONNECT_IPV6
        ESP_ERROR_CHECK( esp_event_handler_unregister( IP_EVENT, IP_EVENT_GOT_IP6, &prvOnGotIpAddressv6 ) );
        ESP_ERROR_CHECK( esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect ) );
    #endif
    esp_err_t err = esp_wifi_stop();

    if( err == ESP_ERR_WIFI_NOT_INIT )
    {
        return;
    }

    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK( esp_wifi_deinit() );
    ESP_ERROR_CHECK( esp_wifi_clear_default_wifi_driver_and_handlers( pxWifiNetif ) );
    esp_netif_destroy( pxWifiNetif );
}
/*-----------------------------------------------------------*/

static esp_err_t prvConnectNetwork( void )
{
    ESP_LOGI( TAG, "Entered prvConnectNetwork" );

    if( xSemphGetIpAddrs != NULL )
    {
        return ESP_ERR_INVALID_STATE;
    }

    ( void ) prvWifiStart();

    /* create semaphore if at least one interface is active */
    xSemphGetIpAddrs = xSemaphoreCreateCounting( NR_OF_IP_ADDRESSES_TO_WAIT_FOR, 0 );

    ESP_ERROR_CHECK( esp_register_shutdown_handler( &prvWifiStop ) );
    ESP_LOGI( TAG, "Waiting for IP(s)" );

    for( int lCounter = 0; lCounter < NR_OF_IP_ADDRESSES_TO_WAIT_FOR; ++lCounter )
    {
        xSemaphoreTake( xSemphGetIpAddrs, portMAX_DELAY );
    }

    /* iterate over active interfaces, and print out IPs of "our" netifs */
    esp_netif_t * pxNetif = NULL;
    esp_netif_ip_info_t xIpInfo;

    for( int lCounter = 0; lCounter < esp_netif_get_nr_of_ifs(); ++lCounter )
    {
        pxNetif = esp_netif_next( pxNetif );

        if( prvIsOurNetif( TAG, pxNetif ) )
        {
            ESP_LOGI( TAG, "Connected to %s", esp_netif_get_desc( pxNetif ) );

            ESP_ERROR_CHECK( esp_netif_get_ip_info( pxNetif, &xIpInfo ) );

            ESP_LOGI( TAG, "- IPv4 address: " IPSTR, IP2STR( &xIpInfo.ip ) );
        }
    }

    ESP_LOGI( TAG, "Exited prvConnectNetwork" );

    return ESP_OK;
}
/*-----------------------------------------------------------*/

/**
 * @brief Callback to confirm time update through NTP.
 */
static void prvTimeSyncNotificationCallback( struct timeval * pxTimeVal )
{
    ( void ) pxTimeVal;
    ESP_LOGI( TAG, "Notification of a time synchronization event" );
    xTimeInitialized = true;
}
/*-----------------------------------------------------------*/

/**
 * @brief Updates the device time using NTP.
 */
static void prvInitializeTime()
{
    sntp_setoperatingmode( SNTP_OPMODE_POLL );
    sntp_setservername( 0, SNTP_SERVER_FQDN );
    sntp_set_time_sync_notification_cb( prvTimeSyncNotificationCallback );
    sntp_init();

    ESP_LOGI( TAG, "Waiting for time synchronization with SNTP server" );

    while( !xTimeInitialized )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}
/*-----------------------------------------------------------*/

uint64_t ullGetUnixTime( void )
{
    time_t now = time( NULL );

    if( now == INDEFINITE_TIME )
    {
        ESP_LOGE( TAG, "Failed obtaining current time.\r\n" );
    }

    return now;
}
/*-----------------------------------------------------------*/

static void write_to_nvs()
{

    char *wifi_ssid_string = "Flanders Make Guest 2\0";
    char *wifi_pw_string = "**********\0";

    char *iot_fqdn_string = "rgtestjaniothub.azure-devices.net\0";

    // Device 1
    char *iot_device_key_string = "mtzH7a7+tP0p2REVdcPAGrrZrsvfs7qRpdA7LFGaKZc=\0";
    char *iot_device_id_string = "espazureiotkit\0";
    char *iot_device_nr_string = "01\0";
    // Device 2
    // char *iot_device_id_string = "espazureiotkit-02\0";
    // char *iot_device_nr_string = "02\0";
    // char *iot_device_key_string = "Y2OmypzIlW5bi5474XyJVIjuylkux52pdvfyO1F9S4o=\0";

    printf("Started writing to NVS.\n");

    esp_err_t err = nvs_flash_init();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    nvs_handle_t my_handle;

    nvs_open("storage", NVS_READWRITE, &my_handle);

    nvs_set_str(my_handle, "wifi_ssid", wifi_ssid_string);
    nvs_set_str(my_handle, "wifi_pw", wifi_pw_string);
    nvs_set_str(my_handle, "iot_fqdn", iot_fqdn_string);
    nvs_set_str(my_handle, "iot_device_id", iot_device_id_string);
    nvs_set_str(my_handle, "iot_device_nr", iot_device_nr_string);
    nvs_set_str(my_handle, "iot_device_key", iot_device_key_string);

    nvs_commit(my_handle);

    // Close
    nvs_close(my_handle);

    printf("Finished writing to NVS.\n");

}

static void read_from_nvs()
{
    printf("Started reading from NVS.\n");

    esp_err_t err = nvs_flash_init();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    nvs_handle_t my_handle;

    nvs_open("storage", NVS_READWRITE, &my_handle);

    size_t required_size = 0;

    nvs_get_str(my_handle, "wifi_ssid", NULL, &required_size);
    m_wifi_ssid_string = malloc(required_size);
    nvs_get_str(my_handle, "wifi_ssid", m_wifi_ssid_string, &required_size);

    nvs_get_str(my_handle, "wifi_pw", NULL, &required_size);
    m_wifi_pw_string = malloc(required_size);
    nvs_get_str(my_handle, "wifi_pw", m_wifi_pw_string, &required_size);

    nvs_get_str(my_handle, "iot_fqdn", NULL, &required_size);
    m_iot_fqdn_string = malloc(required_size);
    nvs_get_str(my_handle, "iot_fqdn", m_iot_fqdn_string, &required_size);

    nvs_get_str(my_handle, "iot_device_id", NULL, &required_size);
    m_iot_device_id_string = malloc(required_size);
    nvs_get_str(my_handle, "iot_device_id", m_iot_device_id_string, &required_size);

    nvs_get_str(my_handle, "iot_device_nr", NULL, &required_size);
    m_iot_device_nr_string = malloc(required_size);
    nvs_get_str(my_handle, "iot_device_nr", m_iot_device_nr_string, &required_size);

    nvs_get_str(my_handle, "iot_device_key", NULL, &required_size);
    m_iot_device_key_string = malloc(required_size);
    nvs_get_str(my_handle, "iot_device_key", m_iot_device_key_string, &required_size);



    printf("Read data => wifi_ssid: %s\n", m_wifi_ssid_string);
    // printf("Read data => wifi_pw: %s\n", wifi_pw_string);
    printf("Read data => iot_fqdn: %s\n", m_iot_fqdn_string);
    printf("Read data => iot_device_id: %s\n", m_iot_device_id_string);
    printf("Read data => iot_device_nr: %s\n", m_iot_device_nr_string);
    printf("Read data => iot_device_key: %s\n", m_iot_device_key_string);

    nvs_close(my_handle);

    printf("Finished reading from NVS.\n");
}


void app_main( void )
{
    // write_to_nvs();

    read_from_nvs();

    ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );

    /*Allow other core to finish initialization */
    vTaskDelay( pdMS_TO_TICKS( 100 ) );

    initialize_sensors( );
    oled_clean_screen();

    
    char* string_to_show[50];

    strcpy(string_to_show, OLED_SPLASH_MESSAGE);
    strcat(string_to_show, "   Device: ");
    strcat(string_to_show, m_iot_device_nr_string);

    oled_show_message( ( uint8_t * ) string_to_show, strlen(string_to_show) );

    // oled_show_message( ( uint8_t * ) OLED_SPLASH_MESSAGE, sizeof( OLED_SPLASH_MESSAGE ) - 1 );

    ( void ) prvConnectNetwork();

    prvInitializeTime();

    vStartDemoTask(m_iot_fqdn_string, m_iot_device_id_string, m_iot_device_key_string);
}
/*-----------------------------------------------------------*/
