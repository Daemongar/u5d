#include "common/common.h"

#include "audio.h"
#include "aud_ops.h"
#include "sfx_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// TODO: clean up

#include <SDL3_mixer/SDL_mixer.h>

static MIX_Mixer* s_mixer;

static MIX_Track* s_bgmTrack;
static MIX_Track* s_sfxTrack;

static SDL_PropertiesID s_bgmTrackProp;

static SDL_AudioSpec s_mixerSpec;

static MIX_Audio* s_bgm[20];
static MIX_Audio* s_sfx[256];

static int s_currentBgmId;
static int s_queuedBgmId;

// BUGFIX: PC-speaker-style synth playback. The original DOS driver dispatches
// sound effects as (freq, delay, dur, pulseWidth, pulseInc)-style parameters
// meant for direct speaker-timer synthesis (see src/audio/aud_sfx.c), but
// neither the DOS PC-speaker driver (aud_spk.c) nor this SDL backend ever
// implemented that synthesis - both were left as no-ops, so no sound played
// for anything not covered by a sampled SFX/*.wav file (which don't exist
// here). This renders square-wave/noise/sweep tones from those parameters
// in software instead.
#define SYNTH_SAMPLE_RATE 44100
#define SYNTH_MAX_SECONDS 4.0

static MIX_Audio* s_synthAudio;

static void AUDIO_SDL_FadeEdges(Sint16* samples, int numSamples)
{
    int fade = numSamples / 40;
    if (fade < 1)
        return;
    if (fade > numSamples / 2)
        fade = numSamples / 2;

    for (int i = 0; i < fade; i++)
    {
        double gain = (double)i / fade;
        samples[i] = (Sint16)(samples[i] * gain);
        samples[numSamples - 1 - i] = (Sint16)(samples[numSamples - 1 - i] * gain);
    }
}

static void AUDIO_SDL_PlaySynthBuffer(Sint16* samples, int numSamples)
{
    if (numSamples <= 0)
        return;

    AUDIO_SDL_FadeEdges(samples, numSamples);

    if (s_synthAudio)
    {
        MIX_DestroyAudio(s_synthAudio);
        s_synthAudio = NULL;
    }

    SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, SYNTH_SAMPLE_RATE };
    s_synthAudio = MIX_LoadRawAudio(s_mixer, samples, (size_t)numSamples * sizeof(Sint16), &spec);
    if (!s_synthAudio)
        return;

    MIX_SetTrackAudio(s_sfxTrack, s_synthAudio);
    MIX_PlayTrack(s_sfxTrack, 0);
}

void AUDIO_SDL_LoadBgmTable(void);
void AUDIO_SDL_LoadSfxTable(void);
void AUDIO_SDL_PlayBgmSub(int id);
void SDLCALL AUDIO_SDL_OnBgmStopped(void* userdata, MIX_Track* track);

void AUDIO_SDL_Init(void)
{
    MIX_Init();
    s_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    MIX_GetMixerFormat(s_mixer, &s_mixerSpec);

    s_bgmTrack = MIX_CreateTrack(s_mixer);
    s_sfxTrack = MIX_CreateTrack(s_mixer);

    MIX_SetTrackGain(s_bgmTrack, 0.3f);
    MIX_SetTrackGain(s_sfxTrack, 0.2f);

    s_bgmTrackProp = SDL_CreateProperties();
    SDL_SetNumberProperty(s_bgmTrackProp, MIX_PROP_PLAY_LOOPS_NUMBER, -1);

    MIX_SetTrackStoppedCallback(s_bgmTrack, AUDIO_SDL_OnBgmStopped, NULL);

    AUDIO_SDL_LoadBgmTable();
    AUDIO_SDL_LoadSfxTable();
}

