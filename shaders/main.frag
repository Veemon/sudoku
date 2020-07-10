#version 410

precision highp float;

in vec2 f_uv;

layout (location = 0) out vec4 frag_color;

uniform sampler2D font;
uniform sampler2D pencil;

void main() {
    float dim_size = 1.0 / 9.0;
    float offset   = dim_size / 2.0;
    vec2 id = vec2(f_uv / dim_size);


    // digits
    frag_color = vec4(vec3(0.97), 1.0);


    

    // pencil mode -- FIXME: texture sampling is wrong
    // vec2 tex_coord = vec2(offset) + floor(id)*dim_size;
    // float pencil_mode = texture(pencil, tex_coord).r;
    // frag_color.rgb = vec3(pencil_mode);
    // return;



    // grid shading
    float eps = 0.005;

    float shade_mask;
    float shade_m1  = smoothstep(3.0-eps, 3.0, id.x) * smoothstep(id.x-eps, id.x, 6.0);
    float shade_m2  = smoothstep(3.0-eps, 3.0, id.y) * smoothstep(id.y-eps, id.y, 6.0);
    shade_mask      = shade_m1 + shade_m2;
    shade_mask      = clamp(shade_mask, 0.0, 1.0);
    shade_mask     -= shade_m1 * shade_m2;
    
    frag_color.rgb *= 1 - shade_mask;
    frag_color.rgb += shade_mask * vec3(0.8);



    // grid Lines
    const float r[3]       = float[3](1.0/3.0, 1.0/9.0, 1.0/27.0);
    const float weights[3] = float[3](0.007, 0.015, 0.025);
    const float shades[3]  = float[3](0.15, 0.5, 0.7);

    vec2 uv;
    float s, w;
    float mask;
    for (int i = 0; i < 3; i++) {
        int j = 2 - i;

        uv = mod(f_uv, r[j]) / r[j];

        w = weights[j] / 2.0;
        s = shades[j];

        mask = 0.0;
        mask += smoothstep(0.0-w-eps, 0.0-w, uv.x) * smoothstep(uv.x-eps, uv.x, 0.0 + w); // left
        mask += smoothstep(1.0-w-eps, 1.0-w, uv.x) * smoothstep(uv.x-eps, uv.x, 1.0 + w); // right
        mask += smoothstep(0.0-w-eps, 0.0-w, uv.y) * smoothstep(uv.y-eps, uv.y, 0.0 + w); // up
        mask += smoothstep(1.0-w-eps, 1.0-w, uv.y) * smoothstep(uv.y-eps, uv.y, 1.0 + w); // down
        mask = clamp(mask, 0.0, 1.0);

        frag_color.rgb *= (1.0 - mask);
        frag_color.rgb += vec3(s) * mask;
    }
}
