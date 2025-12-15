#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <asoundlib.h>

#include <fdk-aac/aacenc_lib.h>

static volatile int stop_flag = 0;
static void handle_sig(int sig) { (void)sig; stop_flag = 1; }

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <out.aac> -d <card> -D <device> -t <seconds>\n", argv[0]);
        return 1;
    }

    const char *outfile = argv[1];
    int card = 0, device = 0, seconds = 5;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i+1 < argc) card = atoi(argv[++i]);
        else if (strcmp(argv[i], "-D") == 0 && i+1 < argc) device = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) seconds = atoi(argv[++i]);
    }

    printf("Recording %d seconds...\n", seconds);

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
        return 1;
    }

    printf("PCM: %u Hz, %u ch, 16 bits\n", cfg.rate, cfg.channels);

    FILE *fp = fopen(outfile, "wb");
    if (!fp) { perror("fopen"); return 1; }

    /* ---- AAC Encoder 初始化 ---- */
    HANDLE_AACENCODER encoder;
    AACENC_ERROR err;

    if ((err = aacEncOpen(&encoder, 0, cfg.channels)) != AACENC_OK) {
        fprintf(stderr, "aacEncOpen(): %d\n", err);
        return 1;
    }

    aacEncoder_SetParam(encoder, AACENC_AOT, AOT_AAC_LC);
    aacEncoder_SetParam(encoder, AACENC_SAMPLERATE, cfg.rate);
    aacEncoder_SetParam(encoder, AACENC_CHANNELMODE, cfg.channels == 1 ? MODE_1 : MODE_2);
    aacEncoder_SetParam(encoder, AACENC_BITRATE, 64000);
    aacEncoder_SetParam(encoder, AACENC_TRANSMUX, TT_MP4_ADTS);

    if ((err = aacEncEncode(encoder, NULL, NULL, NULL, NULL)) != AACENC_OK) {
        fprintf(stderr, "aacEncEncode(): %d\n", err);
        return 1;
    }

    AACENC_InfoStruct info;
    aacEncInfo(encoder, &info);

    printf("AAC encoder: LC, %u Hz, %u ch → ADTS\n", cfg.rate, cfg.channels);

    signal(SIGINT, handle_sig);

    /* 每次读取 PCM period */
    int pcm_bytes = cfg.period_size * cfg.channels * 2;
    uint8_t *pcm_buf = malloc(pcm_bytes);

    /* AAC 输出缓存 */
    uint8_t aac_buf[2048];
    uint8_t adts[7];

    int loops = seconds * (cfg.rate / cfg.period_size);

    while (!stop_flag && loops--) {
        if (pcm_read(pcm, pcm_buf, pcm_bytes)) {
            fprintf(stderr, "pcm_read error\n");
            break;
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
            break;
        }

        if (out_args.numOutBytes > 0) {
            write_adts_header(adts, out_args.numOutBytes, cfg.rate, cfg.channels);
            fwrite(adts, 1, 7, fp);
            fwrite(aac_buf, 1, out_args.numOutBytes, fp);
        }
    }

    free(pcm_buf);
    pcm_close(pcm);
    aacEncClose(&encoder);
    fclose(fp);

    printf("Saved %s\n", outfile);
    return 0;
}
