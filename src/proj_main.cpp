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

#ifdef DEBUG_ENABLE
    #define DEBUG 1
#else
    #define DEBUG 0
#endif


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

    m->x.x =  2 / drl;
    m->y.y =  2 / dtb;
    m->z.z = -2 / dfn;

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

#if DEBUG
        printf("\n[Compilation Fail] :: %s\n%s", filename, *info_log);
        printf("--------------------------------------------------------\n");
        debug_shader(source);
        printf("\n");
#endif

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
    input_queue[input_index].mouse_button  = 0;
    input_queue[input_index].mouse_button |= u8(button == GLFW_MOUSE_BUTTON_LEFT)  * 0x2;
    input_queue[input_index].mouse_button |= u8(button == GLFW_MOUSE_BUTTON_RIGHT) * 0x1;
    input_queue[input_index].action        = action;
    input_queue[input_index].mod           = mods;
    input_index++;
}

static void scroll_callback(GLFWwindow* window, f64 xoffset, f64 yoffset) {
}



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
#define BOARD_FLAG_HOVER   0x0800
#define BOARD_FLAG_R2      0x0400
#define BOARD_FLAG_R3      0x0200

#define BOARD_DIM        16
#define BOARD_SIZE       BOARD_DIM * BOARD_DIM
#define IDX(x,y)         ((y)*BOARD_DIM) + (x)


#define LIST_NULL    0x1
#define LIST_ROOT    0x2
#define LIST_SKIP    0x4
#define LIST_OTHER   0x8

struct ListItem {
    u8 type = LIST_NULL;
    i8 cursor_x;
    i8 cursor_y;
    u32 hover_idx;
    u16* board_data;
    ListItem* next;
    ListItem* prev;
};

u16* list_init(ListItem* root) {
    root->type = LIST_ROOT;
    root->cursor_x = 4;
    root->cursor_y = 4;
    root->board_data = (u16*) malloc(BOARD_SIZE * 2);
    for (u16 i = 0; i < BOARD_SIZE; i++) { 
        root->board_data[i] = BOARD_EMPTY; 
    }
    return root->board_data;
}

ListItem* list_copy(ListItem* node) {
    node->next = (ListItem*) malloc(sizeof(ListItem));
    node->next->board_data = (u16*) malloc(BOARD_SIZE * 2);
    for (u16 i = 0; i < BOARD_SIZE; i++) {
        node->next->board_data[i] = node->board_data[i];
    }
    node->next->prev = node;
    return node->next;
}

void list_free(ListItem* node) {
    free(node->board_data);
    free(node);
}


void _check(u16* board_data, u8 lx, u8 hx, u8 ly, u8 hy) {
    u16 cache[9][9];
    u8  indices[9];

    // clear cache
    for (u8 j = 0; j < 9; j++) {
        indices[j] = 0;
        for (u8 i = 0; i < 9; i++) {
            cache[i][j] = 0;
        }
    }
    
    // record values
    for (u8 j = ly; j < hy; j++) {
        for (u8 i = lx; i < hx; i++) {
            u16 idx = IDX(i, j);
            for (u8 n = 0; n < 9; n++) {
                if (board_data[idx] & (1<<n)) {
                    cache[n][indices[n]] = idx;
                    indices[n]++;
                }
            }
        }
    }

    // check errors
    for (u8 n = 0; n < 9; n++) {
        if (indices[n] < 2) continue;

        // count members statics
        u8 entered_set = 0;
        u8 static_set  = 0;
        for (u8 i = 0; i < 9; i++) {
            if (i >= indices[n]) break;
            if (!(board_data[cache[n][i]] & 0x1ff)) continue;
            if (!(board_data[cache[n][i]] & BOARD_FLAG_PENCIL)) entered_set++;
            if   (board_data[cache[n][i]] & BOARD_FLAG_STATIC)  static_set++;
        }
        
        // enter errors
        for (u8 i = 0; i < 9; i++) {
            if (i >= indices[n]) break;
            if (board_data[cache[n][i]] & BOARD_FLAG_PENCIL) {
                if (static_set > u8((board_data[cache[n][i]] & BOARD_FLAG_STATIC) != 0) ) {
                    board_data[cache[n][i]] |= BOARD_FLAG_ERROR;
                }
            } else {
                if ((entered_set + static_set) > 1) {
                    board_data[cache[n][i]] |= BOARD_FLAG_ERROR;
                }
            }
        }
    }
}

