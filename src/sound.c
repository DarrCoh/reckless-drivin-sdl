/*
 * sound.c - Sound system for Reckless Drivin' SDL port
 *
 * Sound data is stored in pack files as tSound structures:
 *   { UInt32 numSamples, UInt32 priority, UInt32 flags, UInt32 offsets[] }
 * Each offset points to a Mac SoundHeader (stdSH, encode=0x00, 22 bytes)
 * or ExtSoundHeader (extSH, encode=0xFF, 64 bytes) followed by PCM data.
 * All multi-byte fields are big-endian.
 */

#include "compat.h"
#include "platform.h"
#include "objects.h"
#include "roads.h"
#include "preferences.h"
#include "packs.h"
#include "random.h"
#include "vec2d.h"
#include <math.h>
#include <stdio.h>

/* ======================================================================== */
/* Constants (from original)                                                 */
/* ======================================================================== */

#define kNumChannels      3
#define kNumHQChannels    6
#define kMaxPanDist       400.0f
#define kMaxListenDist    1250.0f
#define kDopplerFactor    0.004f
#define kMaxNoiseVelo     65.0f
#define kMinSqueakSlide   0.5f
#define kSqueakFactor     0.5f
#define kNumGears         4
#define kHighestGear      55.0f
#define kHighestGearTurbo 70.0f
#define kShiftTolerance   2.5f

enum {
    kSoundPriorityHigher = 1 << 0
};

/* ======================================================================== */
/* Sound pack header structure                                               */
/* ======================================================================== */

typedef struct {
    UInt32 numSamples;
    UInt32 priority;
    UInt32 flags;
    UInt32 offsets[1];   /* variable-length array; numSamples entries */
} tSound;

/* ======================================================================== */
/* Mac SoundHeader constants                                                 */
/* ======================================================================== */

#define kStdSH_HeaderSize 22   /* Standard SoundHeader (encode=0x00) */
#define kExtSH_HeaderSize 64   /* Extended SoundHeader (encode=0xFF) */

/* ======================================================================== */
/* Globals                                                                   */
/* ======================================================================== */

float gVolume;
int   gGear;

static UInt32 sChannelPriority[kNumHQChannels];
static int sAudioInited = 0;
static int sEngineNeedsRetrigger = 1;
static int sSkidNeedsRetrigger   = 1;

/* ======================================================================== */
/* Helpers: read big-endian tSound fields                                    */
/* ======================================================================== */

static inline UInt32 SoundNumSamples(const tSound *s)
{
    return SWAP32(s->numSamples);
}

static inline UInt32 SoundPriority(const tSound *s)
{
    return SWAP32(s->priority);
}

static inline UInt32 SoundFlags(const tSound *s)
{
    return SWAP32(s->flags);
}

static inline UInt32 SoundOffset(const tSound *s, int index)
{
    return SWAP32(s->offsets[index]);
}

/* Read a big-endian UInt32 from unaligned memory */
static inline UInt32 ReadBE32(const UInt8 *p)
{
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
           ((UInt32)p[2] << 8)  | (UInt32)p[3];
}

/* ======================================================================== */
/* SoundHeader parsing                                                       */
/* ======================================================================== */

/*
 * Parse a Mac SoundHeader at the given offset within a tSound entry.
 * Returns a pointer to the actual PCM sample data, the number of sample
 * frames, the native sample rate in Hz, and the bits per sample.
 *
 * Standard SoundHeader (encode=0x00, 22 bytes):
 *   Offset  Size  Field
 *   0       4     samplePtr        (always 0 for inline data)
 *   4       4     numSampleFrames
 *   8       4     sampleRate       (Fixed 16.16, in Hz)
 *   12      4     loopStart
 *   16      4     loopEnd
 *   20      1     encode           (0x00)
 *   21      1     baseFrequency
 *   22      ...   sampleArea       (unsigned 8-bit PCM)
 *
 * Extended SoundHeader (encode=0xFF, 64 bytes):
 *   Offset  Size  Field
 *   0       4     samplePtr        (always 0 for inline data)
 *   4       4     numChannels
 *   8       4     sampleRate       (Fixed 16.16, in Hz)
 *   12      4     loopStart
 *   16      4     loopEnd
 *   20      1     encode           (0xFF)
 *   21      1     baseFrequency
 *   22      4     numSampleFrames
 *   26      10    AIFFSampleRate   (80-bit extended)
 *   36      4     markerChunk
 *   40      4     instrumentChunks
 *   44      4     AESRecording
 *   48      2     sampleSize       (bits per sample)
 *   50      14    futureUse
 *   64      ...   sampleArea       (signed 16-bit big-endian PCM)
 */
