#include "windows.h"
#include "dsound.h"

struct Sound {
    u32   data_length;
    u16   sample_rate;
    u8    depth;
    u8    channels;
    void* data;
};

i32 wav_to_sound(const char* filename, Sound* sound);

#define EVENT_DEFAULT  0
#define EVENT_TEST     1

struct Event {
    u16 id;
    f32 volume;
    f32 angle;
};

#define N_EVENTS 256
struct ThreadArgs {
    u32 tail              = 0;
    HANDLE mutex          = NULL;
    HWND hwnd             = NULL;
    Event queue[N_EVENTS] = {0};
};

void audio_loop(ThreadArgs* args);
