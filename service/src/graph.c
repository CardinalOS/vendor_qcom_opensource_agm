/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOGTAG "AGM: graph"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include "graph.h"
#include "graph_module.h"
#include "utils.h"

#define DEVICE_RX 0
#define DEVICE_TX 1
#define MONO 1

#define CONVX(x) #x
#define CONV_TO_STRING(x) CONVX(x)

#define ADD_MODULE(x, y) \
                 ({ \
                   module_info_t *add_mod = NULL;\
                   add_mod = calloc(1, sizeof(module_info_t));\
                   *add_mod = x;\
                   if(y != NULL) add_mod->dev_obj = y; \
                   list_add_tail(&graph_obj->tagged_mod_list, &add_mod->list);\
                   add_mod;\
                 })


static int get_acdb_files_from_directory(const char* acdb_files_path,
                                         struct gsl_acdb_data_files *data_files)
{
    int result = 0;
    int ret = 0;
    int i = 0;
    DIR *dir_fp = NULL;
    struct dirent *dentry;

    dir_fp = opendir(acdb_files_path);
    if (dir_fp == NULL) {
        AGM_LOGE("cannot open acdb path %s\n", acdb_files_path);
        ret = EINVAL;
        goto done;
    }

    /* search directory for .acdb files */
    while ((dentry = readdir(dir_fp)) != NULL) {
        if ((strstr(dentry->d_name, ".acdb") != NULL)) {
           if (data_files->num_files >= GSL_MAX_NUM_OF_ACDB_FILES) {
               AGM_LOGE("Reached max num of files, %d!\n", i);
               break;
           }
           result = snprintf(
                    data_files->acdbFiles[i].fileName,
                    sizeof(data_files->acdbFiles[i].fileName),
                    "%s/%s", acdb_files_path, dentry->d_name);
           if ((result < 0) ||
               (result >= (int)sizeof(data_files->acdbFiles[i].fileName))) {
               AGM_LOGE("snprintf failed: %s/%s, err %d\n",
                              acdb_files_path,
                              data_files->acdbFiles[i].fileName,
                              result);
               ret = -EINVAL;
               break;
           }
           data_files->acdbFiles[i].fileNameLen =
                                    strlen(data_files->acdbFiles[i].fileName);
           AGM_LOGI("Load file: %s\n", data_files->acdbFiles[i].fileName);
           i++;
	}
    }

    if (i == 0)
        AGM_LOGE("No .acdb files found in %s!\n", acdb_files_path);

    data_files->num_files = i;

    closedir(dir_fp);
done:
    return ret;
}

static void get_default_channel_map(uint8_t *channel_map, int channels)
{
    if (channels == 1)  {
        channel_map[0] = PCM_CHANNEL_C;
    } else if (channels == 2) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
    } else if (channels == 3) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
    } else if (channels == 4) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_LB;
    	channel_map[3] = PCM_CHANNEL_RB;
    } else if (channels == 5) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
        channel_map[3] = PCM_CHANNEL_LB;
        channel_map[4] = PCM_CHANNEL_RB;
    } else if (channels == 6) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
        channel_map[3] = PCM_CHANNEL_LFE;
        channel_map[4] = PCM_CHANNEL_LB;
        channel_map[5] = PCM_CHANNEL_RB;
    } else if (channels == 7) {
        /*
         * Configured for 5.1 channel mapping + 1 channel for debug
         * Can be customized based on DSP.
         */
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
        channel_map[3] = PCM_CHANNEL_LFE;
        channel_map[4] = PCM_CHANNEL_LB;
        channel_map[5] = PCM_CHANNEL_RB;
        channel_map[6] = PCM_CHANNEL_CS;
    } else if (channels == 8) {
        channel_map[0] = PCM_CHANNEL_L;
        channel_map[1] = PCM_CHANNEL_R;
        channel_map[2] = PCM_CHANNEL_C;
        channel_map[3] = PCM_CHANNEL_LFE;
        channel_map[4] = PCM_CHANNEL_LB;
        channel_map[5] = PCM_CHANNEL_RB;
        channel_map[6] = PCM_CHANNEL_LS;
        channel_map[7] = PCM_CHANNEL_RS;
    }
}

static int get_bit_width(enum agm_pcm_format format)
{
    int bit_width = 16;

    switch (format) {
    case AGM_PCM_FORMAT_S24_3LE:
    case AGM_PCM_FORMAT_S24_LE:
         bit_width = 24;
         break;
    case AGM_PCM_FORMAT_S32_LE:
         bit_width = 32;
         break;
    case AGM_PCM_FORMAT_S16_LE:
    default:
         break;
    }

    return bit_width;
}

#define GET_BITS_PER_SAMPLE(format, bit_width) \
                           (format == AGM_PCM_FORMAT_S24_LE? 32 : bit_width)

#define GET_Q_FACTOR(format, bit_width)\
                     (format == AGM_PCM_FORMAT_S24_LE ? 27 : (bit_width - 1))

int configure_codec_dma_ep(struct module_info *mod,
                           struct graph_obj *graph_obj)
{
    int ret = 0;
    struct device_obj *dev_obj = mod->dev_obj;
    hw_ep_info_t hw_ep_info = dev_obj->hw_ep_info;
    /**
     *Once we have ACDB updated with Codec DMA tagged data will swith to this
     *Till then we hardcode the config and use set custom config.
     */
#if 0
    struct gsl_key_vector tag_key_vect;

