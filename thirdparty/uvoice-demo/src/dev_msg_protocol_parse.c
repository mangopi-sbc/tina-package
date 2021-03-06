#include "dev_msg_protocol_parse.h"

static ASR_config_t m_config =
{
    .can_recog = 1, /*default on*/
    .can_play = 1, /*default on*/
    .wakeup_word = ASR_WAKEUP_WORD_KONGTIAO,
    .voice_type = ASR_PLAY_VOICE_SWEET,
    .idle_timeout = 15,//30, /*10s kenhe 1000*/
    .play_volumn = 2//1.8
};

ASR_config_t* const device_asr_get_config(void)
{
    return (ASR_config_t*)&m_config;
}

AC_t* device_ac_get(void)
{
    return &m_ac;
}

typedef struct {
    uint8_t wifi_status;                //Byte5
    uint8_t ota_status;                 //Byte6
    uint8_t speech_vender;              //Byte7
    uint8_t voice_version_low;          //Byte8
    uint8_t voice_version_high;         //Byte9
    uint8_t ota_version_low;            //Byte10
    uint8_t ota_version_high;           //Byte11
    uint8_t ota_loading_progress;       //Byte12
    uint8_t wakeup_word;                //Byte13
    uint8_t play_volumn;                //Byte14
    uint8_t voice_type;                 //Byte15
    uint8_t idle_timeout_second;        //Byte16
    uint8_t mic_speaker_onoff;          //Byte17
    uint8_t poweron_play_long_onoff;    //Byte18
    uint8_t protocol_version;           //Byte19
    uint8_t resrvd1;                    //Byte20
    uint16_t resrvd2;                   //Byte21-22
    uint32_t resrvd3;                   //Byte23-26
    uint32_t resrvd4;                   //Byte27-30
    //CheckSum                          //Byte31
} __attribute__((__packed__)) heartbeat_t;

//to do heartbeat_data_query function from AC_T
static heartbeat_t heartbeat_data =
{
    .wifi_status = 0x01,
    .ota_status = 0x0,
    .speech_vender = 0x04,
    .voice_version_low = 0x01,
    .voice_version_high = 0x00,
    .ota_version_low = 0x02,
    .ota_version_high = 0x00,
    .ota_loading_progress = 0x0,
    .wakeup_word = ASR_WAKEUP_WORD_KONGTIAO,
    .play_volumn = 2,
    .voice_type = 0x01,
    .idle_timeout_second = WAKEUP_TIMEOUT_CONFIG,
    .mic_speaker_onoff = 0x03,
    .poweron_play_long_onoff = 0x01,
};
static cmd_uart_msg_t* heartbeat_frame = NULL;
static cmd_uart_msg_t* ac_query_frame = NULL;
extern void set_connectting_flag(int flag);

int send_heartbeat(void)
{
    if (heartbeat_frame == NULL) {
        heartbeat_frame = device_asr_alloc_frame(ASR_UART_MSG_HEART_BEAT,
            (uint8_t*)&heartbeat_data, sizeof(heartbeat_data));
        if (heartbeat_frame == NULL) {
            log_e("heartbeat frame alloc fail");
            return ASR_AC_ERROR_MEM_FAIL;
        }
    }

    heartbeat_data.wifi_status = m_ac.wifi_info.ac_board_wifi_status;
    heartbeat_data.ota_status = m_ac.ota_info.ota_status;
    heartbeat_data.speech_vender = (uint8_t)ASR_SPEECH_VENDOR_UVOICETECH;
    heartbeat_data.voice_version_low = (uint16_t)ASR_UVOICE_LIB_VERSION & 0x00FF;
    //m_ac.voice_version_info.running_version & 0x00FF;

    heartbeat_data.voice_version_high = ((uint16_t)ASR_UVOICE_LIB_VERSION & 0xFF00) >> 8;
    //(m_ac.voice_version_info.running_version & 0xFF00) >> 8;

    heartbeat_data.ota_version_low = m_ac.voice_version_info.ota_version & 0x00FF;
    heartbeat_data.ota_version_high = (m_ac.voice_version_info.ota_version & 0xFF00) >> 8;
    //heartbeat_data.ota_loading_progress = m_ac.ota_info.ota_loading_progress;
    //heartbeat_data.wakeup_word = m_config.wakeup_word;
    //heartbeat_data.voice_type = m_config.voice_type;
/*
    heartbeat_data.voice_version_low = 0x0;
    heartbeat_data.voice_version_high = 0x0;
    heartbeat_data.ota_version_low = 0x0;
    heartbeat_data.ota_version_high = 0x0;
*/
    heartbeat_data.ota_loading_progress = 0x0;
    heartbeat_data.wakeup_word = 0x0;
    heartbeat_data.play_volumn = m_ac.volume;
    heartbeat_data.voice_type = 0x0;
    heartbeat_data.idle_timeout_second = (uint8_t)WAKEUP_TIMEOUT_CONFIG; //m_config.idle_timeout;
    heartbeat_data.mic_speaker_onoff = m_ac.local_dev_set_asr | (m_ac.local_dev_set_play << 1);
    heartbeat_data.poweron_play_long_onoff = m_ac.poweron_play_long_onoff;
    heartbeat_data.protocol_version = ASR_MODULE_SW_VERSION;

    memcpy(heartbeat_frame->data, &heartbeat_data, sizeof(heartbeat_data));
    if (device_asr_send_frame_default_no_wait(heartbeat_frame) != 0) {
        return ASR_AC_ERROR_UART_TIMEOUT;
    }
    return ASR_AC_ERROR_NONE;
}

/************************************************************************************************
*                                      ??? ??? <- ????????????                                         *
************************************************************************************************/

