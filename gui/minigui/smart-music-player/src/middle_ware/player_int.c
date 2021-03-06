#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#include "resource.h"
#include "player_int.h"

#define CEDARX_UNUSE(param) (void)param
#define ISNULL(x) if(!x){return -1;}

typedef struct PLAYER_CONTEXT_T {
    TPlayer* mTPlayer;
    int mSeekable;
    int mError;
    int mVideoFrameNum;
    bool mPreparedFlag;
    bool mLoopFlag;
    bool mSetLoop;
    bool mCompleteFlag;
    //char              mUrl[64];
    MediaInfo* mMediaInfo;
    sem_t mPreparedSem;
} player_context_t;

static player_context_t player_context;

//* a callback for tplayer.
static int CallbackForTPlayer(void* pUserData, int msg, int param0,
        void* param1) {
    player_context_t* pPlayer = (player_context_t*) pUserData;

    CEDARX_UNUSE(param1);
    switch (msg) {
    case TPLAYER_NOTIFY_PREPARED: {
        printf("TPLAYER_NOTIFY_PREPARED,has prepared.\n");
        pPlayer->mPreparedFlag = 1;
        sem_post(&pPlayer->mPreparedSem);
        break;
    }
    case TPLAYER_NOTIFY_PLAYBACK_COMPLETE: {
        printf("TPLAYER_NOTIFY_PLAYBACK_COMPLETE\n");

        BroadcastMessage(MSG_PLAYBACK_COMPLETE, 0, 0);
        player_context.mCompleteFlag = 1;
        break;
    }
    case TPLAYER_NOTIFY_SEEK_COMPLETE: {
        printf("TPLAYER_NOTIFY_SEEK_COMPLETE>>>>info: seek ok.\n");
        break;
    }
    case TPLAYER_NOTIFY_MEDIA_ERROR: {
        switch (param0) {
        case TPLAYER_MEDIA_ERROR_UNKNOWN: {
            printf("erro type:TPLAYER_MEDIA_ERROR_UNKNOWN\n");
            break;
        }
        case TPLAYER_MEDIA_ERROR_UNSUPPORTED: {
            printf("erro type:TPLAYER_MEDIA_ERROR_UNSUPPORTED\n");
            break;
        }
        case TPLAYER_MEDIA_ERROR_IO: {
            printf("erro type:TPLAYER_MEDIA_ERROR_IO\n");
            break;
        }
        }
        printf("error: open media source fail.\n");
        break;
    }
    case TPLAYER_NOTIFY_NOT_SEEKABLE: {
        pPlayer->mSeekable = 0;
        printf("info: media source is unseekable.\n");
        break;
    }
    case TPLAYER_NOTIFY_BUFFER_START: {
        printf("have no enough data to play\n");
        break;
    }
    case TPLAYER_NOTIFY_BUFFER_END: {
        printf("have enough data to play again\n");
        break;
    }
    case TPLAYER_NOTIFY_VIDEO_FRAME: {
        //printf("get the decoded video frame\n");
        break;
    }
    case TPLAYER_NOTIFY_AUDIO_FRAME: {
        //printf("get the decoded audio frame\n");
        AudioPcmData* audioPcmData = (AudioPcmData*) param1;
        sm_debug(
                "TPLAYER_NOTIFY_AUDIO_FRAME pData=%p nSize=%u samplerate=%u channels=%u\n",
                audioPcmData->pData, audioPcmData->nSize,
                audioPcmData->samplerate, audioPcmData->channels);

        if (isActivityMusicSpectrumOpen && !isAudioPcmDataCopy && g_AudioPcmData) {
            int ret = pthread_mutex_trylock(&spectrumMutex);
            /* int ret = pthread_mutex_lock(&spectrumMutex); */
            if (0 == ret) {
                /* The lock is not used */
                /* Copy the middle data, the FFT data looks good */
                memcpy(g_AudioPcmData,
                        audioPcmData->pData
                                + (((audioPcmData->nSize - PCM_DATA_SIZE) / 2)
                                        * sizeof(char)),
                        PCM_DATA_SIZE * sizeof(char));
                isAudioPcmDataCopy = TRUE;
                pthread_mutex_unlock(&spectrumMutex);
            } else if (EBUSY == ret) {
                /* The lock is used */
                sm_debug("pthread_mutex_trylock EBUSY\n");
                break;
            }
        }
        break;
    }
    case TPLAYER_NOTIFY_SUBTITLE_FRAME: {
        //printf("get the decoded subtitle frame\n");
        break;
    }
    default: {
        printf("warning: unknown callback from Tinaplayer.\n");
        break;
    }
    }
    return 0;
}

