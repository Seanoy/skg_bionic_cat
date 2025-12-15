#include "audio_controller.h"

static const char *format_to_str(enum pcm_format f) {
    switch (f) {
        case PCM_FORMAT_S8: return "S8";
        case PCM_FORMAT_S16_LE: return "S16_LE";
        case PCM_FORMAT_S24_LE: return "S24_LE";
        case PCM_FORMAT_S32_LE: return "S32_LE";
        default: return "UNKNOWN";
    }
}

static void write_adts_header(uint8_t *adts, int aac_length, int samplerate, int channels) {
    int sf_index;

    switch (samplerate) {
        case 96000: sf_index = 0; break;
        case 88200: sf_index = 1; break;
        case 64000: sf_index = 2; break;
        case 48000: sf_index = 3; break;
        case 44100: sf_index = 4; break;
        case 32000: sf_index = 5; break;
        case 24000: sf_index = 6; break;
        case 22050: sf_index = 7; break;
        case 16000: sf_index = 8; break;
        case 12000: sf_index = 9; break;
        case 11025: sf_index = 10; break;
        case 8000:  sf_index = 11; break;
        default:    sf_index = 3;
    }

    int adts_len = aac_length + 7;

    adts[0] = 0xFF;
    adts[1] = 0xF1;
    adts[2] = (1 << 6) | (sf_index << 2) | ((channels & 4) >> 2);
    adts[3] = ((channels & 3) << 6) | (adts_len >> 11);
    adts[4] = (adts_len >> 3) & 0xFF;
    adts[5] = ((adts_len & 7) << 5) | 0x1F;
    adts[6] = 0xFC;
}
#define PLAYBACK_CARD     0
#define PLAYBACK_DEVICE   0

static struct pcm *open_playback_pcm()
{
    struct pcm_config cfg = {
        .channels = 1,
        .rate = 48000,
        .period_size = 1024,
        .period_count = 4,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = 0,
        .stop_threshold = 0,
        .silence_threshold = 0,
    };

    struct pcm *pcm = pcm_open(PLAYBACK_CARD, PLAYBACK_DEVICE, PCM_OUT, &cfg);
    if (!pcm || !pcm_is_ready(pcm)) {
        fprintf(stderr, "Cannot open playback PCM: %s\n",
                pcm ? pcm_get_error(pcm) : "unknown error");
        if (pcm) pcm_close(pcm);
        return NULL;
    }

    printf("Playback PCM opened: %u Hz, %u ch, S16_LE\n", cfg.rate, cfg.channels);
    return pcm;
}

int play_aac_with_fdk(const uint8_t *aac_data, size_t aac_size)
{
    HANDLE_AACDECODER decoder = NULL;
    struct pcm *pcm = NULL;
    AAC_DECODER_ERROR err;

    /* 1. 打开播放设备 */
    pcm = open_playback_pcm();
    if (!pcm) return -1;

    /* 2. 打开 FDK AAC 解码器 */
    decoder = aacDecoder_Open(TT_MP4_ADTS, 1);  // TT_MP4_ADTS 表示支持 ADTS 头
    if (!decoder) {
        fprintf(stderr, "aacDecoder_Open failed\n");
        pcm_close(pcm);
        return -1;
    }

    /* 可选：限制输出声道数（单声道） */
    aacDecoder_SetParam(decoder, AAC_PCM_MAX_OUTPUT_CHANNELS, 1);
    aacDecoder_SetParam(decoder, AAC_PCM_MIN_OUTPUT_CHANNELS, 1);

    /* 3. 输入/输出缓冲区 */
    const uint8_t *input_ptr = aac_data;
    size_t input_remaining = aac_size;

    INT_PCM pcm_out[2048 * 8];  // 足够大，最大支持 8 声道 2048 样本
    UINT valid_bytes = 0;

    printf("Starting AAC playback (%zu bytes)...\n", aac_size);

    while (input_remaining > 0) {
        /* ---- 喂数据给解码器内部缓冲区 ---- */
        UINT bytes_to_feed = (UINT)input_remaining;
        err = aacDecoder_Fill(decoder,
                              (UCHAR**)&input_ptr,
                              &bytes_to_feed,
                              &input_remaining);

        if (err != AAC_DEC_OK) {
            fprintf(stderr, "aacDecoder_Fill error: 0x%x\n", err);
            break;
        }

        /* ---- 循环解码帧直到内部缓冲区数据不足 ---- */
        while (1) {
            err = aacDecoder_DecodeFrame(decoder,
                                         pcm_out,
                                         sizeof(pcm_out) / sizeof(INT_PCM),
                                         0);  // flags = 0

            if (err == AAC_DEC_NOT_ENOUGH_BITS) {
                // 需要更多 AAC 数据
                break;
            }

            if (err != AAC_DEC_OK) {
                fprintf(stderr, "aacDecoder_DecodeFrame error: 0x%x\n", err);
                // 简单跳过错误帧，继续尝试下一帧
                continue;
            }

            /* ---- 获取解码信息 ---- */
            CStreamInfo *info = aacDecoder_GetStreamInfo(decoder);
            if (!info || info->frameSize <= 0) {
                fprintf(stderr, "Invalid stream info\n");
                continue;
            }

            int samples = info->frameSize * info->numChannels;  // 总样本数

            /* ---- 写入声卡播放 ---- */
            if (pcm_write(pcm, pcm_out, samples * sizeof(INT_PCM)) != 0) {
                fprintf(stderr, "pcm_write failed\n");
                goto cleanup;
            }
        }
    }

    /* ---- 冲刷剩余数据（可选，确保播放完最后一点） ---- */
    do {
        err = aacDecoder_DecodeFrame(decoder, pcm_out,
                                     sizeof(pcm_out)/sizeof(INT_PCM),
                                     AACDEC_FLUSH);
        if (err != AAC_DEC_OK) break;

        CStreamInfo *info = aacDecoder_GetStreamInfo(decoder);
        if (!info || info->frameSize <= 0) break;

        int samples = info->frameSize * info->numChannels;
        pcm_write(pcm, pcm_out, samples * sizeof(INT_PCM));
    } while (1);

cleanup:
    printf("Playback finished.\n");

    if (decoder) aacDecoder_Close(decoder);
    if (pcm) pcm_close(pcm);

    return 0;
}