    /*
     * For Codec dma we need to configure the following tags
     * Media Config :
     * 1.Channels  - Channels are reused to derive the active channel mask
     * 2.Sample Rate
     * 3.Bit Width
     * Codec DMA INTF Cfg:
     * 4.LPAIF type
     * 5.Interface ID.
     */
    tag_key_vect.num_kvps = 5;
    tag_key_vect.kvp = calloc(tag_key_vect.num_kvps,
                                sizeof(struct gsl_key_value_pair));

    tag_key_vect.kvp[0].key = SAMPLINGRATE;
    tag_key_vect.kvp[0].value = media_config.rate;

    tag_key_vect.kvp[1].key = CHANNELS;
    tag_key_vect.kvp[1].value = media_config.channels;

    tag_key_vect.kvp[2].key = BITWIDTH;
    tag_key_vect.kvp[2].value = get_bit_width(media_config.format);

    tag_key_vect.kvp[3].key = LPAIF_TYPE;
    tag_key_vect.kvp[3].value = hw_ep_info.lpaif_type;

    tag_key_vect.kvp[4].key = CODEC_DMA_INTF_ID;
    tag_key_vect.kvp[4].value = hw_ep_info.intf_idx;

    ret = gsl_set_config(graph_obj->graph_handle, mod->tag, &tag_key_vect);

    if (ret != 0) {
        AGM_LOGE("set_config command for module %d failed with error %d",
                      mod->tag, ret);
    }
#else
    struct apm_module_param_data_t *header;
    struct param_id_codec_dma_intf_cfg_t* codec_config;
    size_t payload_size = 0;
    uint8_t *payload = NULL;
    AGM_LOGD("entry mod tag %x miid %x mid %x",mod->tag, mod->miid, mod->mid);
    payload_size = sizeof(struct apm_module_param_data_t) +
        sizeof(struct param_id_codec_dma_intf_cfg_t);

    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = (uint8_t*)malloc((size_t)payload_size);

    header = (struct apm_module_param_data_t*)payload;
    codec_config = (struct param_id_codec_dma_intf_cfg_t*)
                     (payload + sizeof(struct apm_module_param_data_t));

    header->module_instance_id = mod->miid;
    header->param_id = PARAM_ID_CODEC_DMA_INTF_CFG;
    header->error_code = 0x0;
    header->param_size = payload_size;
    codec_config->lpaif_type = hw_ep_info.lpaif_type; 
    codec_config->intf_indx = hw_ep_info.intf_idx;
    codec_config->active_channels_mask = 3;
    
    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("custom_config command for module %d failed with error %d",
                      mod->tag, ret);
    }
#endif
    AGM_LOGD("exit");
    return ret; 
}


int configure_hw_ep_media_config(struct module_info *mod,
                                struct graph_obj *graph_obj)
{
    int ret = 0;
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    struct device_obj *dev_obj = mod->dev_obj;
    struct apm_module_param_data_t* header;
    struct param_id_hw_ep_mf_t* hw_ep_media_conf;
    struct agm_media_config media_config = dev_obj->media_config;
    
    AGM_LOGD("entry mod tag %x miid %x mid %x",mod->tag, mod->miid, mod->mid);
    payload_size = sizeof(struct apm_module_param_data_t) +
                   sizeof(struct param_id_hw_ep_mf_t);

    /*ensure that the payloadszie is byte multiple atleast*/
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = malloc((size_t)payload_size);

    header = (struct apm_module_param_data_t*)payload;
    hw_ep_media_conf = (struct param_id_hw_ep_mf_t*)
                         (payload + sizeof(struct apm_module_param_data_t));
    
    header->module_instance_id = mod->miid;
    header->param_id = PARAM_ID_HW_EP_MF_CFG;
    header->error_code = 0x0;
    header->param_size = payload_size;

    hw_ep_media_conf->sample_rate = media_config.rate;
    hw_ep_media_conf->bit_width = get_bit_width(media_config.format);

    hw_ep_media_conf->num_channels = media_config.channels;
    hw_ep_media_conf->data_format = DATA_FORMAT_FIXED_POINT;

    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("custom_config command for module %d failed with error %d",
                      mod->tag, ret);
    }
   AGM_LOGD("exit");
   return ret;
}

int configure_hw_ep(struct module_info *mod,
                    struct graph_obj *graph_obj)
{
    int ret = 0;
    struct device_obj *dev_obj = mod->dev_obj;
    ret = configure_hw_ep_media_config(mod, graph_obj); 
    if (ret) {
        AGM_LOGE("hw_ep_media_config failed %d", ret);
        return ret;
    }
    switch (dev_obj->hw_ep_info.intf) {
    case CODEC_DMA:
         ret = configure_codec_dma_ep(mod, graph_obj);
         break;
    case MI2S:
    default:
         break;
    }
    return ret;
}

/**
 *PCM DECODER/ENCODER and PCM CONVERTER are configured with the 
 *same PCM_FORMAT_CFG hence reuse the implementation
*/
int configure_pcm_enc_dec_conv(struct module_info *mod,
                               struct graph_obj *graph_obj)
{
    struct session_obj *sess_obj = graph_obj->sess_obj;
    struct media_format_t *media_fmt_hdr;
    struct payload_pcm_output_format_cfg_t *pcm_output_fmt_payload;
    struct apm_module_param_data_t *header;
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    uint8_t *channel_map;
    int ret = 0;
    int num_channels = MONO;

