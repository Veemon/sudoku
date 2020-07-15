// local
#include "proj_types.h"

// third party
#include "windows.h"
#undef near
#undef far

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// system
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

// math
union vec3 {
    struct {
        f32 x;
        f32 y;
        f32 z;
    };
    f32 a[3];
};

union vec4 {
    struct {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };
    f32 a[4];
};

union mat4 {
    struct {
        vec4 x;
        vec4 y;
        vec4 z;
        vec4 w;
    };
    vec4 v[4];
    f32  a[16];
};

void print_vec4(vec4* v) {
    printf("(  ");
    for (u32 i = 0; i < 4; i++) {
        printf("%+.4f  ",v->a[i]);
    }
    printf(")\n");
}

void print_mat4(mat4* m) {
    printf("[\n");
    for (u32 i = 0; i < 4; i++) {
        printf("    ");
        for (u32 j = 0; j < 4; j++) {
            printf("%+.4f  ",m->v[j].a[i]);
        }
        printf("\n");
    }
    printf("]\n");
}

void zero_vec4(vec4* v) {
    for (u32 i = 0; i < 4; i++) {
        v->a[i] = 0.0f;
    }
}

void zero_mat4(mat4* m) {
    for (u32 i = 0; i < 4; i++) {
        zero_vec4(&m->v[i]);
    }
}

void identity(mat4* m) {
    zero_mat4(m);
    for(u32 i = 0; i < 4; i++) {
        m->v[i].a[i] = 1.0;
    }
}

void project_orthographic(mat4* m, f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    // expects identity matrix
    f32 drl = right - left;
    f32 dtb = top   - bottom;
    f32 dfn = far   - near;

    m->x.x = 2 / drl;
    m->y.y = 2 / dtb;
    m->z.z = 2 / dfn;

    m->w.x = -(right + left  ) / drl;
    m->w.y = -(top   + bottom) / dtb;
    m->w.z = -(far   + near  ) / dfn;
}

void translate_mat4(mat4* m, vec3 v) {
    // expects identity matrix
    m->w.x = v.x;
    m->w.y = v.y;
    m->w.z = v.z;
}

void scale_mat4(mat4* m, vec3 v) {
    // expects identity matrix
    m->x.x = v.x;
    m->y.y = v.y;
    m->z.z = v.z;
}





// shaders
void debug_shader(u8* source) {
    // scan through and find how many lines there are
    i32 iter   = 0; 
    u32 n_line = 1; 
    while (source[iter])
    { 
        if (source[iter] == '\n') 
        { 
            n_line++;
        } 
        iter++; 
    } 

    // count how many digits there are in the number of lines
    u8  n_digits = 0; 
    u32 partial  = n_line; 
    while (partial > 0) { partial /= 10; n_digits++; } 

    // print lines of code
    iter = 0; 
    u32 i_line = 0; 
    while (source[iter]) 
    { 
        if (source[iter] == '\n' || !iter) 
        { 
            i_line++; 
            partial = i_line; 
            u8 i_digits = 0; 
            while (partial > 0) { partial /= 10; i_digits++; } 
            if (iter) { printf("\n"); }
            for (i32 it = 0; it < n_digits - i_digits; it ++) { printf(" "); }
            printf("%d |", i_line);
        }

        if (source[iter] == '\t') { printf("    "); }
        else if (source[iter] != '\n'){ printf("%c", source[iter]); }
        iter++;
    }

    printf("\n");
}


GLuint load_shader(const char* filename, GLuint shader_type, u8** info_log) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("File Error: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    u64 length = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8* source = (u8*) malloc(sizeof(u8) * (length + 1));
    fread(source, sizeof(u8), length, file);
    fclose(file);
    source[length] = '\0';

    // create shaders
    GLuint shader = glCreateShader(shader_type);
    const GLchar* gl_source = (const GLchar*) source;
    glShaderSource(shader, 1, &gl_source, nullptr);
    glCompileShader(shader);

    GLint comp_status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &comp_status);
    if (comp_status == GL_FALSE) {
        GLint status_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &status_length);
        *info_log = (u8*) malloc(status_length);
        glGetShaderInfoLog(shader, status_length, &status_length, (GLchar*) *info_log);
        glDeleteShader(shader);

        printf("\n[Compilation Fail] :: %s\n%s", filename, *info_log);
        printf("--------------------------------------------------------\n");
        debug_shader(source);
        printf("\n");

        free(source);
        return NULL;
    }

    free(source);
    return shader;
}

