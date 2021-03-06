/**
*****************************************************************************************
*     Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
* @file     light_lightness_setup_server.c
* @brief    Source file for light lightness setup server model.
* @details  Data types and external functions declaration.
* @author   hector_huang
* @date     2018-7-30
* @version  v1.0
* *************************************************************************************
*/

/* Add Includes here */
#include "light_lightness.h"

extern mesh_msg_send_cause_t light_lightness_default_stat(mesh_model_info_p pmodel_info,
                                                          uint16_t dst,
                                                          uint16_t app_key_index, uint16_t lightness);
extern mesh_msg_send_cause_t light_lightness_range_stat(mesh_model_info_p pmodel_info, uint16_t dst,
                                                        uint16_t app_key_index, generic_stat_t stat, uint16_t range_min, uint16_t range_max);

static bool light_lightness_setup_server_receive(mesh_msg_p pmesh_msg)
{
    bool ret = TRUE;
    uint8_t *pbuffer = pmesh_msg->pbuffer + pmesh_msg->msg_offset;

    switch (pmesh_msg->access_opcode)
    {
    case MESH_MSG_LIGHT_LIGHTNESS_DEFAULT_SET:
    case MESH_MSG_LIGHT_LIGHTNESS_DEFAULT_SET_UNACK:
        if (pmesh_msg->msg_len == sizeof(light_lightness_default_set_t))
        {
            light_lightness_default_set_p pmsg = (light_lightness_default_set_p)pbuffer;
            light_lightness_server_set_default_t set_default = {pmsg->lightness};
            if (NULL != pmesh_msg->pmodel_info->model_data_cb)
            {
                pmesh_msg->pmodel_info->model_data_cb(pmesh_msg->pmodel_info, LIGHT_LIGHTNESS_SERVER_SET_DEFAULT,
                                                      &set_default);
            }

            if (MESH_MSG_LIGHT_LIGHTNESS_DEFAULT_SET == pmesh_msg->access_opcode)
            {
                light_lightness_default_stat(pmesh_msg->pmodel_info, pmesh_msg->src, pmesh_msg->app_key_index,
                                             pmsg->lightness);
            }

        }
        break;
    case MESH_MSG_LIGHT_LIGHTNESS_RANGE_SET:
    case MESH_MSG_LIGHT_LIGHTNESS_RANGE_SET_UNACK:
        if (pmesh_msg->msg_len == sizeof(light_lightness_range_set_t))
        {
            light_lightness_range_set_p pmsg = (light_lightness_range_set_p)pbuffer;
            if ((pmsg->range_min < pmsg->range_max) &&
                (0 != pmsg->range_min) &&
                (0 != pmsg->range_max))
            {
                light_lightness_server_set_range_t set_range = {pmsg->range_min, pmsg->range_max};
                if (NULL != pmesh_msg->pmodel_info->model_data_cb)
                {
                    pmesh_msg->pmodel_info->model_data_cb(pmesh_msg->pmodel_info, LIGHT_LIGHTNESS_SERVER_SET_RANGE,
                                                          &set_range);
                }

                if (MESH_MSG_LIGHT_LIGHTNESS_RANGE_SET == pmesh_msg->access_opcode)
                {
                    light_lightness_range_stat(pmesh_msg->pmodel_info, pmesh_msg->src,
                                               pmesh_msg->app_key_index, GENERIC_STAT_SUCCESS,
                                               pmsg->range_min, pmsg->range_max);
                }
            }

        }
        break;
    default:
        ret = FALSE;
        break;
    }
    return ret;
}

bool light_lightness_setup_server_reg(uint8_t element_index, mesh_model_info_p pmodel_info)
{
    if (NULL == pmodel_info)
    {
        return FALSE;
    }

    pmodel_info->model_id = MESH_MODEL_LIGHT_LIGHTNESS_SETUP_SERVER;
    if (NULL == pmodel_info->model_receive)
    {
        pmodel_info->model_receive = light_lightness_setup_server_receive;
        if (NULL == pmodel_info->model_data_cb)
        {
            printw("light_lightness_setup_server_reg: missing model data process callback!");
        }
    }

    return mesh_model_reg(element_index, pmodel_info);
}

