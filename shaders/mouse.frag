#version 410

precision highp float;

in vec2 f_uv;

layout (location = 0) out vec4 frag_color;

uniform sampler2D  font;
uniform usampler2D board;

uniform vec2 mouse;
uniform vec2 time;
// current time
// time at click

void main() {
#if 1
    float s1 = 0.17;
    float s2 = s1 * 4.0 / 3.0; 
#else 
    float s1 = 0.6;
    float s2 = s1 * 4.0 / 3.0; 
#endif

    float time_delta = time.y - time.x;

    float impulse_overdampen = cos(2*time_delta) * 0.19;
    float impulse1  = 1.0 - exp(1 +  4.0 * time_delta) * impulse_overdampen;
    float impulse2  = 1.0 - exp(1 + 16.0 * time_delta) * impulse_overdampen;
    float impulse3  = 1.0 - exp(1 +  1.2 * time_delta) * impulse_overdampen;

    float v1 = sin(time_delta * 0.55);
    float v2 = cos(time_delta * 0.55 * 2);

    float f  = length(f_uv - mouse); // radial field

    float r  = s1 * 0.100; // disk radius
    float i  = s1 * 0.030; // ring inset
    float w  = s1 * 0.018; // ring width

    float r2 = impulse1 * s1 * 0.070; // shade radius
    float r3 = impulse2 * s1 * 0.160; // shade bg radius

    v1 *= v1;
    v2 *= v2;

    float e1 = impulse1 * s2 * 0.020 + 0.0020*v1; // disk  feather
    float e2 = s2 * 0.010 + 0.0015*v1; // ring  feather
    float e3 = s2 * 0.015 + 0.0018*v2; // shade feather
    float e4 = s2 * 0.085 + 0.0020*v2; // shade bg feather

    float disk  = smoothstep(f, f+e1, r);
    float ring  = smoothstep(f, f+e2, r-i+(w/2)) - smoothstep(f, f+e2, r-i-(w/2));
    
    float shade_bg    = 1.0 - smoothstep(f, f+e4, r3);
    float shade_disk  = 1.0 - smoothstep(f, f+e3, r2);

    // shadow
    vec4 color = vec4(1.0);
    color *= vec4(vec3(shade_bg), 0.0);
    color *= vec4(vec3(shade_disk), 0.0);

    color += vec4(vec3(0.0), smoothstep(0.3, 1.0, 1.0 - shade_bg)   * 0.75);
    color += vec4(vec3(0.0), smoothstep(0.3, 1.0, 1.0 - shade_disk) * 0.90);

    // disk / ring
    float col = disk - ring;
    color += vec4(vec3(col), col * 0.4);
    color = clamp(color, 0.0, 1.0);


    frag_color.xyz = mix(color.xyz, vec3(0.5, 0.5 + 0.7*v1, 0.7 + 0.3*v2), 0.05 + 0.18 * (1.0 - impulse3));
    frag_color.w = color.w;
}
