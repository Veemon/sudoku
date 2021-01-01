#include "proj_sound.h"


SoundLibrary sounds;
AudioBuffers buffers;


// -- Sound Utils

void graph_buffer(u16 x_samples, u16 y_samples, i16* data, u32 length) {
    // grab range of data
    f32 min = F32_MAX;
    f32 max = 0.0f;
    for (u32 i = 0; i < length; i++) {
        if (data[i] > max) max = data[i];
        if (data[i] < min) min = data[i];
    }

    // count the maximum number of digits on lhs of period
    i32 max_offset = 0;
    {
        u8 idx = 0;
        while((abs(max) / pow_10[idx++]) > 1.0f - 1e-8) { max_offset++; }
    }

    // descretize into x_samples chunks
    u32  width = length / x_samples;
    u32  bytes = x_samples * sizeof(f32);
    f32* desc  = (f32*) malloc(bytes);
    memset(desc, 0, bytes);
    for (u32 i = 0; i < length; i++) {
        u32 idx = i / width;
        if (idx > x_samples - 1) break;
        desc[idx] += f32(data[i]);
    }
    for (u32 i = 0; i < x_samples; i++) {
        desc[i] /= (f32)width;
    }

    // draw graph
    f32 y_next;
    f32 y_acc = max;
    f32 y_delta = (max - min) / f32(y_samples-1);
    for (u16 j = 0; j < y_samples; j++) {
        // left padding
        i32 x_offset = 0;
        if (abs(y_acc)>10.0f-1e-8) {
            u8 idx = 0;
            while((abs(y_acc) / pow_10[idx++]) > 1.0f - 1e-8) { x_offset++; }
        } else {
            x_offset = 2;
        }
        for (u8 i = 0; i < (max_offset-x_offset); i++) { printf(" "); }
        printf("%+8.4f |", y_acc);
        
        // draw samples
        y_next = y_acc - y_delta;
        for (u16 i = 0; i < x_samples; i++) {
            if (desc[i] < y_acc + 1e-8 && desc[i] > y_next - 1e-8) {
                printf("o");
            } else {
                printf(" ");
            }
        }
        y_acc = y_next;
        printf("\n");
    }

    free(desc);
}


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
    sound->byte_length = length;

    sound->length = sound->byte_length / (sound->depth * sound->channels);

    sound->data = malloc(length);
    fseek(file, 44, SEEK_SET);
    fread(sound->data, sizeof(u8), length, file);

    printf("[Audio] Sound - %s\n", filename);
    printf("  -  format        %u\n", format);
    printf("  -  channels      %u\n", sound->channels);
    printf("  -  sample rate   %u\n", sound->sample_rate);
    printf("  -  depth         %u\n", sound->depth);
    printf("  -  bytes         %u\n", sound->byte_length);
    printf("  -  length        %u\n", sound->length);
    printf("  -  data          %p\n", sound->data);

    fclose(file);
    return 1;

    #undef BIG_32
    #undef BIG_16
}

// -- RingBuffer Utils

void ring_push(RingBuffer* rb, Event e) {
    rb->ring[rb->ptr] = e;
    rb->ptr = (rb->ptr+1) % N_EVENTS;
}


// -- Audio Pipeline functions

