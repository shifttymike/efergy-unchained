#define THIS_IS_STACK_APPLICATION
#include "TCPIP Stack/TCPIP.h"
#include "board.h"
#include "radio.h"
#include "app.h"
#include "ota_update.h"
#include "version.h"
#include "config_store.h"
#include "mqtt_client.h"
#include "network.h"
#include "web_assets.h"
#include "oem_restore.h"
APP_CONFIG AppConfig;
uint8_t app_network_ready,app_reboot_requested;
static reading_t last_reading;
static radio_frame_t incoming_frame;
static uint16_t valid_readings;
static uint8_t mac_fallback;
static uint32_t flash_jedec;
void __interrupt(high_priority) high_isr(void) {
#ifndef OEM_RESTORE_BUILD
radio_interrupt();
#endif
}
void __interrupt(low_priority) low_isr(void)
{
    if(INTCONbits.TMR0IE && INTCONbits.TMR0IF)TickUpdate();
}
uint16_t app_format_status(char *out,uint16_t capacity)
{
#ifdef OEM_RESTORE_BUILD
    return (uint16_t)snprintf(out,capacity,"{\"firmware\":\"" EFERGY_FIRMWARE_VERSION "\",\"converter\":true,\"ip\":\"%u.%u.%u.%u\"}",AppConfig.MyIPAddr.v[0],AppConfig.MyIPAddr.v[1],AppConfig.MyIPAddr.v[2],AppConfig.MyIPAddr.v[3]);
#else
    return (uint16_t)snprintf(out,capacity,
        "{\"firmware\":\"" EFERGY_FIRMWARE_VERSION "\",\"ip\":\"%u.%u.%u.%u\",\"fallback\":%u,\"mac_fallback\":%u,\"network_state\":%u,\"network_pending\":%u,"
        "\"flash_id\":\"%06lx\",\"radio_captures\":%u,\"valid_readings\":%u,\"legacy_id\":%lu,"
        "\"current_mantissa\":%u,\"current_shift\":%u,\"interval\":%u,\"battery_ok\":%u,"
        "\"mqtt_state\":%u,\"mqtt_error\":%u,\"mqtt_tcp_phase\":%u,\"broker_ip\":\"%u.%u.%u.%u\",\"mqtt_reconnects\":%u,\"mqtt_publications\":%u,"
        "\"config_state\":%u,\"config_error\":%u,\"config_sequence\":%lu,"
        "\"radio_version\":%u,\"capture_profile\":%u,\"capture_bits\":%u,"
        "\"capture_raw\":\"%02x%02x%02x%02x%02x%02x%02x%02x\",\"rssi_raw\":%u,\"voltage_v\":%u,\"web_state\":%u}",
        AppConfig.MyIPAddr.v[0],AppConfig.MyIPAddr.v[1],AppConfig.MyIPAddr.v[2],AppConfig.MyIPAddr.v[3],
        network_fallback,mac_fallback,network_state,network_config_pending(),(unsigned long)flash_jedec,radio_captures,valid_readings,(unsigned long)last_reading.legacy_id,
        last_reading.current_mantissa,last_reading.current_divisor_shift,last_reading.interval_s,last_reading.battery_ok,
        mqtt_state,mqtt_error,mqtt_tcp_phase,(unsigned)(mqtt_resolved_ip&255u),(unsigned)((mqtt_resolved_ip>>8)&255u),
        (unsigned)((mqtt_resolved_ip>>16)&255u),(unsigned)(mqtt_resolved_ip>>24),mqtt_reconnects,mqtt_publications,
        config_store_state,config_store_error,(unsigned long)config_sequence,
        radio_version,incoming_frame.profile,incoming_frame.bits,
        incoming_frame.data[0],incoming_frame.data[1],incoming_frame.data[2],incoming_frame.data[3],
        incoming_frame.data[4],incoming_frame.data[5],incoming_frame.data[6],incoming_frame.data[7],
        valid_readings?last_reading.rssi_raw:255,device_config.voltage_v,web_assets_state);
#endif
}
void main(void)
{
    uint8_t seen_generation=0;
    board_init();TickInit();INTCONbits.GIEL=1;INTCONbits.GIEH=1;
    flash_jedec=flash_id();config_store_init();web_assets_init();memset(&AppConfig,0,sizeof(AppConfig));
    mac_fallback=!board_read_mac(AppConfig.MyMACAddr.v);network_init();
#ifndef OEM_RESTORE_BUILD
    mqtt_init();radio_init(device_config.radio_profile);
#endif
    ota_update_init();
#ifndef OTA_DIAG_STANDALONE
    WDTCONbits.SWDTEN=1;
#endif

    for(;;) {
        StackTask();StackApplications();
        network_task();
#ifndef OEM_RESTORE_BUILD
        if(radio_take(&incoming_frame)) {
            board_radio_activity();
            if(e2_decode_capture(&incoming_frame,&last_reading)) {
                ++valid_readings;mqtt_reading(&last_reading);
            }
        }
        radio_task();
#endif
        config_store_task();web_assets_task();
        if(!oem_restore_busy())ota_update_task();
#ifdef OEM_RESTORE_BUILD
        oem_restore_task();
#endif

#ifndef OEM_RESTORE_BUILD
        if(seen_generation!=config_generation){seen_generation=config_generation;mqtt_reconfigure();}
#endif
        http_server_task();
#ifndef OEM_RESTORE_BUILD
        mqtt_task(app_network_ready);
#endif
        board_leds_task();
        if(app_reboot_requested)Reset();
        ClrWdt();
    }
}
