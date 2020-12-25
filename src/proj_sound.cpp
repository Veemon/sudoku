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

AudioBuffers buffers;

struct SoundLibrary {
    Sound sin;
    Sound sweep;
};

SoundLibrary sounds;


/* 
FIXME: If you run the application you will hear one second of audio, then one second of silence.

The sine wave file is made from:
Frequency      422 Hz
Amplitude       -2 dbFS (db [relative] to Full Scale)
Duration         3 Seconds
Sample Rate     48 KHz

Measured frequencies:
File            420 Hz
Sudoku          ??
*/

void audio_loop(ThreadArgs* args) {
    printf("[Audio] Initialising Output Buffer\n");
    
    HRESULT hr = NULL;

    // initialize
    LPDIRECTSOUND direct_sound;

    hr = DirectSoundCreate(0, &direct_sound, 0);
    DEBUG_ERROR("Failed to init DirectSound\n");

    hr = direct_sound->SetCooperativeLevel(args->hwnd, DSSCL_PRIORITY);
    DEBUG_ERROR("Failed to set CoopLevel\n");

    WAVEFORMATEX wave_fmt = {};
    wave_fmt.wFormatTag      = WAVE_FORMAT_PCM;
    wave_fmt.nChannels       = OUTPUT_CHANNELS;
    wave_fmt.nSamplesPerSec  = OUTPUT_SAMPLE_RATE;
    wave_fmt.wBitsPerSample  = OUTPUT_DEPTH;
    wave_fmt.nBlockAlign     = OUTPUT_SAMPLE_BYTES;
    wave_fmt.nAvgBytesPerSec = OUTPUT_SAMPLE_RATE * OUTPUT_SAMPLE_BYTES;
    wave_fmt.cbSize          = 0;

    printf("  -  format        %u\n", wave_fmt.wFormatTag);
    printf("  -  channels      %u\n", wave_fmt.nChannels);
    printf("  -  sample rate   %u\n", wave_fmt.nSamplesPerSec);
    printf("  -  depth         %u\n", wave_fmt.wBitsPerSample);

    {
        DSBUFFERDESC buffer_desc = {};
        buffer_desc.dwSize  = sizeof(buffer_desc);
        buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

        hr = direct_sound->CreateSoundBuffer(&buffer_desc, &buffers.main_buffer, 0);
        DEBUG_ERROR("Failed to create main buffer\n");

        hr = buffers.main_buffer->SetFormat(&wave_fmt);
        DEBUG_ERROR("Failed to set main format\n");
    }

    {
        DSBUFFERDESC buffer_desc = {};
        buffer_desc.dwSize        = sizeof(buffer_desc);
        buffer_desc.dwFlags       = 0;
        buffer_desc.dwBufferBytes = BUFFER_LEN;
        buffer_desc.lpwfxFormat   = &wave_fmt;

        hr = direct_sound->CreateSoundBuffer(&buffer_desc, &buffers.off_buffer, 0);
        DEBUG_ERROR("Failed to create off buffer\n");
    }


    printf("[Audio] Loading Sounds\n");
    wav_to_sound("./res/422hz_-2db_3s_48khz.wav", &sounds.sin);
    wav_to_sound("./res/10hz_10khz_-2db_3s_48khz.wav", &sounds.sweep);

    // FIXME - remove this when the time comes
    Sound sound = sounds.sin;

    hr = NULL;

    // timing
    f32 total_time = 0.0;
    LARGE_INTEGER start_time, end_time, delta_ms, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_ms.QuadPart = 0;
    f32 delta_s = 0.0;

    RingBuffer* events = &(args->events);
    Status playing[N_EVENTS]; // FIXME update this to N_SOUNDS
    Event  queued[N_EVENTS];
    u32    play_tail  = 0;
    u32    queue_tail = 0;

    // FIXME - debug sound
    playing[0].start_time = total_time;
    playing[0].end_time   = f32(sound.data_length) / sound.sample_rate;
    playing[0].layer      = 0;
    playing[0].respond    = {EVENT_TEST, 0.5, 0.0};
    playing[0].current    = playing[0].respond;

    printf("[Audio] Beginning Polling\n");
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
        // on this topic, what if we made this a part of the event? for example a "looping" flag
        // if something is looping then we also need to emit an event to stop the looping ... 
        if (total_time > playing[0].end_time) {
            playing[0].start_time  = total_time;
            playing[0].end_time   += total_time;
            playing[0].current     = playing[0].respond;
        }

        // Playing Sounds -> Layers
        {
            // FIXME - offset should be calculted based on the elapsed time
            // could do this with a file thats a 3 second long sine wave, each second a different frequency.
            // will fill the whole buffer then 3 times etc.
            u32 offset = 0;
            for (u32 idx = 0; idx < BUFFER_LEN; idx++) {

                // FIXME: not taking into account sound data at all lmao
                // sound data is:
                // - range of [-2^(depth-1), 2^(depth-1)]
                // - interleaved (channels)
                // - variable depth

                // if out of sound samples just play 0
                u32 sample_idx = offset + idx;
                if (sample_idx >= (sound.data_length / sound.depth)) {
                    buffers.layers[0][0][idx] = 0.0;
                    buffers.layers[0][1][idx] = 0.0;
                    continue;
                }

                // FIXME - this obviously needs a cleanup
                if (sound.channels == 1) {
                    if (sound.depth == 16) {
                        i16* interp = (i16*) sound.data;
                        buffers.layers[0][0][idx] = f32(interp[sample_idx]) / (1<<15);
                        buffers.layers[0][1][idx] = f32(interp[sample_idx]) / (1<<15);
                        continue;
                    }
                }

                if (sound.channels == 2) {
                    if (sound.depth == 8) {
                        i8* interp = (i8*) sound.data;
                        buffers.layers[0][0][idx] = f32(interp[(2*sample_idx)])   / (1<<7);
                        buffers.layers[0][1][idx] = f32(interp[(2*sample_idx)+1]) / (1<<7);
                        continue;
                    } 
                    
                    if (sound.depth == 16) {
                        i16* interp = (i16*) sound.data;
                        buffers.layers[0][0][idx] = f32(interp[(2*sample_idx)])   / (1<<15);
                        buffers.layers[0][1][idx] = f32(interp[(2*sample_idx)+1]) / (1<<15);
                        continue;
                    }
                }

            }
        }

        // Mixing Layers -> Master
        {
            // accumulate sounds from all layers
            for (u32 layer_idx = 0; layer_idx < N_LAYERS; layer_idx++) {
                for (u32 idx = 0; idx < BUFFER_LEN; idx++) {
                    buffers.master[0][idx] += buffers.layers[layer_idx][0][idx];
                    buffers.master[1][idx] += buffers.layers[layer_idx][1][idx];
                }
            }
            
            // TODO: this is where we would normalize, or something
        }

        // Master -> Output
        {
            void* region_a       = nullptr;
            void* region_b       = nullptr;
            u32   region_a_bytes = NULL;
            u32   region_b_bytes = NULL;

            // NOTE: Data should not be written to the part of the buffer after the play cursor and before the write cursor
            u32 play_cursor  = NULL;
            u32 write_cursor = NULL;
            hr = buffers.off_buffer->GetCurrentPosition((LPDWORD)&play_cursor, (LPDWORD)&write_cursor);
            DEBUG_ERROR("Failed to get cursor positions\n");

            // write_bytes are the byte size of the locked region
            u32 write_bytes;
            if (write_cursor == play_cursor) {
                write_bytes = BUFFER_LEN;                 // fill the whole buffer up
            } else if (write_cursor > play_cursor) {
                write_bytes  = BUFFER_LEN - write_cursor; // go to the end of the ring
                write_bytes += play_cursor;               // go to the play cursor
            } else if (write_cursor < play_cursor){
                write_bytes = play_cursor - write_cursor; // go to the play cursor
            }

            hr = buffers.off_buffer->Lock(write_cursor, write_bytes, 
                    &region_a, (LPDWORD)&region_a_bytes, 
                    &region_b, (LPDWORD)&region_b_bytes,
                    NULL);

            DEBUG_ERROR("Failed to lock\n");

            if (SUCCEEDED(hr)) { 
                u32  a_offset = region_a_bytes / OUTPUT_SAMPLE_BYTES;
                i16* ra       = (i16*)region_a;
                for (u32 i = 0; i < a_offset; i++) {
                    // set output
                    *(ra++) = i16(buffers.master[0][i] * 32768) * (2.0f*(f32(i) / f32(a_offset))-1.0f);
                    *(ra++) = i16(buffers.master[1][i] * 32768) * (2.0f*(f32(i) / f32(a_offset))-1.0f);

                    // clear master
                    buffers.master[0][i] = 0.0;
                    buffers.master[1][i] = 0.0;
                }

                u32  b_offset = region_b_bytes / OUTPUT_SAMPLE_BYTES;
                i16* rb       = (i16*)region_b;
                for (u32 i = 0; i < b_offset; i++) {
                    // set output
                    *(rb++) = i16(buffers.master[0][i+a_offset] * 32768) * (2.0f*(1.0f - (f32(i) / f32(b_offset)))-1.0f);
                    *(rb++) = i16(buffers.master[1][i+a_offset] * 32768) * (2.0f*(1.0f - (f32(i) / f32(b_offset)))-1.0f);

                    // clear master
                    buffers.master[0][i+a_offset] = 0.0;
                    buffers.master[1][i+a_offset] = 0.0;
                }

                hr = buffers.off_buffer->Unlock(region_a, region_a_bytes, region_b, region_b_bytes);
                DEBUG_ERROR("Failed to unlock\n");

                hr = buffers.off_buffer->Play(NULL, NULL, DSBPLAY_LOOPING);
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
        return;
    }
}
