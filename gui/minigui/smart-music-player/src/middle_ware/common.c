#include <stdio.h>
#include <stdlib.h>
#include <sys/statfs.h>
#include <alsa/asoundlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include "common.h"
#include "resource.h"

/* #define CARD_DEV_NODE1 "/dev/mmcblk1" */
/* #define CARD_DEV_NODE2 "/dev/mmcblk1p1" */
/* #define UDISK_DEV_NODE "/dev/sda1" */

#define BACK_CAR_DEV "/dev/back_car"

/* #define MOUNT_PATH "/mnt/SDCARD" */

#define POWER_KEY_DEV "/dev/key_pwr"

typedef struct __WAVE_HEADER1 {
    unsigned int uRiffFcc; /*  four character code, "RIFF" */
    unsigned int uFileLen; /*  file total length, don't care it */

    unsigned int uWaveFcc; /*  four character code, "WAVE" */

    unsigned int uFmtFcc; /*  four character code, "fmt " */
    unsigned int uFmtDataLen; /*  Length of the fmt data (=16) */
    unsigned short uWavEncodeTag; /*  WAVE File Encoding Tag */
    unsigned short uChannels; /*  Channels: 1 = mono, 2 = stereo */
    unsigned int uSampleRate; /*  Samples per second: e.g., 44100 */
    unsigned int uBytesPerSec; /*  sample rate * block align */
    unsigned short uBlockAlign; /*  channels * bits/sample / 8 */
    unsigned short uBitsPerSample; /*  8 or 16 */

    unsigned int uDataFcc; /*  four character code "data" */
    unsigned int uSampDataSize; /*  Sample data size(n) */

}__attribute__((packed)) wave_header_t;

#define BUF_LEN 1024

int play_wav_music(const char * partname) {
    int i;
    int err;
    wave_header_t wav;
    int headwavcntp;
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params;
    FILE *fp = NULL;
    snd_pcm_format_t pcm_fmt;
    char buf[BUF_LEN];

    fprintf(stderr, "open file : %s\n", partname);
    fp = fopen(partname, "r");
    if (fp == NULL) {
        fprintf(stderr, "open test pcm file err\n");
        return -1;
    }

    headwavcntp = fread(&wav, 1, sizeof(wave_header_t), fp);
    if (headwavcntp != sizeof(wave_header_t)) {
        printf("read wav file head error!\n");
        fclose(fp);
        return -1;
    }

    printf("read wav file head success\n");
    /* printf("bps = %d\n", wav.uBitsPerSample); */
    /* printf("chn = %d\n", wav.uChannels); */
    /* printf("fs = %d\n", wav.uSampleRate); */

    if (wav.uBitsPerSample == 8) {
        pcm_fmt = SND_PCM_FORMAT_S8;
    } else if (wav.uBitsPerSample == 16) {
        pcm_fmt = SND_PCM_FORMAT_S16_LE;
    } else {
        printf("uBitsPerSample not support!\n");
        fclose(fp);
        return -1;
    }
    if ((err = snd_pcm_open(&playback_handle, "default",
            SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "cannot open audio device record.pcm (%s)\n",
                snd_strerror(err));
        fclose(fp);
        return -1;
    }
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_any(playback_handle, hw_params)) < 0) {
        fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n",
                snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_set_access(playback_handle, hw_params,
            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, pcm_fmt))
            < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_set_rate(playback_handle, hw_params,
            wav.uSampleRate, 0)) < 0) {
        fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_set_channels(playback_handle, hw_params,
            wav.uChannels)) < 0) {
        fprintf(stderr, "cannot set channel count (%s)\n", snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params(playback_handle, hw_params)) < 0) {
        fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
        goto play_wav_out;
    }
    snd_pcm_hw_params_free(hw_params);

    while (!feof(fp)) {
        err = fread(buf, 1, BUF_LEN, fp);
        if (err < 0) {
            fprintf(stderr, "read pcm from file err\n");
            goto play_wav_out;
        }
        err = snd_pcm_writei(playback_handle, buf, BUF_LEN / 4);
        if (err < 0) {
            fprintf(stderr, "write to audio interface failed (%s)\n",
                    snd_strerror(err));
            goto play_wav_out;
        }
    }

    play_wav_out: fprintf(stderr, "close file\n");
    fclose(fp);
    fprintf(stderr, "close dev\n");
    snd_pcm_close(playback_handle);
    fprintf(stderr, "ok\n");
    return 0;
}

