#include "audio_engine.h"

#include <stdio.h>
#include <string.h>

#ifndef __3DS__
#include <mutex>
#endif

#include <SDL.h>

#ifdef __3DS__
#include <3ds.h>

namespace fallout {
SDL_AudioDeviceID ctrAudioDevice();
}

// Per-buffer guard on 3DS: holds SDL's audio device lock, the same lock the
// mixer callback runs under. It is recursive, so nested guards on the same
// thread are fine. (A plain LightLock deadlocked here because guarded code
// nests; barriers alone gave no exclusion at all.)
struct LightLockGuard {
    SDL_AudioDeviceID id;
    LightLockGuard(LightLock*) : id(fallout::ctrAudioDevice()) {
        if (id != (SDL_AudioDeviceID)-1) SDL_LockAudioDevice(id);
    }
    ~LightLockGuard() {
        if (id != (SDL_AudioDeviceID)-1) SDL_UnlockAudioDevice(id);
    }
};
#endif

namespace fallout {

// Holds SDL's audio device lock, which the mixer callback runs under, so a
// sound buffer can't be freed while the callback is reading it. The
// per-buffer guards on 3DS don't actually exclude anything (see above).
struct AudioDeviceLockGuard {
    SDL_AudioDeviceID id;
    explicit AudioDeviceLockGuard(SDL_AudioDeviceID _id) : id(_id) { SDL_LockAudioDevice(id); }
    ~AudioDeviceLockGuard() { SDL_UnlockAudioDevice(id); }
};

#define AUDIO_ENGINE_SOUND_BUFFERS 8

struct AudioEngineSoundBuffer {
    bool active;
    unsigned int size;
    int bitsPerSample;
    int channels;
    int rate;
    void* data;
    int volume;
    bool playing;
    bool looping;
    unsigned int pos;
    SDL_AudioStream* stream;
#ifdef __3DS__
    LightLock mutex;
#else
    std::recursive_mutex mutex;
#endif
};

extern bool gProgramIsActive;

static bool soundBufferIsValid(int soundBufferIndex);
static void audioEngineMixin(void* userData, Uint8* stream, int length);

static SDL_AudioSpec gAudioEngineSpec;
static SDL_AudioDeviceID gAudioEngineDeviceId = -1;

#ifdef __3DS__
SDL_AudioDeviceID ctrAudioDevice()
{
    return gAudioEngineDeviceId;
}
#endif
static AudioEngineSoundBuffer gAudioEngineSoundBuffers[AUDIO_ENGINE_SOUND_BUFFERS];

static bool audioEngineIsInitialized()
{
    return gAudioEngineDeviceId != -1;
}

static bool soundBufferIsValid(int soundBufferIndex)
{
    return soundBufferIndex >= 0 && soundBufferIndex < AUDIO_ENGINE_SOUND_BUFFERS;
}

static void audioEngineMixin(void* userData, Uint8* stream, int length)
{
    memset(stream, gAudioEngineSpec.silence, length);

    if (!gProgramIsActive) {
        return;
    }

    for (int index = 0; index < AUDIO_ENGINE_SOUND_BUFFERS; index++) {
        AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[index]);
#ifdef __3DS__
        LightLockGuard lockGuard(&soundBuffer->mutex);
#else
        std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

        if (soundBuffer->active && soundBuffer->playing && soundBuffer->data != NULL && soundBuffer->stream != NULL) {
            int srcFrameSize = soundBuffer->bitsPerSample / 8 * soundBuffer->channels;

            unsigned char buffer[1024];
            int pos = 0;
            while (pos < length) {
                int remaining = length - pos;
                if (remaining > (int)sizeof(buffer)) {
                    remaining = sizeof(buffer);
                }

                // TODO: Make something better than frame-by-frame convertion.
                if (srcFrameSize <= 0 || soundBuffer->pos + srcFrameSize > soundBuffer->size) {
                    if (soundBuffer->looping && soundBuffer->size >= (unsigned int)srcFrameSize && srcFrameSize > 0) {
                        soundBuffer->pos = 0;
                    } else {
                        soundBuffer->playing = false;
                        break;
                    }
                }
                SDL_AudioStreamPut(soundBuffer->stream, (unsigned char*)soundBuffer->data + soundBuffer->pos, srcFrameSize);
                soundBuffer->pos += srcFrameSize;

                int bytesRead = SDL_AudioStreamGet(soundBuffer->stream, buffer, remaining);
                if (bytesRead == -1) {
                    break;
                }

                SDL_MixAudioFormat(stream + pos, buffer, gAudioEngineSpec.format, bytesRead, soundBuffer->volume);

                if (soundBuffer->pos >= soundBuffer->size) {
                    if (soundBuffer->looping) {
                        soundBuffer->pos %= soundBuffer->size;
                    } else {
                        soundBuffer->playing = false;
                        break;
                    }
                }

                pos += bytesRead;
            }
        }
    }
}

