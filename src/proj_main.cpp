/*
TODO
------------------------------
Add sound events

Handle sound variations in proj_main
    u8 variations[N_SOUNDS];
    variations[sound_id] = (variations[sound_id] + rand()) % N_SOUND_X_VARIATIONS

Make the solver smarter
:: src => square / row / column
 - if you see the only place for 1's in the src:a is in src:b,
   propagate to the rest of the src:b, removing 1's

Extend the solver to perform graph traversals

XXX
------------------------------
Make the solver smarter
 - for example, if u see theres only 1 place for a penciled number in the [square,row,col], ink it

*/


// local
#include "proj_types.h"
#include "proj_math.h"
#include "proj_sound.h"

// third party
#include "windows.h"
#include "dsound.h"
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



#define BOARD_EMPTY        0x0000
#define BOARD_1            0x0001
#define BOARD_2            0x0002
#define BOARD_3            0x0004
#define BOARD_4            0x0008
#define BOARD_5            0x0010
#define BOARD_6            0x0020
#define BOARD_7            0x0040
#define BOARD_8            0x0080
#define BOARD_9            0x0100
#define BOARD_ALL          0x01FF

#define BOARD_FLAG_PENCIL  0x8000
#define BOARD_FLAG_ERROR   0x4000
#define BOARD_FLAG_STATIC  0x2000
#define BOARD_FLAG_CURSOR  0x1000
#define BOARD_FLAG_HOVER   0x0800
#define BOARD_FLAG_SOLVE   0x0400
#define BOARD_FLAG_AI      0x0200

#define BOARD_DIM        16
#define BOARD_SIZE       BOARD_DIM * BOARD_DIM
#define IDX(x,y)         (u16(y)*BOARD_DIM) + (x)


#define LIST_NULL    0x1
#define LIST_ROOT    0x2
#define LIST_SKIP    0x4
#define LIST_OTHER   0x8

