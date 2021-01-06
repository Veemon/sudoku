#include "proj_sound.h"

// -- Buffers
AudioBuffers buffers;

void init_buffers(u64 length) {
    buffers.length = length;

    // layers
    for (u16 i = 0; i < N_LAYERS; i++) {
        for (u8 c = 0; c < LAYER_CHANNELS; c++) {
            buffers.layers[i][c] = (f32*) malloc(sizeof(f32) * length);
        }
    }

    // master
    for (u8 c = 0; c < MASTER_CHANNELS; c++) {
        buffers.master[c] = (f32*) malloc(sizeof(f32) * length);
    }
}

// -- Sounds
Sound sounds[N_SOUNDS];

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

    sound->data = malloc(length);
    fseek(file, 44, SEEK_SET);
    fread(sound->data, sizeof(u8), length, file);

    // sample length and time
    sound->length  = length / ((sound->depth>>3) * sound->channels);
    sound->time_us = (i64(sound->length) * pow_10[6]) / sound->sample_rate;

    printf("[Audio] Sound - %s\n", filename);
    printf("  -  format        %u\n",   format);
    printf("  -  channels      %u\n",   sound->channels);
    printf("  -  sample rate   %u\n",   sound->sample_rate);
    printf("  -  depth         %u\n",   sound->depth);
    printf("  -  length        %u\n",   sound->length);
    printf("  -  time us       %lld\n", sound->time_us);
    printf("  -  data          %p\n",   sound->data);

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
void sound_to_layer(Status* status) {
    Sound* sound = &sounds[status->sound_id];
    u16 layer    = status->layer;
    u32 offset   = status->offset;

    // FIXME: assumes mono audio :(
    // NOTE: for now applying the radial interpretation of the angle,
    //       but there are many methods of doing positioning, and so you would
    //       most like not want to do that here.
    f32 lmod = status->volume;
    f32 rmod = status->volume;

    const f32 delta = 0.05; // essentially width of sound
    lmod *= 1.0 - (status->angle - delta);
    rmod *= status->angle + delta;

    lmod = clip(lmod, 0.0f, 1.0f);
    rmod = clip(rmod, 0.0f, 1.0f);

    // NOTE: assumes sound is same sample rate as output device
    for (u32 idx = 0; idx < buffers.length; idx++) {
        i64 sample_idx = offset + idx;
        if (sample_idx > sound->length - 1) {
            return;
        }

        if (sound->channels == 1) {
            if (sound->depth == 8) {
                i8* interp = (i8*) sound->data;
                buffers.layers[layer][0][idx] += f32(interp[sample_idx]) * lmod / (1<<7);
                buffers.layers[layer][1][idx] += f32(interp[sample_idx]) * rmod / (1<<7);
                continue;
            }

            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                buffers.layers[layer][0][idx] += f32(interp[sample_idx]) * lmod / (1<<15);
                buffers.layers[layer][1][idx] += f32(interp[sample_idx]) * rmod / (1<<15);
                continue;
            }
        }

        if (sound->channels == 2) {
            if (sound->depth == 8) {
                i8* interp = (i8*) sound->data;
                buffers.layers[layer][0][idx] += f32(interp[(2*sample_idx)])   / (1<<7);
                buffers.layers[layer][1][idx] += f32(interp[(2*sample_idx)+1]) / (1<<7);
                continue;
            } 
            
            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                buffers.layers[layer][0][idx] += f32(interp[(2*sample_idx)])   / (1<<15);
                buffers.layers[layer][1][idx] += f32(interp[(2*sample_idx)+1]) / (1<<15);
                continue;
            }
        }
    }
}

void mix_to_master() {
    f32 _max = 1.0f;

    // accumulate sounds from all layers
    for (u32 idx = 0; idx < buffers.length; idx++) {
        for (u32 layer_idx = 0; layer_idx < N_LAYERS; layer_idx++) {
            // write to master
            buffers.master[0][idx] += buffers.layers[layer_idx][0][idx];
            buffers.master[1][idx] += buffers.layers[layer_idx][1][idx];

            // clear layer
            buffers.layers[layer_idx][0][idx] = 0.0;
            buffers.layers[layer_idx][1][idx] = 0.0;
        }
        
#if 1
        // get clipping values
        for (u8 c = 0; c < MASTER_CHANNELS; c++) {
            f32 mag = abs(buffers.master[c][idx]);
            if (mag > _max - EPS) _max = mag;
        }
#endif
    }
    
#if 1
    // max normalization
    // -- because the buffer should be small to minimize latency,
    //    it should be fine to apply simple max-rescale over the whole buffer.
    f32 _max_recip = 1.0f / (_max + EPS);
    for (u32 i = 0; i < buffers.length; i++) {
        for (u8 c = 0; c < MASTER_CHANNELS; c++) {
            buffers.master[c][i] *= _max_recip;
        }
    }
#endif

#if 1
    // smooth end transition
    for (u8 c = 0; c < MASTER_CHANNELS; c++) {
        f32 avg = 0.5f * (buffers.master[c][buffers.length-1] + buffers.prev_end[c]);
        buffers.master[c][buffers.length-1] = avg;
    }
#endif
}


