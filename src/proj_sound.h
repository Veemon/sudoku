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



#define SOUND_SIN   0
#define SOUND_SWEEP 1
#define N_SOUNDS    2

struct Sound {
    u32   length;
    u32   offset;
    u16   sample_rate;
    u8    depth;
    u8    channels;
    i64   time_us;
    void* data;
};

void graph_buffer(u16 x_samples, u16 y_samples, i16* data, u32 length);
i32 wav_to_sound(const char* filename, Sound* sound);



#define MODE_DEFAULT         0
#define MODE_START           1
#define MODE_SMOOTH_START    2
#define MODE_STOP            3
#define MODE_SMOOTH_STOP     4

struct Event {
    u16 sound_id;
    u16 layer;
    u8  mode;
    f32 volume;
    f32 angle;
};

struct Status {
    u8    mode          = MODE_DEFAULT;
    u8    layer         = 0;
    f32   volume        = 0.0f;
    f32   angle         = 0.0f;
    i64   last_write_us = 0;
    i64   end_time_us   = 0;
};

#define N_EVENTS 256
struct RingBuffer {
    u32   ptr            = 0;
    Event ring[N_EVENTS] = {0};
};

void ring_push(RingBuffer* rb, Event e);

struct ThreadArgs {
    HWND    hwnd      = NULL;
    HANDLE  mutex     = NULL;
    u8      new_event = 0;
    RingBuffer events;
};


#define OUTPUT_SAMPLE_RATE      48000
#define OUTPUT_DEPTH            16
#define OUTPUT_CHANNELS         2
#define OUTPUT_SAMPLE_BYTES     ((OUTPUT_DEPTH >> 3) * OUTPUT_CHANNELS)
                                
#define N_LAYERS                1
#define LAYER_CHANNELS          2
#define MASTER_CHANNELS         2

// NOTE: if you can fix the audio crackle, you can reduce the buffer size
//       which in turn will reduce the audio latency
// #define BUFFER_LEN              (OUTPUT_SAMPLE_RATE)
#define BUFFER_LEN              2048

struct AudioBuffers {
    f32 layers[N_LAYERS][LAYER_CHANNELS][BUFFER_LEN] = {0.0f};
    f32 master[MASTER_CHANNELS][BUFFER_LEN]          = {0.0f};
    LPDIRECTSOUNDBUFFER main_buffer;
    LPDIRECTSOUNDBUFFER off_buffer;
};

void audio_loop(ThreadArgs* args);



#define DEBUG_ERROR(x)          if (hr < 0) { printf("[Audio] 0x%x - " x, hr); }

#endif