struct ListItem {
    u8 type = LIST_NULL;
    i8 cursor_x;
    i8 cursor_y;
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


u8 _check(u16* board_data, u8 lx, u8 hx, u8 ly, u8 hy) {
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
    u8 score = 0;
    for (u8 n = 0; n < 9; n++) {
        if (indices[n] < 2) {
            if (indices[n] > 0) score++;
            continue;
        }

        // count members statics
        u8 entered_set = 0;
        u8 static_set  = 0;
        for (u8 i = 0; i < 9; i++) {
            if (i >= indices[n]) break;
            if (!(board_data[cache[n][i]] & BOARD_ALL)) continue;
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

    return score;
}

u8 validate_board(u16* board_data) {
    // clear errors and solves
    for (u8 y = 0; y < 9; y++) {
        for (u8 x = 0; x < 9; x++) {
            board_data[IDX(x,y)] &= ~u16(BOARD_FLAG_ERROR | BOARD_FLAG_SOLVE);
        }
    }

    // each check is worth 9
    // - 9 square checks
    // - 9 row checks
    // - 9 col checks
    // => success = 9 * 9 * 3
    u32 score = 0;

    // check squares
    for (u8 square_y = 0; square_y < 3; square_y++) {
        for (u8 square_x = 0; square_x < 3; square_x++) {
            score += _check(board_data, square_x*3, (square_x+1)*3, square_y*3, (square_y+1)*3);
        }
    }

    // check rows
    for (u8 row = 0; row < 9; row++) {
        score += _check(board_data, 0, 9, row, row+1);
    }

    // check cols
    for (u8 col = 0; col < 9; col++) {
        score += _check(board_data, col, col+1, 0, 9);
    }

    // solve check
    if (score >= 9*9*3) {
        for (u8 j = 0; j < 9; j++) {
            for (u8 i = 0; i < 9; i++) {
                if (!(board_data[IDX(i,j)] & BOARD_FLAG_STATIC)) {
                    board_data[IDX(i,j)] &= ~u16(BOARD_FLAG_PENCIL);
                    board_data[IDX(i,j)] |= BOARD_FLAG_SOLVE;
                }
            }
        }
        return 1;
    }
    return 0;
}



#define PROGRESS_DEFAULT        0   // no state change
#define PROGRESS_STATE_CHANGE   1   // state change
#define PROGRESS_INV_CELL       2   // invalid cell
#define PROGRESS_SET_CELL       3   // cell solved

u8 set_pencils(u16* board, u8 clear) {
    u8 statics = 0;
    u16 mask = BOARD_FLAG_PENCIL | BOARD_ALL;

    // set non-statics to full pencils
    for (u16 j = 0; j < 9; j++) {
        for (u16 i = 0; i < 9; i++) {
            u16 idx = IDX(i,j);
            if (!(board[idx] & BOARD_FLAG_STATIC)) {
                if (clear) {
                    board[idx] |= mask;
                } else if (!(board[idx] & BOARD_ALL) || (board[idx] & BOARD_FLAG_PENCIL)) {
                    // set if there is only 1 bit set
                    board[idx] |= mask;
                }
            } else statics++;
        }
    }

    return statics != 81;
}

/*
FIXME
implement tree search for something like this,

300200000
000107000
706030500
070009080
900020004
010800050
009040301
000702000
000008006
*/

#define CACHE_REGION() {\
    u8 flag = 0;\
    flag |= (board[indices[n]] & BOARD_FLAG_STATIC) != 0;\
    flag |= (board[indices[n]] & BOARD_FLAG_PENCIL) == 0;\
    if (flag) {\
        statics |= board[indices[n]];\
    }\
    \
    /* cache region */\
    for (u8 i = 0; i < 9; i++) {\
        if (n == i) continue;\
        if (!n) caches[i] = 0;\
        caches[i] |= board[indices[n]];\
    }\
}


#define UPDATE_REGION() {\
    statics &= BOARD_ALL;\
    while (statics < BOARD_ALL) {\
        u8 set = 0;\
        for (u8 x = 0; x < 9; x++) {\
            if (board[indices[x]] & BOARD_FLAG_PENCIL) {\
                /* update pencil options */ \
                board[indices[x]] &= ~statics;\
                \
                /* ink */ \
                u16 check = board[indices[x]] & BOARD_ALL;\
                if (check && !(check & (check-1))) {\
                    board[indices[x]] &= ~u16(BOARD_FLAG_PENCIL);\
                    statics |= check;\
                    set = 1;\
                } else {\
                    u16 changed = check ^ (check & caches[x] & BOARD_ALL);\
                    if (changed) {\
                        board[indices[x]] &= changed | ~u16(BOARD_ALL);\
                        board[indices[x]] &= ~u16(BOARD_FLAG_PENCIL);\
                        statics |= changed;\
                        set = 1;\
                    }\
                }\
            }\
        }\
        if (!set) break;\
    }\
}


u8 fast_solve(u16* board) {
    u8  solved = 0;

    u16 indices[9];
    u16 caches[9];
    u16 statics;

    // rows
    for (u8 y = 0; y < 9; y++) {
        statics = 0;

        // grab statics
        for (u8 x = 0; x < 9; x++) {
            u8 n = x;
            indices[n] = IDX(x,y);
            CACHE_REGION();
        }

        UPDATE_REGION();
        if (statics == BOARD_ALL) solved++;
    }


    // squares
    for (u8 cy = 0; cy < 9; cy += 3) {
        for (u8 cx = 0; cx < 9; cx += 3) {
            statics = 0;

            // grab statics
            for (u8 y = 0; y < 3; y++) {
                for (u8 x = 0; x < 3; x++) {
                    u8 n = y*3 + x;
                    indices[n] = IDX( (cx+x), (cy+y) );
                    CACHE_REGION();
                }
            }

            UPDATE_REGION();
            if (statics == BOARD_ALL) solved++;
        }
    }


    // cols
    for (u8 y = 0; y < 9; y++) {
        statics = 0;

        // grab statics
        for (u8 x = 0; x < 9; x++) {
            u8 n = x;
            indices[n] = IDX(y,x);
            CACHE_REGION();
        }

        UPDATE_REGION();
        if (statics == BOARD_ALL) solved++;
    }

    // solved [rows + cols + squares]
    return solved == 27;
}


// compares cells to remove options
#define DEDUCE() {\
    bool cell_static = board[cmp_idx] & BOARD_FLAG_STATIC;\
    bool cell_pencil = board[cmp_idx] & BOARD_FLAG_PENCIL;\
    \
    cache |= board[cmp_idx];\
    \
    if (cell_static || !cell_pencil) {\
        u16 tmp = board[base_idx] & BOARD_ALL;\
        board[base_idx] &= ~(BOARD_ALL & board[cmp_idx]);\
        if (tmp != (board[base_idx] & BOARD_ALL)) {\
            state_change = PROGRESS_STATE_CHANGE;\
        }\
    }\
}

// solidifies options
#define INK() {\
    cache &= BOARD_ALL;\
    \
    u16 check = board[base_idx] & BOARD_ALL;\
    if (check && !(check & (check-1))) {\
        board[base_idx] &= ~u16(BOARD_FLAG_PENCIL);\
        state_change     = PROGRESS_SET_CELL;\
    } else if (cache < BOARD_ALL){\
        /*
           For when you inevitably forget,
           0000 0001 1011 1111 :: cache - no 7's in the square/row/col
           0000 0000 1111 0000 :: board - we can put our 7 down
           
           0000 0000 1011 0000 :: board & cache
           0000 0000 0100 0000 :: board ^ (board & cache)
        */\
        u16 changed = board[base_idx] ^ (board[base_idx] & cache);\
        if (changed & BOARD_ALL) {\
            board[base_idx] &= changed | ~u16(BOARD_ALL);\
            board[base_idx] &= ~u16(BOARD_FLAG_PENCIL);\
            state_change     = PROGRESS_SET_CELL;\
        }\
    }\
}