void AUDIO_SDL_Cleanup(void)
{
    // TODO: release audios

    if (s_synthAudio)
    {
        MIX_DestroyAudio(s_synthAudio);
        s_synthAudio = NULL;
    }

    MIX_SetTrackStoppedCallback(s_bgmTrack, NULL, NULL);
    MIX_DestroyTrack(s_bgmTrack);
    MIX_DestroyTrack(s_sfxTrack);
    MIX_DestroyMixer(s_mixer);
    s_mixer = NULL;
    MIX_Quit();
}

void AUDIO_SDL_LoadBgmTable(void)
{
    for (int i = 1; i <= 15; i++)
    {
        char fileName[256] = {0,};
        sprintf(fileName, "BGM/%02d.ogg", i);
        s_bgm[i] = MIX_LoadAudio(s_mixer, fileName, false);

        const char* err = SDL_GetError();
        if (err)
        {
            puts(err);
        }
    }
}

void AUDIO_SDL_LoadSfxTable(void)
{
    for (int i = 0; i < SFX_RULE_COUNT; i++)
    {
        const SfxRule* r = &g_sfxRules[i];
        char fileName[256] = {0,};
        if (s_sfx[r->sfxId] != NULL)
        {
            continue;
        }
        sprintf(fileName, "SFX/%s", r->fileName);
        s_sfx[r->sfxId] = MIX_LoadAudio(s_mixer, fileName, true);
    }
}

void AUDIO_SDL_PlaySfx(int id)
{
    debug("AUDIO_SDL_PlaySfx(%d)", id);

    if (s_sfx[id] == NULL)
        return;

    MIX_SetTrackAudio(s_sfxTrack, s_sfx[id]);
    MIX_PlayTrack(s_sfxTrack, 0);
    const char* err = SDL_GetError();
    if (err) { puts(err); }
}

void AUDIO_SDL_StopSfx(void)
{
    Sint64 frames = MIX_MSToFrames(s_mixerSpec.freq, 10);
    MIX_StopTrack(s_sfxTrack, frames);
    const char* err = SDL_GetError();
    if (err) { puts(err); }
}

void AUDIO_SDL_PlayBgmSub(int id)
{
    s_currentBgmId = id;

    if (s_bgm[id] == NULL)
        return;

    MIX_SetTrackAudio(s_bgmTrack, s_bgm[id]);
    MIX_PlayTrack(s_bgmTrack, s_bgmTrackProp);
    const char* err = SDL_GetError();
    if (err) { puts(err); }
}

void AUDIO_SDL_PlayBgm(int id)
{
    debug("AUDIO_SDL_PlayBgm(%d)", id);

    s_queuedBgmId = 0;

    if (s_currentBgmId == id)
    {
        MIX_SetTrackLoops(s_bgmTrack, -1);
        return;
    }

    AUDIO_SDL_PlayBgmSub(id);
}

void AUDIO_SDL_QueueBgm(int id)
{
    debug("AUDIO_SDL_QueueBgm(%d)", id);

    if (s_bgm[id] == NULL)
        return;

    if (s_currentBgmId == 0 || !MIX_TrackPlaying(s_bgmTrack))
    {
        s_queuedBgmId = 0;
        AUDIO_SDL_PlayBgmSub(id);
        return;
    }

    s_queuedBgmId = id;
    MIX_SetTrackLoops(s_bgmTrack, 0);
}

void SDLCALL AUDIO_SDL_OnBgmStopped(void* userdata, MIX_Track* track)
{
    int queuedBgmId = s_queuedBgmId;

    s_queuedBgmId = 0;
    s_currentBgmId = 0;

    if (queuedBgmId == 0)
    {
        return;
    }

    AUDIO_SDL_PlayBgmSub(queuedBgmId);
}

void AUDIO_SDL_StopBgm(void)
{
    debug("AUDIO_SDL_StopBgm()");

    s_currentBgmId = 0;
    s_queuedBgmId = 0;

    Sint64 frames = MIX_MSToFrames(s_mixerSpec.freq, 500);
    MIX_StopTrack(s_bgmTrack, frames);
    const char* err = SDL_GetError();
    if (err) { puts(err); }
}