size_t record_aac_with_fdk(uint8_t *aac_data, size_t aac_size) {
    int card = 1, device = 0;
    
    printf("Recording ...\n");

    /* ---- Tinyalsa PCM 配置 ---- */
    struct pcm_config cfg = {
        .channels = 1,
        .rate = 48000,
        .period_size = 1024,
        .period_count = 4,
        .format = PCM_FORMAT_S16_LE
    };

    struct pcm *pcm = pcm_open(card, device, PCM_IN, &cfg);
    if (!pcm || !pcm_is_ready(pcm)) {
        fprintf(stderr, "PCM open failed: %s\n", pcm ? pcm_get_error(pcm) : "");
        return -1;
    }

    printf("PCM: %u Hz, %u ch, 16 bits\n", cfg.rate, cfg.channels);

    /* ---- AAC Encoder 初始化 ---- */
    HANDLE_AACENCODER encoder;
    AACENC_ERROR err;

    if ((err = aacEncOpen(&encoder, 0, cfg.channels)) != AACENC_OK) {
        fprintf(stderr, "aacEncOpen(): %d\n", err);
        return -1;
    }

    aacEncoder_SetParam(encoder, AACENC_AOT, AOT_AAC_LC);
    aacEncoder_SetParam(encoder, AACENC_SAMPLERATE, cfg.rate);
    aacEncoder_SetParam(encoder, AACENC_CHANNELMODE, cfg.channels == 1 ? MODE_1 : MODE_2);
    aacEncoder_SetParam(encoder, AACENC_BITRATE, 64000);
    aacEncoder_SetParam(encoder, AACENC_TRANSMUX, TT_MP4_ADTS);

    if ((err = aacEncEncode(encoder, NULL, NULL, NULL, NULL)) != AACENC_OK) {
        fprintf(stderr, "aacEncEncode(): %d\n", err);
        return -1;
    }

    AACENC_InfoStruct info;
    aacEncInfo(encoder, &info);

    printf("AAC encoder: LC, %u Hz, %u ch → ADTS\n", cfg.rate, cfg.channels);

    /* 每次读取 PCM period */
    int pcm_bytes = cfg.period_size * cfg.channels * 2;
    uint8_t *pcm_buf = malloc(pcm_bytes);

    /* AAC 输出缓存 */
    uint8_t aac_buf[2048];
    uint8_t adts[7];

    size_t total_aac_bytes = 0;
    if (pcm_read(pcm, pcm_buf, pcm_bytes)) {
        fprintf(stderr, "pcm_read error\n");
        return -1;
    }

    AACENC_BufDesc in_buf = {0}, out_buf = {0};
    AACENC_InArgs in_args = {0};
    AACENC_OutArgs out_args = {0};

    void *in_ptr = pcm_buf;
    int in_size = pcm_bytes;
    int in_elem_size = 2;

    in_buf.numBufs = 1;
    in_buf.bufs = &in_ptr;
    in_buf.bufferIdentifiers = (int[]){IN_AUDIO_DATA};
    in_buf.bufSizes = &in_size;
    in_buf.bufElSizes = &in_elem_size;

    void *out_ptr = aac_buf;
    int out_size = sizeof(aac_buf);
    int out_elem_size = 1;

    out_buf.numBufs = 1;
    out_buf.bufs = &out_ptr;
    out_buf.bufferIdentifiers = (int[]){OUT_BITSTREAM_DATA};
    out_buf.bufSizes = &out_size;
    out_buf.bufElSizes = &out_elem_size;

    in_args.numInSamples = pcm_bytes / 2;

    if ((err = aacEncEncode(encoder, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
        fprintf(stderr, "aacEncEncode(): %d\n", err);
        return -1;
    }

    if (out_args.numOutBytes > 0) {
        write_adts_header(adts, out_args.numOutBytes, cfg.rate, cfg.channels);
        total_aac_bytes += out_args.numOutBytes + 7;
        
        if (total_aac_bytes < aac_size) {
            memcpy(aac_data + total_aac_bytes - out_args.numOutBytes - 7, adts, 7);
            memcpy(aac_data + total_aac_bytes - out_args.numOutBytes, aac_buf, out_args.numOutBytes);
        } else {
            fprintf(stderr, "Buffer too small for recorded AAC data\n");
            total_aac_bytes -= out_args.numOutBytes + 7;
        }
    }
    
    free(pcm_buf);
    pcm_close(pcm);
    aacEncClose(&encoder);
    printf("Recording finished, total AAC bytes: %zu\n", total_aac_bytes);
    return total_aac_bytes;
}
