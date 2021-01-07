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

void ring_push(RingBuffer* rb, Event e) {
    rb->ptr = (rb->ptr+1) % N_EVENTS;
    rb->ring[rb->ptr] = e;
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

void resample_sound(Sound* sound, u32 rate, u8 quality) {
    // de-interleave data
    f32** data = (f32**) malloc(sizeof(f32*) * sound->channels);
    for (u8 c = 0; c < sound->channels; c++) {
        data[c] = (f32*) malloc(sizeof(f32) * sound->length);
    }

    for (u32 i = 0; i < sound->length; i++) {
        if (sound->channels == 1) {
            if (sound->depth == 8) {
                i8* interp = (i8*) sound->data;
                data[0][i] = f32(interp[i]) / (1<<7);
                continue;
            }

            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                data[0][i]  = f32(interp[i]) / (1<<15);
                continue;
            }
        }

        if (sound->channels == 2) {
            if (sound->depth == 8) {
                i8* interp = (i8*) sound->data;
                data[0][i] = f32(interp[(2*i)])   / (1<<7);
                data[1][i] = f32(interp[(2*i)+1]) / (1<<7);
                continue;
            } 
            
            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                data[0][i] = f32(interp[(2*i)])   / (1<<15);
                data[1][i] = f32(interp[(2*i)+1]) / (1<<15);
                continue;
            }
        }
    }

    // allocate resampling buffers
    // NOTE: for partically long sounds, the length will overflow
    f32** out = (f32**) malloc(sizeof(f32*) * sound->channels);
    u64 length = (u64)rate * sound->length / sound->sample_rate;
    for (u8 c = 0; c < sound->channels; c++) {
        u32 size = sizeof(f32) * length;
        out[c] = (f32*) malloc(size);
        memset(out[c], 0, size);
    }

    if (quality) {
        // FIXME - not implemented correctly
        // whittaker-shannon interpolation
        f32 T = f32(BUFFER_SIZE) / rate;
        for (u8 c = 0; c < sound->channels; c++) {
            for (u32 i = 0; i < length; i++) {
                f32 t = f32(i) * T;
                for (u32 j = 0; j < BUFFER_SIZE; j++) {
                    i32 n = -(i32)(BUFFER_SIZE>>1) + i + j;
                    if (n < 0 || n > sound->length - 1) continue;
                    out[c][i] += data[c][n] * sinc((t - n*T)/T);
                }
            }
        }
    }
    else {
        // linear interpolation
        f64 time = f64(sound->time_us) / pow_10[6];
        f64 dt_in  = time / (sound->sample_rate - 1);
        f64 dt_out = time / (rate - 1);
        
        for (u8 c = 0; c < sound->channels; c++) {
            f64 t_in   = 0.0f;
            f64 t_out  = 0.0f;
            u32 in_idx = 0;
            for (u32 out_idx = 0; out_idx < length; out_idx++) {
                while (t_out > t_in+dt_in) { in_idx++; t_in += dt_in; }

                f64 next_t_in = t_in + dt_in;
                f64 p = (next_t_in - t_out) / next_t_in;

                out[c][out_idx] = (p)*data[c][in_idx];
                if (in_idx+1 > sound->length - 1) break;
                out[c][out_idx] += (1.0f-p)*data[c][in_idx+1];

                t_out += dt_out;
            }
        }
    }

    // re-interleave resampled data
    free(sound->data);
    sound->length      = length;
    sound->sample_rate = rate;
    sound->data        = malloc(length * sound->channels * (sound->depth>>3));
    for (u32 i = 0; i < length; i++) {
        if (sound->channels == 1) {
            if (sound->depth == 8) {
                i8* interp = (i8*) sound->data;
                interp[i]  = i8(out[0][i] * (1<<7));
                continue;
            }

            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                interp[i]   = i16(out[0][i] * (1<<15));
                continue;
            }
        }

        if (sound->channels == 2) {
            if (sound->depth == 8) {
                i8* interp = (i8*) sound->data;
                interp[(2*i)]   = i8(out[0][i] * (1<<7));
                interp[(2*i)+1] = i8(out[1][i] * (1<<7));
                continue;
            } 
            
            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                interp[(2*i)]   = i16(out[0][i] * (1<<15));
                interp[(2*i)+1] = i16(out[1][i] * (1<<15));
                continue;
            }
        }
    }


    // cleanup
    for (u8 c = 0; c < sound->channels; c++) {
        free(data[c]);
        free(out[c]);
    }
    free(data);
    free(out);
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
            if (layer_idx == 0) {
                buffers.master[0][idx] = buffers.layers[layer_idx][0][idx];
                buffers.master[1][idx] = buffers.layers[layer_idx][1][idx];
            } else {
                buffers.master[0][idx] += buffers.layers[layer_idx][0][idx];
                buffers.master[1][idx] += buffers.layers[layer_idx][1][idx];
            }

            // clear layer
            buffers.layers[layer_idx][0][idx] = 0.0;
            buffers.layers[layer_idx][1][idx] = 0.0;
        }
        
        // get clipping values
        for (u8 c = 0; c < MASTER_CHANNELS; c++) {
            f32 mag = abs(buffers.master[c][idx]);
            if (mag > _max - EPS) _max = mag - EPS;
        }
    }
    
    // max normalization
    // -- because the buffer should be small to minimize latency,
    //    it should be fine to apply simple max-rescale over the whole buffer.
    f32 _max_recip = 1.0f / (_max + EPS);
    for (u32 i = 0; i < buffers.length; i++) {
        for (u8 c = 0; c < MASTER_CHANNELS; c++) {
            buffers.master[c][i] *= _max_recip;
        }
    }

    // smooth end transition
    for (u8 c = 0; c < MASTER_CHANNELS; c++) {
        f32 avg = 0.5f * (buffers.master[c][buffers.length-1] + buffers.prev_end[c]);
        buffers.master[c][buffers.length-1] = avg;
    }
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
    // NOTE: 2048 is just a nice buffer size. idk?
    REFERENCE_TIME min_time = (i64)(BUFFER_SIZE) * pow_10[7];
    min_time /= info->mix_fmt->nSamplesPerSec;

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