void AUDIO_SDL_Noop(void)
{
    // no-op
}

static int AUDIO_SDL_GetSfxType(void)
{
    return SFX_TYPE_SYNTH;
}

// freq is direct Hz (confirmed by tickhigh/ticklow and denyhi/denylo pairs in
// sfx_map.h, which only order correctly - hi > lo - under a direct-Hz
// reading). pulseWidth/pulseInc modulate the square wave's duty cycle over
// the tone's duration, giving the "warbling" plucked/chime timbre used for
// fixed-pitch cues like the harpsichord/lute notes.
static void AUDIO_SDL_PlaySynthPulse(int freq, int delay, int dur, int pulseWidth, int pulseInc)
{
    (void)delay;

    if (freq <= 0 || dur <= 0)
        return;

    double freqHz = freq;
    if (freqHz < 20.0) freqHz = 20.0;
    if (freqHz > 20000.0) freqHz = 20000.0;

    double seconds = dur / 1000.0;
    if (seconds > SYNTH_MAX_SECONDS) seconds = SYNTH_MAX_SECONDS;
    int numSamples = (int)(SYNTH_SAMPLE_RATE * seconds);
    if (numSamples <= 0)
        return;

    Sint16* buf = (Sint16*)malloc((size_t)numSamples * sizeof(Sint16));
    if (!buf)
        return;

    double duty0 = pulseWidth > 0 ? (double)(pulseWidth % 20000) / 20000.0 : 0.5;
    if (duty0 < 0.1) duty0 = 0.1;
    if (duty0 > 0.9) duty0 = 0.9;
    double dutyShiftTotal = (pulseInc / 20000.0) * 2.0;

    double phase = 0.0;
    for (int i = 0; i < numSamples; i++)
    {
        double t = (double)i / numSamples;
        double duty = duty0 + dutyShiftTotal * t;
        if (duty < 0.05) duty = 0.05;
        if (duty > 0.95) duty = 0.95;

        phase += freqHz / SYNTH_SAMPLE_RATE;
        double frac = phase - floor(phase);
        buf[i] = (frac < duty) ? 12000 : -12000;
    }

    AUDIO_SDL_PlaySynthBuffer(buf, numSamples);
    free(buf);
}

// rate controls the noise "grain" (samples held per random step - larger
// rate gives coarser, lower-pitched noise); dur is milliseconds directly
// (this reading makes footsteps ~15-25ms clicks and waterfalls/fountains
// 10-25s ambient bursts, both of which make sense for their names).
static void AUDIO_SDL_PlaySynthNoise(int rate, int dur, int limit)
{
    (void)limit;

    if (dur <= 0)
        return;

    double seconds = dur / 1000.0;
    if (seconds > SYNTH_MAX_SECONDS + 2.0) seconds = SYNTH_MAX_SECONDS + 2.0;
    int numSamples = (int)(SYNTH_SAMPLE_RATE * seconds);
    if (numSamples <= 0)
        return;

    Sint16* buf = (Sint16*)malloc((size_t)numSamples * sizeof(Sint16));
    if (!buf)
        return;

    int holdSamples = rate > 0 ? rate : 1;
    if (holdSamples > 200) holdSamples = 200;

    unsigned int seed = 0x9e3779b9u;
    Sint16 current = 0;
    for (int i = 0; i < numSamples; i++)
    {
        if (i % holdSamples == 0)
        {
            seed = seed * 1103515245u + 12345u;
            current = (Sint16)(((seed >> 16) & 0x7fff) - 16384);
        }

        double envelope = 1.0 - (double)i / numSamples;
        buf[i] = (Sint16)(current * envelope * 0.6);
    }

    AUDIO_SDL_PlaySynthBuffer(buf, numSamples);
    free(buf);
}