void sound_to_layer(Sound* sound, u16 layer, u32 offset) {
    // FIXME - offset should be calculted based on the elapsed time
    // could do this with a file thats a 3 second long sine wave, each second a different frequency.
    // will fill the whole buffer then 3 times etc.
    for (u32 idx = 0; idx < BUFFER_LEN; idx++) {

        // FIXME: not taking into account sound data at all lmao
        // sound data is:
        // - range of [-2^(depth-1), 2^(depth-1)]
        // - interleaved (channels)
        // - variable depth

        // if out of sound samples just play 0
        u32 sample_idx = offset + idx;
        if (sample_idx > sound->length - 1) {
            buffers.layers[layer][0][idx] = 0.0;
            buffers.layers[layer][1][idx] = 0.0;
            continue;
        }

        if (sound->channels == 1) {
            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                buffers.layers[layer][0][idx] = f32(interp[sample_idx]) / (1<<15);
                buffers.layers[layer][1][idx] = f32(interp[sample_idx]) / (1<<15);
                continue;
            }
        }

        if (sound->channels == 2) {
            if (sound->depth == 8) {
                i8* interp = (i8*) sound->data;
                buffers.layers[layer][0][idx] = f32(interp[(2*sample_idx)])   / (1<<7);
                buffers.layers[layer][1][idx] = f32(interp[(2*sample_idx)+1]) / (1<<7);
                continue;
            } 
            
            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                buffers.layers[layer][0][idx] = f32(interp[(2*sample_idx)])   / (1<<15);
                buffers.layers[layer][1][idx] = f32(interp[(2*sample_idx)+1]) / (1<<15);
                continue;
            }
        }

    }
}

void mix_to_master() {
    // accumulate sounds from all layers
    for (u32 layer_idx = 0; layer_idx < N_LAYERS; layer_idx++) {
        for (u32 idx = 0; idx < BUFFER_LEN; idx++) {
            buffers.master[0][idx] += buffers.layers[layer_idx][0][idx];
            buffers.master[1][idx] += buffers.layers[layer_idx][1][idx];
        }
    }
    
    // TODO: this is where we would normalize, or something
}


// -- Platform Specifics

void init_directsound(HWND hwnd) {
    HRESULT hr = NULL;

    // initialize
    LPDIRECTSOUND direct_sound;

    hr = DirectSoundCreate(0, &direct_sound, 0);
    DEBUG_ERROR("Failed to init DirectSound\n");

    hr = direct_sound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
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
        buffer_desc.dwFlags       = DSBCAPS_GLOBALFOCUS;
        buffer_desc.dwBufferBytes = BUFFER_LEN;
        buffer_desc.lpwfxFormat   = &wave_fmt;

        hr = direct_sound->CreateSoundBuffer(&buffer_desc, &buffers.off_buffer, 0);
        DEBUG_ERROR("Failed to create off buffer\n");
    }
}


void output_buffer(u32 play_cursor, u32 write_cursor) {
    void* region_a       = nullptr;
    void* region_b       = nullptr;
    u32   region_a_bytes = NULL;
    u32   region_b_bytes = NULL;

    // write_bytes are the byte size of the locked region
    u32 write_bytes = 0;
    if (write_cursor == play_cursor) {
        write_bytes = BUFFER_LEN;                 // fill the whole buffer up
    } else if (write_cursor > play_cursor) {
        write_bytes  = BUFFER_LEN - write_cursor; // go to the end of the ring
        write_bytes += play_cursor;               // go to the play cursor
    } else if (write_cursor < play_cursor){
        write_bytes = play_cursor - write_cursor; // go to the play cursor
    }

    printf("  writing  %u", write_bytes);

    HRESULT hr = NULL;
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
            *(ra++) = i16(buffers.master[0][i] * 32768);
            *(ra++) = i16(buffers.master[1][i] * 32768);

            // clear master
            buffers.master[0][i] = 0.0;
            buffers.master[1][i] = 0.0;
        }

        u32  b_offset = region_b_bytes / OUTPUT_SAMPLE_BYTES;
        i16* rb       = (i16*)region_b;
        for (u32 i = 0; i < b_offset; i++) {
            // set output
            *(rb++) = i16(buffers.master[0][a_offset+i] * 32768);
            *(rb++) = i16(buffers.master[1][a_offset+i] * 32768);

            // clear master
            buffers.master[0][i+a_offset] = 0.0;
            buffers.master[1][i+a_offset] = 0.0;
        }

        // printf("[Audio] Regions -  A: %u    B: %u\n", a_offset, b_offset);

        hr = buffers.off_buffer->Unlock(region_a, region_a_bytes, region_b, region_b_bytes);
        DEBUG_ERROR("Failed to unlock\n");

        hr = buffers.off_buffer->Play(NULL, NULL, DSBPLAY_LOOPING);
        DEBUG_ERROR("Failed to play\n");
    }
}




