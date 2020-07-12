#version 410

precision highp float;

in vec2 f_uv;

layout (location = 0) out vec4 frag_color;

uniform sampler2D  font;
uniform usampler2D pencil;

void main() {
    frag_color = vec4(vec3(0.97), 1.0); // base color



    // identification (welcome to arstotzka)
    vec2 id = floor(vec2((f_uv - 1e-6) * 9));

    const float dim_size = 1.0 / 16.0;         // rescale to account for texture size
    const float offset   = dim_size * 0.5;

    vec2  info_coord = id*dim_size + offset;
    uint  info       = texture(pencil, info_coord).r;   



    // grid shading
    float eps = 0.005;

    float shade_mask;
    float shade_m1  = smoothstep(3.0-eps, 3.0, id.x) * smoothstep(id.x-eps, id.x, 5.0);
    float shade_m2  = smoothstep(3.0-eps, 3.0, id.y) * smoothstep(id.y-eps, id.y, 5.0);
    shade_mask      = shade_m1 + shade_m2;
    shade_mask      = clamp(shade_mask, 0.0, 1.0);
    shade_mask     -= shade_m1 * shade_m2;
    
    frag_color.rgb *= 1 - shade_mask;
    frag_color.rgb += shade_mask * vec3(0.8);



    // digits
    if (info > 0x8000) {
        frag_color.r = 1.0;
        frag_color.g -= 0.6;
        frag_color.b -= 0.6;
    }
    else {
        frag_color.r -= 0.6;
        frag_color.g -= 0.6;
        frag_color.b = 1.0;
    }



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
