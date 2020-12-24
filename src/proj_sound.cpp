#include "proj_types.h"
#include "proj_sound.h"

// FIXME: remove on cleanup
#include "stdio.h"


i32 wav_to_sound(const char* filename, Sound* sound) {
    #define BIG_32(x)   ( (x[3]<<24) | (x[2]<<16) | (x[1]<<8) | (x[0]<<0) )
    #define BIG_16(x)   ( (x[1]<<8)  | (x[0]<<0) )

    u8 bytes[4];

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("[Audio] File Error: %s\n", filename);
        return NULL;
    }

    // Audio Format
    fseek(file, 20, SEEK_SET);
    fread(&bytes[0], sizeof(u8), 2, file);
    u16 format = BIG_16(bytes);
    if (format != 1) {
        printf("[Audio] Only PCM supported  -  format \'%u\' : %s\n", format, filename);
        return NULL;
    }

    // Channels
    fseek(file, 22, SEEK_SET);
    fread(&bytes[0], sizeof(u8), 2, file);
    u16 channels = BIG_16(bytes);
    sound->channels = channels;

    // Sample Rate
    fseek(file, 24, SEEK_SET);
    fread(&bytes[0], sizeof(u8), 4, file);
    u32 sample_rate = BIG_32(bytes);
    sound->sample_rate = sample_rate;

    // Depth
    fseek(file, 34, SEEK_SET);
    fread(&bytes[0], sizeof(u8), 2, file);
    u16 depth = BIG_16(bytes);
    sound->depth = depth;

    // Data
    fseek(file, 40, SEEK_SET);
    fread(&bytes[0], sizeof(u8), 4, file);
    u32 length = BIG_32(bytes);
    sound->data_length = length;

    sound->data = malloc(length);
    fseek(file, 44, SEEK_SET);
    fread(sound->data, sizeof(u8), length, file);

    printf("[Audio] Sound - %s\n", filename);
    printf("  -  format        %u\n", format);
    printf("  -  channels      %u\n", sound->channels);
    printf("  -  sample rate   %u\n", sound->sample_rate);
    printf("  -  depth         %u\n", sound->depth);
    printf("  -  data          %p\n", sound->data);

    fclose(file);
    return 1;

    #undef BIG_32
    #undef BIG_16
}


void ring_push(RingBuffer* rb, Event e) {
    rb->ring[rb->ptr] = e;
    rb->ptr = (rb->ptr+1) % N_EVENTS;
}


// FIXME - could this be better?
#define DEBUG_ERROR(x)          if (hr < 0) { printf("[Audio] 0x%x - " x, hr); }

#define OUTPUT_SAMPLE_RATE      48000
#define OUTPUT_SAMPLE_DEPTH     16
#define OUTPUT_CHANNELS         2
#define OUTPUT_SAMPLE_BYTES     ((OUTPUT_SAMPLE_DEPTH / 8) * OUTPUT_CHANNELS)
#define OUTPUT_LEN              (OUTPUT_SAMPLE_RATE * OUTPUT_SAMPLE_BYTES)       // 1 seconds
                                
#define N_LAYERS                1
#define LAYER_CHANNELS          2
                                
#define MASTER_CHANNELS         2
#define MASTER_LEN              OUTPUT_SAMPLE_RATE // 1 second

/* 
FIXME: If you run the application you will hear one second of audio, then one second of silence.
Not only that but you will hear the sine tone at a different frequency. Thus, I suppose the problem
- We fill the output buffer with 2 seconds of sine data in 1 second of output buffer.
  The remaining second in the output buffer is then 0, so it sounds like a reversing truck.
*/