GLuint build_shader_program(const char* vertex_path, const char* fragment_path, u8** shader_info_log) {
    GLuint vert_shader = load_shader(vertex_path, GL_VERTEX_SHADER, shader_info_log);
    if (vert_shader == NULL) {
        return NULL;
    }

    GLuint frag_shader = load_shader(fragment_path, GL_FRAGMENT_SHADER, shader_info_log);
    if (frag_shader == NULL) {
        return NULL;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
    glLinkProgram(program);

    GLint link_status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        GLint status_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &status_length);
        u8* info_log = (u8*) malloc(status_length);
        glGetProgramInfoLog(program, status_length, &status_length, (GLchar*) info_log);
        glDeleteProgram(program);

        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);

        printf("[Link Fail]\n%s\n", info_log);
        free(info_log);

        return NULL;
    }

    glDetachShader(program, vert_shader);
    glDetachShader(program, frag_shader);

    return program;
}




#define INPUT_QUEUE_LEN           256
#define INPUT_TYPE_KEY_PRESS      1
#define INPUT_TYPE_MOUSE_PRESS    2
#define INPUT_TYPE_MOUSE_MOVE     3

struct InputEvent {
    u8  type              = 0;
    u8  mouse_button      = 0;
    i32 action            = 0;
    i32 key               = 0;
    i32 mod               = 0;
    f32 mouse_position[2] = {0.0f};
};

u32 input_index = 0;
InputEvent input_queue[INPUT_QUEUE_LEN] = {0};

#define IS_KEY_REPEAT event.action == GLFW_REPEAT
#define IS_KEY_DOWN   event.action == GLFW_PRESS
#define IS_KEY_UP     event.action == GLFW_RELEASE
#define KEY_DOWN(x)   (event.action == GLFW_PRESS  ) && (event.key == x)
#define KEY_UP(x)     (event.action == GLFW_RELEASE) && (event.key == x)

#define LOG_EVENT(x) {\
    printf("type                %u\n", x.type         );\
    printf("mouse_button        %u\n", x.mouse_button );\
    printf("action              %d\n", x.action       );\
    printf("key                 %d\n", x.key          );\
    printf("mod                 %d\n", x.mod          );\
    printf("mouse_position      %f   %f\n\n", x.mouse_position[0], x.mouse_position[1]);\
}




static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void mouse_move_callback(GLFWwindow* window, f64 xpos, f64 ypos) {
    input_queue[input_index].type = INPUT_TYPE_MOUSE_MOVE;
    input_queue[input_index].mouse_position[0] = xpos;
    input_queue[input_index].mouse_position[1] = ypos;
    input_index++;
}

static void key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
    if (key >= 0) {
        input_queue[input_index].type     = INPUT_TYPE_KEY_PRESS;
        input_queue[input_index].action   = action;
        input_queue[input_index].mod      = mods;
        input_queue[input_index].key      = key;
        input_index++;
    }
}

static void mouse_click_callback(GLFWwindow* window, i32 button, i32 action, i32 mods) {
    input_queue[input_index].type          = INPUT_TYPE_MOUSE_PRESS;
    input_queue[input_index].mouse_button |= (button == GLFW_MOUSE_BUTTON_LEFT)  * 0x2;
    input_queue[input_index].mouse_button |= (button == GLFW_MOUSE_BUTTON_RIGHT) * 0x1;
    input_queue[input_index].action        = action;
    input_queue[input_index].mod           = mods;
    input_index++;
}

static void scroll_callback(GLFWwindow* window, f64 xoffset, f64 yoffset) {
}




