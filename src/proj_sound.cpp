#include "proj_sound.h"


Sound sounds[N_SOUNDS];
AudioBuffers buffers;

u8 not_playing = 1;

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

#if 0
    // averaging
    for (u32 i = 0; i < length; i++) {
        u32 idx = i / width;
        if (idx > x_samples - 1) break;
        desc[idx] += f32(data[i]);
    }
    for (u32 i = 0; i < x_samples; i++) {
        desc[i] /= (f32)width;
    }
#else
    // midpoints
    u32 hw = width >> 1;
    u32 acc = hw;
    for (u32 i = 0; i < x_samples; i++) {
        desc[i] = data[acc];
        acc += hw;
        if (acc > length - 1) break;
    }
#endif


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

    sound->data = malloc(length);
    fseek(file, 44, SEEK_SET);
    fread(sound->data, sizeof(u8), length, file);

    // sample length and time
    sound->length  = sound->byte_length / ((sound->depth>>3) * sound->channels);
    sound->time_ms = (i64(sound->length) * 1000) / sound->sample_rate;

    printf("[Audio] Sound - %s\n", filename);
    printf("  -  format        %u\n", format);
    printf("  -  channels      %u\n", sound->channels);
    printf("  -  sample rate   %u\n", sound->sample_rate);
    printf("  -  depth         %u\n", sound->depth);
    printf("  -  bytes         %u\n", sound->byte_length);
    printf("  -  length        %u\n", sound->length);
    printf("  -  time ms       %lld\n", sound->time_ms);
    printf("  -  data          %p\n", sound->data);

    fclose(file);
    return 1;

    #undef BIG_32
    #undef BIG_16
}

// -- RingBuffer Utils

void ring_push(RingBuffer* rb, Event e) {
    rb->ptr = (rb->ptr+1) % N_EVENTS;
    rb->ring[rb->ptr] = e;
}


// -- Audio Pipeline functions

void sound_to_layer(Sound* sound, u16 layer, i64 offset) {
    for (u32 idx = 0; idx < BUFFER_LEN; idx++) {
        // FIXME: not taking into account sound data at all lmao
        // sound data is:
        // - range of [-2^(depth-1), 2^(depth-1)]
        // - interleaved (channels)
        // - variable depth

        // if out of sound samples just play 0
        i64 sample_idx = offset + idx;
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
        DSBUFFERDESC buffer_desc;
        memset(&buffer_desc, 0, sizeof(DSBUFFERDESC));

        buffer_desc.dwSize          = sizeof(DSBUFFERDESC);
        buffer_desc.dwFlags         = DSBCAPS_PRIMARYBUFFER;
        buffer_desc.dwBufferBytes   = 0;
        buffer_desc.dwReserved      = 0;
        buffer_desc.lpwfxFormat     = NULL;
        buffer_desc.guid3DAlgorithm = GUID_NULL;

        hr = direct_sound->CreateSoundBuffer(&buffer_desc, &buffers.main_buffer, 0);
        DEBUG_ERROR("Failed to create main buffer\n");

        hr = buffers.main_buffer->SetFormat(&wave_fmt);
        DEBUG_ERROR("Failed to set main format\n");
    }

    {
        DSBUFFERDESC buffer_desc;
        memset(&buffer_desc, 0, sizeof(DSBUFFERDESC));

        buffer_desc.dwSize          = sizeof(DSBUFFERDESC);
        buffer_desc.dwFlags         = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        buffer_desc.dwBufferBytes   = BUFFER_LEN * OUTPUT_SAMPLE_BYTES;
        buffer_desc.dwReserved      = 0;
        buffer_desc.lpwfxFormat     = &wave_fmt;
        buffer_desc.guid3DAlgorithm = GUID_NULL;

        hr = direct_sound->CreateSoundBuffer(&buffer_desc, &buffers.off_buffer, 0);
        DEBUG_ERROR("Failed to create off buffer\n");
    }
}

