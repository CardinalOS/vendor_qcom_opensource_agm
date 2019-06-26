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

#define LOG_TAG "ipc_proxy"

#include <utils/Log.h>

#include <stdlib.h>
#include <utils/RefBase.h>
#include <binder/TextOutput.h>
#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <pthread.h>
#include <cutils/list.h>
#include <signal.h>
#include <sys/prctl.h>
#include <system/thread_defs.h>
#include <sys/resource.h>
#include <cstring>
#include <memory.h>
#include <string.h>
#include "ipc_interface.h"
#include "agm_death_notifier.h"
#include <agm/agm_api.h>
#include "agm_server_wrapper.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef memscpy
#define memscpy(dst, dst_size, src, bytes_to_copy) (void) \
                    memcpy(dst, src, MIN(dst_size, bytes_to_copy))
#endif

android::sp<IAGMClient> clt_binder;

using namespace android;

enum {
    OPEN = ::android::IBinder::FIRST_CALL_TRANSACTION,
    REG_CLIENT,
    UNREG_CLIENT,
    SET_MEDIA_CONFIG,
    INIT,
    AUDIO_INTF_SET_META,
    SESSION_SET_META,
    SESSION_AUDIO_INTF_SET_META,
    SESSION_SET_CONFIG,
    CLOSE,
    PREPARE,
    START,
    STOP,
    PAUSE,
    RESUME,
    CONNECT,
    READ,
    WRITE,
    LOOPBACK,
    GET_BUFF_CNT,
    GET_AIF_LIST,
};

class BpAgmService : public ::android::BpInterface<IAgmService> {
    public:
        BpAgmService(const android::sp<android::IBinder>& impl) : BpInterface<IAgmService>(impl) {
            ALOGE("BpAgmService() called");
            android::Parcel data, reply;
            sp<IBinder> binder = new DummyBnClient();
            android::ProcessState::self()->startThreadPool();
            clt_binder = interface_cast<IAGMClient>(binder);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeStrongBinder(IInterface::asBinder(clt_binder));
            remote()->transact(REG_CLIENT, data, &reply);
            ALOGD("calling REG_CLIENT from BpAgmService ");
        }

        ~BpAgmService() {
            android:: Parcel data, reply;
            ALOGE("~BpAgmservice() destructor called");
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeStrongBinder(IInterface::asBinder(clt_binder));
            remote()->transact(UNREG_CLIENT, data, &reply);
            ALOGD("calling UNREG_CLIENT from BpAgmService ");
        }

        virtual int ipc_agm_audio_intf_set_media_config(uint32_t audio_intf, struct agm_media_config *media_config){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeUint32(audio_intf);
            uint32_t param_size = sizeof(agm_media_config);
            data.writeUint32(param_size);
            android::Parcel::WritableBlob blob;
            data.writeBlob(param_size, false, &blob);
            memset(blob.data(), 0x0, param_size);
            memscpy(blob.data(), param_size, media_config, param_size);
            remote()->transact(SET_MEDIA_CONFIG, data, &reply);
            blob.release();
            return reply.readInt32();
        }

