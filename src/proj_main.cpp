// local
#include "proj_types.h"

// third party
#include "windows.h"
#undef near
#undef far

#include "glad/glad.h"
#include "GLFW/glfw3.h"

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





struct Controls {
    bool keys[350]        = { false };
    bool mouse_buttons[2] = { false };
    f32 mouse_position[2] = { 0.0f };
} controls;


static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void mouse_move_callback(GLFWwindow* window, f64 xpos, f64 ypos) {
    controls.mouse_position[0] = xpos;
    controls.mouse_position[1] = ypos;
}

static void mouse_click_callback(GLFWwindow* window, i32 button, i32 action, i32 mods) {
    controls.mouse_buttons[0] = (button == GLFW_MOUSE_BUTTON_LEFT  && action == GLFW_PRESS);
    controls.mouse_buttons[1] = (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS);
}

static void scroll_callback(GLFWwindow* window, f64 xoffset, f64 yoffset) {
}

static void key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
    if (key >= 0) {
        controls.keys[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }
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
    } else {
        printf("OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);
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
    #define FONT_PAD      4
    #define FONT_DIM      32
    #define FONT_WIDTH    (FONT_DIM + FONT_PAD) * 9

    u8* font = (u8*) malloc(FONT_WIDTH * FONT_DIM);
    for (u16 n = 0; n < 10; n++) {
        for (u16 j = 0; j < FONT_DIM; j++) {
            for (u16 i = 0; i < FONT_DIM; i++) {
                font[j*(FONT_DIM+FONT_PAD) + i] = 0;
            }
        }
    }

    GLuint tex_font;
    glGenTextures(1, &tex_font);
    glBindTexture(GL_TEXTURE_2D, tex_font);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FONT_WIDTH, FONT_DIM, 0, GL_RED, GL_UNSIGNED_BYTE, font);
    glGenerateMipmap(GL_TEXTURE_2D);


    /*
       bottom --  10 bit  - numbers pencilled / active number
       top    --  1  bit  - pencil mode active
    */
    #define PENCIL_DIM        16
    #define PENCIL_SIZE       PENCIL_DIM * PENCIL_DIM
    #define IDX(x,y)          (y*PENCIL_DIM) + x

    u16 pencil_data[PENCIL_SIZE] = {};
    for (u16 i = 0; i < PENCIL_SIZE; i++) { 
        pencil_data[i] = 0b1000001111111111; 
    }

    pencil_data[IDX(0,0)] = 0;
    pencil_data[IDX(8,8)] = 0;

    GLuint tex_pencil;
    glGenTextures(1, &tex_pencil);
    glBindTexture(GL_TEXTURE_2D, tex_pencil);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, PENCIL_DIM, PENCIL_DIM, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, &pencil_data[0]);




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

    GLuint u_font   = glGetUniformLocation(shader_program, "font");
    GLuint u_pencil = glGetUniformLocation(shader_program, "pencil");



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

    f32 recompile_wait  = 1.0;
    f32 recompile_timer = recompile_wait;

    while (!glfwWindowShouldClose(window))
    {
        QueryPerformanceCounter(&start_time);
        glfwPollEvents();

        // quit
        if (controls.keys[GLFW_KEY_ESCAPE]) {
            return;
        }

        // recompile shaders
        if (controls.keys[GLFW_KEY_F5] && recompile_timer > recompile_wait) {
            recompile_timer = 0;
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

                u_font   = glGetUniformLocation(shader_program, "font");
                u_pencil = glGetUniformLocation(shader_program, "pencil");

                glDeleteProgram(old_shader_program);
                glUseProgram(shader_program);
            }
        }

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
        glUniform1i(u_pencil, 1);

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, tex_font);

        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, tex_pencil);

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
        recompile_timer += delta_s;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
