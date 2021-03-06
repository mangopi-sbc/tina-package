enum { __FILE_NUM__ = 0 };

/**
*****************************************************************************************
*     Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file     light_ctl_setup_server.c
  * @brief    Source file for light ctl setup server model.
  * @details  Data types and external functions declaration.
  * @author   hector
  * @date     2018-8-1
  * @version  v1.0
  * *************************************************************************************
  */

/* Add Includes here */
#include "light_ctl.h"

static bool light_ctl_setup_server_receive(mesh_msg_p pmesh_msg)
{
    bool ret = TRUE;
    uint8_t *pbuffer = pmesh_msg->pbuffer + pmesh_msg->msg_offset;

    switch (pmesh_msg->access_opcode)
    {
    case MESH_MSG_LIGHT_CTL_TEMPERATURE_RANGE_SET:
    case MESH_MSG_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
        if (pmesh_msg->msg_len == sizeof(light_ctl_temperature_range_set_t))
        {
            light_ctl_temperature_range_set_t *pmsg = (light_ctl_temperature_range_set_t *)pbuffer;
            if ((pmsg->range_min < pmsg->range_max) &&
                IS_LIGHT_CTL_TEMPERATURE_RANGE_VALID(pmsg->range_min) &&
                IS_LIGHT_CTL_TEMPERATURE_RANGE_VALID(pmsg->range_max))
            {
                light_ctl_server_set_temperature_range_t set_range;
                set_range.range_min = pmsg->range_min;
                set_range.range_max = pmsg->range_max;
                if (NULL != pmesh_msg->pmodel_info->model_data_cb)
                {
                    pmesh_msg->pmodel_info->model_data_cb(pmesh_msg->pmodel_info,
                                                          LIGHT_CTL_SERVER_SET_TEMPERATURE_RANGE, &set_range);
                }
                if (MESH_MSG_LIGHT_CTL_TEMPERATURE_RANGE_SET == pmesh_msg->access_opcode)
                {
                    light_ctl_temperature_range_stat(pmesh_msg->pmodel_info, pmesh_msg->src, pmesh_msg->app_key_index,
                                                     GENERIC_STAT_SUCCESS, set_range.range_min,
                                                     set_range.range_max);
                }
            }
        }
        break;
    case MESH_MSG_LIGHT_CTL_DEFAULT_SET:
    case MESH_MSG_LIGHT_CTL_DEFAULT_SET_UNACK:
        if (pmesh_msg->msg_len == sizeof(light_ctl_default_set_t))
        {
            light_ctl_default_set_t *pmsg = (light_ctl_default_set_t *)pbuffer;
            if (IS_LIGHT_CTL_TEMPERATURE_VALID(pmsg->temperature))
            {
                light_ctl_server_set_default_t set_default;
                set_default.lightness = pmsg->lightness;
                set_default.temperature = pmsg->temperature;
                set_default.delta_uv = pmsg->delta_uv;
                if (NULL != pmesh_msg->pmodel_info->model_data_cb)
                {
                    pmesh_msg->pmodel_info->model_data_cb(pmesh_msg->pmodel_info, LIGHT_CTL_SERVER_SET_DEFAULT,
                                                          &set_default);
                }
                if (MESH_MSG_LIGHT_CTL_DEFAULT_SET == pmesh_msg->access_opcode)
                {
                    light_ctl_default_stat(pmesh_msg->pmodel_info, pmesh_msg->src, pmesh_msg->app_key_index,
                                           set_default.lightness, set_default.temperature,
                                           set_default.delta_uv);
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

static bool is_model_opcode(uint16_t opcode)
{
   bool ret = false;
   // l_info("%s,%d\n",__FILE__,opcode);
    switch (opcode)
    {
        case MESH_MSG_LIGHT_CTL_TEMPERATURE_RANGE_SET:
        case MESH_MSG_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
        case MESH_MSG_LIGHT_CTL_DEFAULT_SET:
        case MESH_MSG_LIGHT_CTL_DEFAULT_SET_UNACK:
            ret = true;
            break;
        default:
            break;
    }
   return ret;
}

bool light_ctl_setup_server_reg(uint8_t element_index, mesh_model_info_p pmodel_info)
{
    if (NULL == pmodel_info)
    {
        return FALSE;
    }
    pmodel_info->model_id = RTK_MESH_MODEL_LIGHT_CTL_SETUP_SERVER;
    if (NULL == pmodel_info->model_receive)
    {
        pmodel_info->model_receive = light_ctl_setup_server_receive;
        if (NULL == pmodel_info->model_data_cb)
        {
            printw("light_ctl_setup_server_reg: missing model data process callback!");
        }
    }
    pmodel_info->is_opcode_of_model = &is_model_opcode;
    return mesh_model_reg(element_index, pmodel_info);
}