    AGM_LOGD("entry mod tag %x miid %x mid %x",mod->tag, mod->miid, mod->mid);
    num_channels = sess_obj->media_config.channels;

    payload_size = sizeof(struct apm_module_param_data_t) +
                   sizeof(struct media_format_t) +
                   sizeof(struct payload_pcm_output_format_cfg_t) +
                   sizeof(uint8_t)*num_channels;

    /*ensure that the payloadszie is byte multiple atleast*/
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = malloc((size_t)payload_size);

    header = (struct apm_module_param_data_t*)payload;

    media_fmt_hdr = (struct media_format_t*)(payload +
                         sizeof(struct apm_module_param_data_t));
    pcm_output_fmt_payload = (struct payload_pcm_output_format_cfg_t*)(payload +
                             sizeof(struct apm_module_param_data_t) +
                             sizeof(struct media_format_t));

    channel_map = (uint8_t*)(payload + sizeof(struct apm_module_param_data_t) +
                                sizeof(struct media_format_t) +
                                sizeof(struct payload_pcm_output_format_cfg_t));

    header->module_instance_id = mod->miid;
    header->param_id = PARAM_ID_PCM_OUTPUT_FORMAT_CFG;
    header->error_code = 0x0;
    header->param_size = payload_size;

    media_fmt_hdr->data_format = DATA_FORMAT_FIXED_POINT;
    media_fmt_hdr->fmt_id = MEDIA_FMT_ID_PCM;
    media_fmt_hdr->payload_size = sizeof(payload_pcm_output_format_cfg_t) +
                                            sizeof(uint8_t) * num_channels;

    pcm_output_fmt_payload->endianness = PCM_LITTLE_ENDIAN;
    pcm_output_fmt_payload->bit_width = get_bit_width(sess_obj->media_config.format);
    /**
     *alignment field is referred to only in case where bit width is 
     *24 and bits per sample is 32, tiny alsa only supports 24 bit
     *in 32 word size in LSB aligned mode(AGM_PCM_FORMAT_S24_LE).
     *Hence we hardcode this to PCM_LSB_ALIGNED;
     */
    pcm_output_fmt_payload->alignment = PCM_LSB_ALIGNED;
    pcm_output_fmt_payload->num_channels = num_channels;
    pcm_output_fmt_payload->bits_per_sample = 
                             GET_BITS_PER_SAMPLE(sess_obj->media_config.format,
                                                 pcm_output_fmt_payload->bit_width);

    pcm_output_fmt_payload->q_factor = GET_Q_FACTOR(sess_obj->media_config.format,
                                                pcm_output_fmt_payload->bit_width);

    if (sess_obj->stream_config.dir == RX)
        pcm_output_fmt_payload->interleaved = PCM_DEINTERLEAVED_UNPACKED;
    else
        pcm_output_fmt_payload->interleaved = PCM_INTERLEAVED;

    /**
     *#TODO:As of now channel_map is not part of media_config
     *ADD channel map part as part of the session/device media config 
     *structure and use that channel map if set by client otherwise
     * use the default channel map
     */
    get_default_channel_map(channel_map, num_channels);
 
    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("custom_config command for module %d failed with error %d",
                      mod->tag, ret);
    }

    AGM_LOGD("exit");
    return ret;
}

int configure_shared_mem_ep(struct module_info *mod,
                            struct graph_obj *graph_obj)
{
    int ret = 0;
    struct session_obj *sess_obj = graph_obj->sess_obj;
    struct media_format_t *media_fmt_hdr;
    struct apm_module_param_data_t *header;
    struct payload_media_fmt_pcm_t *media_fmt_payload;
    uint8_t *payload = NULL;
    size_t payload_size = 0;
    int num_channels = MONO;
    uint8_t *channel_map;

    AGM_LOGD("entry mod tag %x miid %x mid %x",mod->tag, mod->miid, mod->mid);
    num_channels = sess_obj->media_config.channels;

    payload_size = sizeof(struct apm_module_param_data_t) +
                   sizeof(struct media_format_t) +
                   sizeof(struct payload_media_fmt_pcm_t) +
                   sizeof(uint8_t)*num_channels;

    /*ensure that the payloadszie is byte multiple atleast*/
    if (payload_size % 8 != 0)
        payload_size = payload_size + (8 - payload_size % 8);

    payload = malloc((size_t)payload_size);

    header = (struct apm_module_param_data_t*)payload;

    media_fmt_hdr = (struct media_format_t*)(payload +
                         sizeof(struct apm_module_param_data_t));
    media_fmt_payload = (struct payload_media_fmt_pcm_t*)(payload +
                             sizeof(struct apm_module_param_data_t) +
                             sizeof(struct media_format_t));

    channel_map = (uint8_t*)(payload + sizeof(struct apm_module_param_data_t) +
                                sizeof(struct media_format_t) +
                                sizeof(struct payload_media_fmt_pcm_t));

    header->module_instance_id = mod->miid;
    header->param_id = PARAM_ID_MEDIA_FORMAT;
    header->error_code = 0x0;
    header->param_size = payload_size;

    media_fmt_hdr->data_format = DATA_FORMAT_FIXED_POINT;
    media_fmt_hdr->fmt_id = MEDIA_FMT_ID_PCM;
    media_fmt_hdr->payload_size = sizeof(payload_media_fmt_pcm_t) +
                                            sizeof(uint8_t) * num_channels;

