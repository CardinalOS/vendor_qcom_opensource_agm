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

#define LOG_TAG "agm_server_wrapper"

#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>

#include <agm/agm_api.h>
#include "agm_server_wrapper.h"
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>
#include <system/thread_defs.h>
#include <sys/resource.h>
#include <cstring>
#include <memory.h>
#include <pthread.h>
#include <string.h>
#include <binder/IMemory.h>

using namespace android;

#if 0
AgmService::AgmService()
{
    agm_initialized = !agm_init();
    ALOGE("agm_initialized %d", agm_initialized);
}

AgmService::~AgmService()
{
    agm_initialized = agm_deinit();
    ALOGE("agm_initialized %d", agm_initialized);
}
#endif

int AgmService::ipc_agm_session_read(struct session_obj *handle, void *buff, size_t count){
    ALOGE("%s called \n", __func__);
    return agm_session_read(handle, buff, count);
};
int AgmService::ipc_agm_session_write(struct session_obj *handle, void *buff, size_t count){
    ALOGE("%s called \n", __func__);
    return agm_session_write(handle, buff, count);
};

int AgmService::ipc_agm_init(){
    ALOGE("%s called\n", __func__);
    return 0;
};

int AgmService::ipc_agm_audio_intf_set_metadata(uint32_t audio_intf, struct agm_meta_data *metadata){
    ALOGE("%s called\n", __func__);
    return agm_audio_intf_set_metadata(audio_intf, metadata);
};

int AgmService::ipc_agm_session_set_metadata(uint32_t session_id, struct agm_meta_data *metadata){
    ALOGE("%s called\n", __func__);
    return agm_session_set_metadata(session_id, metadata);
};

int AgmService::ipc_agm_session_audio_inf_set_metadata(uint32_t session_id, uint32_t audio_intf, struct agm_meta_data *metadata){
    ALOGE("%s called\n", __func__);
    return agm_session_audio_inf_set_metadata(session_id, audio_intf, metadata);
};

int AgmService::ipc_agm_session_close(struct session_obj *handle){
    ALOGE("%s called\n", __func__);
    return agm_session_close(handle);
};

int AgmService::ipc_agm_session_prepare(struct session_obj *handle){
    ALOGE("%s called\n", __func__);
    return agm_session_prepare(handle);
};

int AgmService::ipc_agm_session_start(struct session_obj *handle){
    ALOGE("%s called\n", __func__);
    return agm_session_start(handle);
};

int AgmService::ipc_agm_session_stop(struct session_obj *handle){
    ALOGE("%s called\n", __func__);
    return agm_session_stop(handle);
};

int AgmService::ipc_agm_session_pause(struct session_obj *handle){
    ALOGE("%s called\n", __func__);
    return agm_session_pause(handle);
};

int AgmService::ipc_agm_session_resume(struct session_obj *handle){
    ALOGE("%s called\n", __func__);
    return agm_session_resume(handle);
};

int AgmService::ipc_agm_session_set_loopback(uint32_t capture_session_id, uint32_t playback_session_id, bool state){
    ALOGE("%s called\n", __func__);
    return agm_session_set_loopback(capture_session_id, playback_session_id, state);
};

size_t AgmService::ipc_agm_get_hw_processed_buff_cnt(struct session_obj *handle, enum direction dir) {
    ALOGE("%s called\n", __func__);
    return agm_get_hw_processed_buff_cnt(handle, dir);
};

int AgmService::ipc_agm_get_aif_info_list(struct aif_info *aif_list, size_t *num_aif_info){
    ALOGE("%s called\n", __func__);
    return agm_get_aif_info_list(aif_list, num_aif_info);
};

int AgmService::ipc_agm_session_open(uint32_t session_id, struct session_obj **handle){
    ALOGE("%s called\n", __func__);
    return agm_session_open(session_id, handle);
};

int AgmService::ipc_agm_session_set_config(struct session_obj *handle,
                                           struct agm_session_config *session_config,
                                           struct agm_media_config *media_config,
                                           struct agm_buffer_config *buffer_config){
    ALOGE("%s called\n", __func__);
    return agm_session_set_config(handle, session_config, media_config, buffer_config);
};

 int AgmService::ipc_agm_session_audio_inf_connect(uint32_t session_id, uint32_t audio_intf, bool state){	
     ALOGE("%s called\n", __func__);	
     return agm_session_audio_inf_connect(session_id, audio_intf, state);	
 };
 
int AgmService::ipc_agm_audio_intf_set_media_config(uint32_t audio_intf, struct agm_media_config *media_config){
    ALOGE("%s called\n", __func__);
    return agm_audio_intf_set_media_config(audio_intf, media_config);
};

