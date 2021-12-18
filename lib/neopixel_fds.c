#include "neopixel_fds.h"
#include "via.h"
#include <string.h>
#include "fds.h"
#include "nrf_error.h"

#ifdef KEYBOARD_CENTRAL
#include "ble_central.h"
#endif //KEYBOARD_CENTRAL


static fds_record_desc_t neopixel_frame_desc[NEOPIXEL_USER_DEFINED_COUNT][NEOPIXEL_MAX_FRAMES];
static fds_record_desc_t neopixel_conf_desc;

neopixel_user_defined_config_t neopixel_user_defined_config[NEOPIXEL_USER_DEFINED_COUNT];
uint8_t neopixel_user_defined[NEOPIXEL_USER_DEFINED_COUNT][NEOPIXEL_MAX_FRAMES][NEOPIXEL_FRAME_BYTES];
/*
static fds_record_t fds_neopixel_records[NEOPIXEL_USER_DEFINED_COUNT];
static fds_record_t fds_neopixel_record_base = {
    .file_id = FDS_NEOPIXEL_FILE_ID,
    .key = FDS_NEOPIXEL_REC_KEY,
    .data.p_data = neopixel_user_defined,
    .data.length_words = (sizeof(uint8_t)*NEOPIXEL_PATTERN_BYTES + 3)/sizeof(uint32_t)
};
*/
static fds_record_t fds_neopixel_frame_records[NEOPIXEL_USER_DEFINED_COUNT][NEOPIXEL_MAX_FRAMES];
static fds_record_t fds_neopixel_frame_record_base = {
    .file_id = NEOPIXEL_FDS_FRAME_FILE_ID,
    //.key = FDS_NEOPIXEL_REC_KEY,
    //.data.p_data = neopixel_user_defined,
    .data.length_words = (sizeof(uint8_t)*NEOPIXEL_FRAME_BYTES + 3)/sizeof(uint32_t)
};

static fds_record_t fds_neopixel_conf_record = {
    .file_id = FDS_NEOPIXEL_CONF_FILE_ID,
    .key = FDS_NEOPIXEL_CONF_REC_KEY,
    .data.p_data = neopixel_user_defined_config,
    .data.length_words = (sizeof(neopixel_user_defined_config_t)*sizeof(neopixel_user_defined_config) + 3)/sizeof(uint32_t)
};


static uint32_t neopixel_user_defined_pattern_updated[NEOPIXEL_USER_DEFINED_COUNT];
static bool neopixel_user_defined_config_updated = false;

void neopixel_fds_init(void){
    ret_code_t ret;
    fds_find_token_t  tok  = {0};
    fds_flash_record_t config = {0};
    
    for(uint8_t i=0;i<NEOPIXEL_USER_DEFINED_COUNT;i++) {
        for(uint8_t j=0;j<NEOPIXEL_MAX_FRAMES;j++) {
            memcpy(&fds_neopixel_frame_records[i][j], &fds_neopixel_frame_record_base, sizeof(fds_record_t));
            fds_neopixel_frame_records[i][j].key = NEOPIXEL_FDS_FRAME_REC_KEY_BASE+i*0x100 +j;
            fds_neopixel_frame_records[i][j].data.p_data = neopixel_user_defined[i][j];
        }
    }

    // not to conf.interval_ticks==0
    memset(neopixel_user_defined_config, 0x02, sizeof(neopixel_user_defined_config_t)*sizeof(neopixel_user_defined_config));
    
    memset(neopixel_user_defined_pattern_updated, 0, sizeof(neopixel_user_defined_pattern_updated)*sizeof(uint32_t));

    ret = fds_record_find(FDS_NEOPIXEL_CONF_FILE_ID, FDS_NEOPIXEL_CONF_REC_KEY, &neopixel_conf_desc, &tok);
    if (ret == NRF_SUCCESS)
    {
        /* A config file is in flash. Let's update it. */
        ret = fds_record_open(&neopixel_conf_desc, &config);
        APP_ERROR_CHECK(ret);
       
        memcpy(neopixel_user_defined_config, config.p_data, sizeof(neopixel_user_defined_config_t)*sizeof(neopixel_user_defined_config));
    }
    else
    {
        create_fds_new_entry(&neopixel_conf_desc, &fds_neopixel_conf_record);
    }
    
    for(uint8_t i=0;i<NEOPIXEL_USER_DEFINED_COUNT;i++) {
        for(uint8_t j=0;j<NEOPIXEL_MAX_FRAMES;j++) {
            memset(&tok, 0, sizeof(fds_find_token_t));
            ret = fds_record_find(NEOPIXEL_FDS_FRAME_FILE_ID, NEOPIXEL_FDS_FRAME_REC_KEY_BASE+i*0x100+j, &neopixel_frame_desc[i][j], &tok);

            if (ret == NRF_SUCCESS)
            {
                /* A config file is in flash. Let's update it. */
                ret = fds_record_open(&neopixel_frame_desc[i][j], &config);
                APP_ERROR_CHECK(ret);
       
                memcpy(neopixel_user_defined[i][j], config.p_data, sizeof(uint8_t)*NEOPIXEL_FRAME_BYTES);
            }
            else
            {
                // migration
                /*
                if( NRF_SUCCESS == fds_record_find(NEOPIXEL_FDS_FRAME_FILE_ID_8140, NEOPIXEL_FDS_FRAME_REC_KEY_BASE+i*0x100+j, &neopixel_frame_desc[i][j], &tok) ) {
                    ret = fds_record_open(&neopixel_frame_desc[i][j], &config);
                    APP_ERROR_CHECK(ret);
       
                    memcpy(neopixel_user_defined[i][j], config.p_data, sizeof(uint8_t)*NEOPIXEL_FRAME_BYTES_8140);
                    ret = fds_record_delete(&neopixel_frame_desc[i][j]);
                    APP_ERROR_CHECK(ret);
                }
                */
                create_fds_new_entry(&neopixel_frame_desc[i][j], &fds_neopixel_frame_records[i][j]);
            }
        }
    }
}