int tplayer_init() {
    //* create a player.
    player_context.mTPlayer = TPlayerCreate(AUDIO_PLAYER);
    if (player_context.mTPlayer == NULL) {
        printf("can not create tplayer, quit.\n");
        return -1;
    }
    //* set callback to player.
    TPlayerSetNotifyCallback(player_context.mTPlayer, CallbackForTPlayer,
            (void*) &player_context);

    /*if(((access("/dev/zero",F_OK)) < 0)||((access("/dev/fb0",F_OK)) < 0)){
     printf("/dev/zero OR /dev/fb0 is not exit\n");
     }else{
     system("dd if=/dev/zero of=/dev/fb0");//clean the framebuffer
     }*/
    //set player start status
    player_context.mError = 0;
    player_context.mSeekable = 1;
    player_context.mPreparedFlag = 0;
    player_context.mLoopFlag = 0;
    player_context.mSetLoop = 0;
    player_context.mMediaInfo = NULL;
    player_context.mCompleteFlag = 0;
    sem_init(&player_context.mPreparedSem, 0, 0);

    TPlayerReset(player_context.mTPlayer);
    TPlayerSetDebugFlag(player_context.mTPlayer, 0);
    /*TPlayerSetRotate(player_context.mTPlayer, rotateDegree);*/
    return 0;
}

int tplayer_exit(void) {
    if (!player_context.mTPlayer) {
        printf("player not init.\n");
        return -1;
    }
    TPlayerReset(player_context.mTPlayer);
    TPlayerDestroy(player_context.mTPlayer);
    player_context.mTPlayer = NULL;
    sem_destroy(&player_context.mPreparedSem);
    return 0;
}

static int semTimedWait(sem_t* sem, int64_t time_ms) {
    int err;

    if (time_ms == -1) {
        err = sem_wait(sem);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += time_ms % 1000 * 1000 * 1000;
        ts.tv_sec += time_ms / 1000 + ts.tv_nsec / (1000 * 1000 * 1000);
        ts.tv_nsec = ts.tv_nsec % (1000 * 1000 * 1000);

        err = sem_timedwait(sem, &ts);
    }

    return err;
}

int tplayer_play_url_first(const char *parth) {
    int waitErr = 0;

    ISNULL(player_context.mTPlayer);
    TPlayerReset(player_context.mTPlayer); //del will cause media source is unseekable
    if (TPlayerSetDataSource(player_context.mTPlayer, parth, NULL) != 0) {
        printf("TPlayerSetDataSource() return fail.\n");
        return -1;
    } else {
        printf("setDataSource end\n");
    }
    player_context.mPreparedFlag = 0;
    if (TPlayerPrepareAsync(player_context.mTPlayer) != 0) {
        printf("TPlayerPrepareAsync() return fail.\n");
        return -1;
    } else {
        printf("prepare\n");
    }
#if 0
	sem_wait(&player_context.mPreparedSem);
#else //tplayer demo
    waitErr = semTimedWait(&player_context.mPreparedSem, 10 * 1000);
    if (waitErr == -1) {
        printf("prepare fail\n");
        return -1;
    }
#endif
    printf("prepared ok\n");
    return 0;
}

int tplayer_play_url(const char *path) {
    int waitErr = 0;

    if (TPlayerReset(player_context.mTPlayer) != 0) {
        printf("TPlayerReset() return fail.\n");
        return -1;
    } else {
        printf("reset the player ok.\n");
        //zwh add from tplayerdemo
        if (player_context.mError == 1) {
            player_context.mError = 0;
        }
        //PowerManagerReleaseWakeLock("tplayerdemo");
    }
    // zwh add from tplayerdemo
    player_context.mSeekable = 1; //* if the media source is not seekable, this flag will be
    //* clear at the TINA_NOTIFY_NOT_SEEKABLE callback.
    //* set url to the tinaplayer.
    if (TPlayerSetDataSource(player_context.mTPlayer, path, NULL) != 0) {
        printf("TPlayerSetDataSource return fail.\n");
        return -1;
    } else {
        printf("setDataSource end\n");
    }

    player_context.mPreparedFlag = 0;
    if (TPlayerPrepareAsync(player_context.mTPlayer) != 0) {
        printf("TPlayerPrepareAsync() return fail.\n");
    } else {
        printf("preparing...\n");
    }
#if 0
	sem_wait(&player_context.mPreparedSem);
#else //tplayer demo
    waitErr = semTimedWait(&player_context.mPreparedSem, 10 * 1000);  //
    if (waitErr == -1) {
        printf("prepare fail\n");
        return -1;
    }
#endif
    printf("prepared ok\n");

    /* printf("TPlayerSetHoldLastPicture()\n"); */
    /* TPlayerSetHoldLastPicture(player_context.mTPlayer, 1); */
    return 0;
}

int tplayer_play(void) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }
    if (TPlayerIsPlaying(player_context.mTPlayer)) {
        printf("already palying!\n");
        return -1;
    }
    player_context.mCompleteFlag = 0;
    return TPlayerStart(player_context.mTPlayer);
}

int tplayer_pause(void) {
    ISNULL(player_context.mTPlayer);
    if (!TPlayerIsPlaying(player_context.mTPlayer)) {
        printf("not playing!\n");
        return -1;
    }
    return TPlayerPause(player_context.mTPlayer);
}