u32 output_buffer(u32 write_cursor, u32 play_cursor) {
    HRESULT hr;

    void* region_a       = nullptr;
    void* region_b       = nullptr;
    u32   region_a_bytes = 0;
    u32   region_b_bytes = 0;

    // write_bytes are the byte size of the locked region
    u32 write_bytes = 0;
    if (write_cursor == play_cursor) {
        write_bytes = BUFFER_LEN * OUTPUT_SAMPLE_BYTES;  // fill the whole buffer up
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

    printf("--- OFF WE GO ---\n");

    if (SUCCEEDED(hr)) { 
        u32  a_offset = region_a_bytes / OUTPUT_SAMPLE_BYTES;
        i16* ra       = (i16*)region_a;
        for (u32 i = 0; i < a_offset; i++) {
            // set output
            *(ra++) = i16(buffers.master[0][i] * (1<<15));
            *(ra++) = i16(buffers.master[1][i] * (1<<15));

            // clear master
            buffers.master[0][i] = 0.0;
            buffers.master[1][i] = 0.0;
        }


        u32  b_offset = region_b_bytes / OUTPUT_SAMPLE_BYTES;
        i16* rb       = (i16*)region_b;
        for (u32 i = 0; i < b_offset; i++) {
            // set output
            *(rb++) = i16(buffers.master[0][a_offset+i] * (1<<15));
            *(rb++) = i16(buffers.master[1][a_offset+i] * (1<<15));

            // clear master
            buffers.master[0][a_offset+i] = 0.0;
            buffers.master[1][a_offset+i] = 0.0;
        }

        hr = buffers.off_buffer->Unlock(region_a, region_a_bytes, region_b, region_b_bytes);

        DEBUG_ERROR("Failed to unlock\n");

        hr = buffers.off_buffer->Play(NULL, NULL, DSBPLAY_LOOPING);
        DEBUG_ERROR("Failed to play\n");
    }

    return region_a_bytes + region_b_bytes;
}




void audio_loop(ThreadArgs* args) {
    printf("[Audio] Initialising Output Buffer\n");
    init_directsound(args->hwnd);

    printf("[Audio] Loading Sounds\n");
    wav_to_sound("./res/422hz_-2db_3s_48khz.wav", &sounds[0] + SOUND_SIN);
    wav_to_sound("./res/10hz_10khz_-2db_3s_48khz.wav",  &sounds[0] + SOUND_SWEEP);


    // timing
    LARGE_INTEGER start_time, end_time, delta_us, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_us.QuadPart = 0;

    i64 total_time_us = 0;
    i64 total_time_ms = 0;

    // loop locals
    RingBuffer* events = &(args->events);
    Status active_sounds[N_SOUNDS];

    LARGE_INTEGER write_delta_us, last_write;
    write_delta_us.QuadPart = 0;
    last_write.QuadPart = 0;

    u32 sound_offset = 0;

    u32 write_offset = 0; // essentially our write cursor
    u32 write_cursor = 0;
    u32 play_cursor  = 0;
    buffers.off_buffer->GetCurrentPosition((LPDWORD)&play_cursor, (LPDWORD)&write_cursor);

    printf("[Audio] Beginning Polling\n");
    while (1) {
        QueryPerformanceCounter(&start_time);

        // respond to main thread
        if (args->new_event) {
            #define iter    args->events.ring[i]
            u32 sig = WaitForSingleObject(args->mutex,0);
            if (sig == WAIT_OBJECT_0) {
                printf("[Audio] Recv. New Events\n");

                // iteration over ring buffer
                u32 start_args[] = {args->events.ptr, 0};
                u32 end_args[]   = {N_EVENTS, args->events.ptr};
                for (u8 outer = 0; outer < 2; outer++) {
                    for (u32 i = start_args[outer]; i < end_args[outer]; i++) {
                        if (iter.mode == MODE_DEFAULT) break;
                        active_sounds[iter.sound_id].mode          = iter.mode;
                        active_sounds[iter.sound_id].layer         = iter.layer;
                        active_sounds[iter.sound_id].start_time_us = total_time_us;
                        active_sounds[iter.sound_id].end_time_ms   = total_time_ms + sounds[iter.sound_id].time_ms;
                        active_sounds[iter.sound_id].volume        = iter.volume;
                        active_sounds[iter.sound_id].angle         = iter.angle;
                    }
                }

                args->new_event = 0;
                ReleaseMutex(args->mutex);

                // Debug active sounds
                #define X   active_sounds[i]
                printf("[Audio] Sounds\n------------------------------------------------\n");
                for (u32 i = 0; i < N_SOUNDS; i++) {
                    printf("[%u] mode: %2u  layer: %2u   t0: %lld  t1: %lld\n", 
                            i, X.mode, X.layer, X.start_time_us, X.end_time_ms);
                }
                #undef X
            }
            #undef iter
        }


        // FIXME - DEBUG
        active_sounds[0].mode = MODE_START;

        // poll for when safe to write
        while (write_cursor < write_offset) {
            buffers.off_buffer->GetCurrentPosition((LPDWORD)&play_cursor, (LPDWORD)&write_cursor);
        }

        // write sounds to layers and mix
        if (active_sounds[0].mode != MODE_DEFAULT) {
            sound_to_layer(&sounds[0], 0, sound_offset);
            mix_to_master();

            u32 bytes_written;
            bytes_written = output_buffer(write_offset, play_cursor);
            write_offset = (write_offset + bytes_written) % (BUFFER_LEN * OUTPUT_SAMPLE_BYTES);

            // Now, why doesn't this work you may ask???
            // An excellent question my beloved friend. Now you see russel,
            // my suspicion leads me to believe that we write to output every iteration.
            // sound_offset += bytes_written / OUTPUT_SAMPLE_BYTES; 
        }


        // timing
        QueryPerformanceCounter(&end_time);
        delta_us.QuadPart = end_time.QuadPart - start_time.QuadPart;
        delta_us.QuadPart *= pow_10[6];
        delta_us.QuadPart /= cpu_freq.QuadPart;

        total_time_us += delta_us.QuadPart;
        total_time_ms = total_time_us / 1000;
    }
}