    media_fmt_payload->endianness = PCM_LITTLE_ENDIAN;
    media_fmt_payload->bit_width = get_bit_width(sess_obj->media_config.format);
    media_fmt_payload->sample_rate = sess_obj->media_config.rate;
    /**
     *alignment field is referred to only in case where bit width is 
     *24 and bits per sample is 32, tiny alsa only supports 24 bit
     *in 32 word size in LSB aligned mode(AGM_PCM_FORMAT_S24_LE).
     *Hence we hardcode this to PCM_LSB_ALIGNED;
     */
    media_fmt_payload->alignment = PCM_LSB_ALIGNED;
    media_fmt_payload->num_channels = num_channels;
    media_fmt_payload->bits_per_sample = 
                             GET_BITS_PER_SAMPLE(sess_obj->media_config.format,
                                                 media_fmt_payload->bit_width);

    media_fmt_payload->q_factor = GET_Q_FACTOR(sess_obj->media_config.format,
                                                media_fmt_payload->bit_width);
    /**
     *#TODO:As of now channel_map is not part of media_config
     *ADD channel map part as part of the session/device media config 
     *structure and use that channel map if set by client otherwise
     * use the default channel map
     */
    get_default_channel_map(channel_map, num_channels);
 
    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size);
    if (ret != 0) {
        AGM_LOGE("custom_config command for module %d failed with error %d",
                      mod->tag, ret);
    }
    AGM_LOGD("exit");
    return ret;
}

static module_info_t hw_ep_module[] = {
   {
       .module = MODULE_HW_EP_RX,
       .tag = DEVICE_HW_ENDPOINT_RX,
       .configure = configure_hw_ep,
   },
   {
       .module = MODULE_HW_EP_TX,
       .tag = DEVICE_HW_ENDPOINT_TX,
       .configure = configure_hw_ep,
   }
};

static module_info_t stream_module_list[] = {
 {
     .module = MODULE_PCM_ENCODER,
     .tag = STREAM_PCM_ENCODER,
     .configure = configure_pcm_enc_dec_conv,
 }, 
 {
     .module = MODULE_PCM_DECODER,
     .tag = STREAM_PCM_DECODER,
     .configure = configure_pcm_enc_dec_conv,
 },
 {
     .module = MODULE_PCM_CONVERTER,
     .tag = STREAM_PCM_CONVERTER,
     .configure = configure_pcm_enc_dec_conv,
 },
 {
     .module = MODULE_SHARED_MEM,
     .tag = STREAM_INPUT_MEDIA_FORMAT,
     .configure = configure_shared_mem_ep,
 },
 };

int configure_buffer_params(struct graph_obj *gph_obj,
                            struct session_obj *sess_obj)
{
    struct gsl_cmd_configure_read_write_params buf_config;
    int ret = 0;
    size_t size = 0;
    enum gsl_cmd_id cmd_id;

    AGM_LOGD("%s sess buf_sz %d num_bufs %d", sess_obj->stream_config.dir == RX?
                 "Playback":"Capture", sess_obj->buffer_config.size,
                  sess_obj->buffer_config.count);

    buf_config.buff_size = sess_obj->buffer_config.size;
    buf_config.num_buffs = sess_obj->buffer_config.count;
    buf_config.start_threshold = sess_obj->stream_config.start_threshold;
    buf_config.stop_threshold = sess_obj->stream_config.stop_threshold;
    /**
     *TODO:expose a flag to chose between different data passing modes
     *BLOCKING/NON-BLOCKING/SHARED_MEM.
     */
    buf_config.attributes = GSL_DATA_MODE_BLOCKING;
    size = sizeof(struct gsl_cmd_configure_read_write_params);

    if (sess_obj->stream_config.dir == RX)
       cmd_id = GSL_CMD_CONFIGURE_WRITE_PARAMS;
    else
       cmd_id = GSL_CMD_CONFIGURE_READ_PARAMS;

    ret = gsl_ioctl(gph_obj->graph_handle, cmd_id, &buf_config, size);

    if (ret != 0) {
        AGM_LOGE("Buffer configuration failed error %d", ret);
    } else {
       gph_obj->buf_config  = buf_config;
    }
    AGM_LOGD("exit");
    return ret;
}

int graph_init()
{
    uint32_t ret = 0;
    struct gsl_acdb_data_files acdb_files;
    struct gsl_init_data init_data;

    /*Populate acdbfiles from the shared file path*/
    acdb_files.num_files = 0;

#ifdef ACDB_PATH
    ret = get_acdb_files_from_directory(CONV_TO_STRING(ACDB_PATH), &acdb_files);
    if (ret != 0)
       return ret;
#else
#  error "Define -DACDB_PATH="PATH" in the makefile to compile"
#endif

    init_data.acdb_files = &acdb_files;
    init_data.acdb_delta_file = NULL;
    init_data.acdb_addr = 0x0;
    init_data.max_num_ready_checks = 1;
    init_data.ready_check_interval_ms = 100;
  
    ret = gsl_init(&init_data);
    if (ret != 0) { 
        AGM_LOGE("gsl_init failed error %d \n", ret);
        goto deinit_gsl;
    }
    return 0;

deinit_gsl:
    gsl_deinit();
    return ret;
}

int graph_deinit()
{ 

    gsl_deinit(); 
    return 0;
}