int tplayer_seekto(int nSeekTimeMs) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }

    /*
     if(TPlayerIsPlaying(player_context.mTPlayer)){
     printf("seekto can not at palying state!\n");
     return -1;
     }
     */
    return TPlayerSeekTo(player_context.mTPlayer, nSeekTimeMs);
}

int tplayer_stop(void) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }
    //zwh add
    if (!TPlayerIsPlaying(player_context.mTPlayer)) {
        printf("not playing!\n");
        return -1;
    }

    return TPlayerStop(player_context.mTPlayer);
}

int tplayer_setvolumn(int volumn) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }
    return TPlayerSetVolume(player_context.mTPlayer, volumn);
}

int tplayer_getvolumn(void) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }
    return TPlayerGetVolume(player_context.mTPlayer);
}

int tplayer_setlooping(bool bLoop) {
    ISNULL(player_context.mTPlayer);
    return TPlayerSetLooping(player_context.mTPlayer, bLoop);
}

int tplayer_setscaledown(TplayerVideoScaleDownType nHorizonScaleDown,
        TplayerVideoScaleDownType nVerticalScaleDown) {
    ISNULL(player_context.mTPlayer);
    return TPlayerSetScaleDownRatio(player_context.mTPlayer, nHorizonScaleDown,
            nVerticalScaleDown);
}

int tplayer_setdisplayrect(int x, int y, unsigned int width,
        unsigned int height) {
    ISNULL(player_context.mTPlayer);
    TPlayerSetDisplayRect(player_context.mTPlayer, x, y, width, height);

    return 0;
}

int tplayer_setrotate(TplayerVideoRotateType rotateDegree) {
    ISNULL(player_context.mTPlayer);
    return TPlayerSetRotate(player_context.mTPlayer, rotateDegree);
}

MediaInfo * tplayer_getmediainfo(void) {
    return TPlayerGetMediaInfo(player_context.mTPlayer);
}

int tplayer_getduration(int* msec) {
    ISNULL(player_context.mTPlayer);
    return TPlayerGetDuration(player_context.mTPlayer, msec);
}

int tplayer_getcurrentpos(int* msec) {
    ISNULL(player_context.mTPlayer);
    return TPlayerGetCurrentPosition(player_context.mTPlayer, msec);
}

int tplayer_getcompletestate(void) {
    return player_context.mCompleteFlag;
}

int tplayer_videodisplayenable(int enable) {
    TPlayerSetVideoDisplay(player_context.mTPlayer, enable);
    return 0;
}

int tplayer_setsrcrect(int x, int y, unsigned int width, unsigned int height) {
    TPlayerSetSrcRect(player_context.mTPlayer, x, y, width, height);
    return 0;
}

int tplayer_setbrightness(unsigned int grade) {
    TPlayerSetBrightness(player_context.mTPlayer, grade);
    return 0;
}

int tplayer_setcontrast(unsigned int grade) {
    TPlayerSetContrast(player_context.mTPlayer, grade);
    return 0;
}

int tplayer_sethue(unsigned int grade) {
    TPlayerSetHue(player_context.mTPlayer, grade);
    return 0;
}

int tplayer_setsaturation(unsigned int grade) {
    TPlayerSetSaturation(player_context.mTPlayer, grade);
    return 0;
}

int tplayer_getplaying(void) {
    ISNULL(player_context.mTPlayer);
    return TPlayerIsPlaying(player_context.mTPlayer);
}

static void* MusicPlayProc(void *arg) {
    sm_debug("music play thread create\n");
    tplayer_play_url(media_list->current_node->path);
    tplayer_play();

    return NULL;
}

void StartMusicPlay(void) {
    sm_debug("start music play\n");
    pthread_t thread_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread_id, &attr, MusicPlayProc, NULL);
    pthread_attr_destroy(&attr);
}

void GetPlayTime(char* curTime, char* totalTime) {
    int ret, curDuration, totalDuration, timeMin, timeSec;
    double sec;

    ret = tplayer_getduration(&totalDuration);

    if (!ret) {
        timeMin = totalDuration / 1000 / 60;
        sec = (double) totalDuration / 1000 / 60 - timeMin;
        timeSec = sec * 60;
        sprintf(totalTime, "%02d:%02d", timeMin, timeSec);
    } else {
        strcpy(totalTime, "00:00");
    }

    ret = tplayer_getcurrentpos(&curDuration);
    if (!ret) {
        timeMin = curDuration / 1000 / 60;
        sec = (double) curDuration / 1000 / 60 - timeMin;
        timeSec = sec * 60;
        sprintf(curTime, "%02d:%02d", timeMin, timeSec);
    } else {
        strcpy(curTime, "00:00");
    }
}

void GetPlayTimePercent(double* percent) {
    int curDuration, totalDuration;
    tplayer_getduration(&totalDuration);
    tplayer_getcurrentpos(&curDuration);
    *percent = (double) curDuration / totalDuration;
}