#if defined(__3DS__) && defined(CTR_AUDIO_STRESS)
// Test-only: hammer create/play/release from a second thread while the mixer runs.
static volatile unsigned int gStressIters = 0;
static void audioStressThread(void*)
{
    unsigned int seed = 12345;
    for (;;) {
        seed = seed * 1103515245 + 12345;
        unsigned int size = 2048 + (seed >> 8) % 40000;
        size &= ~3u;
        int idx = audioEngineCreateSoundBuffer(size, 16, 2, 22050);
        if (idx != -1) {
            void* p1; unsigned int b1; void* p2; unsigned int b2;
            if (audioEngineSoundBufferLock(idx, 0, size, &p1, &b1, &p2, &b2, 0)) {
                memset(p1, 0, b1);
                audioEngineSoundBufferUnlock(idx, p1, b1, p2, b2);
            }
            audioEngineSoundBufferPlay(idx, (seed & 1) ? 1 : 0);
            svcSleepThread((long long)((seed >> 4) % 300) * 1000LL);
            audioEngineSoundBufferRelease(idx);
        } else {
            svcSleepThread(1000000LL);
        }
        gStressIters++;
        if ((gStressIters % 1000) == 0) {
            FILE* fp = fopen("sdmc:/3ds/audio_stress.txt", "w");
            if (fp) { fprintf(fp, "%u\n", gStressIters); fclose(fp); }
        }
    }
}
#endif

bool audioEngineInit()
{
#ifdef __3DS__
    if (SDL_Init(SDL_INIT_AUDIO) == -1) {
        return false;
    }

    for (int i = 0; i < AUDIO_ENGINE_SOUND_BUFFERS; i++) {
        LightLock_Init(&gAudioEngineSoundBuffers[i].mutex);
    }
#endif

    SDL_AudioSpec desiredSpec;
    desiredSpec.freq = 22050;
    desiredSpec.format = AUDIO_S16;
    desiredSpec.channels = 2;
    desiredSpec.samples = 1024;
    desiredSpec.callback = audioEngineMixin;

    gAudioEngineDeviceId = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &gAudioEngineSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    // SDL_OpenAudioDevice returns 0 on failure (not -1). Without this check a
    // failed audio open (e.g. 3DS without sdmc:/3ds/dspfirm.cdc) looks
    // "initialized", sound buffers never advance, and every movie hangs
    // forever in _MVE_sndSync.
    if (gAudioEngineDeviceId == 0 || gAudioEngineDeviceId == (SDL_AudioDeviceID)-1) {
        gAudioEngineDeviceId = -1;
        return false;
    }

    SDL_PauseAudioDevice(gAudioEngineDeviceId, 0);

#if defined(__3DS__) && defined(CTR_AUDIO_STRESS)
    {
        Thread th = threadCreate(audioStressThread, NULL, 16 * 1024, 0x2F, -2, true);
        FILE* fp = fopen("sdmc:/3ds/audio_stress.txt", "w");
        if (fp) { fprintf(fp, "start %p\n", (void*)th); fclose(fp); }
    }
#endif

    return true;
}

void audioEngineExit()
{
    if (audioEngineIsInitialized()) {
        SDL_CloseAudioDevice(gAudioEngineDeviceId);
        gAudioEngineDeviceId = -1;
    }

#ifdef __3DS__
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
#endif
}

void audioEnginePause()
{
    if (audioEngineIsInitialized()) {
        SDL_PauseAudioDevice(gAudioEngineDeviceId, 1);
    }
}

void audioEngineResume()
{
    if (audioEngineIsInitialized()) {
        SDL_PauseAudioDevice(gAudioEngineDeviceId, 0);
    }
}

int audioEngineCreateSoundBuffer(unsigned int size, int bitsPerSample, int channels, int rate)
{
    if (!audioEngineIsInitialized()) {
        return -1;
    }

    for (int index = 0; index < AUDIO_ENGINE_SOUND_BUFFERS; index++) {
        AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[index]);
#ifdef __3DS__
        LightLockGuard lockGuard(&soundBuffer->mutex);
#else
        std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

        if (!soundBuffer->active) {
            soundBuffer->active = true;
            soundBuffer->size = size;
            soundBuffer->bitsPerSample = bitsPerSample;
            soundBuffer->channels = channels;
            soundBuffer->rate = rate;
            soundBuffer->volume = SDL_MIX_MAXVOLUME;
            soundBuffer->playing = false;
            soundBuffer->looping = false;
            soundBuffer->pos = 0;
            soundBuffer->data = malloc(size);
            soundBuffer->stream = SDL_NewAudioStream(bitsPerSample == 16 ? AUDIO_S16 : AUDIO_S8, channels, rate, gAudioEngineSpec.format, gAudioEngineSpec.channels, gAudioEngineSpec.freq);
            // Out of memory (the 3DS heap is small): no sound rather than a
            // crash in the mixer reading from a NULL buffer.
            if (soundBuffer->data == NULL || soundBuffer->stream == NULL) {
                free(soundBuffer->data);
                soundBuffer->data = NULL;
                if (soundBuffer->stream != NULL) {
                    SDL_FreeAudioStream(soundBuffer->stream);
                    soundBuffer->stream = NULL;
                }
                soundBuffer->active = false;
                return -1;
            }
            return index;
        }
    }

    return -1;
}