void check_errors(u16* board_data) {
    // clear errors
    for (u8 y = 0; y < 9; y++) {
        for (u8 x = 0; x < 9; x++) {
            board_data[IDX(x,y)] &= ~BOARD_FLAG_ERROR;
        }
    }

    // check squares
    for (u8 square_y = 0; square_y < 3; square_y++) {
        for (u8 square_x = 0; square_x < 3; square_x++) {
            _check(board_data, square_x*3, (square_x+1)*3, square_y*3, (square_y+1)*3);
        }
    }

    // check rows
    for (u8 row = 0; row < 9; row++) {
        _check(board_data, 0, 9, row, row+1);
    }

    // check cols
    for (u8 col = 0; col < 9; col++) {
        _check(board_data, col, col+1, 0, 9);
    }
}

// TODO solve the AI

#define PROGRESS_NONE   0
#define PROGRESS_CLEAR  1

void make_progress(u8* report, u16* board_data) {
    if (*report == PROGRESS_NONE) {

        // TODO: this is test code, and we seg fault
        for (u8 j = 0; j < 9; j++) {
            for (u8 i = 0; i < 9; i++) {
                board_data[IDX(i,j)] = BOARD_EMPTY;
            }
        }

        *report = PROGRESS_CLEAR;
    }
}