        virtual int ipc_agm_session_set_config(struct session_obj *handle,
            struct agm_session_config *session_config,
            struct agm_media_config *media_config,
            struct agm_buffer_config *buffer_config)
        {
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)handle);

            uint32_t session_param_size = sizeof(agm_session_config);
            data.writeUint32(session_param_size);
            android::Parcel::WritableBlob blob1;
            data.writeBlob(session_param_size, false, &blob1);
            memset(blob1.data(), 0x0, session_param_size);
            memscpy(blob1.data(), session_param_size, session_config, session_param_size);

            uint32_t media_param_size = sizeof(agm_media_config);
            data.writeUint32(media_param_size);
            android::Parcel::WritableBlob blob2;
            data.writeBlob(media_param_size, false, &blob2);
            memset(blob2.data(), 0x0, media_param_size);
            memscpy(blob2.data(), media_param_size, media_config, media_param_size);

            uint32_t buffer_param_size = sizeof(agm_buffer_config);
            data.writeUint32(buffer_param_size);
            android::Parcel::WritableBlob blob3;
            data.writeBlob(buffer_param_size, false, &blob3);
            memset(blob3.data(), 0x0, buffer_param_size);
            memscpy(blob3.data(), buffer_param_size, buffer_config, buffer_param_size);

            remote()->transact(SESSION_SET_CONFIG, data, &reply);
            blob1.release();
            blob2.release();
            blob3.release();
            return reply.readInt32();
        }
        virtual int ipc_agm_init(){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            remote()->transact(INIT, data, &reply);
            return reply.readInt32();
        }
		
	virtual int ipc_agm_session_set_loopback(uint32_t capture_session_id, uint32_t playback_session_id, bool state){
		    android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeUint32(capture_session_id);
            data.writeUint32(playback_session_id);
            data.write(&state, sizeof(bool));
            remote()->transact(LOOPBACK, data, &reply);
            return reply.readInt32();
	}

        virtual int ipc_agm_audio_intf_set_metadata(uint32_t audio_intf, struct agm_meta_data *metadata){
            android::Parcel data, reply;
            android::Parcel::WritableBlob gkv_blob;
            android::Parcel::WritableBlob ckv_blob;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeUint32(audio_intf);
            uint32_t kv_size = sizeof(agm_key_value);
            uint32_t gkv_size = kv_size * (metadata->gkv.num_kvs);
            uint32_t ckv_size = kv_size * (metadata->ckv.num_kvs);
            data.writeUint32(metadata->gkv.num_kvs);
            data.writeUint32(metadata->ckv.num_kvs);
            ALOGE("Bp : amd.gkv.num_kvs = %d",metadata->gkv.num_kvs);
            ALOGE("Bp : amd.ckv.num_kvs = %d",metadata->ckv.num_kvs);
            ALOGE("Bp : gkv_size = %d",gkv_size);
            ALOGE("Bp : ckv_size = %d",ckv_size);
            data.writeBlob(gkv_size, false, &gkv_blob);
            memset(gkv_blob.data(), 0x0, gkv_size);
            memcpy(gkv_blob.data(), metadata->gkv.kv, gkv_size);
            data.writeBlob(ckv_size, false, &ckv_blob);
            memset(ckv_blob.data(), 0x0, ckv_size);
            memcpy(ckv_blob.data(), metadata->ckv.kv, ckv_size);
            remote()->transact(AUDIO_INTF_SET_META, data, &reply);
            gkv_blob.release();
            ckv_blob.release();
            return reply.readInt32();
        }

        virtual int32_t ipc_agm_session_set_metadata(uint32_t session_id, struct agm_meta_data *metadata){
            android::Parcel data, reply;
            uint32_t kv_size = sizeof(agm_key_value);
            uint32_t gkv_size = kv_size * (metadata->gkv.num_kvs);
            uint32_t ckv_size = kv_size * (metadata->ckv.num_kvs);
            android::Parcel::WritableBlob gkv_blob, ckv_blob;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeUint32(session_id);
            data.writeUint32(metadata->gkv.num_kvs);
            data.writeUint32(metadata->ckv.num_kvs);
            data.writeBlob(gkv_size, false, &gkv_blob);
            memset(gkv_blob.data(), 0x0, gkv_size);
            memcpy(gkv_blob.data(), metadata->gkv.kv, gkv_size);
            data.writeBlob(ckv_size, false, &ckv_blob);
            memset(ckv_blob.data(), 0x0, ckv_size);
            memcpy(ckv_blob.data(), metadata->ckv.kv, ckv_size);
            remote()->transact(SESSION_SET_META, data, &reply);
            gkv_blob.release();
            ckv_blob.release();
            return reply.readInt32();
        }

        virtual int32_t ipc_agm_session_audio_inf_set_metadata(uint32_t session_id, uint32_t audio_intf, struct agm_meta_data *metadata){
            android::Parcel data, reply;
            android::Parcel::WritableBlob gkv_blob, ckv_blob;
            uint32_t kv_size = sizeof(agm_key_value);
            uint32_t gkv_size = kv_size * (metadata->gkv.num_kvs);
            uint32_t ckv_size = kv_size * (metadata->ckv.num_kvs);
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeUint32(session_id);
            data.writeUint32(audio_intf);
	    ALOGE("Bn : amd.gkv.num_kvs = %d",metadata->gkv.num_kvs);
            ALOGE("Bn : amd.ckv.num_kvs = %d",metadata->ckv.num_kvs);
	    ALOGE("Bn : gkv_size = %d",gkv_size);
	    ALOGE("Bn : gkv_size = %d",ckv_size);
            data.writeUint32(metadata->gkv.num_kvs);
            data.writeUint32(metadata->ckv.num_kvs);
            data.writeBlob(gkv_size, false, &gkv_blob);
            memset(gkv_blob.data(), 0x0, gkv_size);
            memcpy(gkv_blob.data(), metadata->gkv.kv, gkv_size);
            data.writeBlob(ckv_size, false, &ckv_blob);
            memset(ckv_blob.data(), 0x0, ckv_size);
            memcpy(ckv_blob.data(), metadata->ckv.kv, ckv_size);
            remote()->transact(SESSION_AUDIO_INTF_SET_META, data, &reply);
            gkv_blob.release();
            ckv_blob.release();
            return reply.readInt32();
        }

        virtual int ipc_agm_session_open(uint32_t session_id, struct session_obj **handle){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeUint32(session_id);
            remote()->transact(OPEN, data, &reply);
            *handle = (struct session_obj *)reply.readInt64();
            return reply.readInt32();
        }

        virtual int ipc_agm_session_close(struct session_obj *handle){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)handle);
            remote()->transact(CLOSE, data, &reply);
            return reply.readInt32();
        }

        virtual int ipc_agm_session_prepare(struct session_obj *handle){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)handle);
            remote()->transact(PREPARE, data, &reply);
            return reply.readInt32();
        }

        virtual int ipc_agm_session_start(struct session_obj *handle){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)handle);
            remote()->transact(START, data, &reply);
            return reply.readInt32();
        }

        virtual int ipc_agm_session_stop(struct session_obj *handle){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)handle);
            remote()->transact(STOP, data, &reply);
            return reply.readInt32();
        }

        virtual int ipc_agm_session_pause(struct session_obj *handle){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)handle);
            remote()->transact(PAUSE, data, &reply);
            return reply.readInt32();
        }

        virtual int ipc_agm_session_resume(struct session_obj *handle){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)handle);
            remote()->transact(RESUME, data, &reply);
            return reply.readInt32();
        }

        virtual int ipc_agm_session_audio_inf_connect(uint32_t session_id, uint32_t audio_intf, bool state){
            android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeUint32(session_id);
            data.writeUint32(audio_intf);
            data.write(&state, sizeof(bool));
            remote()->transact(CONNECT, data, &reply);
            return reply.readInt32();
        }
		
	virtual size_t ipc_agm_get_hw_processed_buff_cnt(struct session_obj *handle, enum direction dir) {
	    android::Parcel data, reply;
            ALOGV("%s %d", __func__, __LINE__);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)handle);
	    data.writeUint32(dir);
            remote()->transact(GET_BUFF_CNT, data, &reply);
            return reply.readInt32();
	}

        virtual int ipc_agm_session_read(struct session_obj *session_handle, void *buff, size_t count) {
            int rc = 0;
            android::Parcel data, reply;
            android::Parcel::ReadableBlob blob; 
            ALOGE("\n BP-INTERFACE handle:%p buf:%s count:%d\n", session_handle, buff, count);
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            data.writeInt64((long)session_handle);
            data.writeUint32(count);
            remote()->transact(READ, data, &reply);
            rc = reply.readInt32();
            if (rc != 0) {
                ALOGE("read failed error out %d", rc);
                goto fail_read;
            }
            reply.readBlob(count, &blob);
            memset(buff, 0x0, count);
            memcpy(buff, blob.data(), count);
fail_read:
            return rc;
        }

        virtual int ipc_agm_session_write(struct session_obj *session_handle, void *buff, size_t count) {
            android::Parcel data, reply;
            android::Parcel::WritableBlob blob; 
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            ALOGE("\n BP-INTERFACER handle:%p buf:%s count:%d\n", session_handle, buff, count);
            data.writeInt64((long)session_handle);
            data.writeUint32(count);
            data.writeBlob(count, false, &blob);
            memset(blob.data(), 0x0, count);
            memcpy(blob.data(), buff, count);
            remote()->transact(WRITE, data, &reply);
            blob.release();
            return reply.readInt32();
        }
        
    virtual int ipc_agm_get_aif_info_list(struct aif_info *aif_list, size_t *num_aif_info) {
            android::Parcel data, reply;
            int count = *num_aif_info, cp_val;
            data.writeInterfaceToken(IAgmService::getInterfaceDescriptor());
            ALOGE("\nBUFFER in WRITE ON BP-INTERFACER handle:%p count:%d\n",aif_list, *num_aif_info);
            if (aif_list == NULL && count == 0) {
                data.writeUint32(count);
                data.writeInt64((long)aif_list);
                remote()->transact(GET_AIF_LIST, data, &reply);
                *num_aif_info = reply.readInt32();
                ALOGE ("\n PROXY BP FIRST CASE AFTER COUNT UPDATE :%d\n", *num_aif_info);
                return  reply.readInt32();
            } else if (aif_list != NULL && count != 0) {
                uint32_t total_bytecnt = count * sizeof(struct aif_info);
                data.writeUint32(count);
                data.writeInt64((long)aif_list);
                remote()->transact(GET_AIF_LIST, data, &reply);
                android::Parcel::ReadableBlob aif_list_blob;
                reply.readBlob(total_bytecnt, &aif_list_blob);
                memset(aif_list, 0x0, total_bytecnt);
                memcpy(aif_list, aif_list_blob.data(), total_bytecnt);
                aif_list_blob.release();
                return reply.readInt32();
            }
        }
};