void gsl_callback_func(struct gsl_event_cb_params *event_params,
                       void *client_data)
{
     struct graph_obj *graph_obj = (struct graph_obj *) client_data;

     if (graph_obj == NULL) {
         AGM_LOGE("Invalid graph object");
         return;
     } 
     if (event_params == NULL) {
         AGM_LOGE("event params NULL");
         return;
     }

     if (graph_obj->cb)
         graph_obj->cb((struct graph_event_cb_params *)event_params,
                        graph_obj->client_data);

     return; 
}

int graph_open(struct agm_meta_data *meta_data_kv,
               struct session_obj *ses_obj, struct device_obj *dev_obj,
               struct graph_obj **gph_obj)
{
    struct graph_obj *graph_obj;
    int ret = 0;
    struct listnode *temp_node,*node = NULL;
    size_t module_info_size;
    struct gsl_module_id_info *module_info;
    int i = 0;
    module_info_t *mod = NULL;

    AGM_LOGD("entry");
    if (meta_data_kv == NULL || gph_obj == NULL) {
        AGM_LOGE("Invalid input\n");
        ret = -EINVAL;
        goto done;
    }

    graph_obj = calloc (1, sizeof(struct graph_obj));
    if (graph_obj == NULL) {
        AGM_LOGE("failed to allocate graph object");
        ret = -ENOMEM;
        goto done;
    }
    list_init(&graph_obj->tagged_mod_list);
    pthread_mutex_init(&graph_obj->lock, (const pthread_mutexattr_t *)NULL);
    /**
     *TODO
     *Once the gsl_get_tag_module_list api is availiable use that api
     *to get the list of tagged modules in the current graph and add
     *it to module_list so that we configure only those modules which
     *are present. For now we query for all modules and add only those
     *are present.
     */

    /**
     *TODO:In the current config parameters we dont have a
     *way to know if a hostless session (loopback)
     *is session loopback or device loopback
     *We assume now that if it is a hostless
     *session then it is device loopback always.
     *And hence we configure pcm decoder/encoder/convertor
     *only in case of a no hostless session.
     */
    if (ses_obj != NULL) {
        int count = sizeof(stream_module_list)/sizeof(struct module_info);
        for (i = 0; i < count; i++) {
             mod = &stream_module_list[i];
             ret = gsl_get_tagged_module_info((struct gsl_key_vector *)
                                               &meta_data_kv->gkv,
                                               mod->tag,
                                               &module_info, &module_info_size);
             if (ret != 0) {
                 AGM_LOGI("cannot get tagged module info for module %x",
                           mod->tag);
                 continue;
             }
             mod->miid = module_info->module_entry[0].module_iid;
             mod->mid = module_info->module_entry[0].module_id;
             AGM_LOGD("miid %x mid %x tag %x", mod->miid, mod->mid, mod->tag);
             ADD_MODULE(*mod, NULL); 
             if (module_info)
                free(module_info);
        }
        graph_obj->sess_obj = ses_obj;
    }

    if (dev_obj != NULL) {
        int count = sizeof(hw_ep_module)/sizeof(struct module_info);

        for (i = 0; i < count; i++) {
             mod = &hw_ep_module[i];
             ret = gsl_get_tagged_module_info((struct gsl_key_vector *)
                                               &meta_data_kv->gkv,
                                               mod->tag,
                                               &module_info, &module_info_size);
             if (ret != 0) {
                 AGM_LOGI("cannot get tagged module info for module %x",
                           mod->tag);
                 continue;
             }
             mod->miid = module_info->module_entry[0].module_iid;
             mod->mid = module_info->module_entry[0].module_id;
             AGM_LOGD("miid %x mid %x tag %x", mod->miid, mod->mid, mod->tag);
             ADD_MODULE(*mod, dev_obj); 
             if (module_info)
                free(module_info);
        }
    }

    ret = gsl_open((struct gsl_key_vector *)&meta_data_kv->gkv,
                   (struct gsl_key_vector *)&meta_data_kv->ckv,
                   &graph_obj->graph_handle);
    if (ret != 0) {
       AGM_LOGE("Failed to open the graph with error %d", ret);
       goto free_graph_obj;
    }

    graph_obj->state = OPENED;
    ret = gsl_register_event_cb(graph_obj->graph_handle,
                                gsl_callback_func, graph_obj);
    if (ret != 0) {
        AGM_LOGE("failed to register callback");
        goto close_graph;
    }

    *gph_obj = graph_obj;

    goto done;

close_graph:
    gsl_close(graph_obj->graph_handle);
free_graph_obj:
    /*free the list of modules associated with this graph_object*/
    list_for_each_safe(node, temp_node, &graph_obj->tagged_mod_list) {
        list_remove(node);
        free(node_to_item(node, module_info_t, list));
    }
    free(graph_obj);
done:
    AGM_LOGD("exit graph_handle %p", graph_obj?graph_obj->graph_handle:NULL);
    return ret;   
}

int graph_close(struct graph_obj *graph_obj)
{
    int ret = 0;
    struct listnode *temp_node,*node = NULL;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock); 
    AGM_LOGD("entry handle %p", graph_obj->graph_handle);
    if (graph_obj->state < OPENED) {
        AGM_LOGE("graph object not in opened state");
        pthread_mutex_unlock(&graph_obj->lock); 
        ret = -EINVAL;
    }

    ret = gsl_close(graph_obj->graph_handle);
    if (ret !=0) 
        AGM_LOGE("gsl close failed error %d", ret);
    AGM_LOGE("gsl graph_closed");
    /*free the list of modules associated with this graph_object*/
    list_for_each_safe(node, temp_node, &graph_obj->tagged_mod_list) {
        list_remove(node);
        free(node_to_item(node, module_info_t, list));
    }
 
    pthread_mutex_unlock(&graph_obj->lock);
    pthread_mutex_destroy(&graph_obj->lock);
    free(graph_obj);
    AGM_LOGD("exit");
    return ret; 
}