// -- Platform Specifics

void init_wasapi(WASAPI_Info* info) {
    HRESULT hr;

    hr = CoCreateInstance(
           __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
           __uuidof(IMMDeviceEnumerator),
           (void**)&info->device_enum);
    DEBUG_ERROR("Failed to create device enumerator.\n");

    hr = info->device_enum->GetDefaultAudioEndpoint(eRender, eConsole, &info->device);
    DEBUG_ERROR("Failed to get default endpoint.\n");

    hr = info->device->Activate(
                    __uuidof(IAudioClient), CLSCTX_ALL,
                    NULL, (void**)&info->audio_client);
    DEBUG_ERROR("Failed to activate audio client.\n");

    hr = info->audio_client->GetMixFormat(&info->mix_fmt);
    DEBUG_ERROR("Failed to get mix format\n");

    // minimum size check for EXTENSIBLE format
    if (info->mix_fmt->cbSize >= 22) {
        WAVEFORMATEXTENSIBLE* ext = (WAVEFORMATEXTENSIBLE*)info->mix_fmt;
        info->valid_bits     = ext->Samples.wValidBitsPerSample; 
        info->floating_point = ext->SubFormat.Data1 == 3;
    }

    // REFERENCE_TIME is expressed in 100-nanosecond units
    // FIXME: 2048 is just a nice buffer size. idk?

#if 0
    REFERENCE_TIME min_time = (i64)(2048) * pow_10[7];
    min_time /= info->mix_fmt->nSamplesPerSec;
#endif

    REFERENCE_TIME min_time = 100000000;

    hr = info->audio_client->Initialize(
                         AUDCLNT_SHAREMODE_SHARED,
                         0,
                         min_time,
                         0,
                         info->mix_fmt,
                         NULL);
    DEBUG_ERROR("Failed to init audio client\n");

    hr = info->audio_client->GetBufferSize(&info->length); // samples not bytes
    DEBUG_ERROR("Failed to get buffer size\n");

    printf("[Audio] Mix format\n");
    printf("  -  format        %u\n", info->mix_fmt->wFormatTag);
    printf("  -  channels      %u\n", info->mix_fmt->nChannels);
    printf("  -  sample rate   %u\n", info->mix_fmt->nSamplesPerSec);
    printf("  -  depth         %u\n", info->mix_fmt->wBitsPerSample);
    printf("  -  length        %u\n", info->length);
    printf("  -  cbSize        %u\n", info->mix_fmt->cbSize);
    printf("  -  valid bits    %u\n", info->valid_bits);
    printf("  -  floating      %u\n", info->floating_point);

    hr = info->audio_client->GetService(
                         __uuidof(IAudioRenderClient),
                         (void**)&info->render_client);
    DEBUG_ERROR("Failed to get render client\n");
}

void output_buffer_wasapi(WASAPI_Info* info) {
    HRESULT hr;

    u32 padding;
    hr = info->audio_client->GetCurrentPadding(&padding);
    DEBUG_ERROR("Failed to get padding\n");

    u32 open = info->length - padding;

    u8* data;
    hr = info->render_client->GetBuffer(open, &data);
    DEBUG_ERROR("Failed to Lock buffer\n");

    // FIXME - says 32 bit???
    /*
    i16* interp = (i16*) data;
    for (u32 i = 0; i < open; i++) {
        *interp = i16(buffers.master[0][i] * (1<<15));
         interp++;

        *interp = i16(buffers.master[1][i] * (1<<15));
         interp++;
    }
    */

    i16* sound_ptr = (i16*)sounds[SOUND_SIN_HIGH].data;
    f32* interp = (f32*) data;
    for (u32 i = 0; i < open; i++) {
        if (i > sounds[SOUND_SIN_HIGH].length) {
            *interp = 0.0f;
        } else {
            *interp = f32(sound_ptr[i]) / (1<<15);
        }
         interp++;
    }

    hr = info->render_client->ReleaseBuffer(open, NULL);
    DEBUG_ERROR("Failed to Release buffer\n");
}