void print_conrols() {
    printf("                           Sudoku\n\n");
    printf(" Move Cursor     Pencil Mode     Pen Digit       Clear\n");
    printf("    [WASD]         [Space]       [Shift+N]    [Escape(x2)] \n\n");
    printf("       Move Cursor     Toggle Permanent    Solve \n");
    printf("       [Arrow Keys]         [TAB]         [Enter]\n\n");
}



void main() {
    // Setup GLFW window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return;

    const char* glsl_version = "#version 410";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    i32 window_width  = 1000;
    i32 window_height = 1080;

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Sudoku 2077", NULL, NULL);
    if (window == NULL) exit(-1);

    i32 monitor_x      = 0;
    i32 monitor_y      = 0;
    i32 monitor_width  = 0;
    i32 monitor_height = 0;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    glfwGetMonitorWorkarea(monitor, &monitor_x, &monitor_y, &monitor_width, &monitor_height);	
    glfwSetGamma(monitor, 1.0);

    glfwSetWindowPos(window,
                     monitor_x + (monitor_width  - window_width ) / 2,
                     monitor_y + (monitor_height - window_height) / 2);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetCursorPosCallback(window, mouse_move_callback);
	glfwSetMouseButtonCallback(window, mouse_click_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGL()) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        exit(-1);
    } 

    // opengl settings
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



    // geometry
    f32 quad_vertices[] = {
        // positions      texture coords
        -1.0f,  1.0f,      0.0f, 0.0f,
         1.0f,  1.0f,      1.0f, 0.0f,
        -1.0f, -1.0f,      0.0f, 1.0f,
         1.0f, -1.0f,      1.0f, 1.0f,
    };

    u32 quad_indices[] = {
        0, 1, 2,
        1, 2, 3
    };

    GLuint vbo, vao, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);

        // position
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void*)(0));
        glEnableVertexAttribArray(0);

        // texture
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void*)(2 * sizeof(f32)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);  



    // textures
    #define PATH_TTF_FONT "./res/LemonMilk.otf"

    #define FONT_DIM      128
    #define FONT_WIDTH    FONT_DIM * 9

    u8* font_data = (u8*) malloc(FONT_WIDTH * FONT_DIM);
    for (u32 i = 0; i < FONT_WIDTH*FONT_DIM; i++) {
        font_data[i] = 0;
    }

    {
        u64 length;
        FILE* font_file = fopen(PATH_TTF_FONT, "rb");

        fseek(font_file, 0, SEEK_END);
        length = ftell(font_file);
        fseek(font_file, 0, SEEK_SET);

        u8* font_file_contents = (u8*) malloc(length + 1);
        fread(font_file_contents, 1, length, font_file);
        fclose(font_file);
        font_file_contents[length] = '\0';

        stbtt_fontinfo font;
        stbtt_InitFont(&font, font_file_contents, stbtt_GetFontOffsetForIndex(font_file_contents, 0));

        i32 width;
        i32 height;
        i32 x_offset;
        i32 y_offset;    
        u8* bitmap;
        for (u32 font_idx = 0; font_idx < 9; font_idx++)
        {
            
            bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, FONT_DIM),
                                                          font_idx + '1', 
                                                          &width, 
                                                          &height, 
                                                          &x_offset, 
                                                          &y_offset);
            
            for (u32 j = 0; j < height; j++) {
                for (u32 i = 0; i < width; i++) {
                    u32 idx = font_idx*FONT_DIM +   // center top row to font index
                              ((j + (FONT_DIM/2 - height/2)) * FONT_WIDTH) +
                              (i + (FONT_DIM/2 - width/2) + x_offset);
                    font_data[idx] = bitmap[(j*width) + i];
                }
            }

            free(bitmap);
        }

        free(font_file_contents);

    }

    GLuint tex_font;
    glGenTextures(1, &tex_font);
    glBindTexture(GL_TEXTURE_2D, tex_font);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FONT_WIDTH, FONT_DIM, 0, GL_RED, GL_UNSIGNED_BYTE, font_data);
    glGenerateMipmap(GL_TEXTURE_2D);



    #define BOARD_EMPTY        0
    #define BOARD_1            0x1
    #define BOARD_2            0x2
    #define BOARD_3            0x4
    #define BOARD_4            0x8
    #define BOARD_5            0x10
    #define BOARD_6            0x20
    #define BOARD_7            0x40
    #define BOARD_8            0x80
    #define BOARD_9            0x100
    #define BOARD_ALL          0xFFF

    #define BOARD_FLAG_PENCIL  0x8000
    #define BOARD_FLAG_ERROR   0x4000
    #define BOARD_FLAG_STATIC  0x2000
    #define BOARD_FLAG_CURSOR  0x1000

    #define BOARD_DIM        16
    #define BOARD_SIZE       BOARD_DIM * BOARD_DIM
    #define IDX(x,y)        (y*BOARD_DIM) + x

    u16* board_data = (u16*) malloc(BOARD_SIZE * 2);
    for (u16 i = 0; i < BOARD_SIZE; i++) { 
        board_data[i] = BOARD_EMPTY; 
    }

    GLuint tex_board;
    glGenTextures(1, &tex_board);
    glBindTexture(GL_TEXTURE_2D, tex_board);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, BOARD_DIM, BOARD_DIM, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, board_data);



    // quick checks
    u16 rows[9], cols[9], squares[9];
    for (u8 i = 0; i < 9; i++) {
        rows[i]    = BOARD_EMPTY;
        cols[i]    = BOARD_EMPTY;
        squares[i] = BOARD_EMPTY;
    }




    // shaders
    #define PATH_SHADER_VERT "./shaders/main.vert"
    #define PATH_SHADER_FRAG "./shaders/main.frag"

    u8* info_log = nullptr;
    GLuint shader_program = build_shader_program(PATH_SHADER_VERT, PATH_SHADER_FRAG, &info_log);
    if (shader_program == NULL) {
        printf("[Error] Shader compilation failed.\n%s\n", info_log);
        return;
    }

    GLuint u_proj  = glGetUniformLocation(shader_program, "proj");
    GLuint u_model = glGetUniformLocation(shader_program, "model");

    GLuint u_font  = glGetUniformLocation(shader_program, "font");
    GLuint u_board = glGetUniformLocation(shader_program, "board");



    // orthogonal projection
    mat4 proj;
    identity(&proj);
    f32 window_ratio = f32(window_width) / f32(window_height);
    project_orthographic(&proj, -window_ratio, window_ratio, -1.0f, 1.0f, 0.0f, 100.0f);

    mat4 scale;
    identity(&scale);
    scale_mat4(&scale, {0.85f, 0.85f, 0.85f});

    glUseProgram(shader_program);
    glBindVertexArray(vao);



    // timing
    f32 total_time = 0.0;
    LARGE_INTEGER start_time, end_time, delta_ms, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_ms.QuadPart = 0;

    f32 delta_s = 0.0;

    f32 render_wait     = 1.0 / 60.0f;
    f32 render_timer    = render_wait;

    u8 cursor_x = 4;
    u8 cursor_y = 4;
    board_data[IDX(cursor_x, cursor_y)] |= BOARD_FLAG_CURSOR;

    print_conrols();

    while (!glfwWindowShouldClose(window))
    {
        // input handling
        QueryPerformanceCounter(&start_time);
        glfwPollEvents();

        for (u32 event_idx = 0; event_idx < input_index; event_idx++) {
            u8 handled = 0;
            InputEvent event = input_queue[event_idx];

            if (event.type == INPUT_TYPE_KEY_PRESS) {
                if (!handled && KEY_UP(GLFW_KEY_ESCAPE)) {
                    // quit
                    if (event.mod & GLFW_MOD_SHIFT) return;

                    // clear digits
                    board_data[IDX(cursor_x, cursor_y)] = BOARD_EMPTY | BOARD_FLAG_CURSOR;
                    handled = 1;
                }



                // board clear

                if (KEY_UP(GLFW_KEY_BACKSPACE)) {
                    for (u32 i = 0; i < BOARD_SIZE; i++) {
                        board_data[i] = BOARD_EMPTY;
                    }
                    for (u8 i = 0; i < 9; i++) {
                        rows[i]    = BOARD_EMPTY;
                        cols[i]    = BOARD_EMPTY;
                        squares[i] = BOARD_EMPTY;
                    }
                    board_data[IDX(cursor_x, cursor_y)] |= BOARD_FLAG_CURSOR;
                }


                // keyboard cursor
                if (!handled && IS_KEY_DOWN) {
                    board_data[IDX(cursor_x, cursor_y)] &= ~BOARD_FLAG_CURSOR;
                    if      (event.key == GLFW_KEY_LEFT  || event.key == GLFW_KEY_A) { cursor_x = (!cursor_x)     ? 8 : cursor_x-1; handled = 1; }
                    else if (event.key == GLFW_KEY_RIGHT || event.key == GLFW_KEY_D) { cursor_x = (cursor_x == 8) ? 0 : cursor_x+1; handled = 1; }
                    else if (event.key == GLFW_KEY_UP    || event.key == GLFW_KEY_W) { cursor_y = (!cursor_y)     ? 8 : cursor_y-1; handled = 1; }
                    else if (event.key == GLFW_KEY_DOWN  || event.key == GLFW_KEY_S) { cursor_y = (cursor_y == 8) ? 0 : cursor_y+1; handled = 1; }
                    board_data[IDX(cursor_x, cursor_y)] |= BOARD_FLAG_CURSOR;
                }

                // digit placement
                if (!handled && IS_KEY_DOWN) {
                    if (event.key >= GLFW_KEY_1 && event.key <= GLFW_KEY_9) {
                        u32 idx = IDX(cursor_x, cursor_y);
                        if (!(board_data[idx] & BOARD_FLAG_STATIC)) {
                            // remove old value
                            if (!(board_data[idx] & BOARD_FLAG_PENCIL) && (board_data[idx] & BOARD_FLAG_ERROR)) {
                                cols[cursor_x] ^= board_data[idx];                                         
                                rows[cursor_y] ^= board_data[idx];
                                squares[(cursor_y/3)*3 + (cursor_x/3)] ^= board_data[idx];
                            }

                            board_data[idx] &= ~BOARD_FLAG_ERROR; // clear error flag
                            if (event.mod & GLFW_MOD_SHIFT)   {board_data[idx] &= ~BOARD_FLAG_PENCIL;} // clear pencil flag
                            if (!(board_data[idx] & BOARD_FLAG_PENCIL)) { board_data[idx] &= ~0x1FF; } // clear digits
                            board_data[idx] ^= 1 << (event.key-GLFW_KEY_1);                            // toggle digit
                            
                            // error checking
                            if (!(board_data[idx] & BOARD_FLAG_PENCIL)) {
                                if ((cols[cursor_x] | rows[cursor_y] | squares[(cursor_y/3)*3 + (cursor_x/3)]) & board_data[idx]) {
                                    board_data[idx] |= BOARD_FLAG_ERROR;
                                } else { 
                                    // set quick check flags
                                    cols[cursor_x] ^= board_data[idx];                                         
                                    rows[cursor_y] ^= board_data[idx];
                                    squares[(cursor_y/3)*3 + (cursor_x/3)] ^= board_data[idx];
                                }
                            }
                        }
                        handled = 1;
                    }
                }

                // keyboard static mode
                if (!handled && KEY_DOWN(GLFW_KEY_TAB)) {
                    board_data[IDX(cursor_x, cursor_y)] ^= BOARD_FLAG_STATIC;
                    handled = 1;
                }

                // keyboard pencil mode
                if (!handled && KEY_DOWN(GLFW_KEY_SPACE)) {
                    u32 idx = IDX(cursor_x, cursor_y);
                    if (!(board_data[idx] & BOARD_FLAG_STATIC)) {
                        board_data[idx] ^= BOARD_FLAG_PENCIL;
                    }
                    handled = 1;
                }

                // recompile shaders
                if (!handled && KEY_UP(GLFW_KEY_F5)) {
                    handled = 1;

                    GLuint old_shader_program = shader_program;
                    shader_program = build_shader_program(PATH_SHADER_VERT, PATH_SHADER_FRAG, &info_log);
                    if (shader_program == NULL) {
                        printf("[Error] Shader compilation failed.\n%s\n", info_log);
                        shader_program = old_shader_program;
                        free(info_log);
                    }
                    else {
                        printf("[Success] Shaders recompiled ... \n");

                        u_proj  = glGetUniformLocation(shader_program, "proj");
                        u_model = glGetUniformLocation(shader_program, "model");

                        u_font  = glGetUniformLocation(shader_program, "font");
                        u_board = glGetUniformLocation(shader_program, "board");

                        glDeleteProgram(old_shader_program);
                        glUseProgram(shader_program);
                    }
                }

                // quick paste
                if (!handled && (event.mod & GLFW_MOD_CONTROL) && KEY_UP(GLFW_KEY_V)) {
                    handled = 1;

                    // NOTE: supposedly glfw frees this? seems stupid.
                    const char* clipboard = glfwGetClipboardString(window);
                    char* c_ptr = (char*) clipboard;

                    printf("[Data]\n%s\n\n", clipboard);

                    u16* new_board = (u16*) malloc(BOARD_SIZE * 2);
                    u8 bx = 0, by = 0;
                    u16 r[9], c[9], s[9];
                        
                    while (*c_ptr) {
                        if (*c_ptr < '0' || *c_ptr > '9') {
                            if (*c_ptr == ' ' || *c_ptr == '\t' || *c_ptr == '\r' || *c_ptr == '\n') {
                                c_ptr++; 
                                continue; 
                            }

                            printf("[Error] Found \"%c\" in paste data.\n", *c_ptr);
                            free(new_board);
                            break;
                        }

                        u32 idx = IDX(bx,by);
                        if (*c_ptr == '0') {
                            new_board[idx] = BOARD_EMPTY;
                        } else {
                            new_board[idx] = BOARD_FLAG_STATIC | (1 << (*c_ptr-'1'));
                        }

                        c[cursor_x] ^= board_data[idx];
                        r[cursor_y] ^= board_data[idx];
                        s[(cursor_y/3)*3 + (cursor_x/3)] ^= board_data[idx];

                        c_ptr++;
                        bx++;
                        if (bx > 8) { bx = 0; by++; }
                        if (by > 8) { break; }
                    }

                    if (by > 7) {
                        free(board_data);
                        board_data = new_board;
                        board_data[IDX(cursor_x, cursor_y)] |= BOARD_FLAG_CURSOR;
                        for (u8 i = 0; i < 9; i++) {
                            rows[i]    = r[i];
                            cols[i]    = c[i];
                            squares[i] = s[i];
                        }
                    } else {
                        free(new_board);
                        printf("[Error] Not enough board data.");
                    }
                }
            }
        }
        input_index = 0;



        // window - resizing
        i32 _width = 0, _height = 0;
        glfwGetFramebufferSize(window, &_width, &_height);
        if (_width != window_width || _height != window_height) {
            glViewport(0, 0, _width, _height);
            window_width = _width;
            window_height = _height;

            window_ratio = f32(window_width) / f32(window_height);
            project_orthographic(&proj, -window_ratio, window_ratio, -1.0f, 1.0f, 0.0f, 100.0f);
        }



        // uniforms
        glUniformMatrix4fv(u_proj,  1, GL_FALSE, &proj.a[0]);
        glUniformMatrix4fv(u_model, 1, GL_FALSE, &scale.a[0]);

        glUniform1i(u_font, 0);
        glUniform1i(u_board, 1);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, tex_font);

        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, tex_board);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BOARD_DIM, BOARD_DIM, GL_RED_INTEGER, GL_UNSIGNED_SHORT, board_data);


        // render
        if (render_timer >= render_wait) {
            render_timer -= render_wait;

            glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        glfwSwapBuffers(window);



        // timing
        QueryPerformanceCounter(&end_time);
        delta_ms.QuadPart = end_time.QuadPart - start_time.QuadPart;
        delta_ms.QuadPart *= 1000;
        delta_ms.QuadPart /= cpu_freq.QuadPart;
        delta_s = f32(delta_ms.QuadPart) / 1000.f;
        total_time += delta_s;

        render_timer    += delta_s;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
