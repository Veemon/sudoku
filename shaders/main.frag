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

    const float dim_size = 1.0 / 16.0;          // rescale to account for texture size
    const float offset   = (1.0 / 32.0) - 1e-6; // half of dim size

    vec2 info_coord = grid_id*dim_size + offset;
    vec2 cell_coord = 9.0*f_uv - grid_id;
    vec2 cell_id    = floor((cell_coord - 1e-6) * 3);

    uint  info         = texture(board, info_coord).r;
    bool  pencil_flag  = bool(info & 0x8000);
    bool  error_flag   = bool(info & 0x4000);
    bool  static_flag  = bool(info & 0x2000);
    bool  solve_flag   = bool(info & 0x0400);
    bool  entered_flag = !(static_flag || error_flag || solve_flag);
    float cursor       = float((info & 0x1000)>>12);
    float hover        = float((info & 0x0800)>>11);

    vec3 static_color       = vec3(0.07, 0.07, 0.07);
    vec3 entered_color      = vec3(0.15, 0.28, 0.50);
    vec3 error_color        = vec3(0.65, 0.2, 0.2);
    vec3 solve_color        = vec3(0.28, 0.68, 0.16);
    vec3 static_error_color = vec3(0.77, 0.35, 0.12);
    vec3 hover_color        = vec3(0.33);

    vec3 status_color  = vec3(0.0);
    status_color      += float(static_flag)  * static_color;
    status_color      += float(entered_flag) * entered_color;
    status_color      += float(error_flag)   * error_color;
    status_color      += float(solve_flag)   * solve_color;
    status_color      *= 1.0 - float(static_flag && error_flag);
    status_color      += float(static_flag && error_flag) * static_error_color;
    status_color       = clamp(status_color, 0.0, 1.0);

    // NOTE: ideally this is a lot better done in HSV
    vec3 cursor_color = vec3(0.785) + status_color*0.45;

    float overlap = ceil((cursor-hover)*(cursor-hover));
    hover_color *= overlap;
    hover_color += (1.0 - overlap) * mix(vec3(0.35), status_color, 0.85);
    hover_color  = clamp(hover_color, 0.0, 1.0);


    // grid shading
    {
        float eps = 0.005;

        float shade_mask = 0.0f;
        float shade_m1  = smoothstep(3.0-eps, 3.0, grid_id.x) * smoothstep(grid_id.x-eps, grid_id.x, 5.0);
        float shade_m2  = smoothstep(3.0-eps, 3.0, grid_id.y) * smoothstep(grid_id.y-eps, grid_id.y, 5.0);
        shade_mask      = shade_m1 + shade_m2;
        shade_mask      = clamp(shade_mask, 0.0, 1.0);
        shade_mask     -= shade_m1 * shade_m2;
        
        frag_color.rgb *= 1 - shade_mask;
        frag_color.rgb += shade_mask * vec3(0.85);
    }



    // cursor
    {
        float eps   = 0.77; // outer circle
        float r     = 0.35;  // inner circle
        
        float lr = (r/2);
        float hr = 1.0 - (r/2);

        float shade = 1.0;
        shade *= smoothstep(lr-eps, lr, cell_coord.x) * smoothstep(cell_coord.x-eps, cell_coord.x, hr);
        shade *= smoothstep(lr-eps, lr, cell_coord.y) * smoothstep(cell_coord.y-eps, cell_coord.y, hr);

        frag_color.rgb *= 1 - cursor;
        frag_color.rgb += cursor * cursor_color * shade * 1.04; // oversaturate
        frag_color = clamp(frag_color, 0.0, 1.0);
    }

    // hover
    {
        float eps = 0.50*(1.0 - cursor) + 0.85*cursor; // outer circle
        float r   = 0.38*(1.0 - cursor) + 0.38*cursor; // inner circle

        float lr = (r/2);
        float hr = 1.0 - (r/2);

        float shade = 1.0;
        shade *= smoothstep(lr-eps, lr, cell_coord.x) * smoothstep(cell_coord.x-eps, cell_coord.x, hr);
        shade *= smoothstep(lr-eps, lr, cell_coord.y) * smoothstep(cell_coord.y-eps, cell_coord.y, hr);
        shade  = (cursor * (shade-1.0f) * -1.0f) + ((1.0-cursor) * shade);

        float bot = 0.45*cursor;
        shade  = (shade+(2.0f * bot)) / (1.0f + 2.0f*bot);

        frag_color.rgb *= 1 - hover;
        frag_color.rgb += hover * hover_color * shade;
        frag_color = clamp(frag_color, 0.0, 1.0);
    }


    // digits
    {
        uint nn = 0;
        vec2 ref_coord = cell_coord;

        // grab digit
        if (pencil_flag) {

            // check if our cell has a number
            uint cell_n = uint(cell_id.y)*3 + uint(cell_id.x);
            nn = (((0x1FF&info) & (1<<cell_n))>>cell_n) * (cell_n+1);
            
            // min-max norm
            ref_coord = (f_uv*27.0) - (grid_id*3.0 + cell_id);

        }
        else {
            for (uint i = 0; i < 9; i++) {
               nn += (((0x1FF&info) & (1<<i))>>i) * (i+1);
            }
        }

        float n = float(nn);

        #define SHADOW_DIST      0.02
        #define SHADOW_INTENSITY 0.25
        #define BLUR_N           8
        #define BLUR_SIZE        0.1

        float digit_color = fetch_number(n, ref_coord + vec2(SHADOW_DIST, SHADOW_DIST)*cursor);

        // blurring of shadow
        float shadow_color = 0.0;
        {
            const float blur_delta = BLUR_SIZE / BLUR_N;
            const float blur_reset = blur_delta * -float(BLUR_N/2);
            vec2 blur_offset = vec2(blur_reset);

            for (int y = 0; y < BLUR_N; y++) {
                vec2 blur_coord  = ref_coord + vec2(-SHADOW_DIST, -SHADOW_DIST)*cursor + blur_offset;
                for (int x = 0; x < BLUR_N; x++) {
                    shadow_color += fetch_number(n, blur_coord);
                    blur_offset.x += blur_delta;
                }
                blur_offset.x  = blur_reset;
                blur_offset.y += blur_delta;
            }

            shadow_color /= float(BLUR_N * BLUR_N);
            shadow_color *= cursor;
        }

        float mask = clamp(shadow_color*SHADOW_INTENSITY + digit_color, 0.0, 1.0);

        frag_color.rgb *= 1.0 - mask;
        frag_color.rgb += mask * (status_color + hover*(overlap*vec3(0.58) + (1.0-overlap)*vec3(0.86))); //bright on hover
        frag_color = clamp(frag_color, 0.0, 1.0);
    }



    // grid Lines
    {
        float eps = hover*0.010 + (1.0-hover)*0.0065;

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

            float e = eps * ( hover*float(j==1) + (1.0 - hover) );

            mask = 0.0;
            mask += smoothstep(0.0-w-e, 0.0-w, uv.x) * smoothstep(uv.x-e, uv.x, 0.0+w); // left
            mask += smoothstep(1.0-w-e, 1.0-w, uv.x) * smoothstep(uv.x+e, uv.x, 1.0+w); // right
            mask += smoothstep(0.0-w-e, 0.0-w, uv.y) * smoothstep(uv.y-e, uv.y, 0.0+w); // up
            mask += smoothstep(1.0-w-e, 1.0-w, uv.y) * smoothstep(uv.y+e, uv.y, 1.0+w); // down
            mask = clamp(mask, 0.0, 1.0);

            frag_color.rgb *= (1.0 - mask);
            frag_color.rgb += mix(vec3(s) * mask, 
                    0.9 * mask * (status_color + hover*vec3(0.6)), cursor);
        }
    }
}
