#ifdef BACKEND_3DS

#include <sys/unistd.h>
#include <string.h>

#include <3ds.h>

#include "platform/common/manager.h"
#include "platform/audio.h"

#define BUFFER_SAMPLES 768
#define NUM_BUFFERS 3

static bool ndspInitialized = false;
static bool initialized = false;

static ndspWaveBuf waveBuf[NUM_BUFFERS];
static u32* audioBuffer;

static u32 currBuffer;
static u32 currPos;
static float currentRate = 44140.0f;

void audioInit() {
    if(R_FAILED(ndspInit())) {
        return;
    }

    ndspInitialized = true;

    u32 bufSize = BUFFER_SAMPLES * NUM_BUFFERS * sizeof(u32);
    audioBuffer = (u32*) linearAlloc(bufSize);
    if(audioBuffer == nullptr) {
        audioCleanup();
        return;
    }

    memset(audioBuffer, 0, bufSize);

    currBuffer = 0;
    currPos = 0;
    currentRate = 44140.0f;

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, currentRate);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ndspChnSetMix(0, mix);

    memset(waveBuf, 0, sizeof(waveBuf));
    for(int i = 0; i < NUM_BUFFERS; i++) {
        waveBuf[i].data_vaddr = &audioBuffer[BUFFER_SAMPLES * i];
        waveBuf[i].nsamples = BUFFER_SAMPLES;
        waveBuf[i].status = NDSP_WBUF_FREE;
    }

    // Pre-encolar unicamente 1 bufer de silencio como colchon inicial
    DSP_FlushDataCache(waveBuf[0].data_vaddr, BUFFER_SAMPLES * sizeof(u32));
    ndspChnWaveBufAdd(0, &waveBuf[0]);

    currBuffer = 1;
    currPos = 0;

    initialized = true;
}

void audioCleanup() {
    if(!ndspInitialized) {
        return;
    }

    initialized = false;

    ndspSetCallback(nullptr, nullptr);
    ndspChnReset(0);
    ndspExit();
    ndspInitialized = false;

    if(audioBuffer != nullptr) {
        linearFree(audioBuffer);
        audioBuffer = nullptr;
    }
}

u32 audioGetSampleRate() {
    return 44100;
}

void audioClear() {
    if(!initialized) {
        return;
    }

    ndspChnWaveBufClear(0);
    for(int i = 0; i < NUM_BUFFERS; i++) {
        waveBuf[i].status = NDSP_WBUF_FREE;
    }

    currBuffer = 0;
    currPos = 0;
}

void audioPlay(u32* buffer, long samples) {
    if(!initialized) {
        return;
    }

    long remaining = samples;
    while(remaining > 0) {
        ndspWaveBuf* buf = &waveBuf[currBuffer];

        if(buf->status != NDSP_WBUF_DONE && buf->status != NDSP_WBUF_FREE) {
            if(mgrGetFastForward()) {
                audioClear();
                buf = &waveBuf[currBuffer];
            } else {
                // Buffer saturado: no bloquear con usleep para proteger el ritmo de VSync
                return;
            }
        }

        long currSamples = remaining;
        if((u32) currSamples > buf->nsamples - currPos) {
            currSamples = buf->nsamples - currPos;
        }

        memcpy(&((u32*) buf->data_vaddr)[currPos], &buffer[samples - remaining], (size_t) currSamples * sizeof(u32));

        currPos += currSamples;
        remaining -= currSamples;

        if(currPos >= buf->nsamples) {
            DSP_FlushDataCache(buf->data_vaddr, buf->nsamples * sizeof(u32));

            // Dynamic Rate Control (DRC): inspecciona bufers encolados/activos antes de despachar
            int activeBuffers = 0;
            for(int i = 0; i < NUM_BUFFERS; i++) {
                if(waveBuf[i].status == NDSP_WBUF_PLAYING || waveBuf[i].status == NDSP_WBUF_QUEUED) {
                    activeBuffers++;
                }
            }

            float targetRate = 44140.0f;
            if(activeBuffers >= 2) {
                targetRate = 44180.0f;
            } else if(activeBuffers <= 0) {
                targetRate = 44100.0f;
            }

            if(targetRate != currentRate) {
                currentRate = targetRate;
                ndspChnSetRate(0, currentRate);
            }

            ndspChnWaveBufAdd(0, buf);

            currPos -= buf->nsamples;
            currBuffer = (currBuffer + 1) % NUM_BUFFERS;
        }
    }
}

#endif