static const UInt8 *ParseSoundHeader(const UInt8 *data, int dataLen,
                                     int *outNumFrames, float *outRate,
                                     int *outBitsPerSample)
{
    if (dataLen < kStdSH_HeaderSize) {
        *outNumFrames = 0;
        *outRate = 22050.0f;
        *outBitsPerSample = 8;
        return NULL;
    }

    UInt8 encode = data[20];

    if (encode == 0xFF) {
        /* Extended SoundHeader */
        if (dataLen < kExtSH_HeaderSize) {
            *outNumFrames = 0;
            *outRate = 22050.0f;
            *outBitsPerSample = 16;
            return NULL;
        }

        UInt32 sampleRateFixed = ReadBE32(data + 8);
        UInt32 numFrames = ReadBE32(data + 22);
        UInt16 sampleSize = ((UInt16)data[48] << 8) | data[49];

        *outRate = sampleRateFixed / 65536.0f;
        *outNumFrames = (int)numFrames;
        *outBitsPerSample = (sampleSize > 0) ? (int)sampleSize : 16;

        return data + kExtSH_HeaderSize;
    } else {
        /* Standard SoundHeader (encode=0x00) */
        UInt32 sampleRateFixed = ReadBE32(data + 8);
        UInt32 numFrames = ReadBE32(data + 4);

        *outRate = sampleRateFixed / 65536.0f;
        *outNumFrames = (int)numFrames;
        *outBitsPerSample = 8;

        return data + kStdSH_HeaderSize;
    }
}

/*
 * Get sample data from a tSound entry.  Parses the Mac SoundHeader at
 * the sample's offset to extract the actual PCM data pointer, frame
 * count, sample rate, and bit depth.  The pitch is adjusted to account
 * for the native sample rate vs. the mixer output rate.
 */
static const UInt8 *GetSampleData(const tSound *s, int sampleIndex,
                                  int totalEntrySize,
                                  int *outLen, float *outPitchAdj,
                                  int *outBitsPerSample)
{
    UInt32 numSamples = SoundNumSamples(s);
    if (sampleIndex < 0 || (UInt32)sampleIndex >= numSamples) {
        *outLen = 0;
        *outPitchAdj = 1.0f;
        *outBitsPerSample = 8;
        return NULL;
    }

    UInt32 offset = SoundOffset(s, sampleIndex);
    const UInt8 *base = (const UInt8 *)s;
    const UInt8 *hdrStart = base + offset;

    /* Compute raw data length at this offset */
    int rawLen;
    if ((UInt32)sampleIndex + 1 < numSamples) {
        UInt32 nextOffset = SoundOffset(s, sampleIndex + 1);
        rawLen = (int)(nextOffset - offset);
    } else {
        rawLen = totalEntrySize - (int)offset;
    }

    if (rawLen <= 0) {
        *outLen = 0;
        *outPitchAdj = 1.0f;
        *outBitsPerSample = 8;
        return NULL;
    }

    int numFrames;
    float nativeRate;
    int bitsPerSample;
    const UInt8 *pcmData = ParseSoundHeader(hdrStart, rawLen,
                                            &numFrames, &nativeRate,
                                            &bitsPerSample);
    if (!pcmData || numFrames <= 0) {
        *outLen = 0;
        *outPitchAdj = 1.0f;
        *outBitsPerSample = 8;
        return NULL;
    }

    *outLen = numFrames;
    *outBitsPerSample = bitsPerSample;
    /* Adjust pitch so the mixer plays back at the correct native rate */
    *outPitchAdj = nativeRate / (float)AUDIO_MIX_RATE;

    return pcmData;
}