int graph_prepare(struct graph_obj *graph_obj)
{
    int ret = 0;
    struct listnode *node = NULL;
    module_info_t *mod = NULL;
    struct session_obj *sess_obj = graph_obj->sess_obj;
    struct agm_session_config stream_config = sess_obj->stream_config;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    AGM_LOGD("entry graph_handle %p", graph_obj->graph_handle);
    pthread_mutex_lock(&graph_obj->lock);
    if (!(graph_obj->state & (OPENED || STOPPED))) {
       AGM_LOGE("graph object not in correct state, current state %d",
                    graph_obj->state);
       ret = -EINVAL;
       goto done;
    }

    /**
     *Iterate over mod list to configure each module
     *present in the graph. Also validate if the module list
     *matches the configuration passed by the client.
     */
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        if (mod->is_configured)
            continue;
        if ((mod->tag == STREAM_INPUT_MEDIA_FORMAT) && stream_config.is_hostless) {
            AGM_LOGE("Shared mem mod present for a hostless session error out");
            ret = -EINVAL;
            goto done;
        }

        if (((mod->tag == STREAM_PCM_DECODER) &&
              (stream_config.dir == TX)) ||
            ((mod->tag == STREAM_PCM_ENCODER) &&
              (stream_config.dir == RX))) {
            AGM_LOGE("Session cfg (dir = %d) does not match session module %x",
                      stream_config.dir, mod->module);
             ret = -EINVAL;
             goto done;
        }

        if ((mod->dev_obj != NULL) && (mod->dev_obj->refcnt.start == 0)) {
            if (((mod->tag == DEVICE_HW_ENDPOINT_RX) &&
                (mod->dev_obj->hw_ep_info.dir == AUDIO_INPUT)) || 
               ((mod->tag == DEVICE_HW_ENDPOINT_TX) &&
                (mod->dev_obj->hw_ep_info.dir == AUDIO_OUTPUT))) {
               AGM_LOGE("device cfg (dir = %d) does not match dev module %x",
                         mod->dev_obj->hw_ep_info.dir, mod->module);
               ret = -EINVAL;
               goto done;
            }
        }
        ret = mod->configure(mod, graph_obj);
        if (ret != 0)
            goto done;
        mod->is_configured = true;
    }

    /*Configure buffers only if it is not a hostless session*/
    if (sess_obj != NULL && !stream_config.is_hostless) {
        ret = configure_buffer_params(graph_obj, sess_obj);
        if (ret != 0) {
            AGM_LOGE("buffer configuration failed \n");
            goto done;
        }
    }

    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_PREPARE, NULL, 0);
    if (ret !=0) {
        AGM_LOGE("graph_prepare failed %d", ret);
    } else {
        graph_obj->state = PREPARED;
    }

done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_start(struct graph_obj *graph_obj)
{
    int ret = 0;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %p", graph_obj->graph_handle);
    if (!(graph_obj->state & (PREPARED | STOPPED))) {
       AGM_LOGE("graph object is not in correct state, current state %d",
                    graph_obj->state);
       ret = -EINVAL;
       goto done;
    }
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_START, NULL, 0); 
    if (ret !=0) {
        AGM_LOGE("graph_start failed %d", ret);
    } else {
        graph_obj->state = STARTED;
    }

done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_stop(struct graph_obj *graph_obj)
{
    int ret = 0;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %p", graph_obj->graph_handle);
    if (!(graph_obj->state & (STARTED))) {
       AGM_LOGE("graph object is not in correct state, current state %d",
                    graph_obj->state);
       ret = -EINVAL;
       goto done;
    }
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_STOP, NULL, 0); 
    if (ret !=0) {
        AGM_LOGE("graph start failed %d", ret);
    } else {
        graph_obj->state = STOPPED;
    }

done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_pause(struct graph_obj *graph_obj)
{
     /**
      *#TODO: Add support to set config using TKV to pause/resume modules
      *This is needed for compress decoder/encoders.For PCM 
      *client is suppose to do a set config as there is no Tinyalsa API
      */
     return -ENOSYS;
}

int graph_resume(struct graph_obj *graph_obj)
{
     /**
      *#TODO: Add support to set config using TKV to pause/resume modules
      *This is needed for compress decoder/encoders.For PCM
      *client is suppose to do a set config as there is no TinyAlsa API
      */
     return -ENOSYS;
}

int graph_set_config(struct graph_obj *graph_obj, void *payload,
                     size_t payload_size)
{
    int ret = 0;
    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %p", graph_obj->graph_handle);
    ret = gsl_set_custom_config(graph_obj->graph_handle, payload, payload_size); 
    if (ret !=0)
        AGM_LOGE("graph_set_config failed %d", ret);

    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_write(struct graph_obj *graph_obj, void *buffer, size_t size)
{
    int ret = 0;
    struct gsl_buff gsl_buff;
    uint32_t size_written = 0;
    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock);
    if (!(graph_obj->state & STARTED)) {
        AGM_LOGE("Cannot add a graph in start state");
        ret = -EINVAL;
        goto done;
    }

    /*TODO: Update the write api to take timeStamps/other buffer meta data*/
    gsl_buff.timestamp = 0x0;
    /*TODO: get the FLAG info from client e.g. FLAG_EOS)*/
    gsl_buff.flags = 0;
    gsl_buff.size = size;
    gsl_buff.addr = (uint8_t *)(buffer);
    ret = gsl_write(graph_obj->graph_handle,
                    SHMEM_ENDPOINT, &gsl_buff, &size_written);
    if (ret != 0) {
        AGM_LOGE("gsl_write for size %d failed with error %d", size, ret);
        goto done;
    }

done:
    pthread_mutex_unlock(&graph_obj->lock);
    return ret;
}

