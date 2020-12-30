#include "windows.h"
#include "dsound.h"

struct Sound {
    u32   length;
    u16   sample_rate;
    u8    depth;
    u8    channels;
    u32   byte_length;
    void* data;
};

i32 wav_to_sound(const char* filename, Sound* sound);

#define EVENT_DEFAULT  0
#define EVENT_TEST     1

struct Event {
    u16 id;
    f32 volume;
    f32 angle;    // TODO: imagine 5 channel audio all coming from the left
};

struct Status {
    f32   start_time;
    f32   end_time;
    u8    layer;
    Event respond; // event taken from ring
    Event current; // current status of the event
};

#define N_EVENTS 256
struct RingBuffer {
    u32   ptr            = 0;
    Event ring[N_EVENTS] = {0};
};

void ring_push(RingBuffer* rb, Event e);

struct ThreadArgs {
    HWND       hwnd  = NULL;
    HANDLE     mutex = NULL;
    RingBuffer events;
};

void audio_loop(ThreadArgs* args);
