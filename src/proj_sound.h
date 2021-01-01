#ifndef PROJ_SOUND_H
#define PROJ_SOUND_H

// local
#include "proj_types.h"
#include "proj_math.h"

// system
#include "windows.h"
#include "dsound.h"

// standard (i forgot my convention on this)
#include "stdio.h"

struct Sound {
    u32   length;
    u16   sample_rate;
    u8    depth;
    u8    channels;
    u32   byte_length;
    void* data;
};

#define N_SOUNDS 2
struct SoundLibrary {
    Sound sin;
    Sound sweep;
};

void graph_buffer(u16 x_samples, u16 y_samples, i16* data, u32 length);
i32 wav_to_sound(const char* filename, Sound* sound);



#define EVENT_DEFAULT  0
#define EVENT_TEST     1
struct Event {
    u16 id;
    f32 volume;
    f32 angle;    // TODO: imagine 5 channel audio all coming from the left
};

#define STATUS_FLAG_DEFAULT         0
#define STATUS_FLAG_STOP            1
#define STATUS_FLAG_SMOOTH_STOP     2
struct Status {
    u8    mode        = STATUS_FLAG_DEFAULT;
    u8    layer       = 0;
    f32   start_time  = 0.0f;
    f32   end_time    = 0.0f;
    Event respond; // event taken from ring
    Event current; // current status of the event
};

#define N_EVENTS 256
struct RingBuffer {
    u32   ptr            = 0;
    Event ring[N_EVENTS] = {0};
};

void ring_push(RingBuffer* rb, Event e);



#define OUTPUT_SAMPLE_RATE      48000
#define OUTPUT_DEPTH            16
#define OUTPUT_CHANNELS         2
#define OUTPUT_SAMPLE_BYTES     ((OUTPUT_DEPTH >> 3) * OUTPUT_CHANNELS)
                                
#define N_LAYERS                1
#define LAYER_CHANNELS          2
#define MASTER_CHANNELS         2

#define BUFFER_LEN              OUTPUT_SAMPLE_RATE // 1 second
struct AudioBuffers {
    f32 layers[N_LAYERS][LAYER_CHANNELS][BUFFER_LEN] = {0.0f};
    f32 master[MASTER_CHANNELS][BUFFER_LEN]          = {0.0f};
    LPDIRECTSOUNDBUFFER main_buffer;
    LPDIRECTSOUNDBUFFER off_buffer;
};

struct ThreadArgs {
    HWND    hwnd      = NULL;
    HANDLE  mutex     = NULL;
    u8      new_event = 0;
    RingBuffer events;
};

void audio_loop(ThreadArgs* args);



#define DEBUG_ERROR(x)          if (hr < 0) { printf("[Audio] 0x%x - " x, hr); }

#endif