void audio_loop(ThreadArgs* args) {
    printf("[Audio] Initialising Output Buffer\n");

    WASAPI_Info winfo;
    init_wasapi(&winfo);
    init_buffers(winfo.length);

    printf("[Audio] Loading Sounds\n");
    memset(&sounds[0], 0, sizeof(Sound) * N_SOUNDS);
    {
        Sound* ptr = &sounds[0];
        wav_to_sound("./res/422hz_-2db_3s_48khz.wav",       ptr + SOUND_SIN_LOW);
        wav_to_sound("./res/740hz_-2db_3s_48khz.wav",       ptr + SOUND_SIN_HIGH);
        wav_to_sound("./res/10hz_10khz_-2db_3s_48khz.wav",  ptr + SOUND_SWEEP);
        wav_to_sound("./res/voice_stereo_48khz.wav",        ptr + SOUND_VOICE);
    }

    // FIXME - resample sounds to target sample_rate
    
    output_buffer_wasapi(&winfo);
    HRESULT hr = winfo.audio_client->Start();
    DEBUG_ERROR("FAILED TO START\n");

    printf("[Audio] Ending\n");
    while(1);
    return;


    // timing
    LARGE_INTEGER start_time, end_time, delta_us, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_us.QuadPart = 0;
    i64 total_time_us = 0;

    // loop locals
    RingBuffer* events = &(args->events);
    Status active_sounds[N_EVENTS];
    u16 active_ptr = 0; // -- active_sounds is circular

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
                        if (iter.mode == EventMode::default) break;
                        active_sounds[active_ptr].sound_id      = iter.sound_id;
                        active_sounds[active_ptr].mode          = iter.mode;
                        active_sounds[active_ptr].layer         = iter.layer;
                        active_sounds[active_ptr].offset        = 0;
                        active_sounds[active_ptr].last_write_us = total_time_us;
                        active_sounds[active_ptr].end_time_us   = total_time_us + sounds[iter.sound_id].time_us;
                        active_sounds[active_ptr].volume        = iter.volume;
                        active_sounds[active_ptr].angle         = iter.angle;
                        active_ptr = (active_ptr+1) % N_EVENTS;
                    }
                }

                args->new_event = 0;
                ReleaseMutex(args->mutex);

                // DEBUG
                #define X  active_sounds[i]
                for (u32 i = 0; i < N_EVENTS; i++) {
                    printf("sid: %2u  mode: %2u  layer: %2u  off: %8u  lw: %8lld  ew: %8lld\n",
                            X.sound_id, X.mode, X.layer, X.offset, X.last_write_us, X.end_time_us);
                }
                printf("\n");
                #undef X
            }
            #undef iter
        }


        // FIXME -- writing every frame so we have something to time

        // write sounds to layers
        for (u16 sidx = 0; sidx < N_EVENTS; sidx++) {
            if (active_sounds[sidx].mode != EventMode::default) {
                sound_to_layer(&active_sounds[sidx]);

                // handle dead sounds
                if (total_time_us > active_sounds[sidx].end_time_us) {
                    active_sounds[sidx].mode = EventMode::default;
                }
            }
        }

        // collapse layers to master
        mix_to_master();

        //FIXME need to clear master
         
#if 0   
        if (output_write_timer >= output_time_us) {
            output_write_timer -= output_time_us;

            // update sound timing offsets
            u32 samples_written = bytes_written / OUTPUT_SAMPLE_BYTES;
            for (u16 sidx = 0; sidx < N_EVENTS; sidx++) {
                if (active_sounds[sidx].mode == EventMode::default) continue;
                // active_sounds[sidx].offset += samples_written;
                active_sounds[sidx].offset += output_samples;
            }
        } else {
            memset(&buffers.master[0][0], 0.0f, sizeof(f32) * BUFFER_LEN);
            memset(&buffers.master[1][0], 0.0f, sizeof(f32) * BUFFER_LEN);
        }
#endif

        // timing
        QueryPerformanceCounter(&end_time);
        delta_us.QuadPart = end_time.QuadPart - start_time.QuadPart;
        delta_us.QuadPart *= pow_10[6];
        delta_us.QuadPart /= cpu_freq.QuadPart;

        total_time_us += delta_us.QuadPart;

#if 0        
        // FIXME we literally go too fast
        if (delta_us.QuadPart < 10) {
            printf("%lld\n", delta_us.QuadPart);
        }
#endif
    }
}
