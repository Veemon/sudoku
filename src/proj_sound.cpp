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

i32 ring_push(RingBuffer* rb, Event e) {
    rb->ptr = (rb->ptr+1) % N_EVENTS;
    e.id = rb->id;
    rb->id++;
    if (rb->id < 0) rb->id = 0;
    rb->ring[rb->ptr] = e;
    return e.id;
}

void ring_clear(RingBuffer* rb) {
    rb->ptr = 0;
    memset(&rb->ring[0], 0, sizeof(Event) * N_EVENTS);
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

#define QUALITY_LINEAR   0
#define QUALITY_CUBIC    1
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

            if (sound->depth == 24) {
                u8* bytes  = ((u8*) sound->data) + 3*i;
                i32 interp = ( (bytes[2]<<24) | (bytes[1]<<16) | (bytes[0]<<8) );
                data[0][i] = f32(interp) / (1<<31); // -- see sound_to_layer comment
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

    if (quality == QUALITY_LINEAR) {
        printf("[Audio]  --  Linear Interpolation\n");
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
                if (in_idx == sound->length - 1) {
                    f32 p3 = 2.0 * data[c][in_idx+1] - data[c][in_idx];
                    out[c][out_idx] += (1.0-p) * p3;
                } else {
                    out[c][out_idx] += (1.0-p)*data[c][in_idx+1];
                }

                t_out += dt_out;
            }
        }
    }
    else if (quality == QUALITY_CUBIC) {
        printf("[Audio]  --  Cubic Interpolation\n");
        // cubic interpolation
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

                f64 v;
                if (in_idx == 0) {
                    f64 p0 = 2.0 * data[c][in_idx] - data[c][in_idx+1];
                    v = p * (3.0*(data[c][in_idx] - data[c][in_idx+1]) + data[c][in_idx+2] - p0);
                    v = p * (p0 - 5.0*data[c][in_idx] + 4.0*data[c][in_idx+1] - data[c][in_idx+2] + v);
                    v = p * (data[c][in_idx+1] - p0 + v) * 0.5 + data[c][in_idx];
                } else if (in_idx == sound->length - 1) {
                    f64 p3 = 2.0 * data[c][in_idx+1] - data[c][in_idx];
                    v = p * (3.0*(data[c][in_idx] - data[c][in_idx+1]) + p3 - data[c][in_idx-1]);
                    v = p * (2.0*data[c][in_idx-1] - 5.0*data[c][in_idx] + 4.0*data[c][in_idx+1] - p3 + v);
                    v = p * (data[c][in_idx+1] - data[c][in_idx-1] + v) * 0.5 + data[c][in_idx];
                } else {
                    v = p * (3.0*(data[c][in_idx] - data[c][in_idx+1]) + data[c][in_idx+2] - data[c][in_idx-1]);
                    v = p * (2.0*data[c][in_idx-1] - 5.0*data[c][in_idx] + 4.0*data[c][in_idx+1] - data[c][in_idx+2] + v);
                    v = p * (data[c][in_idx+1] - data[c][in_idx-1] + v) * 0.5 + data[c][in_idx];
                }

                out[c][out_idx] = v;

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

            if (sound->depth == 24) {
                i32 val    = i32(out[0][i] * (1<<31));
                u8* bytes  = ((u8*) sound->data) + 3*i;
                bytes[0]   = (val>>8)  & 0xff;
                bytes[1]   = (val>>16) & 0xff;
                bytes[2]   = (val>>24) & 0xff;
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
void mono_radial_from_angle(Status* status, vec4* contrib) {
    while (status->angle > 2.0f + EPS) status->angle -= 2.0f;
    f32 angle = status->angle;

    f32* l  = &contrib->x;
    f32* lr = &contrib->y;
    f32* rl = &contrib->z;
    f32* r  = &contrib->w;

    const vec2 left_pos  = {-1.0, 0.0};
    const vec2 right_pos = { 1.0, 0.0};

    // playing sounds at the rear quieter to replicate ear facing position
    f32 front_radius = 2.30;
    f32 rear_radius  = 1.00;
    f32 interp_delta = 0.25;
    
    // linearly interpolate between front and rear hemispheres
    f32 p = 1.0;
    if      (angle > 1.0f && angle < 1.0f + interp_delta)  p = (angle - 1.0) / interp_delta; // left discontinuity
    else if (angle > 2.0f - interp_delta)                  p = (2.0 - angle) / interp_delta; // right discontinuity
    rear_radius = p*rear_radius + (1.0-p)*front_radius;

    f32 rad = front_radius;
    if (angle > 1.0f) rad = rear_radius;

    vec2 sound_pos = {rad*(f32)cos(angle*PI), rad*(f32)sin(angle*PI)};
    f32  ldist = mag(sub(sound_pos, left_pos));
    f32  rdist = mag(sub(sound_pos, right_pos));

    // interpret distances to volumes
    f32 rad2_recip = 1.0f / (2.0f * rad);
    *l = status->volume * (1.0f - (ldist * rad2_recip));
    *r = status->volume * (1.0f - (rdist * rad2_recip));

    *lr = 0;
    *rl = 0;
}

void stereo_radial_from_angle(Status* status, vec4* contrib) {
    // FIXME -- need to implement this still
    printf("[Audio]  --  radial for stereo has not been implemented\n");

    while (status->angle > 2.0f + EPS) status->angle -= 2.0f;
    f32 angle = status->angle;

    f32* l  = &contrib->x;
    f32* lr = &contrib->y;
    f32* rl = &contrib->z;
    f32* r  = &contrib->w;

    *l  = 1.0;
    *lr = 0.0;
    *rl = 0.0;
    *r  = 1.0;
}

void sound_to_layer(Status* status, vec4 contrib) {
    f32 l_contrib  = contrib.x;
    f32 lr_contrib = contrib.y;
    f32 rl_contrib = contrib.z;
    f32 r_contrib  = contrib.w;

    Sound* sound = &sounds[status->sound_id];
    u16 layer    = status->layer;
    u32 offset   = status->offset;

    // NOTE: don't need this if your sound loops seemlessly
    //       so really you'd want this to be an argument to the function.
    const u32 loop_atten = 1 << 13; // attenuation zone
    const u32 loop_dead  = 1 <<  9; // dead zone

    f32 loop_vol = 1.0;

    // NOTE: assumes sound is same sample rate as output device
    for (u32 idx = 0; idx < buffers.length; idx++) {
        i64 sample_idx = offset + idx;

        // handle wrapping
        if (sample_idx > sound->length - 1) {
            if (status->mode == EventMode::loop) {
                status->offset %= sound->length;
                offset = status->offset;
                sample_idx -= sound->length;
            } else {
                return;
            }
        }

        // loop smooth volume attenuation
        if (status->mode == EventMode::loop) {
            // loop dead zone
            if (sample_idx < loop_dead || sample_idx > sound->length - 1 - loop_dead) {
                loop_vol = 0.0f;
            }

            // beginning attenuation
            else if (sample_idx < loop_dead + loop_atten) {
                loop_vol = f32(sample_idx - loop_dead) / loop_atten;
            }

            // ending attenuation
            else if (sample_idx > sound->length - 1 - loop_atten - loop_dead) {
                loop_vol = f32(sound->length - 1 - sample_idx - loop_dead) / loop_atten;
            }
        }

        if (sound->channels == 1) {
            f32 val = 0.0;
            if (sound->depth == 8) {
                // NOTE: apparently 8 bit files are in the [0,255] range?
                i8* interp = (i8*) sound->data;
                val = f32(interp[sample_idx]) / (1<<7);
            }
            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                val = f32(interp[sample_idx]) / (1<<15);
            }
            if (sound->depth == 24) {
                u8* bytes  = ((u8*) sound->data) + 3*sample_idx;
                i32 interp = ( (bytes[2]<<24) | (bytes[1]<<16) | (bytes[0]<<8) );
                val = f32(interp) / (1<<31); // -- NOTE: this *seems* correct, but haven't read it anywhwere..
            }
            val *= loop_vol;

            buffers.layers[layer][0][idx] += val * (l_contrib + lr_contrib);
            buffers.layers[layer][1][idx] += val * (r_contrib + rl_contrib);
            continue;
        }

        if (sound->channels == 2) {
            f32 l_val = 0.0;
            f32 r_val = 0.0;
            if (sound->depth == 8) {
                i8* interp = (i8*) sound->data;
                l_val = f32(interp[(2*sample_idx)])   / (1<<7);
                r_val = f32(interp[(2*sample_idx)+1]) / (1<<7);
            } 
            if (sound->depth == 16) {
                i16* interp = (i16*) sound->data;
                l_val = f32(interp[(2*sample_idx)])   / (1<<15);
                r_val = f32(interp[(2*sample_idx)+1]) / (1<<15);
            }
            l_val *= loop_vol;
            r_val *= loop_vol;

            buffers.layers[layer][0][idx] += l_val*l_contrib + r_val*lr_contrib;
            buffers.layers[layer][1][idx] += r_val*r_contrib + l_val*rl_contrib;
            continue;
        }
    }
}

void mix_to_master() {
    f32 _max_sq = 1.0f + EPS;

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
            f32 sq = buffers.master[c][idx] * buffers.master[c][idx];
            if (sq + EPS > _max_sq) _max_sq = sq + EPS;
        }
    }
    
    // max normalization
    // -- because the buffer should be small to minimize latency,
    //    it should be fine to apply simple max-rescale over the whole buffer.
    f32 _max_recip = 1.0 / (sqrt(_max_sq) + EPS);
    for (u8 c = 0; c < MASTER_CHANNELS; c++) {
        for (u32 i = 0; i < buffers.length; i++) {
            buffers.master[c][i] *= _max_recip;
        }
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
            // FIXME -- lmao if its mono
            *interp = buffers.master[0][i];
             interp++;

            *interp = buffers.master[1][i];
             interp++;

             // TODO - this would be nice to design for at some point
             for (u8 c = 2; c < info->mix_fmt->nChannels; c++) {
                 *interp = 0;
                 interp++;
             }
        }
    } else {
        // FIXME -- not tested, likewise to the floating point case, is susceptible to the same flaws.
        if (info->mix_fmt->wBitsPerSample == 8) {
            i8* interp = (i8*) data;
            for (u32 i = 0; i < open; i++) {
                *interp = buffers.master[0][i] * (1<<7);
                 interp++;

                *interp = buffers.master[1][i] * (1<<7);
                 interp++;

                 for (u8 c = 2; c < info->mix_fmt->nChannels; c++) {
                     *interp = 0;
                     interp++;
                 }
            }
        }

        if (info->mix_fmt->wBitsPerSample == 16) {
            i16* interp = (i16*) data;
            for (u32 i = 0; i < open; i++) {
                *interp = buffers.master[0][i] * (1<<15);
                 interp++;

                *interp = buffers.master[1][i] * (1<<15);
                 interp++;

                 for (u8 c = 2; c < info->mix_fmt->nChannels; c++) {
                     *interp = 0;
                     interp++;
                 }
            }
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
        wav_to_sound("./res/sounds/pencil_1.wav",   ptr + SOUND_PENCIL_1);
        wav_to_sound("./res/sounds/pencil_2.wav",   ptr + SOUND_PENCIL_2);
        wav_to_sound("./res/sounds/pencil_3.wav",   ptr + SOUND_PENCIL_3);
        wav_to_sound("./res/sounds/pencil_4.wav",   ptr + SOUND_PENCIL_4);
                                                        
        wav_to_sound("./res/sounds/pen_1.wav",      ptr + SOUND_PEN_1   );
        wav_to_sound("./res/sounds/pen_2.wav",      ptr + SOUND_PEN_2   );
        wav_to_sound("./res/sounds/pen_3.wav",      ptr + SOUND_PEN_3   );
        wav_to_sound("./res/sounds/pen_4.wav",      ptr + SOUND_PEN_4   );
                                                        
        wav_to_sound("./res/sounds/impact_1.wav",   ptr + SOUND_IMPACT_1);
        wav_to_sound("./res/sounds/impact_2.wav",   ptr + SOUND_IMPACT_2);
        wav_to_sound("./res/sounds/impact_3.wav",   ptr + SOUND_IMPACT_3);
    }

    // quick resample sounds to target sample_rate
    printf("[Audio] Resampling\n");
    for (u16 i = 0; i < N_SOUNDS; i++) {
        if (sounds[i].sample_rate != winfo.mix_fmt->nSamplesPerSec) {
            printf(" - [%u]    %u  ->  %u\n", i, sounds[i].sample_rate, winfo.mix_fmt->nSamplesPerSec);
            resample_sound(&sounds[i], winfo.mix_fmt->nSamplesPerSec, 1);
        }
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
                        if (iter.mode == EventMode::default) continue;
                        if (iter.mode == EventMode::interpolate) {
                            for (u32 search_idx = 0; search_idx < N_EVENTS; search_idx++) {
                                if (active_sounds[search_idx].id == iter.target_id) {
                                    active_sounds[search_idx].interp.running      = 1;
                                    active_sounds[search_idx].interp.start_us     = total_time_us;
                                    active_sounds[search_idx].interp.time_us      = ((i64)(iter.interp_time * pow_10[6]));
                                    active_sounds[search_idx].interp.delta_volume = iter.volume;
                                    active_sounds[search_idx].interp.old_volume   = active_sounds[search_idx].volume;
                                    active_sounds[search_idx].interp.delta_angle  = iter.angle;
                                    active_sounds[search_idx].interp.old_angle    = active_sounds[search_idx].angle;
                                    break;
                                }
                            }
                            continue;
                        }
                        if (iter.mode == EventMode::update) {
                            for (u32 search_idx = 0; search_idx < N_EVENTS; search_idx++) {
                                if (active_sounds[search_idx].id == iter.target_id) {
                                    active_sounds[search_idx].sound_id = iter.sound_id;
                                    active_sounds[search_idx].mode     = iter.target_mode;
                                    active_sounds[search_idx].layer    = iter.layer;
                                    active_sounds[search_idx].volume   = iter.volume;
                                    active_sounds[search_idx].angle    = iter.angle;
                                    break;
                                }
                            }
                            continue;
                        }
                        active_sounds[active_ptr].id            = iter.id;
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


        // update interpolation parameters
        for (u16 sidx = 0; sidx < N_EVENTS; sidx++) {
            if (!active_sounds[sidx].interp.running) continue;
            if (total_time_us - active_sounds[sidx].interp.start_us > active_sounds[sidx].interp.time_us) {
                active_sounds[sidx].interp.running = 0;
                active_sounds[sidx].volume = active_sounds[sidx].interp.old_volume + active_sounds[sidx].interp.delta_volume;
                active_sounds[sidx].angle  = active_sounds[sidx].interp.old_angle  + active_sounds[sidx].interp.delta_angle;
                continue;
            }

            f32 p = f32(total_time_us - active_sounds[sidx].interp.start_us) / active_sounds[sidx].interp.time_us;
            active_sounds[sidx].volume = active_sounds[sidx].interp.old_volume + p * active_sounds[sidx].interp.delta_volume;
            active_sounds[sidx].angle  = active_sounds[sidx].interp.old_angle  + p * active_sounds[sidx].interp.delta_angle;
        }

        // write sounds to layers -- every frame so we have something to time
        for (u16 sidx = 0; sidx < N_EVENTS; sidx++) {
            if (active_sounds[sidx].mode == EventMode::default) continue;
            if (active_sounds[sidx].mode == EventMode::stop) {
                active_sounds[sidx].mode = EventMode::default;
                continue;
            }

            Sound* sound = &sounds[0] + active_sounds[sidx].sound_id;
            vec4 contrib;

            if (sound->channels == 1) {
                mono_radial_from_angle(&active_sounds[sidx], &contrib);
            }

            if (sound->channels == 2) {
                stereo_radial_from_angle(&active_sounds[sidx], &contrib);
            }

            sound_to_layer(&active_sounds[sidx], contrib);
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
                    if (active_sounds[sidx].mode == EventMode::loop) {
                        active_sounds[sidx].end_time_us += sounds[active_sounds[sidx].sound_id].time_us;
                    }
                    else {
                        active_sounds[sidx].mode = EventMode::default;
                    }
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