const u8 puzzles[16][81] = {
    {
        8,4,5,6,3,2,1,7,9,
        7,3,2,9,1,8,6,5,4,
        1,9,6,7,4,5,3,2,8,
        6,8,3,5,7,4,9,1,2,
        4,5,7,2,9,1,8,3,6,
        2,1,9,8,6,3,5,4,7,
        3,6,1,4,2,9,7,8,5,
        5,7,4,1,8,6,2,9,3,
        9,2,8,3,5,7,4,6,1
    },
    {
        2,5,6,8,3,1,7,4,9,
        8,3,7,6,4,9,5,1,2,
        1,9,4,7,2,5,3,8,6,
        6,4,1,5,8,7,9,2,3,
        7,2,5,1,9,3,8,6,4,
        3,8,9,4,6,2,1,7,5,
        9,7,8,2,5,4,6,3,1,
        5,6,2,3,1,8,4,9,7,
        4,1,3,9,7,6,2,5,8
    },
    {
        8,5,7,2,6,1,3,9,4,
        3,1,2,4,9,5,7,8,6,
        9,6,4,3,7,8,2,1,5,
        1,9,5,7,3,4,6,2,8,
        7,2,8,9,5,6,1,4,3,
        6,4,3,1,8,2,5,7,9,
        5,8,1,6,4,7,9,3,2,
        4,7,9,5,2,3,8,6,1,
        2,3,6,8,1,9,4,5,7
    },
    {
        8,5,7,3,9,2,4,1,6,
        2,1,4,8,5,6,3,7,9,
        9,3,6,1,4,7,2,8,5,
        5,6,8,4,2,9,1,3,7,
        4,9,2,7,3,1,6,5,8,
        1,7,3,6,8,5,9,4,2,
        3,2,1,5,6,8,7,9,4,
        6,4,5,9,7,3,8,2,1,
        7,8,9,2,1,4,5,6,3
    },
    {
        1,2,5,6,4,9,3,7,8,
        8,3,4,7,1,5,2,9,6,
        6,9,7,3,8,2,4,1,5,
        7,4,6,9,5,3,1,8,2,
        3,5,9,8,2,1,7,6,4,
        2,8,1,4,7,6,9,5,3,
        5,7,3,2,9,8,6,4,1,
        4,6,8,1,3,7,5,2,9,
        9,1,2,5,6,4,8,3,7
    },
    {
        2,3,8,4,6,7,9,1,5,
        4,1,5,2,9,3,6,7,8,
        7,9,6,8,5,1,2,3,4,
        9,7,3,5,4,8,1,6,2,
        6,2,4,1,3,9,8,5,7,
        8,5,1,7,2,6,4,9,3,
        5,8,7,9,1,2,3,4,6,
        1,6,2,3,7,4,5,8,9,
        3,4,9,6,8,5,7,2,1
    },
    {
        3,6,2,7,9,4,1,8,5,
        5,1,8,6,2,3,7,9,4,
        4,9,7,1,8,5,2,3,6,
        8,5,9,4,6,2,3,7,1,
        1,4,6,8,3,7,5,2,9,
        2,7,3,5,1,9,4,6,8,
        9,3,5,2,4,8,6,1,7,
        7,8,1,3,5,6,9,4,2,
        6,2,4,9,7,1,8,5,3
    },
    {
        6,7,5,9,4,8,2,1,3,
        3,2,8,1,6,5,9,7,4,
        1,4,9,7,3,2,5,6,8,
        2,9,1,3,5,7,4,8,6,
        4,8,6,2,9,1,7,3,5,
        5,3,7,6,8,4,1,2,9,
        8,1,4,5,2,3,6,9,7,
        9,5,2,8,7,6,3,4,1,
        7,6,3,4,1,9,8,5,2
    },
    {
        1,9,7,3,8,4,5,6,2,
        8,5,2,6,7,1,9,3,4,
        4,6,3,9,5,2,8,7,1,
        5,8,9,7,1,3,2,4,6,
        6,3,4,2,9,8,7,1,5,
        2,7,1,4,6,5,3,9,8,
        3,1,5,8,4,7,6,2,9,
        7,4,6,5,2,9,1,8,3,
        9,2,8,1,3,6,4,5,7
    },
    {
        9,7,2,8,6,3,5,4,1,
        6,1,8,7,4,5,9,2,3,
        4,5,3,2,9,1,6,8,7,
        5,4,9,1,2,8,7,3,6,
        8,2,1,6,3,7,4,5,9,
        7,3,6,4,5,9,2,1,8,
        2,9,5,3,8,6,1,7,4,
        1,8,4,9,7,2,3,6,5,
        3,6,7,5,1,4,8,9,2
    },
    {
        3,4,5,8,7,1,2,6,9,
        2,7,9,6,5,3,1,8,4,
        8,6,1,4,2,9,5,3,7,
        1,9,7,3,4,6,8,5,2,
        4,5,2,7,1,8,3,9,6,
        6,8,3,5,9,2,7,4,1,
        7,3,8,2,6,4,9,1,5,
        5,1,6,9,3,7,4,2,8,
        9,2,4,1,8,5,6,7,3
    },
    {
        2,9,4,8,6,3,5,1,7,
        7,1,5,4,2,9,6,3,8,
        8,6,3,7,5,1,4,9,2,
        1,5,2,9,4,7,8,6,3,
        4,7,9,3,8,6,2,5,1,
        6,3,8,5,1,2,9,7,4,
        9,8,6,1,3,4,7,2,5,
        5,2,1,6,7,8,3,4,9,
        3,4,7,2,9,5,1,8,6
    },
    {
        7,6,3,1,2,8,4,5,9,
        9,2,4,5,6,7,8,3,1,
        8,5,1,9,3,4,2,7,6,
        4,1,8,2,9,5,3,6,7,
        2,7,5,6,4,3,1,9,8,
        6,3,9,7,8,1,5,4,2,
        3,4,2,8,7,6,9,1,5,
        1,8,6,3,5,9,7,2,4,
        5,9,7,4,1,2,6,8,3
    },
    {
        8,7,3,9,6,1,4,2,5,
        6,2,4,7,5,3,9,1,8,
        9,5,1,2,4,8,3,7,6,
        5,1,8,6,9,4,2,3,7,
        2,6,9,1,3,7,5,8,4,
        4,3,7,5,8,2,6,9,1,
        3,8,2,4,7,5,1,6,9,
        7,9,5,3,1,6,8,4,2,
        1,4,6,8,2,9,7,5,3
    },
    {
        7,9,2,5,6,8,1,4,3,
        4,5,3,2,1,9,8,6,7,
        8,6,1,3,7,4,9,5,2,
        6,2,5,8,9,3,7,1,4,
        3,7,9,1,4,2,6,8,5,
        1,4,8,7,5,6,2,3,9,
        2,8,4,9,3,1,5,7,6,
        9,3,7,6,8,5,4,2,1,
        5,1,6,4,2,7,3,9,8
    },
    {
        1,6,2,8,5,7,4,9,3,
        5,3,4,1,2,9,6,7,8,
        7,8,9,6,4,3,5,2,1,
        4,7,5,3,1,2,9,8,6,
        9,1,3,5,8,6,7,4,2,
        6,2,8,7,9,4,1,3,5,
        3,5,6,4,7,8,2,1,9,
        2,4,1,9,3,5,8,6,7,
        8,9,7,2,6,1,3,5,4
    }
};