int graph_read(struct graph_obj *graph_obj, void *buffer, size_t size)
{
    int ret = 0;
    struct gsl_buff gsl_buff;
    int size_read = 0;
    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock);
    if (!(graph_obj->state & STARTED)) {
        AGM_LOGE("Cannot add a graph in start state");
        ret = -EINVAL;
        goto done;
    }

    /*TODO: Update the write api to take timeStamps/other buffer meta data*/
    gsl_buff.timestamp = 0x0;
    /*TODO: get the FLAG info from client e.g. FLAG_EOS)*/
    gsl_buff.flags = 0;
    gsl_buff.size = size;
    gsl_buff.addr = (uint8_t *)(buffer);
    ret = gsl_read(graph_obj->graph_handle,
                    SHMEM_ENDPOINT, &gsl_buff, &size_read);
    if ((ret != 0) || (size_read == 0)) {
        AGM_LOGE("size_requested %d size_read %d error %d",
                      size, size_read, ret);
    }
done:
    pthread_mutex_unlock(&graph_obj->lock);
    return ret;
}

int graph_add(struct graph_obj *graph_obj,
              struct agm_meta_data *meta_data_kv,
              struct device_obj *dev_obj)
{
    int ret = 0;
    struct session_obj *sess_obj;
    struct gsl_cmd_graph_select add_graph;
    module_info_t *mod = NULL;
    struct listnode *node = NULL;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %p", graph_obj->graph_handle);

    if (graph_obj->state < OPENED) {
        AGM_LOGE("Cannot add a graph in %d state", graph_obj->state);
        ret = -EINVAL;
        goto done;
    }
    /**
     *This api is used to add a new device leg ( is device object is
     *present in the argument or to add an already exisiting graph.
     *Hence we need not add any new session (stream) related modules
     */

    /*Add the new GKV to the current graph*/
    add_graph.graph_key_vector.num_kvps = meta_data_kv->gkv.num_kvs;
    add_graph.graph_key_vector.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->gkv.kv;
    add_graph.cal_key_vect.num_kvps = meta_data_kv->ckv.num_kvs;
    add_graph.cal_key_vect.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->ckv.kv;
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_ADD_GRAPH, &add_graph,
                    sizeof(struct gsl_cmd_graph_select));
    if (ret != 0) {
        AGM_LOGE("graph add failed with error %d", ret);
        goto done;
    }
    if (dev_obj != NULL) {
        module_info_t *add_module, *temp_mod = NULL;
        size_t module_info_size;
        struct gsl_module_id_info *module_info;
        bool mod_present = false;
        if (dev_obj->hw_ep_info.dir == AUDIO_OUTPUT)
            mod = &hw_ep_module[0];
        else 
            mod = &hw_ep_module[1];

        ret = gsl_get_tagged_module_info((struct gsl_key_vector *)
                                           &meta_data_kv->gkv,
                                           mod->tag,
                                           &module_info, &module_info_size);
        if (ret != 0) {
            AGM_LOGE("cannot get tagged module info for module %x",
                          mod->tag);
            ret = -EINVAL;
            goto done;
        }
        mod->miid = module_info->module_entry[0].module_iid;
        mod->mid = module_info->module_entry[0].module_id;
        /**
         *Check if this is the same device object as was passed for graph open
         *or a new one.We do this by comparing the module_iid of the module
         *present in the graph object with the one returned from the above api.
         *if it is a new module we add it to the list and configure it.
         */
        list_for_each(node, &graph_obj->tagged_mod_list) {
            temp_mod = node_to_item(node, module_info_t, list);
            if (temp_mod->miid == mod->miid) {
                mod_present = true;
                break;
            }
        }
        if (!mod_present) {
            /**
             *This is a new device object, add this module to the list and 
             */
            AGM_LOGD("Adding the new module tag %x mid %x miid %x", mod->tag, mod->mid, mod->miid);
            add_module = ADD_MODULE(*mod, dev_obj);
            add_module->miid = module_info->module_entry[0].module_iid;
            add_module->mid = module_info->module_entry[0].module_id; 
        }
    }
    /*configure the newly added modules*/
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        if (mod->is_configured)
            continue;
        ret = mod->configure(mod, graph_obj);
        if (ret != 0)
            goto done;
        mod->is_configured = true;
    }
done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_change(struct graph_obj *graph_obj,
                     struct agm_meta_data *meta_data_kv,
                     struct device_obj *dev_obj)
{
    int ret = 0;
    struct session_obj *sess_obj;
    struct gsl_cmd_graph_select change_graph;
    module_info_t *mod = NULL;
    struct listnode *node, *temp_node = NULL;