// Fixed-pitch square wave beep, freq in direct Hz, dur in milliseconds.
static void AUDIO_SDL_PlaySynthTone(int freq, int dur)
{
    if (freq <= 0 || dur <= 0)
        return;

    double freqHz = freq;
    if (freqHz < 20.0) freqHz = 20.0;
    if (freqHz > 20000.0) freqHz = 20000.0;

    double seconds = dur / 1000.0;
    if (seconds > SYNTH_MAX_SECONDS) seconds = SYNTH_MAX_SECONDS;
    if (seconds < 0.005) seconds = 0.005;
    int numSamples = (int)(SYNTH_SAMPLE_RATE * seconds);
    if (numSamples <= 0)
        return;

    Sint16* buf = (Sint16*)malloc((size_t)numSamples * sizeof(Sint16));
    if (!buf)
        return;

    double phase = 0.0;
    for (int i = 0; i < numSamples; i++)
    {
        phase += freqHz / SYNTH_SAMPLE_RATE;
        double frac = phase - floor(phase);
        buf[i] = (frac < 0.5) ? 12000 : -12000;
    }

    AUDIO_SDL_PlaySynthBuffer(buf, numSamples);
    free(buf);
}

// Linear frequency sweep from startFreq to endFreq (direct Hz) over dur
// milliseconds. Uses phase accumulation (integrating instantaneous
// frequency) rather than naively evaluating freq(t)*t, which would produce
// an audible click/discontinuity as the frequency changes.
static void AUDIO_SDL_PlaySynthSweepTone(int startFreq, int endFreq, int tickStep, int dur)
{
    (void)tickStep;

    if (dur <= 0)
        return;
    if (startFreq <= 0) startFreq = 100;
    if (endFreq <= 0) endFreq = 100;

    double seconds = dur / 1000.0;
    if (seconds > SYNTH_MAX_SECONDS + 4.0) seconds = SYNTH_MAX_SECONDS + 4.0;
    int numSamples = (int)(SYNTH_SAMPLE_RATE * seconds);
    if (numSamples <= 0)
        return;

    Sint16* buf = (Sint16*)malloc((size_t)numSamples * sizeof(Sint16));
    if (!buf)
        return;

    double phase = 0.0;
    for (int i = 0; i < numSamples; i++)
    {
        double t = (double)i / numSamples;
        double freqHz = startFreq + (endFreq - startFreq) * t;
        if (freqHz < 20.0) freqHz = 20.0;
        if (freqHz > 20000.0) freqHz = 20000.0;

        phase += freqHz / SYNTH_SAMPLE_RATE;
        double frac = phase - floor(phase);
        buf[i] = (frac < 0.5) ? 12000 : -12000;
    }

    AUDIO_SDL_PlaySynthBuffer(buf, numSamples);
    free(buf);
}

static AudioMusicDriverOps s_musicOps =
{
    .Initialize = AUDIO_SDL_Init,
    .Cleanup = AUDIO_SDL_Cleanup,
    .PlayBgm = AUDIO_SDL_PlayBgm,
    .QueueBgm = AUDIO_SDL_QueueBgm,
    .StopBgm = AUDIO_SDL_StopBgm
};

static AudioSfxDriverOps s_sfxOps =
{
    .Initialize = AUDIO_SDL_Noop,
    .Cleanup = AUDIO_SDL_Noop,
    .PlaySfx = AUDIO_SDL_PlaySfx,
    .StopSfx = AUDIO_SDL_StopSfx,
    .GetSfxType = AUDIO_SDL_GetSfxType,
    .PlaySynthPulse = AUDIO_SDL_PlaySynthPulse,
    .PlaySynthNoise = AUDIO_SDL_PlaySynthNoise,
    .PlaySynthTone = AUDIO_SDL_PlaySynthTone,
    .PlaySynthSweepTone = AUDIO_SDL_PlaySynthSweepTone
};

AudioMusicDriverOps* AUDIO_SDL_GetMusicOps(void)
{
    return &s_musicOps;
}

AudioSfxDriverOps* AUDIO_SDL_GetSfxOps(void)
{
    return &s_sfxOps;
}
