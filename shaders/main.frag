#version 410

precision highp float;

in vec2 f_uv;

layout (location = 0) out vec4 frag_color;

uniform sampler2D image;

void main() {


    // digits
    frag_color = vec4(vec3(0.9), 1.0);

    float dim_size = 1.0 / 9.0;
    ivec2 id = ivec2(f_uv / dim_size);

    vec2 uv = f_uv;
    uv = mod(uv, dim_size) / dim_size;



    // grid shading
    float shade_mask;
    float shade_m1  = step(3, id.x) * step(id.x, 5);
    float shade_m2  = step(3, id.y) * step(id.y, 5);
    shade_mask      = shade_m1 + shade_m2;
    shade_mask      = clamp(shade_mask, 0.0, 1.0);
    shade_mask     -= shade_m1 * shade_m2;
    
    frag_color.rgb *= 1 - shade_mask;
    frag_color.rgb += shade_mask * vec3(0.8);



    // grid Lines
    const float r[3]       = float[3](1.0/3.0, 1.0/9.0, 1.0/27.0);
    const float weights[3] = float[3](0.007, 0.015, 0.025);
    const float shades[3]  = float[3](0.15, 0.5, 0.7);

    float s, w;
    float mask;
    for (int i = 0; i < 3; i++) {
        int j = 2 - i;

        uv = mod(f_uv, r[j]) / r[j];

        w = weights[j] / 2.0;
        s = shades[j];

        mask = 0.0;
        mask += step(0.0 - w, uv.x) * step(uv.x, 0.0 + w); // left
        mask += step(1.0 - w, uv.x) * step(uv.x, 1.0 + w); // right
        mask += step(0.0 - w, uv.y) * step(uv.y, 0.0 + w); // up
        mask += step(1.0 - w, uv.y) * step(uv.y, 1.0 + w); // down
        mask = clamp(mask, 0.0, 1.0);

        frag_color.rgb *= (1.0 - mask);
        frag_color.rgb += vec3(s) * mask;
    }
}