// NOTE: u should really apply memoization here
// for use in the generator you could even write a faster solve thats not designed for rendering?
u8 make_progress(u16* board, u8 base_x, u8 base_y, u8 stage) {
    u8 state_change = PROGRESS_DEFAULT;

    // skip statics and already set cells
    u16 base_idx = IDX(base_x, base_y);
    {
        bool cell_static = board[base_idx] & BOARD_FLAG_STATIC;
        bool cell_pencil = board[base_idx] & BOARD_FLAG_PENCIL;
        if (cell_static || !cell_pencil) return PROGRESS_INV_CELL;
    }

    u16 cache = 0;

    // check square
    if (stage == 0) {
        for (u16 cmp_y = 0; cmp_y < 3; cmp_y++) {
            for (u16 cmp_x = 0; cmp_x < 3; cmp_x++) {
                u16 cmp_inner_x = (base_x/3)*3+cmp_x;
                u16 cmp_inner_y = (base_y/3)*3+cmp_y;
                u16 cmp_idx = IDX(cmp_inner_x, cmp_inner_y);
                if (cmp_idx == base_idx) continue;
                DEDUCE();
            }
        }
        INK();
    }

    // check row
    if (stage == 1) {
        for (u32 cmp_x = 0; cmp_x < 9; cmp_x++) {
            u16 cmp_idx = IDX(cmp_x, base_y);
            DEDUCE();
        }
        INK();
    }

    // check col
    if (stage == 2) {
        for (u16 cmp_y = 0; cmp_y < 9; cmp_y++) {
            u16 cmp_idx = IDX(base_x, cmp_y);
            DEDUCE();
        }
        INK();
    }

    return state_change;
}



#define PATTERN_SPIRAL_INNER    0
#define PATTERN_SPIRAL_OUTER    1
#define PATTERN_ROW_SNAKE       2
#define PATTERN_COL_SNAKE       3

#define N_PATTERNS              4

u8 pattern_idx = 0;
u8 patterns[N_PATTERNS][81][2];

