struct Sound {
    u32   data_length;
    u16   sample_rate;
    u8    depth;
    u8    channels;
    void* data;
};

i32 wav_to_sound(const char* filename, Sound* sound);


struct Event {
    f32 volume;
    f32 angle;
};


void audio_loop();