void save_neopixel(void) {
    CRITICAL_REGION_ENTER();

    if(neopixel_user_defined_config_updated) {
        update_fds_entry(&neopixel_conf_desc, &fds_neopixel_conf_record);
        neopixel_user_defined_config_updated = false;
    }

    for(uint8_t i=0;i<NEOPIXEL_USER_DEFINED_COUNT;i++) {
        for(uint8_t j=0;j<NEOPIXEL_MAX_FRAMES;j++) {
            if(neopixel_user_defined_pattern_updated[i] & (1<<j)) {
                update_fds_entry(&neopixel_frame_desc[i][j], &fds_neopixel_frame_records[i][j]);
                neopixel_user_defined_pattern_updated[i] &= ~(1<<j);
            }
        }
    }

    CRITICAL_REGION_EXIT();
}


void raw_hid_receive_neopixel(uint8_t *data, uint8_t length) {
    neopixel_user_defined_config_t *conf;
#ifdef KEYBOARD_CENTRAL
    uint8_t uart_data[8];
#endif

    switch( *data ) {
    case id_get_keyboard_value:
        switch( *(data+1) ) {
        case KBD_NEOPIXEL:
            save_neopixel();
            *(data+2) = NEOPIXEL_MAX_CHAINS;
#ifdef KEYBOARD_CENTRAL
            uart_data[0] = UART_NEOPIXEL_SAVE_ID;
            uart_send_central(uart_data,1);
#endif
            break;
        
#ifdef KEYBOARD_CENTRAL
        case KBD_NEOPIXEL_PERIPH:
            *data = UART_NEOPIXEL_GET_PATTERN_ID;
            uart_send_central(data, length);
            *data = id_get_keyboard_value;
            break;
#endif //KEYBOARD_CENTRAL
        }
        break;
    case id_set_keyboard_value:
        switch( *(data+1) ) {
        case KBD_NEOPIXEL:
            memcpy(
                neopixel_user_defined[*(data+2)][*(data+3)]+(NEOPIXEL_BYTES_PER_PIXEL*(*(data+4))),
                data+5,
                length-5
                );
            *data = id_unhandled;
            neopixel_user_defined_pattern_updated[*(data+2)] |= 1<<(*(data+3));
            break;
        case KBD_NEOPIXEL_CONF:
            conf = &neopixel_user_defined_config[*(data+2)];
            conf->frame_count = *(data+3);
            conf->interval_ticks = *(data+4);
            *data = id_unhandled;
            neopixel_user_defined_config_updated = true;
            break;

#ifdef KEYBOARD_CENTRAL
        case KBD_NEOPIXEL_PERIPH:
            *data = UART_NEOPIXEL_SET_PATTERN_ID;
            uart_send_central(data, length);
            *data = id_unhandled;
            break;
        case KBD_NEOPIXEL_PERIPH_CONF:
            *data = UART_NEOPIXEL_SET_PATTERN_CONF_ID;
            uart_send_central(data, length);
            *data = id_unhandled;
            break;
#endif //KEYBOARD_CENTRAL
        }
        break; // id_set_keyboard_value

    default:
        break;
    }
}