void generate_puzzle(u16* board) {
    printf("new puzzle\n");
    for (u8 i = 0; i < 9; i++){ 
        for (u8 j = 0; j < 9; j++){ 
            board[IDX(i,j)] = BOARD_FLAG_STATIC & (1 << i);
        }
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

    i32 monitor_x      = 0;
    i32 monitor_y      = 0;
    i32 monitor_width  = 0;
    i32 monitor_height = 0;
    i32 monitor_rate   = 0;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    glfwGetMonitorWorkarea(monitor, &monitor_x, &monitor_y, &monitor_width, &monitor_height);	
    monitor_rate = glfwGetVideoMode(monitor)->refreshRate;
    glfwSetGamma(monitor, 1.0);

    i32 window_width  = f32(monitor_height) * 0.9;
    i32 window_height = f32(monitor_height) * 0.9;

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Sudoku 2077", NULL, NULL);
    if (window == NULL) exit(-1);

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


    ListItem  history_root;
    ListItem* history_ptr = &history_root;
    u16* board_data = list_init(&history_root);

    GLuint tex_board;
    glGenTextures(1, &tex_board);
    glBindTexture(GL_TEXTURE_2D, tex_board);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, BOARD_DIM, BOARD_DIM, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, board_data);



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

    const f32 target_scale = 0.85f;
    mat4 scale;
    identity(&scale);
    scale_mat4(&scale, {target_scale, target_scale, target_scale});

    glUseProgram(shader_program);
    glBindVertexArray(vao);


    // mouse synch
    i8 mouse_target_x;
    i8 mouse_target_y;
    u8 hover_idx = 0xFF;

    f32 x_ratio = 1.0f, y_ratio = 1.0f;
    f32 adx     = 0.0f, ady     = 0.0f;



    // timing
    f32 total_time = 0.0;
    LARGE_INTEGER start_time, end_time, delta_ms, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_ms.QuadPart = 0;

    f32 delta_s = 0.0;

    f32 render_wait  = 1.0 / f32(monitor_rate);
    f32 render_timer = render_wait;

    i8 cursor_x = 4;
    i8 cursor_y = 4;
    u32 cursor_idx = IDX(cursor_x, cursor_y);
    board_data[cursor_idx] |= BOARD_FLAG_CURSOR;

    bool waiting_for_solution = false;
    u8 progress_report = PROGRESS_NONE;

    while (!glfwWindowShouldClose(window))
    {
        // state reset
        QueryPerformanceCounter(&start_time);
        glfwPollEvents();


        // window - resizing
        i32 _width = 0, _height = 0;
        glfwGetFramebufferSize(window, &_width, &_height);
        if (_width != window_width || _height != window_height) {
            glViewport(0, 0, _width, _height);

            window_width  = _width;
            window_height = _height;

            x_ratio = f32(window_width) / f32(window_height);
            y_ratio = f32(window_height) / f32(window_width);

            if (window_width > window_height) {
                adx = (x_ratio - 1) / 2.0f;
                project_orthographic(&proj, -x_ratio, x_ratio, -1.0f, 1.0f, 0.0f, 100.0f);
            }

            if (window_height > window_width) {
                ady = (y_ratio - 1) / 2.0f;
                project_orthographic(&proj, -1.0f, 1.0f, -y_ratio, y_ratio, 0.0f, 100.0f);
            }
        }


        // event handling
        for (u32 event_idx = 0; event_idx < input_index; event_idx++) {

            // reset for new event
            InputEvent event = input_queue[event_idx];

            history_ptr = list_copy(history_ptr);
            board_data  = history_ptr->board_data;

            u8 handled          = 0;            // set ths if the event was handled
            u8 board_undo       = 0;            // set this for a board undo
            u8 board_input      = 0;            // set this if there was a board change
            u8 board_input_type = LIST_OTHER;   // set this to LIST_SKIP if skippable in undo chain

            u8 made_progress = 0; // FIXME: how does this work again? maybe not in event loop?



            // mouse coords [0,1] -> [-ratio_delta, 1 + ratio_delta]
            f32 screen_x = event.mouse_position[0] / window_width;
            f32 screen_y = event.mouse_position[1] / window_height;

            screen_x = screen_x*(1.0f + adx + adx) - adx;
            screen_y = screen_y*(1.0f + ady + ady) - ady;

            // remap coords to account for board scale
            f32 d = 0.5 * (1.0f - target_scale);
            screen_x = screen_x*(1.0f+d+d) - d; 
            screen_y = screen_y*(1.0f+d+d) - d; 

            // clear current hover
            if (hover_idx < 0xFF) { 
                // NOTE: instead of saving this to a new history node, override the previous node
                history_ptr->prev->board_data[hover_idx] &= ~BOARD_FLAG_HOVER; 
                hover_idx = 0xFF;
            }

            // get hover with deadband
            if (screen_x < 1.0f && screen_y < 1.0f) {
                const f32 gamma = 0.8f * (1.0f/9.0f);

                f32 xn = screen_x * 9.0f;
                i8  xi = i8(xn);
                f32 dx  = xn - f32(xi);

                f32 yn = screen_y * 9.0f;
                i8  yi = i8(yn);
                f32 dy  = yn - f32(yi);

                // NOTE: operates as an override of previous node in history
                if (gamma < dx && dx < 1.0f - gamma) {
                    if (gamma < dy && dy < 1.0f - gamma) {
                        mouse_target_x = xi;
                        mouse_target_y = yi;
                        hover_idx = IDX(mouse_target_x, mouse_target_y);
                        history_ptr->prev->board_data[hover_idx] |= BOARD_FLAG_HOVER; 
                        history_ptr->prev->hover_idx = hover_idx;
                    }
                }

            } 

            if (event.type == INPUT_TYPE_MOUSE_PRESS) {
                if (!handled && event.mouse_button & 0x2) {
                    // move cursor to hover
                    if (hover_idx < 0xFF) {
                        board_data[cursor_idx] &= ~BOARD_FLAG_CURSOR;

                        cursor_x = mouse_target_x;
                        cursor_y = mouse_target_y;
                        cursor_idx = IDX(cursor_x, cursor_y);

                        board_data[cursor_idx] |= BOARD_FLAG_CURSOR;
                        board_input      = 1;
                        board_input_type = LIST_SKIP;
                    }

                    handled = 1;
                }
            }


            if (event.type == INPUT_TYPE_KEY_PRESS) {
                // quit or clear
                if (!handled && KEY_UP(GLFW_KEY_ESCAPE)) {
                    waiting_for_solution = false;

                    // quit
                    if (event.mod & GLFW_MOD_SHIFT) return;

                    // clear digits
                    board_data[cursor_idx] = BOARD_EMPTY | BOARD_FLAG_CURSOR;
                    board_input = 1;
                    handled = 1;
                }

                // check - solve
                if (waiting_for_solution || KEY_UP(GLFW_KEY_ENTER)) {
                    // check if there's statics
                    if (!waiting_for_solution) {
                        for (u8 j = 0; j < 9; j++) {
                            for (u8 i = 0; i < 9; i++) {
                                if (board_data[IDX(i,j)] & BOARD_FLAG_STATIC) {
                                    waiting_for_solution = true;
                                    break;
                                }
                            }
                            if (waiting_for_solution) break;
                        }
                    }
                    
                    // FIXME: you probably doing want to do this only when theres a key press event....
                    // perform solution iteration
                    if (waiting_for_solution) {
                        if (!made_progress) {
                            make_progress(&progress_report, board_data);
                            made_progress = 1;
                        }
                        continue;
                    }
                }

                // board clear
                if (KEY_UP(GLFW_KEY_BACKSPACE)) {
                    for (u32 i = 0; i < BOARD_SIZE; i++) {
                        board_data[i] = BOARD_EMPTY;
                    }
                    board_data[cursor_idx] |= BOARD_FLAG_CURSOR;
                    board_input = 1;
                    handled = 1;
                }


                // new puzzle
                if (!handled && (event.mod & GLFW_MOD_CONTROL) && KEY_DOWN(GLFW_KEY_N)) {
                    generate_puzzle(board_data);
                    handled     = 1;
                    board_input = 1;
                }


                // keyboard cursor
                if (!handled && IS_KEY_DOWN) {
                    board_data[cursor_idx] &= ~BOARD_FLAG_CURSOR;

                    u8 dr = 1;
                    if (event.mod & GLFW_MOD_SHIFT) { dr = 3; }
                    if      (event.key == GLFW_KEY_LEFT  || event.key == GLFW_KEY_A) { cursor_x -= dr; handled = 1; }
                    else if (event.key == GLFW_KEY_RIGHT || event.key == GLFW_KEY_D) { cursor_x += dr; handled = 1; }
                    else if (event.key == GLFW_KEY_UP    || event.key == GLFW_KEY_W) { cursor_y -= dr; handled = 1; }
                    else if (event.key == GLFW_KEY_DOWN  || event.key == GLFW_KEY_S) { cursor_y += dr; handled = 1; }

                    while (cursor_x < 0) { cursor_x += 9; } cursor_x %= 9;
                    while (cursor_y < 0) { cursor_y += 9; } cursor_y %= 9;

                    cursor_idx = IDX(cursor_x, cursor_y);
                    board_data[cursor_idx] |= BOARD_FLAG_CURSOR;

                    if (handled) {
                        board_input = 1;
                        board_input_type = LIST_SKIP;
                    }
                }

                // digit placement
                if (!handled && IS_KEY_DOWN) {
                    if (event.key >= GLFW_KEY_1 && event.key <= GLFW_KEY_9) {
                        u32 idx = cursor_idx;
                        if (!(board_data[idx] & BOARD_FLAG_STATIC)) {
                            u16 target = 1 << (event.key-GLFW_KEY_1);
                            // clear pencil flag
                            if (event.mod & GLFW_MOD_SHIFT) {board_data[idx] &= ~BOARD_FLAG_PENCIL;}                              
                            // clear digits
                            if (!(board_data[idx] & BOARD_FLAG_PENCIL)) board_data[idx] &= (~0x1FF | (board_data[idx] & target)); 
                            // pre-empt placement
                            if (event.mod & GLFW_MOD_SHIFT) {board_data[idx] ^= target;}                                          
                            // toggle digit
                            board_data[idx] ^= target;                                                                            
                        }
                        handled = 1;
                        board_input = 1;
                    }
                }

                // keyboard static mode
                if (!handled && KEY_DOWN(GLFW_KEY_TAB) && (board_data[cursor_idx] & 0x1ff)) {
                    board_data[cursor_idx] ^= BOARD_FLAG_STATIC;
                    handled = 1;
                    board_input = 1;
                }

                // keyboard pencil mode
                if (!handled && KEY_DOWN(GLFW_KEY_SPACE)) {
                    if (!(board_data[cursor_idx] & BOARD_FLAG_STATIC)) {
                        if (board_data[cursor_idx] & BOARD_FLAG_PENCIL) {
                            board_data[cursor_idx] = BOARD_FLAG_CURSOR;
                        }
                        else {
                            board_data[cursor_idx] |= BOARD_FLAG_PENCIL;
                        }
                    }
                    handled = 1;
                    board_input = 1;
                    board_input_type = LIST_SKIP;
                }

                // undo
                if (!handled && (event.mod & GLFW_MOD_CONTROL) && KEY_DOWN(GLFW_KEY_Z)) {
                    handled    = 1;
                    board_undo = 1;

                    u8 count = 0;
                    while (count < 2 || (history_ptr->type & (LIST_NULL | LIST_SKIP))) {
                        if (history_ptr->type == LIST_ROOT) break;
                            
                        ListItem* tmp = history_ptr;
                        history_ptr = history_ptr->prev;
                        list_free(tmp);
                        
                        count++;
                    }
                    
                    cursor_x   = history_ptr->cursor_x;
                    cursor_y   = history_ptr->cursor_y;
                    cursor_idx = IDX(cursor_x, cursor_y);

                    board_data = history_ptr->board_data;
                    board_data[history_ptr->hover_idx] &= ~BOARD_FLAG_HOVER;
                    hover_idx = 0xFF;
                }

                // hard undo
                if (!handled && (event.mod & GLFW_MOD_ALT) && KEY_DOWN(GLFW_KEY_Z)) {
                    handled    = 1;
                    board_undo = 1;

                    while (history_ptr->type != LIST_ROOT) {
                        ListItem* tmp = history_ptr;
                        history_ptr = history_ptr->prev;
                        list_free(tmp);
                    }
                    
                    cursor_x   = history_ptr->cursor_x;
                    cursor_y   = history_ptr->cursor_y;
                    cursor_idx = IDX(cursor_x, cursor_y);

                    board_data = history_ptr->board_data;
                    board_data[history_ptr->hover_idx] &= ~BOARD_FLAG_HOVER;
                    hover_idx = 0xFF;
                }

                // quick paste
                if (!handled && (event.mod & GLFW_MOD_CONTROL) && KEY_UP(GLFW_KEY_V)) {
                    handled = 1;

                    // NOTE: supposedly glfw frees this? seems stupid.
                    const char* clipboard = glfwGetClipboardString(window);
                    char* c_ptr = (char*) clipboard;

                    printf("[Data]\n%s\n\n", clipboard);

                    u8 bx = 0, by = 0;
                        
                    while (*c_ptr) {
                        if (*c_ptr < '0' || *c_ptr > '9') {
                            if (*c_ptr == ' ' || *c_ptr == '\t' || *c_ptr == '\r' || *c_ptr == '\n') {
                                c_ptr++; 
                                continue; 
                            }

                            printf("[Error] Found \"%c\" in paste data.\n", *c_ptr);
                            break;
                        }

                        u32 idx = IDX(bx,by);
                        if (*c_ptr == '0') {
                            board_data[idx] = BOARD_EMPTY;
                        } else {
                            board_data[idx] = BOARD_FLAG_STATIC | (1 << (*c_ptr-'1'));
                        }

                        c_ptr++;
                        bx++;
                        if (bx > 8) { bx = 0; by++; }
                        if (by > 8) { break; }
                    }

                    if (by > 7) {
                        board_input = 1;
                        board_data[cursor_idx] |= BOARD_FLAG_CURSOR;
                    } else {
                        printf("[Error] Not enough board data.");
                    }
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


            } // end of key press


            // alter board history
            if (!board_input) {
                // if there was an undo, handle the history cleanup in there
                if (!board_undo) {
                    ListItem* tmp = history_ptr;
                    history_ptr = history_ptr->prev;
                    board_data  = history_ptr->board_data;
                    list_free(tmp);
                }
            } else {
                history_ptr->type = board_input_type;
                check_errors(board_data);
                history_ptr->cursor_x = cursor_x;
                history_ptr->cursor_y = cursor_y;
            }

        } // end of events

        input_index = 0;



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