void audio_loop(ThreadArgs* args) {
    printf("[Audio] Initialising Output Buffer\n");
    init_directsound(args->hwnd);

    printf("[Audio] Loading Sounds\n");
    wav_to_sound("./res/422hz_-2db_3s_48khz.wav", &sounds.sin);
    wav_to_sound("./res/10hz_10khz_-2db_3s_48khz.wav", &sounds.sweep);

    // timing
    f32 total_time = 0.0;
    LARGE_INTEGER start_time, end_time, delta_ms, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_ms.QuadPart = 0;
    f32 delta_s = 0.0;

    RingBuffer* events = &(args->events);
    Status playing[N_SOUNDS];
    u32 play_tail  = 0;


    // loop locals
    f32 last_write = 0.0;
    u32 offset = 0;

    printf("[Audio] Beginning Polling\n");
    while (1) {
        QueryPerformanceCounter(&start_time);

#if 1
        // respond to main thread
        if (args->new_event) {
            u32 sig = WaitForSingleObject(args->mutex,0);
            if (sig == WAIT_OBJECT_0) {
                for (u32 i = args->events.ptr; i < N_EVENTS; i++) {

                }

                args->new_event = 0;
                ReleaseMutex(args->mutex);
            }
        }
#endif

        // ==================================================================

#if 0
        // NOTE: Data should not be written to the part of the buffer after the play cursor and before the write cursor
        u32 play_cursor  = NULL;
        u32 write_cursor = NULL;
        hr = buffers.off_buffer->GetCurrentPosition((LPDWORD)&play_cursor, (LPDWORD)&write_cursor);
        DEBUG_ERROR("Failed to get cursor positions\n");

        u32 distance = play_cursor - play_cursor;
        if (write_cursor < play_cursor) {
            distance = BUFFER_LEN - play_cursor + write_cursor;
        }

        printf("[Audio]  Play %u    Write %u", play_cursor, write_cursor);
        const f32 recip = 1.0f / (f32(OUTPUT_SAMPLE_RATE) * f32(OUTPUT_SAMPLE_BYTES) * f32(OUTPUT_CHANNELS));
        f32 w = distance * recip;
        if ((total_time - last_write) + 1e-8 >= w) {
            last_write = total_time;
            offset += distance;

            sound_to_layer(&sounds.sin, 0, offset);
            mix_to_master();
            output_buffer(play_cursor, write_cursor);
        }
        printf("\n");

        // FIXME
        /* 
           I don't understand the sound buffer.
           the play-write cursor dynamic means it doesn't sync up with my timing

           [0,1,2,3,4,5,6]    44 800 Hz         let the distance d = write - play
            ^     ^               16 bit        let the number of elapsed samples s = d / ((16 / 8) * 2)
            p     w                2 channel    If w = s/44800, then the next time you write must be w seconds from the time you last wrote.

            the offset will then be shifted by s each time

        */

        // FIXME - if we play sound at 44KHz, and this thread is running > 1GHz,
        //         there is only so much processing we can do before we should really be just going to sleep.
        //         that way we're also not hogging the event mutex 

        // FIXME - we can go faster than our delta_s can record, so just delay by 1ms
        // if we run this at 30ms it plays smoother?
        Sleep(1);
#endif

        // timing
        QueryPerformanceCounter(&end_time);
        delta_ms.QuadPart = end_time.QuadPart - start_time.QuadPart;
        delta_ms.QuadPart *= 1000;
        delta_ms.QuadPart /= cpu_freq.QuadPart;
        delta_s = f32(delta_ms.QuadPart) / 1000.f;

        total_time   += delta_s;
    }
}