/* ======================================================================== */
/* Volume                                                                    */
/* ======================================================================== */

void SetGameVolume(int volume)
{
    if (volume == -1)
        gVolume = gPrefs.volume / 256.0f;
    else
        gVolume = volume / 256.0f;
}

void SetSystemVolume(void)
{
    /* no-op in SDL port */
}

/* ======================================================================== */
/* Channel init / shutdown                                                   */
/* ======================================================================== */

void InitChannels(void)
{
    if (!sAudioInited) {
        Platform_InitAudio();
        sAudioInited = 1;
    }

    for (int i = 0; i < kNumHQChannels; i++)
        sChannelPriority[i] = 0;

    sEngineNeedsRetrigger = 1;
    sSkidNeedsRetrigger   = 1;
}

void BeQuiet(void)
{
    int numChan = gPrefs.hqSound ? kNumHQChannels : kNumChannels;
    for (int i = 0; i < numChan; i++) {
        Platform_StopChannel(kAudioChannel0 + i);
        sChannelPriority[i] = 0;
    }
    Platform_StopChannel(kAudioChannelEngine);
    Platform_StopChannel(kAudioChannelSkid);

    sEngineNeedsRetrigger = 1;
    sSkidNeedsRetrigger   = 1;
}

/* ======================================================================== */
/* Pick a free (lowest-priority) channel                                     */
/* ======================================================================== */

static int PickChannel(UInt32 *outPriority)
{
    int numChan = gPrefs.hqSound ? kNumHQChannels : kNumChannels;
    int bestChan = 0;
    UInt32 bestPri = 0xFFFFFFFF;

    for (int i = 0; i < numChan; i++) {
        if (sChannelPriority[i] < bestPri) {
            bestPri = sChannelPriority[i];
            bestChan = i;
        }
    }
    if (outPriority)
        *outPriority = bestPri;
    return bestChan;
}

/* ======================================================================== */
/* Engine / skid continuous sound                                            */
/* ======================================================================== */

