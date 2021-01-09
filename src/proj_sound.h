#ifndef PROJ_SOUND_H
#define PROJ_SOUND_H

// local
#include "proj_types.h"
#include "proj_math.h"

// third party
#include "windows.h"
#include "Audioclient.h"
#include "Audiopolicy.h"
#include "Mmdeviceapi.h"

// system
#include "stdio.h"





#define SOUND_SIN_LOW       0
#define SOUND_SIN_HIGH      1
#define SOUND_SWEEP         2
#define SOUND_VOICE         3
#define N_SOUNDS            4

struct Sound {
    u32   length;
    u16   sample_rate;
    u8    depth;
    u8    channels;
    i64   time_us;
    void* data;
};

i32 wav_to_sound(const char* filename, Sound* sound);
void resample_sound(Sound* sound, u32 rate);

enum class EventMode {
    default,
    update,
    interpolate,
    start,
    loop,
    stop,
    stop_all
};

struct Event {
    EventMode mode        = EventMode::default;
    EventMode target_mode = EventMode::default;
    i32       id          = -1;
    i32       target_id   = -1;
    i32       sound_id    = NULL;
    u16       layer       = 0;
    f32       volume      = 0.0f;
    f32       angle       = 0.0f;
    f32       interp_time = 0.0f;
};

struct Status {
    EventMode mode          = EventMode::default;
    i32       id            = -1;
    i32       sound_id      = NULL;
    u8        layer         = 0;
    u32       offset        = 0;
    f32       volume        = 0.0f;
    f32       angle         = 0.0f;
    i64       last_write_us = 0;
    i64       end_time_us   = 0;

    struct _interp {
        u8  running      = 0;
        i64 start_us     = 0;
        i64 time_us      = 0;
        f32 delta_volume = 0.0f;
        f32 old_volume   = 0.0f;
        f32 delta_angle  = 0.0f;
        f32 old_angle    = 0.0f;
    } interp;
};

#define N_EVENTS    32
struct RingBuffer {
    i32   id             = 0;
    u32   ptr            = 0;
    Event ring[N_EVENTS];
};

i32  ring_push(RingBuffer* rb, Event e);
void ring_clear(RingBuffer* rb);

#define N_LAYERS                1
#define LAYER_CHANNELS          2
#define MASTER_CHANNELS         2

struct AudioBuffers {
    u64  length;
    f32* layers[N_LAYERS][LAYER_CHANNELS];
    f32* master[MASTER_CHANNELS];
};

void init_buffers(u64 length);

#define BUFFER_SIZE     512
struct WASAPI_Info {
    u32 length         = NULL;
    u8  started        = 0;
    u8  floating_point = 0;
    u16 valid_bits     = 0;

    IMMDeviceEnumerator* device_enum  = NULL;
    IMMDevice* device                 = NULL;
    IAudioClient* audio_client        = NULL;
    IAudioRenderClient* render_client = NULL;

    WAVEFORMATEX* mix_fmt;
};

void init_wasapi(WASAPI_Info* info);
u32 output_buffer_wasapi(WASAPI_Info* info);


struct ThreadArgs {
    HANDLE  mutex     = NULL;
    u8      new_event = 0;
    u8      init      = 0;
    RingBuffer events;
};


void sound_to_layer(Status* status);
void mix_to_master();
void audio_loop(ThreadArgs* args);




#define DEBUG_ERROR(x)          if (hr < 0) { printf("[Audio] 0x%x - " x, hr); }

#endif