static wave_header_t keytone_head;
static char *keytone_buff = NULL;
static unsigned int keytone_len;

int keytone_play(void) {
    int err;
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params = NULL;
    snd_pcm_format_t pcm_fmt;
    if (keytone_head.uBitsPerSample == 8) {
        pcm_fmt = SND_PCM_FORMAT_S8;
    } else if (keytone_head.uBitsPerSample == 16) {
        pcm_fmt = SND_PCM_FORMAT_S16_LE;
    } else {
        printf("uBitsPerSample not support!\n");
        return -1;
    }
    if ((err = snd_pcm_open(&playback_handle, "default",
            SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "cannot open audio device record.pcm (%s)\n",
                snd_strerror(err));
        return -1;
    }
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_any(playback_handle, hw_params)) < 0) {
        fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n",
                snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_set_access(playback_handle, hw_params,
            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, pcm_fmt))
            < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_set_rate(playback_handle, hw_params,
            keytone_head.uSampleRate, 0)) < 0) {
        fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params_set_channels(playback_handle, hw_params,
            keytone_head.uChannels)) < 0) {
        fprintf(stderr, "cannot set channel count (%s)\n", snd_strerror(err));
        goto play_wav_out;
    }
    if ((err = snd_pcm_hw_params(playback_handle, hw_params)) < 0) {
        fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
        goto play_wav_out;
    }
    snd_pcm_hw_params_free(hw_params);
    snd_pcm_writei(playback_handle, keytone_buff, keytone_len);
    play_wav_out: snd_pcm_close(playback_handle);
    return 0;
}

int keytone_init(const char * partname) {
    int ret = 0;
    FILE *fp = NULL;
    int r_size;
    int f_size;
    fprintf(stderr, "open file : %s\n", partname);
    fp = fopen(partname, "r");
    if (fp == NULL) {
        fprintf(stderr, "open test pcm file err\n");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    f_size = ftell(fp);
    keytone_buff = malloc(f_size);
    if (!keytone_buff) {
        printf("keytone_buff malloc failed!\n");
        fclose(fp);
        return -1;
    }
    fseek(fp, 0, SEEK_SET);
    r_size = fread(&keytone_head, 1, sizeof(wave_header_t), fp);
    if (r_size != sizeof(wave_header_t)) {
        printf("read 111 wav file head error!\n");
        fclose(fp);
        free(keytone_buff);
        keytone_buff = NULL;
        return -1;
    }
    printf("read wav file head success\n");
    keytone_len = fread(keytone_buff, 1, f_size, fp);
    if (keytone_len <= 0) {
        printf("read 222 wav file head error!\n");
        fclose(fp);
        free(keytone_buff);
        keytone_buff = NULL;
        return -1;
    }
    fclose(fp);
    return 0;
}

int keytone_exit(void) {
    if (keytone_buff) {
        free(keytone_buff);
        keytone_buff = NULL;
    }
    return 0;
}

int get_local_time(struct tm *u_time) {
    time_t timep;
    struct tm *p;
    time(&timep);
    p = localtime(&timep); /*Get local time*/
    u_time->tm_year = 1900 + p->tm_year;
    u_time->tm_mon = 1 + p->tm_mon;
    u_time->tm_mday = p->tm_mday;
    u_time->tm_hour = p->tm_hour;
    u_time->tm_min = p->tm_min;
    u_time->tm_sec = p->tm_sec;

    return 0;
}

int set_local_time(struct tm *u_time) {
    char buff[64];
    memset(buff, 0, sizeof(buff));
    sprintf(buff, "date -s \"%d-%d-%d %d:%d:%d\"", u_time->tm_year,
            u_time->tm_mon, u_time->tm_mday, u_time->tm_hour, u_time->tm_min,
            u_time->tm_sec);
    system(buff);
    system("hwclock -w");
    return 0;
}

int back_car_det(void) {
    int fd;
    char val;
    fd = open(BACK_CAR_DEV, O_RDONLY);
    if (fd < 0) {
        printf("open error\n");
        return -1;
    }
    if (read(fd, &val, 1) != 1) {
        close(fd);
        return -1;
    }
    close(fd);
    return val;
}

int power_off(void) {
    system("poweroff");
    return 0;
}
