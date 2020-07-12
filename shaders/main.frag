#version 410

precision highp float;

in vec2 f_uv;

layout (location = 0) out vec4 frag_color;

uniform sampler2D  font;
uniform usampler2D pencil;

void main() {
    frag_color = vec4(vec3(0.97), 1.0); // base color

    // FIXME: testing font
    frag_color.rgb = vec3(texture(font, f_uv).r);


    // identification (welcome to arstotzka)
    vec2 id = floor(vec2((f_uv - 1e-6) * 9));

    const float dim_size = 1.0 / 16.0;         // rescale to account for texture size
    const float offset   = dim_size * 0.5;

    vec2 info_coord = id*dim_size + offset;
    vec2 cell_coord = 9.0*f_uv - id;
    vec2 cell_id    = floor((cell_coord - 1e-6) * 3);

    uint  info        = texture(pencil, info_coord).r;
    bool  pencil_mode = bool(info & 0x8000);



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
#if 0
    if (pencil_mode) {
        frag_color.rgb = vec3(0.8);

        vec2 outer = vec2(8,0);
        vec2 inner = vec2(2,2);

        float outer_mask = 1.0;
        outer_mask *= step(outer.x, id.x) * step(id.x, outer.x);
        outer_mask *= step(outer.y, id.y) * step(id.y, outer.y);

        float cell_mask = 1.0;
        cell_mask *= step(inner.x, cell_id.x) * step(cell_id.x, inner.x);
        cell_mask *= step(inner.y, cell_id.y) * step(cell_id.y, inner.y);

        float mask = outer_mask * cell_mask;

        if ( bool(info & (1<<9)) ) { }
        if ( bool(info & (1<<8)) ) { }
        if ( bool(info & (1<<7)) ) { }
    }
    else {
        if (info == 1) { frag_color.rgb *= vec3(0.7); }
        if (info == 5) { frag_color.rgb *= vec3(0.5); }
        if (info == 9) { frag_color.rgb *= vec3(0.2); }
    }
#endif



    // grid Lines
    const float r[3]       = float[3](1.0/3.0, 1.0/9.0, 1.0/27.0);
    const float weights[3] = float[3](0.007, 0.015, 0.025);
    const float shades[3]  = float[3](0.15, 0.5, 0.7);

    vec2 uv;
    float s, w;
    float mask;
    for (int i = int(!pencil_mode); i < 3; i++) {
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