void audio_loop(ThreadArgs* args) {
    printf("[Audio] Initialising Audio\n");

    LPDIRECTSOUNDBUFFER main_buffer;
    LPDIRECTSOUNDBUFFER off_buffer;
    
    f32 layers[N_LAYERS][LAYER_CHANNELS][MASTER_LEN] = {0.0f};
    f32 master[MASTER_CHANNELS][MASTER_LEN]          = {0.0f};
    
    HRESULT hr = NULL;

    // initialize
    LPDIRECTSOUND direct_sound;
    hr = DirectSoundCreate(0, &direct_sound, 0);
    DEBUG_ERROR("Failed to init DirectSound\n");

    WAVEFORMATEX wave_fmt = {};
    wave_fmt.wFormatTag      = WAVE_FORMAT_PCM;
    wave_fmt.nChannels       = OUTPUT_CHANNELS;
    wave_fmt.nSamplesPerSec  = OUTPUT_SAMPLE_RATE;
    wave_fmt.wBitsPerSample  = OUTPUT_SAMPLE_DEPTH;
    wave_fmt.nBlockAlign     = OUTPUT_CHANNELS*(OUTPUT_SAMPLE_DEPTH / 8);
    wave_fmt.nAvgBytesPerSec = OUTPUT_SAMPLE_RATE * wave_fmt.nBlockAlign;
    wave_fmt.cbSize          = 0;

    hr = direct_sound->SetCooperativeLevel(args->hwnd, DSSCL_PRIORITY);
    DEBUG_ERROR("Failed to set CoopLevel\n");

    {
        DSBUFFERDESC buffer_desc = {};
        buffer_desc.dwSize  = sizeof(buffer_desc);
        buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

        hr = direct_sound->CreateSoundBuffer(&buffer_desc, &main_buffer, 0);
        DEBUG_ERROR("Failed to create main buffer\n");

        hr = main_buffer->SetFormat(&wave_fmt);
        DEBUG_ERROR("Failed to set main format\n");
    }

    {
        DSBUFFERDESC buffer_desc = {};
        buffer_desc.dwSize        = sizeof(buffer_desc);
        buffer_desc.dwFlags       = 0;
        buffer_desc.dwBufferBytes = OUTPUT_LEN;
        buffer_desc.lpwfxFormat   = &wave_fmt;

        hr = direct_sound->CreateSoundBuffer(&buffer_desc, &off_buffer, 0);
        DEBUG_ERROR("Failed to create off buffer\n");
    }


    printf("[Audio] Loading Sounds\n");

    Sound sound;
    wav_to_sound("./res/test.wav", &sound);

    printf("[Audio] Beginning Polling\n");

    hr = NULL;

    // ring buffers are nice because the structure implies invalidation of old events
    RingBuffer* events = &(args->events);
    Status playing[N_EVENTS];
    Event  queued[N_EVENTS];
    u32    play_tail  = 0;
    u32    queue_tail = 0;

    u32 sound_acc = 0; // the following was used to play some digital signal

    // timing
    f32 total_time = 0.0;
    LARGE_INTEGER start_time, end_time, delta_ms, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_ms.QuadPart = 0;
    f32 delta_s = 0.0;

    // FIXME - debug sound
    playing[0].start_time = total_time;
    playing[0].end_time   = f32(sound.data_length) / sound.sample_rate;
    playing[0].layer      = 0;
    playing[0].respond    = {EVENT_TEST, 0.5, 0.0};
    playing[0].current    = playing[0].respond;

    while (1) {
        QueryPerformanceCounter(&start_time);

        // queue sounds in response to events from the main thread
        u32 sig = WaitForSingleObject(args->mutex,0);
        if (sig == WAIT_OBJECT_0) {
            const u32 a[2] = {events->ptr, 0};
            const u32 b[2] = {N_EVENTS, events->ptr};
            for (u8 out = 0; out < 2; out++) {
                for (u32 idx = a[out]; idx < b[out]; idx++) {
                    if (events->ring[idx].id == EVENT_DEFAULT) continue;
                    queued[queue_tail] = events->ring[idx];
                    queue_tail = (queue_tail+1) % N_EVENTS; // FIXME - if we actually overflow, we don't want this
                    printf("[Audio] Queued event - %u\n", events->ring[idx].id);
                }
            }
            ReleaseMutex(args->mutex);
        }

        // FIXME - something isnt getting cleared here
        if (queue_tail > 0) {
            printf("[Audio] Sound Queue Length - %u\n", queue_tail);
        }

        // FIXME - convert queued sounds to playing sounds?

        /*
           Workflow
           ----------------------------
           Respond to Events
           Update Playing Sounds (need to do something from the queue)
           Write to Layer                   -  Layers have n channels
           Mix Layers to Master             -  Master has  n channels
           Write Master to Output           -  Output is the flat DirectSound buffer
        */

        // FIXME - debug sound loop
        if (total_time > playing[0].end_time) {
            playing[0].start_time  = total_time;
            playing[0].end_time   += total_time;
            playing[0].current     = playing[0].respond;
        }

        // Playing Sounds -> Layers
        printf("[Audio] Writing to Layers\n");
        {
            // FIXME - offset should be calculted based on the elapsed time
            u32 offset = 0;
            for (u32 idx = 0; idx < MASTER_LEN; idx++) {
                // sound data is:
                // - range of [-2^(depth-1), 2^(depth-1)]
                // - interleaved (channels)
                // - variable depth

                // if out of sound samples just play 0
                u32 i = offset + idx;
                if (i >= (sound.data_length / sound.depth)) {
                    layers[0][0][idx] = 0.0;
                    layers[0][1][idx] = 0.0;
                    continue;
                }

                if (sound.depth == 8) {
                    i8* interp = (i8*) sound.data;
                    layers[0][0][idx] = f32(interp[(2*i)])   / 128;
                    layers[0][1][idx] = f32(interp[(2*i)+1]) / 128;
                } else if (sound.depth == 16) {
                    i16* interp = (i16*) sound.data;
                    layers[0][0][idx] = f32(interp[(2*i)])   / 32768;
                    layers[0][1][idx] = f32(interp[(2*i)+1]) / 32768;
                }
            }
        }

        // Mixing Layers -> Master
        printf("[Audio] Mixing Layers\n");
        {
            // accumulate sounds from all layers
            for (u32 layer_idx = 0; layer_idx < N_LAYERS; layer_idx++) {
                for (u32 idx = 0; idx < MASTER_LEN; idx++) {
                    master[0][idx] += layers[layer_idx][0][idx];
                    master[1][idx] += layers[layer_idx][1][idx];
                }
            }
            
            // TODO: this is where we would normalize, or something
        }

        // Master -> Output
        printf("[Audio] Outputting Master\n");
        {
            u32 play_cursor  = NULL;
            u32 write_cursor = NULL;
            hr = off_buffer->GetCurrentPosition((LPDWORD)&play_cursor, (LPDWORD)&write_cursor);
            DEBUG_ERROR("Failed to get cursor positions\n");

            u32 write_bytes;
            u32 lock_bytes = (sound_acc*OUTPUT_SAMPLE_BYTES) % OUTPUT_LEN;
            if (lock_bytes == play_cursor) {
                write_bytes = OUTPUT_LEN;
            }
            else if (lock_bytes > play_cursor) {
                write_bytes  = OUTPUT_LEN - lock_bytes;
                write_bytes += play_cursor;
            } else {
                write_bytes = play_cursor - lock_bytes;
            }

            void* region_a       = nullptr;
            void* region_b       = nullptr;
            u32   region_a_bytes = NULL;
            u32   region_b_bytes = NULL;

            hr = off_buffer->Lock(lock_bytes, write_bytes, 
                    &region_a, (LPDWORD)&region_a_bytes, 
                    &region_b, (LPDWORD)&region_b_bytes,
                    NULL);

            DEBUG_ERROR("Failed to lock\n");

            if (SUCCEEDED(hr)) { 
                u32 a_offset = region_a_bytes / OUTPUT_SAMPLE_BYTES;
                i16* ra      = (i16*)region_a;
                for (u32 i = 0; i < a_offset; i++) {
                    // set output
                    *(ra++) = i16(master[0][i] * 32768);
                    *(ra++) = i16(master[1][i] * 32768);

                    // clear master
                    master[0][i] = 0.0;
                    master[1][i] = 0.0;
                }

                u32 b_offset = region_b_bytes / OUTPUT_SAMPLE_BYTES;
                i16* rb      = (i16*)region_b;
                for (u32 i = 0; i < b_offset; i++) {
                    // set output
                    *(rb++) = i16(master[0][i+a_offset] * 32768);
                    *(rb++) = i16(master[1][i+a_offset] * 32768);

                    // clear master
                    master[0][i+a_offset] = 0.0;
                    master[1][i+a_offset] = 0.0;
                }

                hr = off_buffer->Unlock(region_a, region_a_bytes, region_b, region_b_bytes);
                DEBUG_ERROR("Failed to unlock\n");

                hr = off_buffer->Play(NULL, NULL, DSBPLAY_LOOPING);
                DEBUG_ERROR("Failed to play\n");
            }
        }

        // FIXME - if we play sound at 44KHz, and this thread is running > 1GHz,
        //         there is only so much processing we can do before we should really be just going to sleep.
        //         that way we're also not hogging the event mutex 

        // timing
        QueryPerformanceCounter(&end_time);
        delta_ms.QuadPart = end_time.QuadPart - start_time.QuadPart;
        delta_ms.QuadPart *= 1000;
        delta_ms.QuadPart /= cpu_freq.QuadPart;
        delta_s = f32(delta_ms.QuadPart) / 1000.f;

        total_time += delta_s;
    }
}