u32 output_buffer_wasapi(WASAPI_Info* info) {
    HRESULT hr;

    u32 padding;
    hr = info->audio_client->GetCurrentPadding(&padding);
    DEBUG_ERROR("Failed to get padding\n");

    u32 open = info->length - padding;

    u8* data;
    hr = info->render_client->GetBuffer(open, &data);
    DEBUG_ERROR("Failed to Lock buffer\n");

    if (info->floating_point) {
        f32* interp = (f32*) data;
        for (u32 i = 0; i < open; i++) {
            *interp = buffers.master[0][i];
             interp++;

            *interp = buffers.master[1][i];
             interp++;
        }
    }

    hr = info->render_client->ReleaseBuffer(open, NULL);
    DEBUG_ERROR("Failed to Release buffer\n");

    if (!info->started) {
        info->started = 1;
        HRESULT hr = info->audio_client->Start();
        DEBUG_ERROR("FAILED TO START\n");
    }

    return open;
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

    // resample sounds to target sample_rate
    printf("[Audio] Resampling\n");
    for (u16 i = 0; i < N_SOUNDS; i++) {
        printf(" - [%u]    %u  ->  %u\n", i, sounds[i].sample_rate, winfo.mix_fmt->nSamplesPerSec);
        resample_sound(&sounds[i], winfo.mix_fmt->nSamplesPerSec, 0);
    }


    // let main thread now we have initialized
    args->init = 1;


    // timing
    LARGE_INTEGER start_time, end_time, delta_us, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_us.QuadPart = 0;
    i64 total_time_us = 0;

    i64 output_time_us = (u64(winfo.length)>>1) * pow_10[6];
    output_time_us /= winfo.mix_fmt->nSamplesPerSec;
    i64 output_write_timer = output_time_us;

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
            if (active_sounds[sidx].mode == EventMode::default) continue;
            sound_to_layer(&active_sounds[sidx]);
        }

        // collapse layers to master
        mix_to_master();

        if (output_write_timer >= output_time_us) {
            output_write_timer -= output_time_us;
    
            u32 samples_written = output_buffer_wasapi(&winfo);

            // update sound timing offsets
            for (u16 sidx = 0; sidx < N_EVENTS; sidx++) {
                if (active_sounds[sidx].mode == EventMode::default) continue;
                active_sounds[sidx].offset += samples_written;
                
                // handle dead sounds
                if (total_time_us > active_sounds[sidx].end_time_us) {
                    active_sounds[sidx].mode = EventMode::default;
                }
            }
        }

        // timing
        QueryPerformanceCounter(&end_time);
        delta_us.QuadPart = end_time.QuadPart - start_time.QuadPart;
        delta_us.QuadPart *= pow_10[6];
        delta_us.QuadPart /= cpu_freq.QuadPart;

        total_time_us      += delta_us.QuadPart;
        output_write_timer += delta_us.QuadPart;
    }
}