IMPLEMENT_META_INTERFACE(AgmService, "AgmService");


android::status_t BnAgmService::onTransact(uint32_t code, const android::Parcel& data, android::Parcel* reply, uint32_t flags)
{
    ALOGD("BnAgmService::onTransact(%i) %i", code, flags);
    int rc = -EINVAL;
    data.checkInterface(this);

    switch (code) {
    case REG_CLIENT : {
        sp<IBinder> binder = data.readStrongBinder();
        agm_register_client(binder);
        break; }

    case UNREG_CLIENT : {
        sp<IBinder> binder = data.readStrongBinder();
        agm_unregister_client(binder);
        break; }

   case OPEN : {
        uint32_t session_id;
        struct session_obj *handle = NULL;
        session_id = data.readUint32();
        rc = ipc_agm_session_open(session_id, &handle);
		if(handle != NULL)
			agm_add_session_obj_handle(handle);reply->writeInt64((long)handle);
        reply->writeInt32(rc);
        break; }

    case SESSION_SET_META : {
        uint32_t session_id = 0;
        struct agm_meta_data amd = {0};
        uint32_t gkv_blob_size, ckv_blob_size = 0;
	android::Parcel::ReadableBlob ckv_blob, gkv_blob;
        session_id = data.readUint32();
        amd.gkv.num_kvs = data.readUint32();
        amd.ckv.num_kvs = data.readUint32();
        gkv_blob_size = amd.gkv.num_kvs * sizeof(struct agm_key_value);
        ckv_blob_size = amd.ckv.num_kvs * sizeof(struct agm_key_value);
	ALOGE("Bn:gkv.num_kvs = %d size %d", amd.gkv.num_kvs, gkv_blob_size);
	ALOGE("Bn:ckv.num_kvs = %d size %d", amd.ckv.num_kvs, ckv_blob_size);
        amd.ckv.kv = (struct agm_key_value *)
                      calloc(amd.ckv.num_kvs, sizeof(agm_key_value));
        amd.gkv.kv = (struct agm_key_value *)
                      calloc(amd.gkv.num_kvs, sizeof(agm_key_value));
        if (!amd.ckv.kv || !amd.gkv.kv) {
            ALOGE("%s:%d calloc failed", __func__, __LINE__);
            rc = -ENOMEM;
            goto fail_ses_set_meta;
        }
        data.readBlob(gkv_blob_size, &gkv_blob);
        memcpy(amd.gkv.kv, gkv_blob.data(), gkv_blob_size);
        gkv_blob.release();
        data.readBlob(ckv_blob_size, &ckv_blob);
        memcpy(amd.ckv.kv, ckv_blob.data(), ckv_blob_size);
        ckv_blob.release();
        rc = ipc_agm_session_set_metadata(session_id, &amd);
fail_ses_set_meta:
        if (amd.ckv.kv)
            free(amd.ckv.kv);
        if (amd.gkv.kv)
            free(amd.gkv.kv);
        reply->writeInt32(rc);
        break;
    }
    case SESSION_AUDIO_INTF_SET_META : {
        uint32_t session_id, audio_intf = 0;
        struct agm_meta_data amd = {0};
        uint32_t gkv_blob_size, ckv_blob_size = 0;
	android::Parcel::ReadableBlob ckv_blob, gkv_blob;
        session_id = data.readUint32();
        audio_intf = data.readUint32();
        amd.gkv.num_kvs = data.readUint32();
        amd.ckv.num_kvs = data.readUint32();
        gkv_blob_size = amd.gkv.num_kvs * sizeof(struct agm_key_value);
        ckv_blob_size = amd.ckv.num_kvs * sizeof(struct agm_key_value);
	ALOGE("Bn:gkv.num_kvs = %d size %d", amd.gkv.num_kvs, gkv_blob_size);
	ALOGE("Bn:ckv.num_kvs = %d size %d", amd.ckv.num_kvs, ckv_blob_size);
        amd.ckv.kv = (struct agm_key_value *)
                      calloc(amd.ckv.num_kvs, sizeof(agm_key_value));
        amd.gkv.kv = (struct agm_key_value *)
                      calloc(amd.gkv.num_kvs, sizeof(agm_key_value));
        if (!amd.ckv.kv || !amd.gkv.kv) {
            ALOGE("%s:%d calloc failed", __func__, __LINE__);
            rc = -ENOMEM;
            goto fail_ses_aud_set_meta;
        }
        data.readBlob(gkv_blob_size, &gkv_blob);
        memcpy(amd.gkv.kv, gkv_blob.data(), gkv_blob_size);
        gkv_blob.release();
        data.readBlob(ckv_blob_size, &ckv_blob);
        memcpy(amd.ckv.kv, ckv_blob.data(), ckv_blob_size);
        ckv_blob.release();
        rc = ipc_agm_session_audio_inf_set_metadata(session_id, audio_intf, &amd);
fail_ses_aud_set_meta:
        if (amd.ckv.kv)
            free(amd.ckv.kv);
        if (amd.gkv.kv)
            free(amd.gkv.kv);
        reply->writeInt32(rc);
        break;
    }
    case INIT : {
        rc = ipc_agm_init();
        reply->writeInt32(rc);
        break;
        }
		
	case GET_BUFF_CNT :{
        enum direction dir;
		struct session_obj *handle = (struct session_obj *)data.readInt64();
		dir = (direction) data.readUint32();
        rc = ipc_agm_get_hw_processed_buff_cnt(handle, dir);
        reply->writeInt32(rc);
        break; }
		
    case AUDIO_INTF_SET_META : {
        uint32_t audio_intf;
        struct agm_meta_data amd = {0};
        uint32_t gkv_blob_size, ckv_blob_size = 0;
	android::Parcel::ReadableBlob ckv_blob, gkv_blob;
        audio_intf = data.readUint32();
        amd.gkv.num_kvs = data.readUint32();
        amd.ckv.num_kvs = data.readUint32();
        gkv_blob_size = amd.gkv.num_kvs * sizeof(struct agm_key_value);
        ckv_blob_size = amd.ckv.num_kvs * sizeof(struct agm_key_value);
	ALOGE("Bn:gkv.num_kvs = %d size %d", amd.gkv.num_kvs, gkv_blob_size);
	ALOGE("Bn:ckv.num_kvs = %d size %d", amd.ckv.num_kvs, ckv_blob_size);
        amd.ckv.kv = (struct agm_key_value *)
                      calloc(amd.ckv.num_kvs, sizeof(agm_key_value));
        amd.gkv.kv = (struct agm_key_value *)
                      calloc(amd.gkv.num_kvs, sizeof(agm_key_value));
        if (!amd.ckv.kv || !amd.gkv.kv) {
            ALOGE("%s:%d calloc failed", __func__, __LINE__);
            rc = -ENOMEM;
            goto fail_audio_set_meta;
        }
        data.readBlob(gkv_blob_size, &gkv_blob);
        memcpy(amd.gkv.kv, gkv_blob.data(), gkv_blob_size);
        gkv_blob.release();
        data.readBlob(ckv_blob_size, &ckv_blob);
        memcpy(amd.ckv.kv, ckv_blob.data(), ckv_blob_size);
        ckv_blob.release();
        ALOGE("call ipc_agm_audio_intf_set_metadata");
	rc = ipc_agm_audio_intf_set_metadata(audio_intf, &amd);
fail_audio_set_meta: 
        if (amd.ckv.kv)
            free(amd.ckv.kv);
        if (amd.gkv.kv)
            free(amd.gkv.kv);
        reply->writeInt32(rc);
        break;
    }
	
	case LOOPBACK : {
	    uint32_t capture_session_id;
        uint32_t playback_session_id;
        capture_session_id = data.readUint32();
        playback_session_id = data.readUint32();
        int state = data.readUint32();
        rc = ipc_agm_session_set_loopback(capture_session_id, playback_session_id, state);
        reply->writeInt32(rc);
        break; }
		
    case CLOSE : {
        struct session_obj *handle = (struct session_obj *)data.readInt64();
        rc = ipc_agm_session_close(handle);
		agm_remove_session_obj_handle(handle);
        reply->writeInt32(rc);
        break; }

    case PREPARE : {
        struct session_obj *handle = (struct session_obj *)data.readInt64();
        rc = ipc_agm_session_prepare(handle);
        reply->writeInt32(rc);
        break; }

    case START : {
        struct session_obj *handle = (struct session_obj *)data.readInt64();
        rc = ipc_agm_session_start(handle);
        reply->writeInt32(rc);
        break; }

    case STOP : {
        struct session_obj *handle = (struct session_obj *)data.readInt64();
        rc = ipc_agm_session_stop(handle);
        reply->writeInt32(rc);
        break; }

    case PAUSE : {
        struct session_obj *handle = (struct session_obj *)data.readInt64();
        rc = ipc_agm_session_pause(handle);
        reply->writeInt32(rc);
        break; }

    case RESUME : {
        struct session_obj *handle = (struct session_obj *)data.readInt64();
        rc = ipc_agm_session_resume(handle);
        reply->writeInt32(rc);
        break; }

    case CONNECT : {
        uint32_t session_id;
        uint32_t audio_intf;
        session_id = data.readUint32();
        audio_intf = data.readUint32();
        int state = data.readUint32();
        rc = ipc_agm_session_audio_inf_connect(session_id, audio_intf, state);
        reply->writeInt32(rc);
        break; }

    case SET_MEDIA_CONFIG : {
        uint32_t audio_intf;
        audio_intf = data.readUint32();
        struct agm_media_config media_config;
        android::Parcel::ReadableBlob blob;
        uint32_t blob_size;
        data.readUint32(&blob_size);
        data.readBlob(blob_size, &blob);
        memset(&media_config, 0x0, sizeof(agm_media_config));
        memscpy(&media_config, sizeof(agm_media_config), blob.data(), blob_size);
        blob.release();
        rc = ipc_agm_audio_intf_set_media_config(audio_intf, &media_config);
        reply->writeInt32(rc);
        break; }

    case SESSION_SET_CONFIG : {
        struct session_obj *handle = (struct session_obj *)data.readInt64();
        struct agm_session_config session_config;
        struct agm_media_config media_config;
        struct agm_buffer_config buffer_config;
        android::Parcel::ReadableBlob blob;
        uint32_t blob1_size;
        data.readUint32(&blob1_size);
        data.readBlob(blob1_size, &blob);
        memset(&session_config, 0x0, sizeof(agm_session_config));
        memscpy(&session_config, sizeof(agm_session_config), blob.data(), blob1_size);
        blob.release();
        uint32_t blob2_size;
        data.readUint32(&blob2_size);
        data.readBlob(blob2_size, &blob);
        memset(&media_config, 0x0, sizeof(agm_media_config));
        memscpy(&media_config, sizeof(agm_media_config), blob.data(), blob2_size);
        blob.release();
        uint32_t blob3_size;
        data.readUint32(&blob3_size);
        data.readBlob(blob3_size, &blob);
        memset(&buffer_config, 0x0, sizeof(agm_buffer_config));
        memscpy(&buffer_config, sizeof(agm_buffer_config), blob.data(), blob3_size);
        blob.release();
        rc = ipc_agm_session_set_config(handle, &session_config, &media_config, &buffer_config);
        reply->writeInt32(rc);
        break; }

    case READ : {
        int32_t rc;
        size_t byte_count;
        struct session_obj *handle;
        void *buf = NULL;
        android::Parcel::WritableBlob blob;
        handle = (struct session_obj *)data.readInt64();
        byte_count = data.readUint32();
        buf = (void *)calloc(1, byte_count);
        if (buf == NULL) {
            ALOGE("%s:%d calloc failed", __func__, __LINE__);
            rc = -ENOMEM;
            reply->writeInt32(rc);
            goto free_buff; 
        }
        rc = ipc_agm_session_read(handle, buf, byte_count);
        reply->writeInt32(rc);
        if (rc != 0) {
            ALOGE("session_read failed %d", rc);
            goto free_buff;
        }
        reply->writeBlob(byte_count, false, &blob);
        memset(blob.data(), 0x0, byte_count);
        memcpy(blob.data(), buf, byte_count);
free_buff:
        if (buf)
           free(buf);
       break; }

    case WRITE: {
        uint32_t rc;
        size_t byte_count;
        void *buf;
        struct session_obj *handle;
        android::Parcel::ReadableBlob blob;
        handle = (struct session_obj *)data.readInt64();
        byte_count = data.readUint32();
        buf = calloc(1,byte_count);
        if (buf == NULL) {
            ALOGE("%s:%d calloc failed", __func__, __LINE__);
            rc = -ENOMEM;
            goto fail_write; 
        }
        data.readBlob(byte_count, &blob);
        memcpy(buf, blob.data(), byte_count);
        rc = ipc_agm_session_write(handle, buf, byte_count);
        blob.release();
fail_write:
        if (buf)
           free(buf);
        reply->writeInt32(rc);
       break; }

    case GET_AIF_LIST: {
        uint32_t rc, cp_val;
        size_t count = 0;
        struct aif_info *aif_list, *aif_list_bn;
        count = (size_t) data.readUint32();
        if (count == 0) {
            aif_list = (struct aif_info*)data.readInt64();
            rc = ipc_agm_get_aif_info_list(aif_list, &count);
            ALOGE ("\n PROXY BN FIRST CASE AFTER COUNT UPDATE :%d\n", count);
            reply->writeInt32(count);
            reply->writeInt32(rc);
        } else if (count != 0){
            aif_list_bn = (struct aif_info*)calloc(count, sizeof(struct aif_info));
            if (aif_list_bn == NULL) {
                ALOGE("\n No memory allocated\n");
                return -ENOMEM;
            }
            rc = ipc_agm_get_aif_info_list(aif_list_bn, &count);
            int total_bytecnt =  count*sizeof(struct aif_info);
            if(!rc) {
            for (cp_val= 0; cp_val < count; cp_val++)
            ALOGE("\n BN-INTERFACE name:%s count:%d\n",aif_list_bn[cp_val].aif_name, total_bytecnt);
            }
            android::Parcel::WritableBlob aif_list_blob;
            reply->writeBlob(total_bytecnt, false, &aif_list_blob);
            memset(aif_list_blob.data(), 0x0, total_bytecnt);
            memcpy(aif_list_blob.data(), aif_list_bn, total_bytecnt);
            aif_list_blob.release();
            reply->writeInt32(rc);
            free(aif_list_bn);
        ALOGE("BUFFER in WRITE ON TRANSACT return:%d",rc);
        }
        break; }
    default:
        return BBinder::onTransact(code, data, reply, flags);
    }
return 0;
}