void debug_pattern(u8 pattern_id) {
    u8 quit = 0;
    u8 debug[81] = {};
    for (u8 i = 0; i < 81; i++) {
        u8 x = patterns[pattern_id][i][0];
        u8 y = patterns[pattern_id][i][1];
        if (x > 8 || y > 8) {
            printf("idx:  %u    x: %u  y: %u\n", i,x,y);
            quit = 1;
        } else {
            debug[(9*y) + x] = 1;
        }
    }
    if (quit) return;
    printf("\n");
    for (u8 x = 0; x < 9; x++) {
        for (u8 y = 0; y < 9; y++) {
            if (debug[(9*y) + x]) printf("X ");
            else                 printf("- ");
        }
        printf("\n");
    }
    printf("\n");
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


void swap_col(u16* board, u8 a, u8 b) {
    for (u8 i = 0; i < 9; i++) {
        u16 tmp = board[IDX(a, i)];
        board[IDX(a, i)] = board[IDX(b, i)];
        board[IDX(b, i)] = tmp;
    }
}

void swap_row(u16* board, u8 a, u8 b) {
    for (u8 i = 0; i < 9; i++) {
        u16 tmp = board[IDX(i, a)];
        board[IDX(i, a)] = board[IDX(i, b)];
        board[IDX(i, b)] = tmp;
    }
}

u8 puzzle_idx = rand() % 16;
void generate_puzzle(u16* board) {
    // grab puzzle and apply mapping
    u8 mapping[] = {1,2,3,4,5,6,7,8,9};
    for (u8 i = 0; i < 8; i++) {
        u8 idx       = i + rand() / (RAND_MAX / (9 - i) + 1);
        u8 tmp       = mapping[idx];
        mapping[idx] = mapping[i];
        mapping[i]   = tmp;
    }
    
    for (u16 j = 0; j < 9; j++) { 
        for (u16 i = 0; i < 9; i++) { 
            u16 x;
            x = puzzles[puzzle_idx][(j*9)+i];
            x = mapping[x - 1];
            board[IDX(i,j)] = BOARD_FLAG_STATIC | (1<<(x-1));
        }
    }

    puzzle_idx = rand() % 16; // global puzzle selector

    // permute
    for (u32 i = 0; i < 1000; i++) {
        u32 a = rand() % 9;
        u32 p = a % 3;

        u32 b;
        if      (p == 0) b = a + 1 + (rand() % 2);
        else if (p == 2) b = a - 1 - (rand() % 2);
        else if (p == 1) b = a + ((rand()%1) * 2 - 1)*(rand() % 1);

        if (rand()%2 == 0) swap_row(board, a, b);
        else               swap_col(board, a, b);
    }


    // hide tiles until puzzle cannot be solved in N board iterations
    #define N               18
    #define ACCEPTED_FAILS  45
    #define OUTPUT_BOARD() {\
        for (u32 y = 0; y < 9; y++) {\
            for (u32 x = 0; x < 9; x++) {\
                u16 val = board[IDX(x,y)];\
                if (val & BOARD_FLAG_PENCIL) { printf("0 "); continue; }\
                val &= BOARD_ALL;\
                u8 d;\
                for (d = 0; d < 9; d++) { if ((val>>d) & 0x1) break; }\
                printf("%u ", (d+1)%10);\
            }\
            printf("\n");\
        }\
        printf("\n");\
    }

    u8 pattern_idx = rand() % N_PATTERNS;

    u32  hidden[81] = {0};
    u8   hidden_idx = 0;
    u32  fails = 0;
    while (1) {
        // - hide tile
        u16 rnd_x = rand() % 9;
        u16 rnd_y = rand() % 9;
        u16 idx = IDX(rnd_x, rnd_y);

        // save, and skip if already hidden
        u16 tmp = board[idx];
        board[idx] = BOARD_EMPTY;
        if (board[idx] == tmp) continue;

        u8 solved = 0;
        set_pencils(board, 1);
        for (u32 n = 0; n < N; n++) {
#if 0
            // - solve with patterns
            for (u8 i = 0; i < 81; i++) {
                u8 x = patterns[pattern_idx][i][0];
                u8 y = patterns[pattern_idx][i][1];
                for (u32 s = 0; s < 3; s++) {
                    u8 status = make_progress(board, x, y, s);
                    if (status == PROGRESS_INV_CELL) break; 
                }
            }

            // if we solved it, add the hidden tile to our collection
            // and retry another tile
            solved = validate_board(board);
            if (solved) {
                hidden[hidden_idx] = idx;
                hidden_idx++;
                break;
            } 

#else
            // - solve quickly
            solved = fast_solve(board);
            if (solved) {
                hidden[hidden_idx] = idx;
                hidden_idx++;
                break;
            } 
#endif

        }

        pattern_idx = rand() % N_PATTERNS;

        // rehide tiles
        for (u8 i = 0; i < hidden_idx; i++) {
            board[hidden[i]] = BOARD_EMPTY;
        }

        // if we couldn't solve it, undo the tile placement and record a fail
        if (!solved) {
            fails++;
            board[idx] = tmp;
            if (fails > ACCEPTED_FAILS) break;
        }

    }

    #undef N
    #undef ACCEPTED_FAILS
    #undef OUTPUT_BOARD
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

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Sudoku", NULL, NULL);
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

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

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



    // shaders: TODO abstract shader program?
    /*
       struct Shader {
            GLuint    program;
            HashTable uniforms;
       };
    */

    #define PATH_SHADER_MAIN_VERT "./shaders/main.vert"
    #define PATH_SHADER_MAIN_FRAG "./shaders/main.frag"

    u8* info_log;

    info_log = nullptr;
    GLuint shader_program_main = build_shader_program(PATH_SHADER_MAIN_VERT, PATH_SHADER_MAIN_FRAG, &info_log);
    if (shader_program_main == NULL) {
        printf("[Error] Shader compilation failed.\n%s\n", info_log);
        return;
    }

    GLuint u_proj  = glGetUniformLocation(shader_program_main, "proj"); // TODO: uniform buffer objects?
    GLuint u_model = glGetUniformLocation(shader_program_main, "model");

    GLuint u_font  = glGetUniformLocation(shader_program_main, "font");
    GLuint u_board = glGetUniformLocation(shader_program_main, "board");



    #define PATH_SHADER_MOUSE_VERT "./shaders/mouse.vert"
    #define PATH_SHADER_MOUSE_FRAG "./shaders/mouse.frag"

    info_log = nullptr;
    GLuint shader_program_mouse = build_shader_program(PATH_SHADER_MOUSE_VERT, PATH_SHADER_MOUSE_FRAG, &info_log);
    if (shader_program_mouse == NULL) {
        printf("[Error] Shader compilation failed.\n%s\n", info_log);
        return;
    }

    GLuint u_proj_mouse  = glGetUniformLocation(shader_program_mouse, "proj"); // TODO: uniform buffer objects?
    GLuint u_model_mouse = glGetUniformLocation(shader_program_mouse, "model");

    GLuint u_mouse_mouse = glGetUniformLocation(shader_program_mouse, "mouse");
    GLuint u_time_mouse  = glGetUniformLocation(shader_program_mouse, "time");


    // orthogonal projection and scale matrice
    glBindVertexArray(vao);

    mat4 proj;
    identity(&proj);
    f32 window_ratio = f32(window_width) / f32(window_height);
    project_orthographic(&proj, -window_ratio, window_ratio, -1.0f, 1.0f, 0.0f, 100.0f);

    const f32 target_scale = 0.85f;
    mat4 scale;
    identity(&scale);
    scale_mat4(&scale, {target_scale, target_scale, 1.0});

    mat4 scale_mouse;
    identity(&scale_mouse);

    
    // pre-compute pattern traversal arrays
    {
        #define BREAK  if (n > 80) break
        #define SET(i) patterns[i][n][0] = x; patterns[i][n][1] = y; n++

        // INNER SPIRAL --------
        u8 n = 0, x = 4, y = 4;
        SET(PATTERN_SPIRAL_INNER);
        
        u8 c = 1;
        while (n < 81) {
            /*  up  */ for (u8 i=0; i<c; i++) { y--; SET(PATTERN_SPIRAL_INNER); BREAK; } BREAK;
            /* left */ for (u8 i=0; i<c; i++) { x--; SET(PATTERN_SPIRAL_INNER); }
            c++;                                      
                                                      
            /*  down */ for (u8 i=0; i<c; i++) { y++; SET(PATTERN_SPIRAL_INNER); }
            /* right */ for (u8 i=0; i<c; i++) { x++; SET(PATTERN_SPIRAL_INNER); }
            c++;
        }

        // OUTER SPIRAL --------
        n = 0, x = 9, y = 0;
        
        c = 9;
        while (n < 81) {
            /* left */ for (u8 i=0; i<c; i++) { x--; SET(PATTERN_SPIRAL_OUTER); BREAK; } BREAK;
            c--;       

            /*  down */ for (u8 i=0; i<c; i++) { y++; SET(PATTERN_SPIRAL_OUTER); }
            /* right */ for (u8 i=0; i<c; i++) { x++; SET(PATTERN_SPIRAL_OUTER); }
            c--;

            /* up */ for (u8 i=0; i<c; i++) { y--; SET(PATTERN_SPIRAL_OUTER); }
        }

        #undef BREAK
        #undef SET
    }
    {
        // ROW SNAKE -----
        u8 n = 0;
        u8 dir = 0;
        for (u8 y = 0; y < 9; y++) {
            for (u8 x = 0; x < 9; x++) {
                if (!dir) patterns[PATTERN_ROW_SNAKE][n][0] = x;
                else      patterns[PATTERN_ROW_SNAKE][n][0] = 8 - x;
                patterns[PATTERN_ROW_SNAKE][n][1] = y;
                n++;
            }
            dir = (dir+1)%2;
        }

        // COL SNAKE -----
        n = 0;
        dir = 1;
        for (u8 x = 0; x < 9; x++) {
            for (u8 y = 0; y < 9; y++) {
                patterns[PATTERN_COL_SNAKE][n][0] = x;
                if (!dir) patterns[PATTERN_COL_SNAKE][n][1] = y;
                else      patterns[PATTERN_COL_SNAKE][n][1] = 8 - y;
                n++;
            }
            dir = (dir+1)%2;
        }
    }


    // timing
    LARGE_INTEGER start_time, end_time, delta_ms, cpu_freq;
    QueryPerformanceFrequency(&cpu_freq);
    delta_ms.QuadPart = 0;

    QueryPerformanceCounter(&start_time);
    srand(start_time.QuadPart);

    i64 total_time_ms = 0;
    f32 total_time    = 0.0f;

    i64 solve_true_ms  = 16;
    i64 solve_wait_ms  = solve_true_ms;
    i64 solve_timer_ms = solve_wait_ms;

    i64 render_wait_ms  = 1000 / monitor_rate;
    i64 render_timer_ms = render_wait_ms;





    // audio
    ThreadArgs audio_args;
    audio_args.mutex = CreateMutex(NULL, FALSE, NULL);

    RingBuffer  local_events_data;
    RingBuffer* local_events = &local_events_data;
    RingBuffer* audio_events = &audio_args.events;

    HANDLE audio_thread = CreateThread( 
            NULL,                                // default security attributes
            0,                                   // use default stack size  
            (LPTHREAD_START_ROUTINE) audio_loop, // thread function name
            &audio_args,                         // argument to thread function - note: may not like u touching another threads stack
            0,                                   // use default creation flags 
            NULL);                               // returns the thread identifier



    // mouse sync
    i8 mouse_target_x;
    i8 mouse_target_y;
    u8 hover_idx = 0xFF;

    f32 x_ratio  = 1.0f, y_ratio  = 1.0f;
    f32 ratio_dx = 0.0f, ratio_dy = 0.0f;


    // audio thread sync
    u8 audio_updated = 0; // set this if an audio event occurredc


    // game
    f32 mouse_x = 0;
    f32 mouse_y = 0;

    f32 total_time_at_click = -1.0;

    i8 cursor_x = 4;
    i8 cursor_y = 4;
    u32 cursor_idx = IDX(cursor_x, cursor_y);
    board_data[cursor_idx] |= BOARD_FLAG_CURSOR;

    bool waiting_for_solve = false;

    u32 ai_logic_idx  = 0;
    u32 ai_cursor_idx = 0xff;
    u8  stage         = 0;

    u16 board_iterations   = 0;
    u16 stagnation_counter = 0xFFFF; 

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

            if (window_width >= window_height) {
                ratio_dx = x_ratio - 1.0f;
                ratio_dy = 0.0f;

                project_orthographic(&proj, -x_ratio, x_ratio, -1.0f, 1.0f, 0.0f, 100.0f);
                scale_mat4(&scale_mouse, {x_ratio, 1.0, 1.0});
            }

            if (window_height > window_width) {
                ratio_dx = 0.0f;
                ratio_dy = y_ratio - 1.0f;

                project_orthographic(&proj, -1.0f, 1.0f, -y_ratio, y_ratio, 0.0f, 100.0f);
                scale_mat4(&scale_mouse, {1.0, y_ratio, 1.0});
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


            // mouse coords [0,dim] -> [0,1]
            f32 screen_x = event.mouse_position[0] / window_width;
            f32 screen_y = event.mouse_position[1] / window_height;

            // mouse coords [0,1] -> [0, 1 + ratio_delta]
            mouse_x = screen_x * (1.0f + ratio_dx);
            mouse_y = screen_y * (1.0f + ratio_dy);

            // mouse coords [0,1] -> [-ratio_delta, 1 + ratio_delta]
            screen_x = screen_x*(1.0f + ratio_dx) - (ratio_dx/2.0f);
            screen_y = screen_y*(1.0f + ratio_dy) - (ratio_dy/2.0f);

            // remap coords to account for board scale
            f32 d = 0.5 * (1.0f - target_scale);
            screen_x = screen_x*(1.0f + d + d) - d; 
            screen_y = screen_y*(1.0f + d + d) - d; 

            // get hover with deadband
            hover_idx = 0xFF;
            if (screen_x > 0.0f - EPS && screen_y > 0.0f - EPS && 
                screen_x < 1.0f + EPS && screen_y < 1.0f + EPS) {
                const f32 gamma = 0.1f * (1.0f/9.0f);

                f32 xn = screen_x * 9.0f;
                i8  xi = i8(xn);
                f32 dx = xn - f32(xi);

                f32 yn = screen_y * 9.0f;
                i8  yi = i8(yn);
                f32 dy = yn - f32(yi);

                if (gamma < dx && dx < 1.0f - gamma) {
                    if (gamma < dy && dy < 1.0f - gamma) {
                        mouse_target_x = xi;
                        mouse_target_y = yi;
                        hover_idx = IDX(mouse_target_x, mouse_target_y);
                    }
                }

            } 

            if (event.type == INPUT_TYPE_MOUSE_PRESS) {
                if (!handled && event.action == GLFW_PRESS && event.mouse_button & 0x2) {
                    // move cursor to hover
                    if (hover_idx < 0xFF) {
                        board_data[cursor_idx] &= ~u16(BOARD_FLAG_CURSOR);

                        cursor_x = mouse_target_x;
                        cursor_y = mouse_target_y;
                        cursor_idx = IDX(cursor_x, cursor_y);

                        board_data[cursor_idx] |= BOARD_FLAG_CURSOR;
                        board_input      = 1;
                        board_input_type = LIST_SKIP;
                    }

                    total_time_at_click = total_time;
                    handled = 1;
                }
            }


            if (event.type == INPUT_TYPE_KEY_PRESS) {
                // quit or clear
                if (!handled && KEY_DOWN(GLFW_KEY_ESCAPE)) {
                    // quit
                    if (event.mod & GLFW_MOD_SHIFT) return;

                    // clear digits
                    board_data[cursor_idx] = BOARD_EMPTY | BOARD_FLAG_CURSOR;
                    board_input = 1;
                    handled = 1;
                }

                // check - solve
                if (!handled && KEY_UP(GLFW_KEY_ENTER)) {
                    // check if there's statics
                    if (!waiting_for_solve) {
                        for (u8 j = 0; j < 9; j++) {
                            for (u8 i = 0; i < 9; i++) {
                                if (board_data[IDX(i,j)] & BOARD_FLAG_STATIC) {
                                    waiting_for_solve = true;
                                    break;
                                }
                            }
                            if (waiting_for_solve) {
                                solve_wait_ms = solve_true_ms;

                                ai_logic_idx     = 0;
                                ai_cursor_idx    = 0xff;
                                board_iterations = 0;
                                board_input      = 1;

                                waiting_for_solve = set_pencils(board_data, !(event.mod & GLFW_MOD_SHIFT));
                                break;
                            }
                        }
                    }
                    handled = 1;
                } 
                else if (!handled && waiting_for_solve && !event.mod && event.key < GLFW_KEY_KP_9) {
                    // didn't press enter, then if were currently solving we should stop
                    waiting_for_solve = false;
                    ai_cursor_idx = 0xff;
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
                    board_data[cursor_idx] |= BOARD_FLAG_CURSOR;
                    handled     = 1;
                    board_input = 1;
                }


                // retry (remove all non statics)
                if (!handled && (event.mod & GLFW_MOD_CONTROL) && KEY_DOWN(GLFW_KEY_R)) {
                    handled     = 1;
                    board_input = 1;
                    for (u32 j = 0; j < 9; j++){
                        for (u32 i = 0; i < 9; i++){
                            if (!(board_data[IDX(i,j)] & BOARD_FLAG_STATIC)) {
                                board_data[IDX(i,j)] &= ~u16(BOARD_ALL | BOARD_FLAG_PENCIL);
                            }
                        }
                    }
                }


                // keyboard cursor
                if (!handled && IS_KEY_DOWN) {
                    board_data[cursor_idx] &= ~u16(BOARD_FLAG_CURSOR);

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
                            if (event.mod & GLFW_MOD_SHIFT) {board_data[idx] &= ~u16(BOARD_FLAG_PENCIL);}                              
                            // clear digits
                            if (!(board_data[idx] & BOARD_FLAG_PENCIL)) board_data[idx] &= (~u16(BOARD_ALL) | (board_data[idx] & target)); 
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
                if (!handled && KEY_DOWN(GLFW_KEY_TAB) && (board_data[cursor_idx] & BOARD_ALL)) {
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

#if 0
                // FIXME - audio debug events
                if (!handled && KEY_UP(GLFW_KEY_X)) {
                    // To test the sound angle of stereo data
                    Event e;
                    e.mode     = EventMode::start;
                    e.sound_id = SOUND_VOICE;
                    e.layer    = 0;
                    e.volume   = 1.0f;
                    e.angle    = 1.0f;

                    ring_push(local_events, e);
                    audio_updated = 1;
                }                                                           

                if (!handled && KEY_UP(GLFW_KEY_C)) {
                    // For testing sound quality
                    Event e;
                    e.mode     = EventMode::start;
                    e.sound_id = SOUND_SWEEP;
                    e.layer    = 0;
                    e.volume   = 1.0f;
                    e.angle    = 0.5f;

                    ring_push(local_events, e);
                    audio_updated = 1;
                }                                                           

                if (!handled && KEY_UP(GLFW_KEY_V)) {                       
                    // For testing normalization and mono-angle
                    Event e;
                    e.mode     = EventMode::start;
                    e.layer    = 0;

                    e.sound_id = SOUND_SIN_LOW;
                    e.volume   = 1.0f;
                    e.angle    = 0.1f;
                    ring_push(local_events, e);

                    e.sound_id = SOUND_SIN_HIGH;
                    e.volume   = 1.0f;
                    e.angle    = 0.9f;
                    ring_push(local_events, e);

                    audio_updated = 1;
                }                                                           

                const f32 delta = 0.05f;
                static i32 loop_id = 0;
                static i16 angle_counter = 0;
                if (!handled && KEY_UP(GLFW_KEY_B)) {
                    Event e;
                    e.layer    = 0;
                    e.sound_id = SOUND_SIN_LOW;
                    e.volume   = 1.0f;
                    e.angle    = 0.5f;
                    if (event.mod & GLFW_MOD_SHIFT) {
                        e.mode         = EventMode::interpolate;
                        e.target_id    = loop_id;
                        e.target_mode  = EventMode::loop;
                        e.angle        = 3.0f;
                        e.volume       = 0.0f;
                        e.interp_time  = 8.0f;

                        ring_push(local_events, e);

                    } else if (event.mod & GLFW_MOD_CONTROL) {
                        angle_counter++;

                        // For testing angle sweeping
                        e.mode         = EventMode::update;
                        e.target_id    = loop_id;
                        e.target_mode  = EventMode::loop;
                        e.angle       += angle_counter * delta;
                        ring_push(local_events, e);
                
                        printf("[Main] Angle: %.4f\n", angle_counter*0.1f);
                    } else if (event.mod & GLFW_MOD_ALT) {
                        angle_counter--;

                        // For testing angle sweeping
                        e.mode         = EventMode::update;
                        e.target_id    = loop_id;
                        e.target_mode  = EventMode::loop;
                        e.angle       += angle_counter * delta;
                        ring_push(local_events, e);

                        printf("[Main] Angle: %.4f\n", angle_counter*0.1f);
                    } else if (event.mod & GLFW_MOD_SHIFT) {
                        // For testing ID-stopping
                        e.mode        = EventMode::update;
                        e.target_mode = EventMode::stop;
                        e.target_id   = loop_id;
                        ring_push(local_events, e);
                    } else {
                        // For testing looping
                        e.mode  = EventMode::loop;
                        loop_id = ring_push(local_events, e);
                    }
                    audio_updated = 1;
                }
                                                                            
                if (!handled && KEY_UP(GLFW_KEY_M)) {                       
                    Event e1;
                    e1.mode     = EventMode::start;
                    e1.sound_id = SOUND_SIN_LOW;
                    e1.layer    = 0;
                    e1.volume   = 1.0f;
                    e1.angle    = 0.5f;

                    Event e2;
                    e2.mode     = EventMode::start;
                    e2.sound_id = SOUND_SIN_HIGH;
                    e2.layer    = 0;
                    e2.volume   = 1.0f;
                    e2.angle    = 0.5f;

                    // For testing the buffer
                    for (u32 i = 0; i < 2*N_EVENTS; i++) {
                        ring_push(local_events, e1);
                        ring_push(local_events, e2);
                    }
                    audio_updated = 1;                                      
                }
#endif
                                                                            
                // recompile shaders
                if (!handled && KEY_UP(GLFW_KEY_F5)) {
                    handled = 1;

                    GLuint old_shader_program_main = shader_program_main;
                    shader_program_main = build_shader_program(PATH_SHADER_MAIN_VERT, PATH_SHADER_MAIN_FRAG, &info_log);
                    if (shader_program_main == NULL) {
                        printf("[Error] Shader compilation failed.\n%s\n", info_log);
                        shader_program_main = old_shader_program_main;
                        free(info_log);
                    }
                    else {
                        printf("[Success] Shaders recompiled ... \n");

                        u_proj  = glGetUniformLocation(shader_program_main, "proj");
                        u_model = glGetUniformLocation(shader_program_main, "model");

                        u_font  = glGetUniformLocation(shader_program_main, "font");
                        u_board = glGetUniformLocation(shader_program_main, "board");

                        glDeleteProgram(old_shader_program_main);
                        glUseProgram(shader_program_main);
                    }
                }

            } // end of key press


            // alter board history
            if (!board_input) {
                // if there was an undo, handle the history cleanup in there
                if (!board_undo) {
                    ListItem* tmp = history_ptr;
                    history_ptr   = history_ptr->prev;
                    board_data    = history_ptr->board_data;
                    list_free(tmp);
                }
            } else {
                history_ptr->type     = board_input_type;
                history_ptr->cursor_x = cursor_x;
                history_ptr->cursor_y = cursor_y;
                validate_board(board_data);
            }

        } // end of events
        input_index = 0;

        // ------- End of Event Handling --------


        // make solution progress
        if (waiting_for_solve) {
            if (solve_timer_ms >= solve_wait_ms) {
                solve_timer_ms -= solve_wait_ms;

                u8 status = PROGRESS_INV_CELL; 
                while(status == PROGRESS_INV_CELL) {
                    u8  base_x    = patterns[pattern_idx][ai_logic_idx][0];
                    u8  base_y    = patterns[pattern_idx][ai_logic_idx][1];
                    ai_cursor_idx = IDX(base_x, base_y);
                    status = make_progress(board_data, base_x, base_y, stage);

                    // keep track of stagnation
                    if (stagnation_counter == 0xFFFF && status == PROGRESS_DEFAULT) {
                        stagnation_counter = board_iterations;
                    }

                    // set AI cursor
                    if (status == PROGRESS_STATE_CHANGE || status == PROGRESS_SET_CELL) {
                        stagnation_counter = 0xFFFF;
                    } 

                    // p1: no state change => time to give up
                    // p3: board solved => time to stop
                    bool p1 = (stagnation_counter != 0xFFFF && board_iterations - stagnation_counter > 2*N_PATTERNS);
                    bool p2 = (status == PROGRESS_STATE_CHANGE || status == PROGRESS_SET_CELL);
                    u8 p3 = 0;
                    if (!p1 && p2) p3 = validate_board(board_data);
                    if (p1 || p3) {
                        solve_wait_ms     = solve_true_ms;
                        waiting_for_solve = false;
                        board_iterations  = 0xFFFF;

                        pattern_idx   = 0;
                        ai_logic_idx  = 0;
                        ai_cursor_idx = 0xff;

                        break;
                    }

                    // speed up over time
                    if (status == PROGRESS_SET_CELL) {
                        if (solve_wait_ms <= 1) {
                            solve_wait_ms = 1;
                        } else {
                            solve_wait_ms *= 0.92f;
                        }
                    }

                    // nothing set, retry again next iteration
                    if (status == PROGRESS_DEFAULT) {
                        solve_timer_ms = solve_wait_ms;
                    }

                    if (status == PROGRESS_INV_CELL) { stage = 0; ai_logic_idx++; }
                    else {
                        stage++;
                        if (status == PROGRESS_SET_CELL) stage = 3; // force increment
                        if (stage  > 2) { stage  = 0; ai_logic_idx++; }
                    }
                    if (ai_logic_idx > 81) { 
                        u8 tmp = pattern_idx;
                        pattern_idx = rand() % N_PATTERNS; 
                        if (pattern_idx == tmp) pattern_idx = (pattern_idx+1) % N_PATTERNS;

                        ai_logic_idx = 0; 
                        board_iterations++; 
                    }
                }
            }
        }

        // update audio
        if (audio_updated) {
            if (audio_args.init) {
                u32 sig = WaitForSingleObject(audio_args.mutex,0);
                if (sig == WAIT_OBJECT_0) {
                    printf("[Main] Copying Event Ring\n");
                    audio_updated = 0;

                    memcpy(audio_events, local_events, sizeof(RingBuffer));
                    audio_args.new_event = 1;
                    ReleaseMutex(audio_args.mutex);

                    ring_clear(local_events);
                }
            } else {
                printf("[Main] Audio thread not initialized ... \n");
                audio_updated = 0;
                ring_clear(local_events);
            }
        }

        // add volatile cell states, removed after render
        if (hover_idx < 0xFF) {
            board_data[hover_idx] |= BOARD_FLAG_HOVER;
        }

        if (ai_cursor_idx < 0xFF) {
            board_data[ai_cursor_idx] |= BOARD_FLAG_AI;
        }

        // render
        if (render_timer_ms >= render_wait_ms) {
            render_timer_ms -= render_wait_ms;

            glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // main program 
            glUseProgram(shader_program_main);

            glUniformMatrix4fv(u_proj,  1, GL_FALSE, &proj.a[0]);
            glUniformMatrix4fv(u_model, 1, GL_FALSE, &scale.a[0]);

            glUniform1i(u_font, 0);
            glUniform1i(u_board, 1);

            glActiveTexture(GL_TEXTURE0 + 0);
            glBindTexture(GL_TEXTURE_2D, tex_font);

            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, tex_board);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BOARD_DIM, BOARD_DIM, GL_RED_INTEGER, GL_UNSIGNED_SHORT, board_data);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            
            // mouse program
            glUseProgram(shader_program_mouse);

            glUniformMatrix4fv(u_proj_mouse,  1, GL_FALSE, &proj.a[0]);
            glUniformMatrix4fv(u_model_mouse, 1, GL_FALSE, &scale_mouse.a[0]);

            glUniform2f(u_time_mouse, total_time, total_time_at_click);
            glUniform2f(u_mouse_mouse, mouse_x, mouse_y);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        glfwSwapBuffers(window);


        // remove volatile cell states
        if (hover_idx < 0xFF) {
            board_data[hover_idx] &= ~u16(BOARD_FLAG_HOVER);
        }

        if (ai_cursor_idx < 0xFF) {
            board_data[ai_cursor_idx] &= ~u16(BOARD_FLAG_AI);
        }
        


        // timing
        QueryPerformanceCounter(&end_time);
        delta_ms.QuadPart = end_time.QuadPart - start_time.QuadPart;
        delta_ms.QuadPart *= 1000;
        delta_ms.QuadPart /= cpu_freq.QuadPart;

        total_time_ms   += delta_ms.QuadPart;
        render_timer_ms += delta_ms.QuadPart;
        solve_timer_ms  += delta_ms.QuadPart;

        total_time = f64(total_time_ms) / 1000.0;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