bool audioEngineSoundBufferRelease(int soundBufferIndex)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioDeviceLockGuard deviceLock(gAudioEngineDeviceId);
    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    soundBuffer->active = false;

    free(soundBuffer->data);
    soundBuffer->data = nullptr;

    SDL_FreeAudioStream(soundBuffer->stream);
    soundBuffer->stream = nullptr;

    return true;
}

bool audioEngineSoundBufferSetVolume(int soundBufferIndex, int volume)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    soundBuffer->volume = volume;

    return true;
}

bool audioEngineSoundBufferGetVolume(int soundBufferIndex, int* volumePtr)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    *volumePtr = soundBuffer->volume;

    return true;
}

bool audioEngineSoundBufferSetPan(int soundBufferIndex, int pan)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    // NOTE: Audio engine does not support sound panning. I'm not sure it's
    // even needed. For now this value is silently ignored.

    return true;
}

bool audioEngineSoundBufferPlay(int soundBufferIndex, unsigned int flags)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    soundBuffer->playing = true;

    if ((flags & AUDIO_ENGINE_SOUND_BUFFER_PLAY_LOOPING) != 0) {
        soundBuffer->looping = true;
    }

    return true;
}

bool audioEngineSoundBufferStop(int soundBufferIndex)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    soundBuffer->playing = false;

    return true;
}

bool audioEngineSoundBufferGetCurrentPosition(int soundBufferIndex, unsigned int* readPosPtr, unsigned int* writePosPtr)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    if (readPosPtr != nullptr) {
        *readPosPtr = soundBuffer->pos;
    }

    if (writePosPtr != nullptr) {
        *writePosPtr = soundBuffer->pos;

        if (soundBuffer->playing) {
            // 15 ms lead
            // See: https://docs.microsoft.com/en-us/previous-versions/windows/desktop/mt708925(v=vs.85)#remarks
            *writePosPtr += soundBuffer->rate / 150;
            *writePosPtr %= soundBuffer->size;
        }
    }

    return true;
}

bool audioEngineSoundBufferSetCurrentPosition(int soundBufferIndex, unsigned int pos)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    soundBuffer->pos = pos % soundBuffer->size;

    return true;
}

bool audioEngineSoundBufferLock(int soundBufferIndex, unsigned int writePos, unsigned int writeBytes, void** audioPtr1, unsigned int* audioBytes1, void** audioPtr2, unsigned int* audioBytes2, unsigned int flags)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    if (audioBytes1 == nullptr) {
        return false;
    }

    if ((flags & AUDIO_ENGINE_SOUND_BUFFER_LOCK_FROM_WRITE_POS) != 0) {
        if (!audioEngineSoundBufferGetCurrentPosition(soundBufferIndex, nullptr, &writePos)) {
            return false;
        }
    }

    if ((flags & AUDIO_ENGINE_SOUND_BUFFER_LOCK_ENTIRE_BUFFER) != 0) {
        writeBytes = soundBuffer->size;
    }

    if (writePos + writeBytes <= soundBuffer->size) {
        *(unsigned char**)audioPtr1 = (unsigned char*)soundBuffer->data + writePos;
        *audioBytes1 = writeBytes;

        if (audioPtr2 != nullptr) {
            *audioPtr2 = nullptr;
        }

        if (audioBytes2 != nullptr) {
            *audioBytes2 = 0;
        }
    } else {
        unsigned int remainder = writePos + writeBytes - soundBuffer->size;
        *(unsigned char**)audioPtr1 = (unsigned char*)soundBuffer->data + writePos;
        *audioBytes1 = soundBuffer->size - writePos;

        if (audioPtr2 != nullptr) {
            *(unsigned char**)audioPtr2 = (unsigned char*)soundBuffer->data;
        }

        if (audioBytes2 != nullptr) {
            *audioBytes2 = writeBytes - (soundBuffer->size - writePos);
        }
    }

    // TODO: Mark range as locked.

    return true;
}

bool audioEngineSoundBufferUnlock(int soundBufferIndex, void* audioPtr1, unsigned int audioBytes1, void* audioPtr2, unsigned int audioBytes2)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    // TODO: Mark range as unlocked.

    return true;
}

bool audioEngineSoundBufferGetStatus(int soundBufferIndex, unsigned int* statusPtr)
{
    if (!audioEngineIsInitialized()) {
        return false;
    }

    if (!soundBufferIsValid(soundBufferIndex)) {
        return false;
    }

    AudioEngineSoundBuffer* soundBuffer = &(gAudioEngineSoundBuffers[soundBufferIndex]);
#ifdef __3DS__
    LightLockGuard lockGuard(&soundBuffer->mutex);
#else
    std::lock_guard<std::recursive_mutex> lock(soundBuffer->mutex);
#endif

    if (!soundBuffer->active) {
        return false;
    }

    if (statusPtr == nullptr) {
        return false;
    }

    *statusPtr = 0;

    if (soundBuffer->playing) {
        *statusPtr |= AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING;

        if (soundBuffer->looping) {
            *statusPtr |= AUDIO_ENGINE_SOUND_BUFFER_STATUS_LOOPING;
        }
    }

    return true;
}

} // namespace fallout