    if (graph_obj == NULL) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }

    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %p", graph_obj->graph_handle);
    if (graph_obj->state & STARTED) {
        AGM_LOGE("Cannot change graph in start state");
        ret = -EINVAL;
        goto done;
    }

    /**
     *GSL closes the old graph if CHANGE_GRAPH command is issued.
     *Hence reset is_configured to ensure that all the modules are 
     *configured once again.
     *
     */
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        mod->is_configured = false; 
    }

    if (dev_obj != NULL) {
        mod = NULL;
        module_info_t *add_module, *temp_mod = NULL;
        size_t module_info_size;
        struct gsl_module_id_info *module_info;
        bool mod_present = false;
        if (dev_obj->hw_ep_info.dir == AUDIO_OUTPUT)
            mod = &hw_ep_module[0];
        else 
            mod = &hw_ep_module[1];

        ret = gsl_get_tagged_module_info((struct gsl_key_vector *)
                                           &meta_data_kv->gkv,
                                           mod->tag,
                                           &module_info, &module_info_size);
        if (ret != 0) {
            AGM_LOGE("cannot get tagged module info for module %x",
                          mod->tag);
            ret = -EINVAL;
            goto done;
        }
        /**
         *Check if this is the same device object as was passed for graph open
         *or a new one.We do this by comparing the module_iid of the module
         *present in the graph object with the one returned from the above api.
         *If this is a new module, we delete the older device tagged module
         *as it is not part of the graph anymore (would have been removed as a
         *part of graph_remove).
         */
        list_for_each(node, &graph_obj->tagged_mod_list) {
            temp_mod = node_to_item(node, module_info_t, list);
            if (temp_mod->miid = module_info->module_entry[0].module_iid) {
                mod_present = true;
                break;
            }
        }
        if (!mod_present) {
            /*This is a new device object, add this module to the list and 
             *delete the current hw_ep(Device module) from the list.
             */
            list_for_each_safe(node, temp_node, &graph_obj->tagged_mod_list) {
                temp_mod = node_to_item(node, module_info_t, list);
                if ((temp_mod->tag = DEVICE_HW_ENDPOINT_TX) ||
                    (temp_mod->tag = DEVICE_HW_ENDPOINT_RX)) {
                    list_remove(node);
                    free(temp_mod);
                    temp_mod = NULL; 
                }
            }
            add_module = ADD_MODULE(*mod, dev_obj);
            add_module->miid = module_info->module_entry[0].module_iid;
            add_module->mid = module_info->module_entry[0].module_id; 
        }
    } 
    /*Send the new GKV for CHANGE_GRAPH*/
    change_graph.graph_key_vector.num_kvps = meta_data_kv->gkv.num_kvs;
    change_graph.graph_key_vector.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->gkv.kv;
    change_graph.cal_key_vect.num_kvps = meta_data_kv->ckv.num_kvs;
    change_graph.cal_key_vect.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->ckv.kv;
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_CHANGE_GRAPH, &change_graph,
                    sizeof(struct gsl_cmd_graph_select));
    if (ret != 0) {
        AGM_LOGE("graph add failed with error %d", ret);
        goto done;
    }
    /*configure modules again*/
    list_for_each(node, &graph_obj->tagged_mod_list) {
        mod = node_to_item(node, module_info_t, list);
        ret = mod->configure(mod, graph_obj);
        if (ret != 0)
            goto done;
        mod->is_configured = true;
    }
done:
    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_remove(struct graph_obj *graph_obj,
                 struct agm_meta_data *meta_data_kv)
{
    int ret = 0;
    struct gsl_cmd_remove_graph rm_graph;

    if ((graph_obj == NULL)) {
        AGM_LOGE("invalid graph object");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock);
    AGM_LOGD("entry graph_handle %p", graph_obj->graph_handle);

    /**
     *graph_remove would only pass the graph which needs to be removed.
     *to GSL. Once graph remove is done, sesison obj will have to issue
     *graph_change/graph_add if reconfiguration of modules is needed, otherwise
     *graph_start will suffice.graph remove wont reconfigure the modules.
     */
    rm_graph.graph_key_vector.num_kvps = meta_data_kv->gkv.num_kvs;
    rm_graph.graph_key_vector.kvp = (struct gsl_key_value_pair *)
                                     meta_data_kv->gkv.kv;
    ret = gsl_ioctl(graph_obj->graph_handle, GSL_CMD_REMOVE_GRAPH, &rm_graph,
                    sizeof(struct gsl_cmd_remove_graph));
    if (ret != 0) {
        AGM_LOGE("graph add failed with error %d", ret);
    }

    pthread_mutex_unlock(&graph_obj->lock);
    AGM_LOGD("exit");
    return ret;
}

int graph_register_cb(struct graph_obj *graph_obj, event_cb cb,
                      void *client_data)
{
    int ret = 0;
    if ((graph_obj == NULL) || (cb == NULL)) {
        AGM_LOGE("invalid graph object or null callback");
        return -EINVAL;
    }
    pthread_mutex_lock(&graph_obj->lock);
    graph_obj->cb = cb;
    graph_obj->client_data = client_data; 
    pthread_mutex_unlock(&graph_obj->lock);

    return 0;
}

size_t graph_get_hw_processed_buff_cnt(struct graph_obj *graph_obj,
                                       enum direction dir)
{
    if ((graph_obj == NULL)) {
        AGM_LOGE("invalid graph object or null callback");
        return 0;
    }
    /*TODO: Uncomment that call once platform moves to latest GSL release*/
    return 2 /*gsl_get_processed_buff_cnt(graph_obj->graph_handle, dir)*/;

}