void SetCarSound(float engine, float skidL, float skidR, float velo)
{
    if (!gPrefs.engineSound || !gPrefs.sound)
        return;

    float engineVol, gearVelo;

    if (!(*gRoadInfo).water) {
        float highestGear = (gPlayerAddOns & kAddOnTurbo)
                            ? kHighestGearTurbo : kHighestGear;
        while (velo > (gGear + 1) * highestGear / kNumGears + kShiftTolerance
               && (gGear + 1) < kNumGears)
            gGear++;
        while (velo < gGear * highestGear / kNumGears - kShiftTolerance
               && gGear > 0)
            gGear--;

        gearVelo = fabsf(velo) / highestGear * kNumGears - gGear;
        velo /= kMaxNoiseVelo;
        if (gearVelo > 2) gearVelo = 2;
    } else {
        velo /= kMaxNoiseVelo;
        gearVelo = (engine + velo) / 2;
    }

    velo = fabsf(velo);

    if (engine != -1)
        engineVol = -gVolume / ((0.6f * engine + 0.15f * velo + 0.25f * gearVelo) - 2);
    else
        engineVol = 0;

    /* Re-trigger engine loop if needed */
    if (sEngineNeedsRetrigger) {
        int entrySize;
        tSound *sound = (tSound *)GetSortedPackEntry(kPackSnds, 132, &entrySize);
        if (sound) {
            int sampleLen, bps;
            float pitchAdj;
            UInt32 ns = SoundNumSamples(sound);
            int idx = (ns > 0) ? RanInt(0, ns) : 0;
            const UInt8 *sampleData = GetSampleData(sound, idx, entrySize,
                                                    &sampleLen, &pitchAdj, &bps);
            if (sampleData && sampleLen > 0) {
                Platform_PlaySound(kAudioChannelEngine, sampleData, sampleLen,
                                   engineVol * gVolume, 0.0f, pitchAdj, bps);
                sEngineNeedsRetrigger = 0;
            }
        }
    }

    /* Set engine volume and pitch */
    {
        float vol = 0.28f * engineVol * gVolume;
        Platform_SetChannelVolume(kAudioChannelEngine, vol, vol);
        float pitch = 0.2f + 0.3f * engine + 0.2f * velo + 0.3f * gearVelo;
        Platform_SetChannelPitch(kAudioChannelEngine, pitch);
    }

    /* Skid processing */
    skidL -= kMinSqueakSlide;
    skidL /= kSqueakFactor;
    if (skidL < 0) skidL = 0;
    else if (skidL > 1) skidL = 1;

    skidR -= kMinSqueakSlide;
    skidR /= kSqueakFactor;
    if (skidR < 0) skidR = 0;
    else if (skidR > 1) skidR = 1;

    /* Re-trigger skid loop if needed */
    if (sSkidNeedsRetrigger) {
        int skidEntrySize;
        tSound *sound = (tSound *)GetSortedPackEntry(kPackSnds,
                            (*gRoadInfo).skidSound, &skidEntrySize);
        if (sound) {
            int sampleLen, bps;
            float pitchAdj;
            UInt32 ns = SoundNumSamples(sound);
            int idx = (ns > 0) ? RanInt(0, ns) : 0;
            const UInt8 *sampleData = GetSampleData(sound, idx, skidEntrySize,
                                                    &sampleLen, &pitchAdj, &bps);
            if (sampleData && sampleLen > 0) {
                Platform_PlaySound(kAudioChannelSkid, sampleData, sampleLen,
                                   0.0f, 0.0f, pitchAdj, bps);
                sSkidNeedsRetrigger = 0;
            }
        }
    }

    /* Set skid volume and pitch */
    {
        float volL = skidL * gVolume * (velo * 0.5f + 0.5f);
        float volR = skidR * gVolume * (velo * 0.5f + 0.5f);
        Platform_SetChannelVolume(kAudioChannelSkid, volL, volR);

        float skidPitch;
        if (skidL + skidR)
            skidPitch = -1.0f / ((skidL + skidR) * 0.25f + velo * 0.5f - 2.0f);
        else
            skidPitch = 0.0f;
        Platform_SetChannelPitch(kAudioChannelSkid, skidPitch);
    }
}

/* ======================================================================== */
/* Start the engine and skid loops                                           */
/* ======================================================================== */

void StartCarChannels(void)
{
    Platform_StopChannel(kAudioChannelEngine);
    Platform_StopChannel(kAudioChannelSkid);

    sEngineNeedsRetrigger = 1;
    sSkidNeedsRetrigger   = 1;

    if (gPrefs.engineSound && gPrefs.sound) {
        int engEntrySize;
        tSound *sound = (tSound *)GetSortedPackEntry(kPackSnds, 132, &engEntrySize);
        if (sound) {
            int sampleLen, bps;
            float pitchAdj;
            UInt32 ns = SoundNumSamples(sound);
            int idx = (ns > 0) ? RanInt(0, ns) : 0;
            const UInt8 *sampleData = GetSampleData(sound, idx, engEntrySize,
                                                    &sampleLen, &pitchAdj, &bps);
            if (sampleData && sampleLen > 0) {
                Platform_PlaySound(kAudioChannelEngine, sampleData, sampleLen,
                                   0.0f, 0.0f, pitchAdj, bps);
                sEngineNeedsRetrigger = 0;
            }
        }

        {
            int skidEntrySize;
            sound = (tSound *)GetSortedPackEntry(kPackSnds,
                        (*gRoadInfo).skidSound, &skidEntrySize);
            if (sound) {
                int sampleLen, bps;
                float pitchAdj;
                UInt32 ns = SoundNumSamples(sound);
                int idx = (ns > 0) ? RanInt(0, ns) : 0;
                const UInt8 *sampleData = GetSampleData(sound, idx, skidEntrySize,
                                                        &sampleLen, &pitchAdj, &bps);
                if (sampleData && sampleLen > 0) {
                    Platform_PlaySound(kAudioChannelSkid, sampleData, sampleLen,
                                       0.0f, 0.0f, pitchAdj, bps);
                    sSkidNeedsRetrigger = 0;
                }
            }
        }
    }

    SetCarSound(0, 0, 0, 0);
}

