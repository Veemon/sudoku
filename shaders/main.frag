#version 410

precision highp float;

in vec2 f_uv;

layout (location = 0) out vec4 frag_color;

uniform sampler2D  font;
uniform usampler2D board;


float fetch_number(float n, vec2 ref_coord) {
    ref_coord.x = ref_coord.x * (1.0/9.0) + ((n-1.0)/9.0);
    return texture(font, ref_coord).r;
}


void main() {
    // base color
    frag_color = vec4(vec3(0.97), 1.0); 



    // identification (welcome to arstotzka)
    vec2 grid_id = floor(vec2((f_uv - 1e-6) * 9));

    const float dim_size = 1.0 / 16.0;         // rescale to account for texture size
    const float offset   = dim_size * 0.5;

    vec2 info_coord = grid_id*dim_size + offset;
    vec2 cell_coord = 9.0*f_uv - grid_id;
    vec2 cell_id    = floor((cell_coord - 1e-6) * 3);

    uint  info         = texture(board, info_coord).r;
    bool  pencil_flag  = bool(info & 0x8000);
    bool  error_flag   = bool(info & 0x4000);
    bool  static_flag  = bool(info & 0x2000);
    bool  entered_flag = !(static_flag || error_flag);

    vec3 static_color  = vec3(0.1, 0.1, 0.1);
    vec3 entered_color = vec3(0.1, 0.3, 0.5);
    vec3 error_color   = vec3(0.6, 0.1, 0.1);

    vec3 status_color = vec3(0.0);
    status_color     += float(static_flag)  * static_color;
    status_color     += float(entered_flag) * entered_color;
    status_color     += float(error_flag)   * error_color;



    // grid shading
    float eps = 0.005;

    float shade_mask;
    float shade_m1  = smoothstep(3.0-eps, 3.0, grid_id.x) * smoothstep(grid_id.x-eps, grid_id.x, 5.0);
    float shade_m2  = smoothstep(3.0-eps, 3.0, grid_id.y) * smoothstep(grid_id.y-eps, grid_id.y, 5.0);
    shade_mask      = shade_m1 + shade_m2;
    shade_mask      = clamp(shade_mask, 0.0, 1.0);
    shade_mask     -= shade_m1 * shade_m2;
    
    frag_color.rgb *= 1 - shade_mask;
    frag_color.rgb += shade_mask * vec3(0.85);



    // digits
    {
        uint nn = 0;
        vec2 ref_coord = cell_coord;

        if (pencil_flag) {

            // check if our cell has a number
            uint cell_n = uint(cell_id.y)*3 + uint(cell_id.x);
            nn = ((info & (1<<cell_n))>>cell_n) * (cell_n+1);
            
            // min-max norm
            ref_coord = (f_uv*27.0) - (grid_id*3.0 + cell_id);

        }
        else {
            for (uint i = 0; i < 9; i++) {
               nn += ((info & (1<<i))>>i) * (i+1);
            }
        }

        float n = float(nn);

        float m1 = fetch_number(n, ref_coord + vec2(-0.02, -0.015)); 
        float m2 = fetch_number(n, ref_coord); 
        float mask = clamp(m1*0.4 + m2, 0.0, 1.0);

        frag_color.rgb *= 1.0 - mask;
        frag_color.rgb += mask * status_color;
    }



    // grid Lines
    const float r[3]       = float[3](1.0/3.0, 1.0/9.0, 1.0/27.0);
    const float weights[3] = float[3](0.007, 0.015, 0.025);
    const float shades[3]  = float[3](0.15, 0.5, 0.7);

    vec2 uv;
    float s, w;
    float mask;
    for (int i = int(!pencil_flag); i < 3; i++) {
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
