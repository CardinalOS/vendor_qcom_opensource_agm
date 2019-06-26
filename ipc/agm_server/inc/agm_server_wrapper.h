/*
** Copyright (c) 2019, The Linux Foundation. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above
**     copyright notice, this list of conditions and the following
**     disclaimer in the documentation and/or other materials provided
**     with the distribution.
**   * Neither the name of The Linux Foundation nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
** WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
** ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
** OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
** IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/


#ifndef __Agm_SERVER_H__
#define __Agm_SERVER_H__

#include <utils/Log.h>
#include <agm/agm_api.h>
#include "ipc_interface.h"
class AgmService : public BnAgmService {
    public:
        AgmService()
        {
          ALOGE("AGMService constructor");
          agm_initialized = agm_init();
        }
        virtual int ipc_agm_init();
        virtual int ipc_agm_audio_intf_set_metadata(uint32_t audio_intf, struct agm_meta_data *metadata);
        virtual int ipc_agm_session_set_metadata(uint32_t session_id, struct agm_meta_data *metadata);
        virtual int ipc_agm_session_audio_inf_set_metadata(uint32_t session_id, uint32_t audio_intf, struct agm_meta_data *metadata);
        virtual int ipc_agm_session_close(struct session_obj *handle);
        virtual int ipc_agm_audio_intf_set_media_config(uint32_t audio_intf, struct agm_media_config *media_config);
        virtual int ipc_agm_session_prepare(struct session_obj *handle);
        virtual int ipc_agm_session_start(struct session_obj *handle);
        virtual int ipc_agm_session_stop(struct session_obj *handle);
        virtual int ipc_agm_session_pause(struct session_obj *handle);
        virtual int ipc_agm_session_resume(struct session_obj *handle);
        virtual int ipc_agm_session_open(uint32_t session_id, struct session_obj **handle);
        virtual int ipc_agm_session_read(struct session_obj *handle, void *buff, size_t count);
        virtual int ipc_agm_session_write(struct session_obj *handle, void *buff, size_t count);
        virtual int ipc_agm_session_audio_inf_connect(uint32_t session_id, uint32_t audio_intf, bool state);
        virtual int ipc_agm_session_set_loopback(uint32_t capture_session_id, uint32_t playback_session_id, bool state);
		virtual size_t ipc_agm_get_hw_processed_buff_cnt(struct session_obj *handle, enum direction dir);
		virtual int ipc_agm_get_aif_info_list(struct aif_info *aif_list, size_t *num_aif_info);
        virtual int ipc_agm_session_set_config(struct session_obj *handle, struct agm_session_config *session_config, struct agm_media_config *media_config,
       						                struct agm_buffer_config *buffer_config);
    private:
         bool agm_initialized = false;
};

#endif