/* ======================================================================== */
/* Positional sound with distance attenuation and Doppler effect             */
/* ======================================================================== */

void PlaySound(t2DPoint pos, t2DPoint velo, float freq, float vol, int id)
{
    if (!gPrefs.sound)
        return;

    int entrySize;
    tSound *sound = (tSound *)GetSortedPackEntry(kPackSnds, id, &entrySize);
    if (!sound)
        return;

    UInt32 soundPri = SoundPriority(sound);
    UInt32 soundFlags = SoundFlags(sound);

    UInt32 chanPriority;
    int chanIdx = PickChannel(&chanPriority);

    if (soundFlags & kSoundPriorityHigher) {
        if (chanPriority >= soundPri)
            return;
    } else {
        if (chanPriority > soundPri)
            return;
    }

    float dist = 1.0f - VEC2D_Value(VEC2D_Difference(pos, gCameraObj->pos))
                        * (1.0f / kMaxListenDist);
    if (dist <= 0)
        return;

    sChannelPriority[chanIdx] = soundPri;

    float pan = (pos.x - gCameraObj->pos.x) * (1.0f / kMaxPanDist);
    if (pan > 1.0f)  pan = 1.0f;
    if (pan < -1.0f) pan = -1.0f;

    if (gPrefs.hqSound) {
        t2DPoint veloDiff = VEC2D_Difference(velo, gCameraObj->velo);
        t2DPoint spaceDiff = VEC2D_Norm(VEC2D_Difference(gCameraObj->pos, pos));
        freq *= powf(2.0f, VEC2D_DotProduct(veloDiff, spaceDiff) * kDopplerFactor);
    }

    vol *= gVolume;

    UInt32 ns = SoundNumSamples(sound);
    int sampleIdx = (ns > 0) ? RanInt(0, ns) : 0;
    int sampleLen, bps;
    float pitchAdj;
    const UInt8 *sampleData = GetSampleData(sound, sampleIdx, entrySize,
                                            &sampleLen, &pitchAdj, &bps);
    if (!sampleData || sampleLen <= 0)
        return;

    /* Combine game freq with rate-correction pitch */
    Platform_PlaySound(kAudioChannel0 + chanIdx, sampleData, sampleLen,
                       vol * dist, pan, freq * pitchAdj, bps);
}

/* ======================================================================== */
/* Simple (non-positional) sound                                             */
/* ======================================================================== */

void SimplePlaySound(int id)
{
    if (!gPrefs.sound)
        return;

    int entrySize;
    tSound *sound = (tSound *)GetSortedPackEntry(kPackSnds, id, &entrySize);
    if (!sound)
        return;

    UInt32 soundPri = SoundPriority(sound);
    UInt32 soundFlags = SoundFlags(sound);

    UInt32 chanPriority;
    int chanIdx = PickChannel(&chanPriority);

    if (soundFlags & kSoundPriorityHigher) {
        if (chanPriority >= soundPri)
            return;
    } else {
        if (chanPriority > soundPri)
            return;
    }

    sChannelPriority[chanIdx] = soundPri;

    UInt32 ns = SoundNumSamples(sound);
    int sampleIdx = (ns > 0) ? RanInt(0, ns) : 0;
    int sampleLen, bps;
    float pitchAdj;
    const UInt8 *sampleData = GetSampleData(sound, sampleIdx, entrySize,
                                            &sampleLen, &pitchAdj, &bps);
    if (!sampleData || sampleLen <= 0)
        return;

    Platform_PlaySound(kAudioChannel0 + chanIdx, sampleData, sampleLen,
                       gVolume, 0.0f, pitchAdj, bps);
}
