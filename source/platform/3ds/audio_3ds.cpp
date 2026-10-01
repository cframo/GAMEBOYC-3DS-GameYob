#ifdef BACKEND_3DS

#include <algorithm>
#include <atomic>
#include <string.h>

#include <3ds.h>

#include "platform/common/manager.h"
#include "platform/audio.h"

#define RING_SAMPLES 8192
#define RING_MASK (RING_SAMPLES - 1)

#define NDSP_BUFFER_SAMPLES 1024
#define NDSP_NUM_BUFFERS 3

static bool ndspInitialized = false;
static bool initialized = false;

static std::atomic<size_t> ringHead{0};
static std::atomic<size_t> ringTail{0};

static u32* audioLinearMem = nullptr;
static u32* ringBuffer = nullptr;
static u32* ndspBuffers = nullptr;

static ndspWaveBuf waveBuf[NDSP_NUM_BUFFERS];

static s16 lastSampleL = 0;
static s16 lastSampleR = 0;
static const float PREAMP_GAIN = 1.35f;

static inline s16 amplifySample(s16 sample, float gain) {
    int val = (int) (sample * gain);
    if(val > 32767) {
        return 32767;
    }
    if(val < -32768) {
        return -32768;
    }
    return (s16) val;
}

static void refillWaveBuf(ndspWaveBuf* buf) {
    size_t head = ringHead.load(std::memory_order_acquire);
    size_t tail = ringTail.load(std::memory_order_relaxed);

    size_t available = head - tail;
    size_t toCopy = available;
    if(toCopy > NDSP_BUFFER_SAMPLES) {
        toCopy = NDSP_BUFFER_SAMPLES;
    }

    u32* dst = (u32*) buf->data_vaddr;

    for(size_t i = 0; i < toCopy; i++) {
        u32 raw = ringBuffer[(tail + i) & RING_MASK];
        s16 sampleL = (s16) (raw & 0xFFFF);
        s16 sampleR = (s16) (raw >> 16);

        sampleL = amplifySample(sampleL, PREAMP_GAIN);
        sampleR = amplifySample(sampleR, PREAMP_GAIN);

        dst[i] = (u32) ((u16) sampleL | ((u32) (u16) sampleR << 16));
    }

    if(toCopy > 0) {
        ringTail.store(tail + toCopy, std::memory_order_release);
        u32 lastSample = dst[toCopy - 1];
        lastSampleL = (s16) (lastSample & 0xFFFF);
        lastSampleR = (s16) (lastSample >> 16);
    }

    // Underrun soft decay: si faltan muestras para completar las 1024,
    // atenuar suavemente hacia 0 para prevenir chasquidos (audio pops).
    for(size_t i = toCopy; i < NDSP_BUFFER_SAMPLES; i++) {
        lastSampleL = (s16) ((lastSampleL * 31) / 32);
        lastSampleR = (s16) ((lastSampleR * 31) / 32);
        dst[i] = (u32) ((u16) lastSampleL | ((u32) (u16) lastSampleR << 16));
    }

    DSP_FlushDataCache(buf->data_vaddr, NDSP_BUFFER_SAMPLES * sizeof(u32));
    ndspChnWaveBufAdd(0, buf);
}

static void ndspAudioCallback(void* data) {
    if(!initialized) {
        return;
    }

    for(int i = 0; i < NDSP_NUM_BUFFERS; i++) {
        if(waveBuf[i].status == NDSP_WBUF_DONE || waveBuf[i].status == NDSP_WBUF_FREE) {
            refillWaveBuf(&waveBuf[i]);
        }
    }
}

void audioInit() {
    if(R_FAILED(ndspInit())) {
        return;
    }

    ndspInitialized = true;

    u32 totalSamples = RING_SAMPLES + (NDSP_NUM_BUFFERS * NDSP_BUFFER_SAMPLES);
    audioLinearMem = (u32*) linearAlloc(totalSamples * sizeof(u32));
    if(audioLinearMem == nullptr) {
        audioCleanup();
        return;
    }

    memset(audioLinearMem, 0, totalSamples * sizeof(u32));

    ringBuffer = audioLinearMem;
    ndspBuffers = &audioLinearMem[RING_SAMPLES];

    ringHead.store(0, std::memory_order_relaxed);
    ringTail.store(0, std::memory_order_relaxed);
    lastSampleL = 0;
    lastSampleR = 0;

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, audioGetSampleRate());
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ndspChnSetMix(0, mix);

    memset(waveBuf, 0, sizeof(waveBuf));
    for(int i = 0; i < NDSP_NUM_BUFFERS; i++) {
        waveBuf[i].data_vaddr = &ndspBuffers[i * NDSP_BUFFER_SAMPLES];
        waveBuf[i].nsamples = NDSP_BUFFER_SAMPLES;
        waveBuf[i].status = NDSP_WBUF_FREE;
    }

    initialized = true;

    // Encolar los 3 buffers iniciales con silencio para arrancar el pipeline del DSP
    for(int i = 0; i < NDSP_NUM_BUFFERS; i++) {
        DSP_FlushDataCache(waveBuf[i].data_vaddr, NDSP_BUFFER_SAMPLES * sizeof(u32));
        ndspChnWaveBufAdd(0, &waveBuf[i]);
    }

    ndspSetCallback(ndspAudioCallback, nullptr);
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

    if(audioLinearMem != nullptr) {
        linearFree(audioLinearMem);
        audioLinearMem = nullptr;
        ringBuffer = nullptr;
        ndspBuffers = nullptr;
    }
}

u32 audioGetSampleRate() {
    return 44100;
}

void audioClear() {
    if(!initialized) {
        return;
    }

    // Vaciar el ring buffer atomicamente
    ringTail.store(ringHead.load(std::memory_order_relaxed), std::memory_order_release);
    lastSampleL = 0;
    lastSampleR = 0;
}

void audioPlay(u32* buffer, long samples) {
    if(!initialized || buffer == nullptr || samples <= 0) {
        return;
    }

    size_t head = ringHead.load(std::memory_order_relaxed);
    size_t tail = ringTail.load(std::memory_order_acquire);

    size_t occupied = head - tail;
    size_t freeSpace = (occupied < RING_SAMPLES) ? (RING_SAMPLES - occupied) : 0;

    size_t toWrite = (size_t) samples;
    if(toWrite > freeSpace) {
        toWrite = freeSpace;
    }

    for(size_t i = 0; i < toWrite; i++) {
        ringBuffer[(head + i) & RING_MASK] = buffer[i];
    }

    if(toWrite > 0) {
        ringHead.store(head + toWrite, std::memory_order_release);
    }
}

#endif