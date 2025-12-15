#ifndef AUDIO_CONTROLLER_H_
#define AUDIO_CONTROLLER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <asoundlib.h>
#include <fdk-aac/aacenc_lib.h>
#include <fdk-aac/aacdecoder_lib.h>

int play_aac_with_fdk(const uint8_t *aac_data, size_t aac_size);
size_t record_aac_with_fdk(uint8_t *aac_data, size_t aac_size);

#endif