void cmd_all_voice_reply_function(char *buf, uint16_t asr_id, char play_enable)
{
    if (buf) {
        LOGT("* %s *", buf);
    }
#ifdef AUTO_TEST_ENABLE
    if (asr_id == 291) {
#else
    if (asr_id != 0xFFFF) {
#endif
        if (play_enable == 0x01) {
            LOCK_RING_BUFFER();
            send_to_ring_buffer(&asr_id, 1);
            UNLOCK_RING_BUFFER();
        }
    }
    LOGT("snd ring asr_id = %d", asr_id);
}

int cmd_heartbeat(cmd_uart_msg_t* UART)
{
    int16_t temp;
    if (UART->msgLen >= 0x20) {
        if (UART->data[0] == 0x00) {
            m_ac.wifi_info.ac_board_wifi_status = WIFI_STATUS_DISCONNECT;
        } else if (UART->data[0] == 0x01) {
            m_ac.wifi_info.ac_board_wifi_status = WIFI_STATUS_CONNECTED;
        }

        m_ac_config.onoff = UART->data[1];
        m_ac.onoff_status = UART->data[1];

        temp = UART->data[4] | (UART->data[5] << 8);
        m_ac_config.minT = (float)(temp / 100);
        LOGT("m_ac_config.minT:%f", m_ac_config.minT);

        temp = UART->data[6] | (UART->data[7] << 8);
        m_ac_config.maxT = (float)(temp / 100);
        LOGT("m_ac_config.maxT:%f", m_ac_config.maxT);
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_wakeup(cmd_uart_msg_t* UART)
{
    uint16_t asr_id = 0xFFFF;

    if (UART->msgLen >= 0x0A) {
        if (UART->play == 0x01) {
            if (UART->data[0] == 0x01) {
                //cmd_all_voice_reply_function("??????:?????????", 1, (UART->play & m_ac.local_dev_set_play));
            } else if (UART->data[0] == 0x00) {
                //cmd_all_voice_reply_function("??????:?????????", 1, (UART->play & m_ac.local_dev_set_play));
            } else {
                return ASR_AC_ERROR_INVALID_PARAM;
            }
/*
            asr_id = random_value_in_range(1, 6);
            if ( asr_id != 1) {
                asr_id += 294;
            }
*/
            cmd_all_voice_reply_function("??????/??????", 0xFFFF, (UART->play & m_ac.local_dev_set_play));

            m_ac.onoff_status = UART->data[0];
            LOGT("m_ac.onoff_status = %d", m_ac.onoff_status);
        }
    }
    return ASR_AC_ERROR_NONE;
}

int cmd_on(cmd_uart_msg_t* UART)
{
    int temp = 0;
    if (UART->msgLen >= 0x10) {
        if ((UART->data[0] & 0x0F) >= ASR_AC_MODE_MIN
                && (UART->data[0] & 0x0F) <= ASR_AC_MODE_MAX) {
            m_ac.mode = (ASR_AC_MODE)(UART->data[0] & 0x0F);
        } else {
            return ASR_AC_ERROR_INVALID_PARAM;
        }

        LOGT("m_ac.mode = %d", m_ac.mode);
        if (m_ac.mode == ASR_AC_MODE_WIND) {
            if (UART->data[9 - 5] & 0x80) {/*Negative*/
                temp = -(0xffff-(UART->data[8 - 5] + (UART->data[9 - 5] << 8))+1);
            } else {
                temp = (UART->data[8 - 5] + (UART->data[9 - 5] << 8));
            }
        } else {
            if (UART->data[2] & 0x80) {/*Negative*/
                temp = -(0xffff-(UART->data[1] + (UART->data[2] << 8))+1);
            } else {
                temp = (UART->data[1] + (UART->data[2] << 8));
            }
        }

        m_ac.setT = (temp / 100.0f);
        temp = (temp % 100);

        if(UART->data[4] & 0x80)/*Negative*/
            m_ac.indoorT = -(0xffff-(UART->data[3] + (UART->data[4] << 8))+1) / 100.0f;
        else
            m_ac.indoorT = (UART->data[3] + (UART->data[4] << 8)) / 100.0f;

        if(UART->data[6] & 0x80)/*Negative*/
            m_ac.outdoorT = -(0xffff-(UART->data[5] + (UART->data[6] << 8))+1) / 100.0f;
        else
            m_ac.outdoorT = (UART->data[5] + (UART->data[6] << 8)) / 100.0f;

        if ((UART->data[7] >= 1 && UART->data[7] <= 100) || (UART->data[7] == 0x66)){
            m_ac.wind_speed = UART->data[7];
        } else {
            return ASR_AC_ERROR_INVALID_PARAM;
        }
        LOGT("m_ac.wind_speed = %d\n", m_ac.wind_speed);
        LOGT("pre m_ac.onoff_status = %d\n", m_ac.onoff_status);

        if (((UART->data[0] & 0xF0) >> 4) == 0x01) {
            if (m_ac.onoff_status == 0x00) {
                cmd_all_voice_reply_function("???????????????", 3, (UART->play & m_ac.local_dev_set_play));
                if (m_ac.mode == ASR_AC_MODE_AUTO) {
                    cmd_all_voice_reply_function("????????????", 133, (UART->play & m_ac.local_dev_set_play));
                } else if (m_ac.mode == ASR_AC_MODE_HEAT) {
                    cmd_all_voice_reply_function("????????????", 135, (UART->play & m_ac.local_dev_set_play));
                } else if (m_ac.mode == ASR_AC_MODE_DRY) {
                    cmd_all_voice_reply_function("????????????", 137, (UART->play & m_ac.local_dev_set_play));
                } else if (m_ac.mode == ASR_AC_MODE_COOL) {
                    cmd_all_voice_reply_function("????????????", 134, (UART->play & m_ac.local_dev_set_play));
                } else if (m_ac.mode == ASR_AC_MODE_WIND) {
                    cmd_all_voice_reply_function("????????????", 136, (UART->play & m_ac.local_dev_set_play));
                } else {
                    LOGW("should't see me!!!");
                }

                if (temp == 0) {
                    cmd_all_voice_reply_function("xx???", ((m_ac.setT * 100 - temp)/100.0f) + 17, \
			(UART->play & m_ac.local_dev_set_play));
                } else {
                    cmd_all_voice_reply_function("xx???", ((m_ac.setT * 100 - temp)/100.0f) + 68, \
			(UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("xx???", (temp / 10) + 17, (UART->play & m_ac.local_dev_set_play));
                }

                if (m_ac.wind_speed == 1) {
                    temp = 0;
                } else if (m_ac.wind_speed == 20) {
                    temp = 1;
                } else if (m_ac.wind_speed == 40) {
                    temp = 2;
                } else if (m_ac.wind_speed == 60) {
                    temp = 3;
                } else if (m_ac.wind_speed == 80) {
                    temp = 4;
                } else if (m_ac.wind_speed == 100) {
                    temp = -1;
                } else if (m_ac.wind_speed == 102) {
                    temp = -187;
                }

                cmd_all_voice_reply_function("x%", temp + 199, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.onoff_status == 0x01) {
                cmd_all_voice_reply_function("???????????????", 3, (UART->play & m_ac.local_dev_set_play));
            }  else {
                LOGW("should't see me!!!");
                return ASR_AC_ERROR_INVALID_PARAM;
            }
        } else if (((UART->data[0] & 0xF0) >> 4) == 0) {
            cmd_all_voice_reply_function("???????????????", 4, (UART->play & m_ac.local_dev_set_play));
        } else {
            LOGW("should't see me!!!");
            return ASR_AC_ERROR_INVALID_PARAM;
        }
        m_ac.onoff_status = ((UART->data[0] & 0xF0) >> 4);
        LOGT("now m_ac.onoff_status = %d", m_ac.onoff_status);
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_wind_speed(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        int temp = -1;

        m_ac.wind_speed = UART->data[0];
        LOGT("m_ac.wind_speed = %d\n", m_ac.wind_speed);
        if (UART->data[1] == 0x01) {
            cmd_all_voice_reply_function("????????????", 5, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_POWER_ON_FIRST;
        } else if ((UART->data[1] == 0x02) || (UART->data[1] == 0x03)){
            cmd_all_voice_reply_function("????????????????????????????????????", 13, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_NOT_ALLOWED;
        } else if (UART->data[1] == 0x04) {
            cmd_all_voice_reply_function("???????????????", 14, (UART->play & m_ac.local_dev_set_play));
            cmd_all_voice_reply_function("?????????", 11, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_NOT_ALLOWED;
        } else if (UART->data[1] == 0x05) {
            cmd_all_voice_reply_function("???????????????", 14, (UART->play & m_ac.local_dev_set_play));
            cmd_all_voice_reply_function("?????????", 9, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_NOT_ALLOWED;
        } else if (UART->data[1] == 0x00) {
            if (m_ac.wind_speed == 1) {
		if (m_ac.wind_play_type == 1) { //?????????/?????????/?????????
			temp = 1;
		} else {
                	temp = 0;
		}
            } else if (m_ac.wind_speed == 20) {
                temp = 1;
            } else if (m_ac.wind_speed == 40) {
                temp = 2;
            } else if (m_ac.wind_speed == 60) {
		if (m_ac.wind_play_type == 1) { //?????????/?????????/?????????
			temp = 2;
		} else {
                	temp = 3;
		}
            } else if (m_ac.wind_speed == 80) {
                temp = 4;
            } else if (m_ac.wind_speed == 100) {
		if (m_ac.wind_play_type == 1) { //?????????/?????????/?????????
			temp = 3;
		} else {
                	temp = -1;
		}
            } else if ((m_ac.wind_speed == 101) || (m_ac.wind_speed == 102)) {
                temp = -187;
            }

            if (m_ac.wind_speed != 102) {
                cmd_all_voice_reply_function("???????????????", 166, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.wind_speed == 102) {
                //cmd_all_voice_reply_function("?????????", 6, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("???????????????", 166, (UART->play & m_ac.local_dev_set_play));
            }

	    if (m_ac.wind_play_type == 1) {
	    	cmd_all_voice_reply_function("1x%", temp + 8, (UART->play & m_ac.local_dev_set_play));
	    } else {
            	cmd_all_voice_reply_function("2x%", temp + 199, (UART->play & m_ac.local_dev_set_play));
	    }
            return ASR_AC_ERROR_NONE;
        }
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_temp(cmd_uart_msg_t* UART)
{
    int temp = 0;

    if (UART->msgLen >= 0x0A) {
        if (UART->data[1] & 0x80)
            //m_ac.setT = -(0xffff-(UART->data[0] + (UART->data[1] << 8))+1) / 100.0f;
            temp = -(0xffff-(UART->data[0] + (UART->data[1] << 8))+1);
        else
            //m_ac.setT = (UART->data[0] + (UART->data[1] << 8)) / 100.0f;
            temp = (UART->data[0] + (UART->data[1] << 8));

        m_ac.setT = (temp / 100.0f);
        temp = (temp % 100);

        if (UART->data[2] == 0x01) {
            cmd_all_voice_reply_function("????????????", 5, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_POWER_ON_FIRST;
        } else if (UART->data[2] == 0x02) {
            cmd_all_voice_reply_function("??????????????????????????????", 130, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_NOT_ALLOWED;
        } else if (UART->data[2] == 0x03) {
            cmd_all_voice_reply_function("???????????????", 14, (UART->play & m_ac.local_dev_set_play));
            cmd_all_voice_reply_function("????????????", 131, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_NOT_ALLOWED;
        } else if (UART->data[2] == 0x04) {
            cmd_all_voice_reply_function("???????????????", 14, (UART->play & m_ac.local_dev_set_play));
            cmd_all_voice_reply_function("????????????", 132, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_NOT_ALLOWED;
        } else if (UART->data[2] == 0x05) {
            cmd_all_voice_reply_function("0.5???????????????", NULL, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_NOT_ALLOWED;
        } else if (UART->data[2] == 0x00) {
            if ((UART->data[2] == 0x00) && (UART->data[3] == 0x00)) {
                if (temp == 0) {
                    cmd_all_voice_reply_function("?????????", 6, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("xx???", ((m_ac.setT * 100 - temp)/100.0f) + 17, \
			(UART->play & m_ac.local_dev_set_play));
                } else {
                    cmd_all_voice_reply_function("?????????", 6, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("xx???", ((m_ac.setT * 100 - temp)/100.0f) + 68, \
			(UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("xx???", (temp / 10) + 17, (UART->play & m_ac.local_dev_set_play));
                }
            }
            return ASR_AC_ERROR_NONE;
        }
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_mode(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        if (UART->data[1] == 0x02) {
            cmd_all_voice_reply_function("????????????", 5, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_POWER_ON_FIRST;
        }
        if (UART->data[0] >= ASR_AC_MODE_MIN && UART->data[0] <= ASR_AC_MODE_MAX) {
            m_ac.mode = (ASR_AC_MODE)UART->data[0];
            cmd_all_voice_reply_function("?????????", 6, (UART->play & m_ac.local_dev_set_play));
            if (m_ac.mode == ASR_AC_MODE_AUTO) {
                cmd_all_voice_reply_function("????????????", 133, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.mode == ASR_AC_MODE_HEAT) {
                cmd_all_voice_reply_function("????????????", 135, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.mode == ASR_AC_MODE_DRY) {
                cmd_all_voice_reply_function("????????????", 137, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.mode == ASR_AC_MODE_COOL) {
                cmd_all_voice_reply_function("????????????", 134, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.mode == ASR_AC_MODE_WIND) {
                cmd_all_voice_reply_function("????????????", 136, (UART->play & m_ac.local_dev_set_play));
            }
            return ASR_AC_ERROR_NONE;
        } else {
            return ASR_AC_ERROR_INVALID_PARAM;
        }
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_swing(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        LOGT("m_ac.swing = %d", m_ac.swing);
        if ((UART->data[1] == 0x03) || (UART->data[2] == 0x03)){
            cmd_all_voice_reply_function("????????????", 5, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_POWER_ON_FIRST;
        } else if (UART->data[1] == 0x01) {
            if (m_ac.swing == ASR_AC_SWING_UP_DOWN_ON) {
                cmd_all_voice_reply_function("?????????", 8, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("????????????", 139, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.swing == ASR_AC_SWING_UP_DOWN_OFF) {
                cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("????????????", 139, (UART->play & m_ac.local_dev_set_play));
            }
            return ASR_AC_ERROR_NONE;
        } else if (UART->data[2] == 0x01) {
            if (m_ac.swing == ASR_AC_SWING_LEFT_RIGHT_ON) {
                cmd_all_voice_reply_function("?????????", 8, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("????????????", 140, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.swing == ASR_AC_SWING_LEFT_RIGHT_OFF) {
                cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("????????????", 140, (UART->play & m_ac.local_dev_set_play));
            }
            return ASR_AC_ERROR_NONE;
        }
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_blow(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        if (UART->data[0] == 0x03) {
            cmd_all_voice_reply_function("????????????", 5, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_POWER_ON_FIRST;
        } else if (UART->data[0] == 0x01) {
            if (m_ac.blow == ASR_AC_BLOW_UP) {
                cmd_all_voice_reply_function("???????????????", 160, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.blow == ASR_AC_BLOW_DOWN) {
                cmd_all_voice_reply_function("???????????????", 161, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.blow == ASR_AC_BLOW_LEFT) {
                cmd_all_voice_reply_function("???????????????", 158, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.blow == ASR_AC_BLOW_RIGHT) {
                cmd_all_voice_reply_function("???????????????", 159, (UART->play & m_ac.local_dev_set_play));
            }
            return ASR_AC_ERROR_NONE;
        }
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_func(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x20) {
        if ((((UART->data[20 - 5] & 0xF0) >> 4) == 0x00) || (((UART->data[20 - 5] & 0xF0) >> 4) == 0x01)) {
            m_ac.onoff_status = ((UART->data[20 - 5] & 0xF0) >> 4);
        }

        if (((UART->data[20 - 5] & 0x0F) >= ASR_AC_MODE_MIN) && ((UART->data[20 - 5] & 0x0F) <= ASR_AC_MODE_MAX)) {
            m_ac.mode = (UART->data[20 - 5] & 0x0F);
        }

        if ((UART->data[9 - 5] == 0x03) || (UART->data[11 - 5] == 0x04) || \
            (UART->data[12 - 5] == 0x04) || (UART->data[13 - 5] == 0x04) || \
            (UART->data[18 - 5] == 0x04) || (UART->data[19 - 5] == 0x04)) {
            cmd_all_voice_reply_function("????????????", 5, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_POWER_ON_FIRST;
        } else if (UART->data[9 - 5] == 0x01) {
            if (m_ac.screen_display == ASR_AC_SCREEN_DISPLAY_ON) {
                cmd_all_voice_reply_function("?????????", 8, (UART->play & m_ac.local_dev_set_play));
            } else if (m_ac.screen_display == ASR_AC_SCREEN_DISPLAY_OFF) {
                cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
            }
            cmd_all_voice_reply_function("??????", 156, (UART->play & m_ac.local_dev_set_play));
            return ASR_AC_ERROR_NONE;
        } else if ((UART->data[11 - 5] == 0x01) || (UART->data[11 - 5] == 0x03)) {
            if (UART->data[11 - 5] == 0x01) {
                if (m_ac.strong_mode == ASR_AC_STRONG_MODE_ON) {
                    cmd_all_voice_reply_function("?????????", 8, (UART->play & m_ac.local_dev_set_play));
                } else if (m_ac.strong_mode == ASR_AC_STRONG_MODE_OFF) {
                    cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
                }
                cmd_all_voice_reply_function("??????", 150, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NONE;
            } else if (UART->data[11 - 5] == 0x03) {
                cmd_all_voice_reply_function("??????", 150, (UART->play & m_ac.local_dev_set_play));
                //cmd_all_voice_reply_function("??????", 290, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("????????????????????????????????????????????????", 148, \
			(UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NOT_ALLOWED;
            }
        } else if ((UART->data[12 - 5] == 0x01) || (UART->data[12 - 5] == 0x03)) {
            if (UART->data[12 - 5] == 0x01) {
                if (m_ac.aux_electric_heat_mode == ASR_AC_AUX_ELECTRIC_HEAT_MODE_ON) {
                    cmd_all_voice_reply_function("?????????", 8, (UART->play & m_ac.local_dev_set_play));
                } else if (m_ac.aux_electric_heat_mode == ASR_AC_AUX_ELECTRIC_HEAT_MODE_OFF) {
                    cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
                }
                cmd_all_voice_reply_function("?????????", 151, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NONE;
            } else if (UART->data[12 - 5] == 0x03) {
                cmd_all_voice_reply_function("?????????", 151, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("???????????????????????????????????????????????????", 152, \
			(UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NOT_ALLOWED;
            }
        } else if ((UART->data[13 - 5] == 0x01) || (UART->data[13 - 5] == 0x03)) {
            if (UART->data[13 - 5] == 0x01) {
                if (m_ac.anti_blow == ASR_AC_ANTI_BLOW_ON) {
                    cmd_all_voice_reply_function("?????????", 8, (UART->play & m_ac.local_dev_set_play));
                } else if (m_ac.anti_blow == ASR_AC_ANTI_BLOW_OFF) {
                    cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
                }
                cmd_all_voice_reply_function("?????????", 153, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NONE;
            } else if (UART->data[13 - 5] == 0x03) {
                cmd_all_voice_reply_function("?????????", 153, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("???????????????????????????????????????????????????????????????", 271, \
			(UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NOT_ALLOWED;
            }
        } else if ((UART->data[14 - 5] == 0x01) || (UART->data[14 - 5] == 0x02) || \
		   (UART->data[14 - 5] == 0x03) || (m_ac.self_clean == ASR_AC_SELF_CLEAN_OFF)) {
            if (UART->data[14 - 5] == 0x01) {
                cmd_all_voice_reply_function("?????????????????????", 274, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NONE;
            } else if (UART->data[14 - 5] == 0x02) {
                cmd_all_voice_reply_function("?????????????????????", 272, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("??????45????????????", 273, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("????????????????????????", 302, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NONE;
            } else if ((UART->data[14 - 5] == 0x03) || (m_ac.self_clean == ASR_AC_SELF_CLEAN_OFF)) {
                cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("?????????", 149, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NONE;
            }
        } else if ((UART->data[18 - 5] == 0x01) || (UART->data[18 - 5] == 0x03)) {
            if (UART->data[18 - 5] == 0x01) {
            	if (m_ac.confort_eco == ASR_AC_CONFORT_ECO_ON) {
                	cmd_all_voice_reply_function("?????????", 8, (UART->play & m_ac.local_dev_set_play));
            	} else if (m_ac.confort_eco == ASR_AC_CONFORT_ECO_OFF) {
                	cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
            	}
            	cmd_all_voice_reply_function("??????", 189, (UART->play & m_ac.local_dev_set_play));
            	return ASR_AC_ERROR_NONE;
	    } else if (UART->data[18 - 5] == 0x03) {
            	cmd_all_voice_reply_function("??????", 189, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("??????????????????????????????", 154, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NOT_ALLOWED;
	    }
        } else if ((UART->data[19 - 5] == 0x01) || (UART->data[19 - 5] == 0x03)) {
            if (UART->data[19 - 5] == 0x01) {
                if (m_ac.anti_cold == ASR_AC_ANTI_COLD_ON) {
                    cmd_all_voice_reply_function("?????????", 8, (UART->play & m_ac.local_dev_set_play));
                } else if (m_ac.anti_cold == ASR_AC_ANTI_COLD_OFF) {
                    cmd_all_voice_reply_function("?????????", 7, (UART->play & m_ac.local_dev_set_play));
                }
                cmd_all_voice_reply_function("?????????", 204, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NONE;
            } else if (UART->data[19 - 5] == 0x03) {
                cmd_all_voice_reply_function("?????????", 204, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("??????????????????????????????", 154, (UART->play & m_ac.local_dev_set_play));
                return ASR_AC_ERROR_NOT_ALLOWED;
            }
        }
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_timer_func(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        if (UART->data[5 - 5] == 0x01) {
            m_ac.onoff_status = 1;
        } else if (UART->data[5 - 5] == 0x02) {
            m_ac.onoff_status = 0;
        }

        if ((UART->data[6 -5] == 0x01) || (UART->data[7 -5] == 0x01) || (UART->data[8 -5] == 0x01)) {
            if (m_ac.timer_status != ASR_AC_TIMER_DISABLE) {
                cmd_all_voice_reply_function("????????????", 143, (UART->play & m_ac.local_dev_set_play));
            }
            switch(m_ac.timer_status)
            {
                case ASR_AC_TIMER_05_ON:
                case ASR_AC_TIMER_05_OFF:
                    cmd_all_voice_reply_function("???????????????", 144, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_10_ON:
                case ASR_AC_TIMER_10_OFF:
                    cmd_all_voice_reply_function("???", 119, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("?????????", 145, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_20_ON:
                case ASR_AC_TIMER_20_OFF:
                    cmd_all_voice_reply_function("???", 120, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("?????????", 145, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_30_ON:
                case ASR_AC_TIMER_30_OFF:
                    cmd_all_voice_reply_function("???", 121, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("?????????", 145, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_40_ON:
                case ASR_AC_TIMER_40_OFF:
                    cmd_all_voice_reply_function("???", 122, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("?????????", 145, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_50_ON:
                case ASR_AC_TIMER_50_OFF:
                    cmd_all_voice_reply_function("???", 123, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("?????????", 145, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_60_ON:
                case ASR_AC_TIMER_60_OFF:
                    cmd_all_voice_reply_function("???", 124, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("?????????", 145, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_70_ON:
                case ASR_AC_TIMER_70_OFF:
                    cmd_all_voice_reply_function("???", 125, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("?????????", 145, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_80_ON:
                case ASR_AC_TIMER_80_OFF:
                    cmd_all_voice_reply_function("???", 126, (UART->play & m_ac.local_dev_set_play));
                    cmd_all_voice_reply_function("?????????", 145, (UART->play & m_ac.local_dev_set_play));
                break;

                case ASR_AC_TIMER_DISABLE:
                    if (m_ac.onoff_status == 0x00) {
                        cmd_all_voice_reply_function("???????????????", 301, (UART->play & m_ac.local_dev_set_play));
                    } else if (m_ac.onoff_status == 0x01) {
                        cmd_all_voice_reply_function("???????????????", 301, (UART->play & m_ac.local_dev_set_play));
                    }
                break;

                default:
                    printf("error para\n");
                break;
            }
        	if ((m_ac.onoff_status == 0x00) && (m_ac.timer_status != ASR_AC_TIMER_DISABLE)) {
        		cmd_all_voice_reply_function("??????", 141, (UART->play & m_ac.local_dev_set_play));
        	} else if ((m_ac.onoff_status == 0x01) && (m_ac.timer_status != ASR_AC_TIMER_DISABLE)) {
        		cmd_all_voice_reply_function("??????", 142, (UART->play & m_ac.local_dev_set_play));
	        }
        } else if ((UART->data[6 -5] == 0x02) || (UART->data[7 -5] == 0x02) || (UART->data[8 -5] == 0x02)) {
        	if ((m_ac.onoff_status == 0x00) && (UART->data[8 -5] == 0x02)) {
            		cmd_all_voice_reply_function("???????????????", 4, (UART->play & m_ac.local_dev_set_play));
	        } else if ((m_ac.onoff_status == 0x01) && (UART->data[7 -5] == 0x02)) {
        		cmd_all_voice_reply_function("???????????????", 3, (UART->play & m_ac.local_dev_set_play));
	        }

	}

        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_volume(cmd_uart_msg_t* UART) {
    if (UART->msgLen >= 0x0A) {
        //m_ac.volume = UART->data[6 -5];
        LOGT("m_ac.volume = %d\n", m_ac.volume);
        if (UART->data[5 -5] == 0x01) {
            LOGD("??????/?????????????????????????????????");
        }

        if ((UART->data[6 -5] >= 0) && (UART->data[6 -5] <= 100)) {
            //LOGD("?????????????????????????????????");
            m_ac.volume = UART->data[6 -5];
            LOGD("?????????????????? m_ac.volume = %d\n", m_ac.volume);
        }
/*
        if ((UART->data[6 -5] >= 0) && (UART->data[6 -5] <= 100)){
            if ((m_ac.volume > 0) && (m_ac.volume <= 20)) {
                //system("amixer cset name='head phone volume' 46");
                cmd_all_voice_reply_function("??????1", 309, (UART->play & m_ac.local_dev_set_play));
            } else if ((m_ac.volume > 20) && (m_ac.volume <= 40)) {
                //system("amixer cset name='head phone volume' 49");
                cmd_all_voice_reply_function("??????2", 310, (UART->play & m_ac.local_dev_set_play));
            } else if ((m_ac.volume > 40) && (m_ac.volume <= 60)) {
                //system("amixer cset name='head phone volume' 53");
                cmd_all_voice_reply_function("??????3", 311, (UART->play & m_ac.local_dev_set_play));
            } else if ((m_ac.volume > 60) && (m_ac.volume <= 80)) {
                //system("amixer cset name='head phone volume' 55");
                cmd_all_voice_reply_function("??????4", 312, (UART->play & m_ac.local_dev_set_play));
            } else if ((m_ac.volume > 80) && (m_ac.volume <= 100)) {
                //system("amixer cset name='head phone volume' 59");
                cmd_all_voice_reply_function("??????5", 313, (UART->play & m_ac.local_dev_set_play));
            }
        }
*/
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_factory(cmd_uart_msg_t* UART) {
    if (UART->msgLen >= 0x0A) {
        LOGD("????????????????????????????????????");
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_set_wifi(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        if (UART->data[5 - 5] == 0x00) {
            LOGD("??????????????????");
            return ASR_AC_ERROR_NONE;
        }
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_passthrough(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        if (m_ac.asr_type == ASR_AC_ASR_TYPE_BACK_HOME) {
            cmd_all_voice_reply_function("??????,????????????", 176, (UART->play & m_ac.local_dev_set_play));
        } else if (m_ac.asr_type == ASR_AC_ASR_TYPE_LEAVE_HOME) {
            cmd_all_voice_reply_function("????????????", 175, (UART->play & m_ac.local_dev_set_play));
        } else if (m_ac.asr_type == ASR_AC_ASR_TYPE_WEATHER) {
            //nothing to do
        } else {
	    printf("                                                                                                     \
		????????????\n");
            cmd_all_voice_reply_function("????????????", 168, (UART->play & m_ac.local_dev_set_play));
        }
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_query_weather(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x22) {
        if (UART->data[5 - 5] == ASR_DEV_QUERY_WEATHER_DISCONNECT) {
            cmd_all_voice_reply_function("?????????????????????????????????", 294, (UART->play & m_ac.local_dev_set_play));
        } else if (UART->data[5 - 5] == ASR_DEV_QUERY_WEATHER_FAIL) {
            LOGD("??????????????????");
        } else if (UART->data[5 - 5] == ASR_DEV_QUERY_WEATHER_SUCCESS) {
            int temp = 0;
            float setT = 0;
            LOGD("??????????????????");
            //???????????????XX???XX??????????????????????????????
            cmd_all_voice_reply_function("????????????", 264, (UART->play & m_ac.local_dev_set_play));
            if (UART->data[18 - 5] == UART->data[19 - 5]) {
                cmd_all_voice_reply_function("xxx", 205 + UART->data[18 - 5], (UART->play & m_ac.local_dev_set_play));
            } else {
                cmd_all_voice_reply_function("xxx", 205 + UART->data[18 - 5], (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("???", 265, (UART->play & m_ac.local_dev_set_play));
                cmd_all_voice_reply_function("xxx", 205 + UART->data[19 - 5], (UART->play & m_ac.local_dev_set_play));
            }
            //?????????XX???XX????????????????????????17???19????????????
            if (UART->data[23 - 5] & 0x80)
                temp = -(0xffff-(UART->data[22 - 5] + (UART->data[23 - 5] << 8))+1);
            else
                temp = (UART->data[22 - 5] + (UART->data[23 - 5] << 8));

            if ((temp >= -50 * 100) && (temp <= 50 * 100)) {
                setT = (temp / 100.0f);
                temp = (temp % 100);
                cmd_all_voice_reply_function("??????", 254, (UART->play & m_ac.local_dev_set_play));
                if (temp == 0) {
                    if ((setT >= 0) && (setT <= 50)) {
                        cmd_all_voice_reply_function("xx???", ((setT * 100 - temp)/100.0f) + 17, \
				(UART->play & m_ac.local_dev_set_play));
                    } else if ((setT < 0) && (setT >= -50)){
                        cmd_all_voice_reply_function("??????", 15, (UART->play & m_ac.local_dev_set_play));
                        cmd_all_voice_reply_function("xx???", ((-(setT * 100) - (-temp))/100.0f) + 17, \
				(UART->play & m_ac.local_dev_set_play));
                    }
                } else {
                    if ((setT >= 0) && (setT <= 50)) {
                        cmd_all_voice_reply_function("xx???", ((setT * 100 - temp)/100.0f) + 68, \
				(UART->play & m_ac.local_dev_set_play));
                        cmd_all_voice_reply_function("xx???", (temp / 10) + 17, (UART->play & m_ac.local_dev_set_play));
                    } else if ((setT < 0) && (setT >= -50)){
                        cmd_all_voice_reply_function("??????", 15, (UART->play & m_ac.local_dev_set_play));
                        cmd_all_voice_reply_function("xx???", ((-(setT * 100) - (-temp))/100.0f) + 68, \
				(UART->play & m_ac.local_dev_set_play));
                        cmd_all_voice_reply_function("xx???", ((-temp) / 10) + 17, (UART->play & m_ac.local_dev_set_play));
                    }
                }

                cmd_all_voice_reply_function("???", 266, (UART->play & m_ac.local_dev_set_play));

                temp = 0;
                if (UART->data[21 - 5] & 0x80)
                    temp = -(0xffff-(UART->data[20 - 5] + (UART->data[21 - 5] << 8))+1);
                else
                    temp = (UART->data[20 - 5] + (UART->data[21 - 5] << 8));
                if ((temp >= -50 * 100) && (temp <= 50 * 100)) {
                    setT = (temp / 100.0f);
                    temp = (temp % 100);
                    if (temp == 0) {
                        if ((setT >= 0) && (setT <= 50)) {
                            cmd_all_voice_reply_function("xx???", ((setT * 100 - temp)/100.0f) + 17, \
				(UART->play & m_ac.local_dev_set_play));
                        } else if ((setT < 0) && (setT >= -50)){
                            cmd_all_voice_reply_function("??????", 15, (UART->play & m_ac.local_dev_set_play));
                            cmd_all_voice_reply_function("xx???", ((-(setT * 100) - (-temp))/100.0f) + 17, \
				(UART->play & m_ac.local_dev_set_play));
                        }
                    } else {
                        if ((setT >= 0) && (setT <= 50)) {
                            cmd_all_voice_reply_function("xx???", ((setT * 100 - temp)/100.0f) + 68, \
				(UART->play & m_ac.local_dev_set_play));
                            cmd_all_voice_reply_function("xx???", (temp / 10) + 17, (UART->play & m_ac.local_dev_set_play));
                        } else if ((setT < 0) && (setT >= -50)){
                            cmd_all_voice_reply_function("??????", 15, (UART->play & m_ac.local_dev_set_play));
                            cmd_all_voice_reply_function("xx???", ((-(setT * 100) - (-temp))/100.0f) + 68, \
				(UART->play & m_ac.local_dev_set_play));
                            cmd_all_voice_reply_function("xx???", ((-temp) / 10) + 17, (UART->play & m_ac.local_dev_set_play));
                        }
                    }
                }
            }
            //??????????????????/???/????????????/????????????/?????????????????????????????????
            cmd_all_voice_reply_function("????????????", 256, (UART->play & m_ac.local_dev_set_play));
            cmd_all_voice_reply_function("xxx", UART->data[30 - 5] + 257, (UART->play & m_ac.local_dev_set_play));

            //???????????????????????????????????????XX%??????????????????35%
            cmd_all_voice_reply_function("xxx", UART->data[31 - 5] + 268, (UART->play & m_ac.local_dev_set_play));
        }
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

/************************************************************************************************
*                                     ??? ??? -> ????????????                                          *
************************************************************************************************/
static int voice_board_uart_ack(ASR_UART_MSG_TYPE msg_type, ASR_FRAME_MSG_ID msg_id)
{
    int asr_param = 0;
    ASR_AC_STATUS status = ASR_AC_ERROR_NONE;
    ASR_UART_MSG_TYPE asr_msg_type;

    asr_msg_type = msg_type;
    asr_param = msg_id;

    status = device_asr_send_msg(asr_msg_type, asr_param);
    return status;
}

int cmd_board_startup(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_BOARD_STARTUP_PLAY);
        cmd_all_voice_reply_function("?????????????????????????????????", 291, UART->play);
        //cmd_all_voice_reply_function("?????????????????????????????????", 292, UART->play);
        return ASR_AC_ERROR_NONE;
/*
        int32_t time_period = 0;
        gettimeofday(&timer_msg_end,NULL);

        time_period = (timer_msg_end.tv_sec * 1000 + timer_msg_end.tv_usec / 1000) - \
		(timer_msg_start.tv_sec * 1000 + timer_msg_start.tv_usec / 1000);
        if (time_period >= 1000) {
            gettimeofday(&timer_msg_start,NULL);
            //cmd_all_voice_reply_function("???????????????????????????", 322, UART->play);
            //cmd_all_voice_reply_function("???????????????????????????????????????????????????", 181, UART->play);
            cmd_all_voice_reply_function("?????????????????????????????????", 180, UART->play);
            voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_BOARD_STARTUP_PLAY);
            return ASR_AC_ERROR_NONE;
        }
*/
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_board_beep_notice(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        int offset = 0;
        int32_t time_period = 0;
        gettimeofday(&timer_end,NULL);
        if (UART->data[5 - 5] == ASR_BEEP_NOTICE_STARTUP) { //????????????
            offset = 0;
        } else if (UART->data[5 - 5] == ASR_BEEP_NOTICE_POWERON) { //????????????
            offset = 0;
        } else if (UART->data[5 - 5] == ASR_BEEP_NOTICE_POWEROFF) { //????????????
            offset = 0;
        } else if (UART->data[5 - 5] == ASR_BEEP_NOTICE_DING) { //??????????????????
            offset = 0;
        } else if (UART->data[5 - 5] == ASR_BEEP_NOTICE_SET_MODE) { //????????????
            offset = 0;
        } else if (UART->data[5 - 5] == ASR_BEEP_NOTICE_INC_TEMP) { //????????????
            offset = 0;
        } else if (UART->data[5 - 5] == ASR_BEEP_NOTICE_DEC_TEMP) { //????????????
            offset = 0;
        }
        time_period = (timer_end.tv_sec * 1000 + timer_end.tv_usec / 1000) - (timer_start.tv_sec * 1000 \
		+ timer_start.tv_usec / 1000);
        if (time_period >= 800) {
            cmd_all_voice_reply_function("?????????????????????...", 354 - 42, UART->play);
            gettimeofday(&timer_start,NULL);
        }
        usleep(25 * 1000);

        //test for cloud device configs
/*
        char data[120] ={0x00,0xFA,0xB1,0xC1,0xD1,0xE1,0xF1,0x91,   \
                         0x01,0xFA,0xB2,0xC2,0xD2,0xE2,0xF2,0x92,   \
                         0x00,0xFD,0xB3,0xC3,0xD3,0xE3,0xF3,0x93,   \
                         0x01,0xFD,0xB4,0xC4,0xD4,0xE4,0xF4,0x94,   \
                         0x00,0xFC,0xB5,0xC5,0xD5,0xE5,0xF5,0x95,   \
                         0x01,0xFC,0xB6,0xC6,0xD6,0xE6,0xF6,0x96,   \
                         0x00,0xAC,0xB7,0xC7,0xD7,0xE7,0xF7,0x97,   \
                         0x01,0xAC,0xB8,0xC8,0xD8,0xE8,0xF8,0x98,   \
                         0x00,0x13,0xB9,0xC9,0xD9,0xE9,0xF9,0x99,   \
                         0x01,0x13,0xBA,0xCA,0xDA,0xEA,0xFA,0x9A,   \
                         0x0B,0xAB,0xBB,0xCB,0xDB,0xEB,0xFB,0x9B,   \
                         0x0C,0xAC,0xBC,0xCC,0xDC,0xEC,0xFC,0x9C,   \
                         0x0D,0xAD,0xBD,0xCD,0xDD,0xED,0xFD,0x9D,   \
                         0x0E,0xAE,0xBE,0xCE,0xEE,0xEE,0xFE,0x9E,   \
                         0x0F,0xAF,0xBF,0xCF,0xFF,0xEF,0xFF,0x9F};

        int data_num = 0;
        //data_num = (UART->msgLen - 6 - 2) / 8;
        data_num = sizeof(data) / 8;
        LOGI("cloud configs snd dev nums: %d, sizeof(device_configs_t) = %d", data_num, sizeof(device_configs_t));
        device_configs_t* frame = (device_configs_t*)malloc(sizeof(device_configs_t) \
                + data_num * 8 * sizeof(char));
        if (frame == NULL) {
            LOGE("frame malloc fail!");
            return NULL;
        }

        for (int i = 0; i < data_num; i++) {
            //frame->info[i * 8] = UART->data[i * 8 * sizeof(char) + 2];
            //frame->info[i * 8 + 1] = UART->data[i * 8 * sizeof(char) + 1 + 2];
            //memcpy(frame->(info+ i * 8 + 2), UART->(data + i * 8 * sizeof(char) + 2 + 2), 8 * sizeof(char));
            frame->info[i * 8] = data[i * 8 * sizeof(char)];
            frame->info[i * 8 + 1] = data[i * 8 * sizeof(char) + 1];
            memcpy(&frame->info[i * 8 + 2], &data[i * 8 * sizeof(char) + 2], 6 * sizeof(char));
        }

        FILE *fp;
        fp = fopen(FILE_OF_DEVICE_INFO, "w+");
        if (!fp)
        {
            LOGE("open/create file: %s fail", FILE_OF_DEVICE_INFO);
            return RESULT_ERR;
        } else {
            LOGD("open/create file: %s success", FILE_OF_DEVICE_INFO);
        }

        if (fwrite(frame, sizeof(device_configs_t) + data_num * 8 * sizeof(char), 1, fp) != 1) {
            LOGE("write file: %s fail", FILE_OF_DEVICE_INFO);
            return RESULT_ERR;
        } else {
            LOGD("write file: %s success", FILE_OF_DEVICE_INFO);
        }
        fclose(fp);

        fp = fopen(FILE_OF_DEVICE_INFO,"r+");
        if(fp == NULL)
        {
            LOGE("open/create file: %s fail", FILE_OF_DEVICE_INFO);
            return RESULT_ERR;
        } else {
            LOGD("open/create file: %s success", FILE_OF_DEVICE_INFO);
        }
        memset(frame, 0, data_num * 8 * sizeof(char));
        for (int i = 0; i < data_num; i++) {
            fread(frame, sizeof(device_configs_t) + data_num * 8 * sizeof(char), 1, fp);
            for (int j = 0; j < 8; j++) {
                printf("%02X ", frame->info[i * 8 + j]);
            }
            printf("\n");
        }
        fclose(fp);
        if (frame != NULL) {
            free(frame);
        }
*/
        voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_BOARD_BEEP_NOTICE);
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_board_factory(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        if (factoryThread == NULL) {
            LOGD("????????????????????????");
            voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_BOARD_FACTORY_TEST);
        } else {
            sleep(5);
        }
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_board_wifi_status(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x1A) {
        //int32_t time_period = 0;
        //gettimeofday(&timer_msg_end,NULL);

        //time_period = (timer_msg_end.tv_sec * 1000 + timer_msg_end.tv_usec / 1000) - \
		(timer_msg_start.tv_sec * 1000 + timer_msg_start.tv_usec / 1000);
        //if (time_period >= 1000) {
            LOGD("??????wifi????????????");
            //gettimeofday(&timer_msg_start,NULL);

            if (UART->data[5 - 5] == 0x00) {
                if (UART->data[14 - 5] == ROUTER_STATUS_CONNECTED) {
                    cmd_all_voice_reply_function("?????????????????????", 277, (UART->play & m_ac.local_dev_set_play));
                } else if (UART->data[14 - 5] == ROUTER_STATUS_DISCONNECTED) {
                    //cmd_all_voice_reply_function("??????????????????,???????????????APP,??????????????????,??????????????????", \
			407, (UART->play & m_ac.local_dev_set_play));
                } else if (UART->data[14 - 5] == ROUTER_STATUS_CONNECTING) {
                    cmd_all_voice_reply_function("??????????????????", 276, (UART->play & m_ac.local_dev_set_play));
                } else if (UART->data[14 - 5] == ROUTER_STATUS_PWD_ERROR) {
                    cmd_all_voice_reply_function("wifi??????????????????????????????", 278, (UART->play & m_ac.local_dev_set_play));
                } else if (UART->data[14 - 5] == ROUTER_STATUS_NO_ROUTER) {
                    LOGD("????????????????????????");
                    cmd_all_voice_reply_function("????????????????????????????????????", 279, (UART->play & m_ac.local_dev_set_play));
                } else if (UART->data[14 - 5] == ROUTER_STATUS_GETIP_FAIL) {
                    LOGD("IP????????????");
                    cmd_all_voice_reply_function("????????????????????????????????????", 279, (UART->play & m_ac.local_dev_set_play));
                }  else if (UART->data[14 - 5] == ROUTER_STATUS_NOT_STABLE) {
                    LOGD("???????????????");
                    cmd_all_voice_reply_function("????????????????????????????????????", 279, (UART->play & m_ac.local_dev_set_play));
                }  else if (UART->data[14 - 5] == ROUTER_STATUS_WIFI_FAULT) {
                    LOGD("WIFI??????");
                    cmd_all_voice_reply_function("????????????????????????????????????", 279, (UART->play & m_ac.local_dev_set_play));
                }  else if (UART->data[14 - 5] == ROUTER_STATUS_SN_ABNORMAL) {
                    LOGD("SN??????");
                    cmd_all_voice_reply_function("????????????????????????????????????", 279, (UART->play & m_ac.local_dev_set_play));
                }
            } else if (UART->data[5 - 5] == 0x01) {
                LOGD("??????????????????");
#ifdef WAKEUP_TIMEOUT_STD_ENABLE
                cmd_all_voice_reply_function("??????????????????,???????????????APP,??????????????????,??????????????????", \
			162, (UART->play & m_ac.local_dev_set_play));
#else
                cmd_all_voice_reply_function("?????????????????????????????????", 294, m_ac.eii_dev_set_play);
#endif
            } else if (UART->data[5 - 5] == 0x02) {
                LOGD("????????????");
                cmd_all_voice_reply_function("????????????????????????????????????", 279, (UART->play & m_ac.local_dev_set_play));
            }
            voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_BOARD_WIFI_STATUS);
            return ASR_AC_ERROR_NONE;
        //}
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

int cmd_board_passthrough(cmd_uart_msg_t* UART)
{
    if (UART->data[0] == ASR_DEV_CLOUD_SND_CONFIGS) {
        LOGD("???????????????????????????");
        int dev_num = 0;
        dev_num = (UART->msgLen - 6 - 2) / 8;
        LOGI("cloud snd dev config nums: %d", dev_num);

        if (UART->data[1] == 0x00) { //?????????????????????????????????
            m_ac.eii_dev_set_asr  =   ASR_DISABLE;
            system("rm -f /etc/std/eii_asr_enable");
            goto success;
        }else if (UART->data[1] == 0x01) { //?????????????????????????????????
            m_ac.eii_dev_set_asr  =   ASR_ENABLE;
            system("touch /etc/std/eii_asr_enable");
        }

        device_configs_t* frame = (device_configs_t*)malloc(sizeof(device_configs_t) \
                + dev_num * 8 * sizeof(char));
        if (frame == NULL) {
            LOGE("frame malloc fail!");
            return RESULT_ERR;
        }

        for (int i = 0; i < dev_num; i++) {
            frame->info[i * 8] = UART->data[i * 8 + 2];
            frame->info[i * 8 + 1] = UART->data[i * 8 + 1 + 2];
            memcpy(&frame->info[i * 8 + 2], &UART->data[i * 8 + 2 + 2], 6 * sizeof(char));
        }

        FILE *fp;
        fp = fopen(FILE_OF_DEVICE_INFO, "w+");
        if (!fp)
        {
            LOGE("open/create file: %s fail", FILE_OF_DEVICE_INFO);
            goto fail;
        } else {
            LOGD("open/create file: %s success", FILE_OF_DEVICE_INFO);
        }

        if (fwrite(frame, sizeof(device_configs_t) + dev_num * 8 * sizeof(char), 1, fp) != 1) {
            LOGE("file write error\n");
            goto fail;
        } else {
            LOGD("write file: %s success", FILE_OF_DEVICE_INFO);
        }

	if (fp != NULL) {
            fclose(fp);
    	}
        if (frame != NULL) {
            free(frame);
        }
        goto success;
        //voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_BOARD_WIFI_PASSTHROUGH);
        //return ASR_AC_ERROR_NONE;
    	fail:
    	if (fp != NULL) {
            fclose(fp);
    	}
        if (frame != NULL) {
            free(frame);
        }
    	return RESULT_ERR;
    } else if (UART->data[0] == ASR_DEV_CLOUD_RECV_CONFIGS) {
        LOGD("???????????????????????????");
    } else if (UART->data[0] == ASR_DEV_CLOUD_QUERY_WEATHER) {
        if (UART->data[1] == ASR_DEV_QUERY_WEATHER_DISCONNECT) {
            cmd_all_voice_reply_function("?????????????????????????????????", 294, m_ac.eii_dev_set_play);
        } else if (UART->data[1] == ASR_DEV_QUERY_WEATHER_FAIL) {
            LOGD("??????????????????");
        } else if (UART->data[1] == ASR_DEV_QUERY_WEATHER_SUCCESS) {
            int temp = 0;
            float setT = 0;
	    int id1 = -1, id2 = -1;
            LOGD("??????????????????");
            //???????????????XX???XX??????????????????????????????
	    //UART->play not used in EII?
            //cmd_all_voice_reply_function("????????????", 264, (UART->play & m_ac.local_dev_set_play));
            cmd_all_voice_reply_function("????????????", 264, m_ac.eii_dev_set_play);
            if (UART->data[18 - 5 - 11] == UART->data[19 - 5 - 11]) {
                cmd_all_voice_reply_function("xxx", 205 + UART->data[18 - 5 - 11], m_ac.eii_dev_set_play);
            } else {
                cmd_all_voice_reply_function("xxx", 205 + UART->data[18 - 5 - 11], m_ac.eii_dev_set_play);
                cmd_all_voice_reply_function("???", 265, m_ac.eii_dev_set_play);
                cmd_all_voice_reply_function("xxx", 205 + UART->data[19 - 5 - 11], m_ac.eii_dev_set_play);
            }
            //?????????XX???XX????????????????????????17???19????????????
            if (UART->data[23 - 5 - 11] & 0x80)
                temp = -(0xffff-(UART->data[22 - 5 - 11] + (UART->data[23 - 5 - 11] << 8))+1);
            else
                temp = (UART->data[22 - 5 - 11] + (UART->data[23 - 5 - 11] << 8));

            if ((temp >= -50 * 100) && (temp <= 50 * 100)) {
                setT = (temp / 100.0f);
                temp = (temp % 100);
                cmd_all_voice_reply_function("??????", 254, m_ac.eii_dev_set_play);
                if (temp == 0) {
                    if ((setT >= 0) && (setT <= 50)) {
                        cmd_all_voice_reply_function("xx???", ((setT * 100 - temp)/100.0f) + 17, \
				m_ac.eii_dev_set_play);
                    } else if ((setT < 0) && (setT >= -50)){
                        cmd_all_voice_reply_function("??????", 15, m_ac.eii_dev_set_play);
                        cmd_all_voice_reply_function("xx???", ((-(setT * 100) - (-temp))/100.0f) + 17, \
				m_ac.eii_dev_set_play);
                    }
                } else {
                    if ((setT >= 0) && (setT <= 50)) {
                        cmd_all_voice_reply_function("xx???", ((setT * 100 - temp)/100.0f) + 68, \
				m_ac.eii_dev_set_play);
                        cmd_all_voice_reply_function("xx???", (temp / 10) + 17, m_ac.eii_dev_set_play);
                    } else if ((setT < 0) && (setT >= -50)){
                        cmd_all_voice_reply_function("??????", 15, m_ac.eii_dev_set_play);
                        cmd_all_voice_reply_function("xx???", ((-(setT * 100) - (-temp))/100.0f) + 68, \
				m_ac.eii_dev_set_play);
                        cmd_all_voice_reply_function("xx???", ((-temp) / 10) + 17, m_ac.eii_dev_set_play);
                    }
                }

                cmd_all_voice_reply_function("???", 266, m_ac.eii_dev_set_play);

                temp = 0;
                if (UART->data[21 - 5 - 11] & 0x80)
                    temp = -(0xffff-(UART->data[20 - 5 - 11] + (UART->data[21 - 5 - 11] << 8))+1);
                else
                    temp = (UART->data[20 - 5 - 11] + (UART->data[21 - 5 - 11] << 8));
                if ((temp >= -50 * 100) && (temp <= 50 * 100)) {
                    setT = (temp / 100.0f);
                    temp = (temp % 100);
                    if (temp == 0) {
                        if ((setT >= 0) && (setT <= 50)) {
                            cmd_all_voice_reply_function("xx???", ((setT * 100 - temp)/100.0f) + 17, \
				m_ac.eii_dev_set_play);
                        } else if ((setT < 0) && (setT >= -50)){
                            cmd_all_voice_reply_function("??????", 15, m_ac.eii_dev_set_play);
                            cmd_all_voice_reply_function("xx???", ((-(setT * 100) - (-temp))/100.0f) + 17, \
				m_ac.eii_dev_set_play);
                        }
                    } else {
                        if ((setT >= 0) && (setT <= 50)) {
                            cmd_all_voice_reply_function("xx???", ((setT * 100 - temp)/100.0f) + 68, \
				m_ac.eii_dev_set_play);
                            cmd_all_voice_reply_function("xx???", (temp / 10) + 17, m_ac.eii_dev_set_play);
                        } else if ((setT < 0) && (setT >= -50)){
                            cmd_all_voice_reply_function("??????", 15, m_ac.eii_dev_set_play);
                            cmd_all_voice_reply_function("xx???", ((-(setT * 100) - (-temp))/100.0f) + 68, \
				m_ac.eii_dev_set_play);
                            cmd_all_voice_reply_function("xx???", ((-temp) / 10) + 17, m_ac.eii_dev_set_play);
                        }
                    }
                }
            }
            //??????????????????/???/????????????/????????????/?????????????????????????????????
            cmd_all_voice_reply_function("????????????", 256, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("xxx", UART->data[30 - 5 - 11] + 257, m_ac.eii_dev_set_play);

	    //???????????????????????????????????????XX%??????????????????35%
	    id1 = UART->data[31 - 5 - 11] / 10;
	    id2 = UART->data[31 - 5 - 11] % 10;
            LOGT("??????:%d, id1 = %d, id2 = %d\n", UART->data[31 - 5 - 11], id1, id2);
	    if (UART->data[31 - 5 - 11] == 0 ) {
		cmd_all_voice_reply_function("?????????", 167, m_ac.eii_dev_set_play);
		cmd_all_voice_reply_function("???", 118, m_ac.eii_dev_set_play);
	    } else if ((UART->data[31 - 5 - 11] > 0 ) || (UART->data[31 - 5 - 11] < 100 )) {
		cmd_all_voice_reply_function("?????????", 167, m_ac.eii_dev_set_play);
		if ((id1 > 0) && (id1 <= 9)) {
			cmd_all_voice_reply_function("x", 118 + id1, m_ac.eii_dev_set_play);
			cmd_all_voice_reply_function("???", 128, m_ac.eii_dev_set_play);
		}

		if ((id2 > 0) && (id2 <= 9)) {
			cmd_all_voice_reply_function("x", 118 + id2, m_ac.eii_dev_set_play);
		}
	    } else if (UART->data[31 - 5 - 11] == 100 ) {
		cmd_all_voice_reply_function("????????????", 198, m_ac.eii_dev_set_play);
	    }
/*
	    if ((UART->data[31 - 5 - 11] >= 0 ) || (UART->data[31 - 5 - 11] <= 39 )) {
		cmd_all_voice_reply_function("xxx", 0 + 268, m_ac.eii_dev_set_play);
	    } else if ((UART->data[31 - 5 - 11] >= 40 ) || (UART->data[31 - 5 - 11] <= 70 )) {
		cmd_all_voice_reply_function("xxx", 1 + 268, m_ac.eii_dev_set_play);
	    } else if ((UART->data[31 - 5 - 11] >= 71 ) || (UART->data[31 - 5 - 11] <= 100 )) {
		cmd_all_voice_reply_function("xxx", 2 + 268, m_ac.eii_dev_set_play);
	    }
*/
        }
    } else if (UART->data[0] == ASR_DEV_CLOUD_SND_EII_CONFIGS) { //???????????????????????????????????????????????????????????????
        if (UART->data[1] == 0x00) {    //???????????????????????????????????????????????????
            m_ac.eii_dev_set_asr  =   ASR_DISABLE;
            system("rm -f /etc/std/eii_asr_enable");
            m_ac.eii_dev_set_play =   ASR_DISABLE;
            system("rm -f /etc/std/eii_play_enable");
        } else if (UART->data[1] == 0x01) { //???????????????????????????????????????????????????
            m_ac.eii_dev_set_asr  =   ASR_DISABLE;
            system("rm -f /etc/std/eii_asr_enable");
            m_ac.eii_dev_set_play =   ASR_ENABLE;
            system("touch /etc/std/eii_play_enable");
        } else if (UART->data[1] == 0x02) { //???????????????????????????????????????????????????
            m_ac.eii_dev_set_asr  =   ASR_ENABLE;
            system("touch /etc/std/eii_asr_enable");
            m_ac.eii_dev_set_play =   ASR_DISABLE;
            system("rm -f /etc/std/eii_play_enable");
        } else if (UART->data[1] == 0x03) { //???????????????????????????????????????????????????
            m_ac.eii_dev_set_asr  =   ASR_ENABLE;
            system("touch /etc/std/eii_asr_enable");
            m_ac.eii_dev_set_play =   ASR_ENABLE;
            system("touch /etc/std/eii_play_enable");
        }
    } else if (UART->data[0] == ASR_DEV_CLOUD_RECV_EII_CONFIGS) { //???????????????????????????????????????????????????????????????
        LOGD("???????????????????????????????????????????????????????????????");
        voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_PASSTHROUGH);
    } else if (UART->data[0] == ASR_DEV_CTL_REPLY_A5) {
/*
        if (m_ac.asr_type == ASR_AC_ASR_TYPE_OFF) {
            cmd_all_voice_reply_function("???????????????", 170, m_ac.eii_dev_set_play);
        } else if (m_ac.asr_type == ASR_AC_ASR_TYPE_ON) {
            cmd_all_voice_reply_function("???????????????", 169, m_ac.eii_dev_set_play);
        } else if (m_ac.asr_type == ASR_AC_ASR_TYPE_SET) {
            cmd_all_voice_reply_function("???????????????", 171, m_ac.eii_dev_set_play);
        }
*/
        int32_t time_period = 0;
        gettimeofday(&timer_end,NULL);
        time_period = (timer_end.tv_sec * 1000 + timer_end.tv_usec / 1000) - (timer_start.tv_sec * 1000 \
                + timer_start.tv_usec / 1000);
        if (time_period >= 1000) {
		gettimeofday(&timer_start,NULL);
	        eii_ctl_timeout_flag = 0x0;
		//DeviceType
	        //if (UART->data[2] == ASR_OBJ_AC) {  //????????????????????????
	        if (UART->data[1] == ASR_OBJ_AC) {  //???????????????????????? //abnormal
			if (m_ac.asr_type == ASR_AC_ASR_TYPE_OFF) {
				cmd_all_voice_reply_function("????????????", 309, m_ac.eii_dev_set_play);
	   			cmd_all_voice_reply_function("?????????", 7, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_ON) {
				cmd_all_voice_reply_function("????????????", 309, m_ac.eii_dev_set_play);
		    		cmd_all_voice_reply_function("?????????", 8, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_SET) {
				cmd_all_voice_reply_function("???????????????", 171, m_ac.eii_dev_set_play);
			}
	        } else if (UART->data[2] == ASR_OBJ_FAN) {  //??????
			if (m_ac.asr_type == ASR_AC_ASR_TYPE_OFF) {
				cmd_all_voice_reply_function("??????", 306, m_ac.eii_dev_set_play);
				cmd_all_voice_reply_function("?????????", 7, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_ON) {
				cmd_all_voice_reply_function("??????", 306, m_ac.eii_dev_set_play);
				cmd_all_voice_reply_function("?????????", 8, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_SET) {
				cmd_all_voice_reply_function("???????????????", 171, m_ac.eii_dev_set_play);
			}
	        //} else if (UART->data[2] == ASR_OBJ_AIR_CLEANER) {  //???????????????
	        } else if (UART->data[1] == ASR_OBJ_AIR_CLEANER) {  //??????????????? //abnormal
			if (m_ac.asr_type == ASR_AC_ASR_TYPE_OFF) {
				cmd_all_voice_reply_function("?????????", 308, m_ac.eii_dev_set_play);
				cmd_all_voice_reply_function("?????????", 7, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_ON) {
				cmd_all_voice_reply_function("?????????", 308, m_ac.eii_dev_set_play);
				cmd_all_voice_reply_function("?????????", 8, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_SET) {
				cmd_all_voice_reply_function("???????????????", 171, m_ac.eii_dev_set_play);
			}
	        } else if (UART->data[2] == ASR_OBJ_HUMIDIFIER) {   //?????????
			if (m_ac.asr_type == ASR_AC_ASR_TYPE_OFF) {
				cmd_all_voice_reply_function("?????????", 307, m_ac.eii_dev_set_play);
				cmd_all_voice_reply_function("?????????", 7, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_ON) {
				cmd_all_voice_reply_function("?????????", 307, m_ac.eii_dev_set_play);
				cmd_all_voice_reply_function("?????????", 8, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_SET) {
				cmd_all_voice_reply_function("???????????????", 171, m_ac.eii_dev_set_play);
			}
	        //} else if (UART->data[2] == ASR_OBJ_SWEEP_ROBOT) {  //???????????????
	        } else if (UART->data[1] == ASR_OBJ_SWEEP_ROBOT) {  //??????????????? //abnormal
			if (m_ac.asr_type == ASR_AC_ASR_TYPE_ROBOT_CLEAN) {
				cmd_all_voice_reply_function("???????????????", 303, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_ROBOT_PAUSE) {
				cmd_all_voice_reply_function("???????????????", 304, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_ROBOT_CHARGING) {
				cmd_all_voice_reply_function("??????????????????", 305, m_ac.eii_dev_set_play);
			}
	        //} else if (UART->data[2] == ASR_OBJ_SMART_LAMP) { //????????????
	        } else if (UART->data[1] == ASR_OBJ_SMART_LAMP) { //???????????? //abnormal
			if (m_ac.asr_type == ASR_AC_ASR_TYPE_OFF) {
				cmd_all_voice_reply_function("???", 310, m_ac.eii_dev_set_play);
				cmd_all_voice_reply_function("?????????", 7, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_ON) {
				cmd_all_voice_reply_function("???", 310, m_ac.eii_dev_set_play);
				cmd_all_voice_reply_function("?????????", 8, m_ac.eii_dev_set_play);
			} else if (m_ac.asr_type == ASR_AC_ASR_TYPE_SET) {
				cmd_all_voice_reply_function("???????????????", 171, m_ac.eii_dev_set_play);
			}
		}
	}
    } else if (UART->data[0] == ASR_DEV_CTL_WIFI_STATUS) {
        if (UART->data[1] == ASR_DEV_REPLY_WIFI_CONNECTING) {
            cmd_all_voice_reply_function("??????????????????", 276, m_ac.eii_dev_set_play);
        } else if (UART->data[1] == ASR_DEV_REPLY_WIFI_SUCCESS) {
            cmd_all_voice_reply_function("?????????????????????", 277, m_ac.eii_dev_set_play);
        } else if (UART->data[1] == ASR_DEV_REPLY_WIFI_PWD_ERR) {
            cmd_all_voice_reply_function("wifi??????????????????????????????", 278, m_ac.eii_dev_set_play);
        } else if (UART->data[1] == ASR_DEV_REPLY_WIFI_FAIL) {
            cmd_all_voice_reply_function("????????????????????????????????????", 279, m_ac.eii_dev_set_play);
        }
    } else if (UART->data[0] == ASR_DEV_NOTICE_AA) { //??????????????????
        //RoomType
        if (UART->data[1] == ASR_LIVING_ROOM) {

        } else if (UART->data[1] == ASR_BED_ROOM) {

        } else if (UART->data[1] == ASR_ALL_ROOM) {

        }

        //DeviceType
        if (UART->data[2] == ASR_OBJ_AC) {  //????????????????????????

        } else if (UART->data[2] == ASR_OBJ_FAN) {  //??????

        } else if (UART->data[2] == ASR_OBJ_AIR_CLEANER) {  //???????????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????????????????????????????????????????", 188, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_HUMIDIFIER) {   //?????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("???????????????????????????", 187, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_SWEEP_ROBOT) {  //???????????????

        } else if (UART->data[2] == ASR_OBJ_SMART_LAMP) { //????????????

        } else if (UART->data[2] == ASR_OBJ_RICE_COOKER) { //?????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????????????????", 179, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_CULINARY_MACHINE) {//?????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????????????????", 180, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_PRESSURE_COOKER) {//?????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????????????????", 182, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_DISH_WASHER) {//?????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????", 183, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_RIPPLE_WASHING_MACHINE) {   //???????????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("???????????????", 185, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_CYLINDER_WASHING_MACHINE) { //???????????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("???????????????", 185, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_COMPOUND_WASHING_MACHINE) { //??????????????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("???????????????", 185, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_AIR_WATER_HEATER) { //??????????????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????????????????", 186, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_ELECTRIC_WATER_HEATER) {    //????????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????????????????", 186, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_GAS_WATER_HEATER) { //????????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????????????????", 186, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_ELECTRIC_BOILER) {  //????????????
            cmd_all_voice_reply_function("??????", 173, m_ac.eii_dev_set_play);
            cmd_all_voice_reply_function("????????????", 184, m_ac.eii_dev_set_play);
        } else if (UART->data[2] == ASR_OBJ_BROADCAST_ALL) {    //??????????????????

        }
    }

success:
    voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_BOARD_WIFI_PASSTHROUGH);
    return ASR_AC_ERROR_NONE;
}

int cmd_board_set_configs(cmd_uart_msg_t* UART)
{
    if (UART->msgLen >= 0x0A) {
        LOGD("??????????????????");
        if (UART->data[9 - 5] == 0x00) { //???????????????????????????????????????
            m_ac.local_dev_set_asr  =   ASR_DISABLE;
            system("rm -f /etc/std/local_asr_enable");
            m_ac.local_dev_set_play =   ASR_DISABLE;
            system("rm -f /etc/std/local_playback_enable");
            LOGD("???????????????????????????????????????");
        } else if (UART->data[9 - 5] == 0x01) { //???????????????????????????????????????
            m_ac.local_dev_set_asr  =   ASR_ENABLE;
            system("touch /etc/std/local_asr_enable");
            m_ac.local_dev_set_play =   ASR_DISABLE;
            system("rm -f /etc/std/local_playback_enable");
            LOGD("???????????????????????????????????????");
        } else if (UART->data[9 - 5] == 0x02) { //???????????????????????????????????????
            m_ac.local_dev_set_asr  =   ASR_DISABLE;
            system("rm -f /etc/std/local_asr_enable");
            m_ac.local_dev_set_play =   ASR_ENABLE;
            system("touch /etc/std/local_playback_enable");
            LOGD("???????????????????????????????????????");
        } else if (UART->data[9 - 5] == 0x03) { //???????????????????????????????????????
            m_ac.local_dev_set_asr  =   ASR_ENABLE;
            system("touch /etc/std/local_asr_enable");
            m_ac.local_dev_set_play =   ASR_ENABLE;
            system("touch /etc/std/local_playback_enable");
            LOGD("???????????????????????????????????????");
        }

        if ((UART->data[8 - 5] >= 0) && (UART->data[8 - 5] <= 100)) {
            m_ac.volume = UART->data[8 - 5];
            LOGD("m_ac.volume = %d", m_ac.volume);
        }

        voice_board_uart_ack(ASR_UART_MSG_TYPE_ACK_BOARD_STD, ASR_UART_MSG_BOARD_SET_CONFIGS);

        if (m_ac.volume == 0) {
            system("amixer cset name='head phone volume' 0");
            LOGD("??????");
        } else if ((m_ac.volume > 0) && (m_ac.volume <= 20)) {
            system("amixer cset name='head phone volume' 46");
            LOGD("??????1");
        } else if ((m_ac.volume > 20) && (m_ac.volume <= 40)) {
            system("amixer cset name='head phone volume' 49");
            LOGD("??????2");
        } else if ((m_ac.volume > 40) && (m_ac.volume <= 60)) {
            system("amixer cset name='head phone volume' 53");
            LOGD("??????3");
        } else if ((m_ac.volume > 60) && (m_ac.volume <= 80)) {
            system("amixer cset name='head phone volume' 55");
            LOGD("??????4");
        } else if ((m_ac.volume > 80) && (m_ac.volume <= 100)) {
            system("amixer cset name='head phone volume' 59");
            LOGD("??????5");
        }
        return ASR_AC_ERROR_NONE;
    }
    return ASR_AC_ERROR_UART_REPLY_ERROR;
}

