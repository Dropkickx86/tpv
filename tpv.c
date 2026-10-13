#include <argp.h>
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#define STBI_NO_GIF
#include "stb_image.h"

struct arguments {
    int fast;
    int no_scaling;
    int no_vert_scaling;
    int color_depth;
    int grayscale;
    int arg_inline;
    int progress;
    char *filepath;
};

typedef struct {
    int w, h, c;
    unsigned char *data;
} ImageInfo;

unsigned char apply_image_alpha(unsigned char color, unsigned char bg, unsigned char a);
void apply_image_scale(ImageInfo *img, int scalefactor, int progress);
unsigned char convert_88(unsigned char *color);
unsigned char convert_256(unsigned char *color);
unsigned char convert_bw(unsigned char *color);
unsigned char convert_color_88(unsigned char *color);
unsigned char convert_color_256(unsigned char *color);
unsigned char convert_grayscale(unsigned char *color);
unsigned char convert_grayscale_88(unsigned char *color);
unsigned char convert_grayscale_256(unsigned char *color);
unsigned char convert_systemcolor(unsigned char *color, int extended);
unsigned char get_ansi_level_88(unsigned char value);
unsigned char get_ansi_level_256(unsigned char);
float get_euclidean_distance_3D(unsigned char *p1, unsigned char *p2);
void get_term_bg(unsigned char *bg_col);
int get_term_colordepth();
void get_term_colorpalette();
struct winsize get_term_size();
void next_entry(char *buffer);
error_t parse_opt(int key, char *arg, struct argp_state *state);
void print_clear();
void print_picture(ImageInfo *img, const struct winsize *term, int fast, int colordepth);
void print_pixel_8bit(unsigned char top, unsigned char bot);
void print_pixel_24bit(unsigned char *top, unsigned char *bot);
void print_pixel_bw(unsigned char top, unsigned char bot);
void print_pixel_systemcolor(unsigned char top, unsigned char bot);
void set_term_mode_raw(struct termios *term_current);

int errno_safe = 0;
char doc[] = "CLI tool to print images in terminal";
char args_doc[] = "filepath";
struct argp_option options[] = {
    {"fast", 'f', 0, 0, "Prints image by skipping pixels instead of scaling. Fast but inaccurate", 0},
    {"no-scaling", 'n', 0, 0, "Prints image without scaling it down to fit the terminal window", 0},
    {"no-vert-scaling", 'v', 0, 0, "Prints image after scaling it down to fit on width, ignoring image height", 0},
    {"color-depth", 'c', "depth", 0, "Choose color depth. Defaults to highest capability of terminal", 0},
    {"grayscale", 'g', 0, 0, "Print in grayscale", 0},
    {"inline", 'i', 0, 0, "Prints without clearing the screen", 0},
    {"progress", 'p', 0, 0, "Prints the progress when scaling images", 0},
    {"help", 'h', 0, 0, "Shows this help list", 0},
    {0},
};
struct argp argp = {options, parse_opt, args_doc, doc, 0, 0, 0};
unsigned char palette[16][3] = {
    {0, 0, 0},
    {170, 0, 0},
    {0, 170, 0},
    {170, 85, 0},
    {0, 0, 170},
    {170, 0, 170},
    {0, 170, 170},
    {170, 170, 170},
    {85, 85, 85},
    {255, 85, 85},
    {85, 255, 85},
    {255, 255, 85},
    {85, 85, 255},
    {255, 85, 255},
    {85, 255, 255},
    {255, 255, 255}
};
unsigned char palette_bw[2] = {0, 7};

int main(int argc, char **argv){
    //Argument parsing, can terminate execution
    struct arguments arguments = {0};
    argp_parse(&argp, argc, argv, ARGP_NO_HELP, 0, &arguments);

    //Variable declarations, definitions, checks
    const struct winsize term = get_term_size();
    if (term.ws_row == 0){
        printf("tpv: failed getting terminal size");
        return errno_safe;
    }
    unsigned char termbg[3] = {0};
    get_term_bg(termbg);
    int colordepth = get_term_colordepth();
    if (colordepth == 1 && errno_safe != 0){
        return errno_safe;
    }
    else if (arguments.color_depth == 0){
        arguments.color_depth = colordepth;
    }
    else if (arguments.color_depth > colordepth){
        printf("tpv: c %d: requested color depth is higher than maximum supported by terminal\n", arguments.color_depth);
        return EINVAL;
    }
    if (arguments.color_depth <= 4){
        get_term_colorpalette();
    }
    ImageInfo img;
    img.data = stbi_load(arguments.filepath, &img.w, &img.h, &img.c, 0);
    if (!img.data){
        printf("%s", stbi_failure_reason());
        return EXIT_FAILURE;
    }

    //scaling
    if (!arguments.no_scaling && !arguments.fast){
        int scalefactor;
        if (!arguments.no_vert_scaling && (img.h / (term.ws_row * 2)) > (img.w / term.ws_col)){
            scalefactor = (img.h / (term.ws_row * 2)) + 1;
        }
        else {
            scalefactor = (img.w / term.ws_col) + 1;
        }
        if (scalefactor > 1){
            apply_image_scale(&img, scalefactor, arguments.progress);
        }
    }

    //common handling
    if (arguments.grayscale && img.c > 2){
        for (int pixel = 0; pixel < (img.h * img.w * img.c); pixel += img.c){
            unsigned char value;
            if (arguments.color_depth <= 4){
                value = convert_bw(&img.data[pixel]);
                if (value){
                    value = palette[palette_bw[1]][0];
                }
                else {
                    value = palette[palette_bw[0]][0];
                }
            }
            else {
                value = convert_grayscale(&img.data[pixel]);
            }
            img.data[pixel] = value;
            img.data[pixel + 1] = value;
            img.data[pixel + 2] = value;
        }
    }
    if (img.c == 2 || img.c == 4){
        for (int pixel = 0; pixel < (img.h * img.w * img.c); pixel += img.c){
            for (int offset = 0; offset < img.c - 1; offset++){
                img.data[pixel + offset] = apply_image_alpha(img.data[pixel + offset], termbg[offset], img.data[pixel + img.c - 1]);
            }
        }
    }

    //render
    if (!arguments.arg_inline){
        print_clear();
    }
    if (img.h % 2 != 0){
        unsigned char *temp = realloc(img.data, sizeof(unsigned char) * ((img.w * img.h * img.c) + (img.w * img.c)));
        if (temp){
            img.data = temp;
            for (int pixel = (img.w * img.h * img.c); pixel < ((img.w * img.h * img.c) + (img.w * img.c)); pixel += img.c){
                for (int ch = 0; ch < img.c; ch++){
                    if (ch == img.c){
                        img.data[pixel + ch] = 255;
                    }
                    else {
                        img.data[pixel + ch] = termbg[ch];
                    }
                }
            }
        }
    }
    print_picture(&img, &term, arguments.fast, arguments.color_depth);

    //cleanup
    free(img.data);
    return 0;
}

unsigned char apply_image_alpha(unsigned char color, unsigned char bg, unsigned char a){
    if (a != 255){
        float alpha = (float)a / 255;
        color = roundf(((float)color * alpha) + ((float)bg * (1.0 - alpha)));
    }
    return color;
}

void apply_image_scale(ImageInfo *img, int scalefactor, int progress){
    int finalIndex, cursor = 5;
    if (progress){
        float h = ceil((float)img->h / (float)scalefactor);
        float w = ceil((float)img->w / (float)scalefactor);
        finalIndex = (int) h * (int) w * img->c;
        printf("[--------------------]");
        printf("\033[2G");
    }
    int index = 0;
    for (int row = 0; row < img->h; row += scalefactor){
        for (int col = 0; col < img->w; col += scalefactor){
            for (int ch = 0; ch < img->c; ch++){
                int val = 0;
                int amount = 0;
                int rowoffset = 0;
                while (rowoffset < scalefactor && row + rowoffset < img->h){
                    int coloffset = 0;
                    while (coloffset < scalefactor && col + coloffset < img->w){
                        val += img->data[((row + rowoffset) * img->w * img->c) + ((col + coloffset) * img->c) + ch];
                        amount++;
                        coloffset++;
                    }
                    rowoffset++;
                }
                img->data[index++] = val / amount;
                if (progress && index % 500 == 0){
                    float perc = ((float)index / (float)finalIndex) * 100;
                    if (perc >= cursor){
                        printf("#");
                        cursor += 5;
                    }
                }
            }
        }
    }
    float h = ceil((float)img->h / (float)scalefactor);
    float w = ceil((float)img->w / (float)scalefactor);
    img->h = (int) h;
    img->w = (int) w;
    if (progress){
        printf("\n");
    }
}

unsigned char convert_88(unsigned char *color){
    if (abs(color[0] - color[1]) < 10 && abs(color[1] - color[2]) < 10 && abs(color[0] - color[2]) < 10){
        unsigned char gray_index = convert_grayscale_88(color);
        unsigned char color_index = convert_color_88(color);
        unsigned char val_color[3], val_gray[3];
        if (gray_index > 80){
            val_gray[0] = roundf(((float)gray_index - 81) * 23.2) + 92;
            val_gray[1] = val_gray[0];
            val_gray[2] = val_gray[0];
        }
        else {
            val_gray[0] = 46;
            val_gray[1] = 46;
            val_gray[2] = 46;
        }
        if (color_index == 16){
            val_color[0] = 0;
            val_color[1] = 0;
            val_color[2] = 0;
        }
        else {
            val_color[0] = 55 + (40 * ((color_index - 16) / 36));
            val_color[1] = 55 + (40 * (((color_index - 16) / 6) % 6));
            val_color[2] = 55 + (40 * ((color_index - 16) % 6));
        }
        float dist_gray = get_euclidean_distance_3D(color, val_gray);
        float dist_color = get_euclidean_distance_3D(color, val_color);
        if (dist_gray < dist_color){
            return gray_index;
        }
        else {
            return color_index;
        }
    }
    else {
        return convert_color_88(color);
    }
}

unsigned char convert_256(unsigned char *color){
    if (abs(color[0] - color[1]) < 10 && abs(color[1] - color[2]) < 10 && abs(color[0] - color[2]) < 10){
        unsigned char gray_index = convert_grayscale_256(color);
        unsigned char color_index = convert_color_256(color);
        unsigned char val_color[3], val_gray[3] = {8 + 10 * (gray_index - 232), 8 + 10 * (gray_index - 232), 8 + 10 * (gray_index - 232)};
        if (color_index == 16){
            val_color[0] = 0;
            val_color[1] = 0;
            val_color[2] = 0;
        }
        else {
            val_color[0] = 55 + (40 * ((color_index - 16) / 36));
            val_color[1] = 55 + (40 * (((color_index - 16) / 6) % 6));
            val_color[2] = 55 + (40 * ((color_index - 16) % 6));
        }
        float dist_gray = get_euclidean_distance_3D(color, val_gray);
        float dist_color = get_euclidean_distance_3D(color, val_color);
        if (dist_gray < dist_color){
            return gray_index;
        }
        else {
            return color_index;
        }
    }
    else {
        return convert_color_256(color);
    }
}

unsigned char convert_bw(unsigned char *color){
    unsigned char value = convert_grayscale(color);
    if (value > 127){
        return 1;
    }
    else {
        return 0;
    }
}

unsigned char convert_color_88(unsigned char *color){
    return (unsigned char) 16 + (16 * get_ansi_level_88(color[0])) + (4 * get_ansi_level_88(color[1])) + get_ansi_level_88(color[2]);
}

unsigned char convert_color_256(unsigned char *color){
    return (unsigned char) 16 + (36 * get_ansi_level_256(color[0])) + (6 * get_ansi_level_256(color[1])) + get_ansi_level_256(color[2]);
}

unsigned char convert_grayscale(unsigned char *color){
    return ((float)color[0] * 0.2989) + ((float)color[1] * 0.5870) + ((float)color[2] * 0.1140);
}

unsigned char convert_grayscale_88(unsigned char *color){
    float value = (float) convert_grayscale(color);
    if (value < 29){
        return 16;
    }
    else if (value < 70){
        return 80;
    }
    else if (value > 243){
        return 79;
    }
    else {
        return roundf((value - 92) / 23.2) + 81;
    }
}

unsigned char convert_grayscale_256(unsigned char *color){
    float value = (float) convert_grayscale(color);
    if (value < 5){
        return 16;
    }
    else if (value > 242){
        return 231;
    }
    else {
        return roundf((value - 8) / 10) + 232;
    }
}

unsigned char convert_systemcolor(unsigned char *color, int extended){
    #ifndef NO_SATURATION_INCREASE
    if (abs(color[0] - color[1]) > 40 || abs(color[0] - color[2]) > 40 || abs(color[1] - color[2]) > 40){
        unsigned char gray_value = convert_grayscale(color);
        int r = gray_value + ((color[0] - gray_value) * 10);
        int g = gray_value + ((color[1] - gray_value) * 10);
        int b = gray_value + ((color[2] - gray_value) * 10);
        if (r > 255){
            color[0] = 255;
        }
        else if (r < 0){
            color[0] = 0;
        }
        else {
            color[0] = r;
        }
        if (g > 255){
            color[1] = 255;
        }
        else if (g < 0){
            color[1] = 0;
        }
        else {
            color[1] = g;
        }
        if (b > 255){
            color[2] = 255;
        }
        else if (b < 0){
            color[2] = 0;
        }
        else {
            color[2] = b;
        }
    }
    #endif
    
    float closestvalue = 442.0;
    int closestindex = 0;
    for (int i = 0; i < 8 + (8 * extended); i++){
        float value = get_euclidean_distance_3D(color, palette[i]);
        if (value == 0){
            return (unsigned char) i;
        }
        else if (value < closestvalue){
            closestvalue = value;
            closestindex = i;
        }
    }
    return (unsigned char) closestindex;
}

unsigned char get_ansi_level_88(unsigned char value){
    const unsigned char levels[4] = {0, 139, 205, 255};
    unsigned char level = 0;
    unsigned char nearest = 255;
    for (unsigned char i = 0; i < 4; i++){
        int diff = abs(value - levels[i]);
        if (diff == 0){
            return i;
        }
        else if (diff < nearest){
            level = i;
            nearest = diff;
        }
    }
    return level;
}

unsigned char get_ansi_level_256(unsigned char value){
    const unsigned char levels[6] = {0, 95, 135, 175, 215, 255};
    unsigned char level = 0;
    unsigned char nearest = 255;
    for (unsigned char i = 0; i < 6; i++){
        int diff = abs(value - levels[i]);
        if (diff == 0){
            return i;
        }
        else if (diff < nearest){
            level = i;
            nearest = diff;
        }
    }
    return level;
}

float get_euclidean_distance_3D(unsigned char *p1, unsigned char *p2){
    int dist_r = (p1[0] - p2[0]) * (p1[0] - p2[0]);
    int dist_g = (p1[1] - p2[1]) * (p1[1] - p2[1]);
    int dist_b = (p1[2] - p2[2]) * (p1[2] - p2[2]);
    int dist_tot = dist_r + dist_g + dist_b;
    return sqrtf((float) dist_tot);
}

void get_term_bg(unsigned char *bg_col){
    struct termios term_old;
    char buffer[64] = {0};
    int inamount = 0;
    set_term_mode_raw(&term_old);
    if (errno_safe != 0){
        errno_safe = 0;
        return;
    }
    printf("\033]11;?\007");
    printf("\033[c");
    fflush(stdout);
    do {
        inamount += read(STDIN_FILENO, buffer + inamount, sizeof(buffer) - 1);
    } while (buffer[inamount - 1] != 'c');
    fflush(stdin);
    tcsetattr(STDIN_FILENO, TCSANOW, &term_old);
    if (buffer[1] == ']'){
        unsigned int r, g, b;
        sscanf(buffer, "\033]11;rgb:%x/%x/%x", &r, &g, &b);
        bg_col[0] = (unsigned char)(r >> 8);
        bg_col[1] = (unsigned char)(g >> 8);
        bg_col[2] = (unsigned char)(b >> 8);
    }
}

int get_term_colordepth(){
    struct termios term_old;
    char buffer[256] = {0};
    const char *env_colorterm = getenv("COLORTERM");
    const char *env_term = getenv("TERM");
    int inamount = 0;
    set_term_mode_raw(&term_old);
    if (errno_safe != 0){
        return 1;
    }
    printf("\033[38;2;1;2;3m");
    printf("\033P$qm\033\\");
    printf("\033P+q524742\033\\");
    printf("\033]4;255;?\033\\");
    printf("\033]4;87;?\033\\");
    printf("\033]4;15;?\033\\");
    printf("\033]4;7;?\033\\");
    printf("\033[c");
    printf("\033[0m");
    fflush(stdout);
    do {
        inamount += read(STDIN_FILENO, buffer + inamount, sizeof(buffer) - 1);
    } while (buffer[inamount - 1] != 'c');
    fflush(stdin);
    tcsetattr(STDIN_FILENO, TCSANOW, &term_old);
    while (buffer[1] == 'P' && buffer[0] != '\0'){ 
        if (buffer[2] == '1' && buffer[5] != '0'){
            return 24;
        }
        else {
            next_entry(buffer);
        }
    }
    if ((env_colorterm && (strcmp(env_colorterm, "truecolor") == 0 || strcmp(env_colorterm, "24bit") == 0)) || (env_term && (strcmp(env_term, "st-256color") == 0))){
        return 24;
    }
    if (buffer[1] == ']'){
        if (buffer[4] == '2'){
            return 8;
        }
        else if (buffer[4] == '8'){
            return 7;
        }
        else if (buffer[4] == '1'){
            return 4;
        }
        else if (buffer[4] == '7'){
            return 3;
        }
    }
    if (env_term && strcmp(env_term, "linux") == 0){
        return 3;
    }
    return 1;
}

void get_term_colorpalette(){
    struct termios term_old;
    char buffer[430] = {0};
    int inamount = 0;
    set_term_mode_raw(&term_old);
    if (errno_safe != 0){
        errno_safe = 0;
        return;
    }
    printf("\033]4;0;?\033\\");
    printf("\033]4;1;?\033\\");
    printf("\033]4;2;?\033\\");
    printf("\033]4;3;?\033\\");
    printf("\033]4;4;?\033\\");
    printf("\033]4;5;?\033\\");
    printf("\033]4;6;?\033\\");
    printf("\033]4;7;?\033\\");
    printf("\033]4;8;?\033\\");
    printf("\033]4;9;?\033\\");
    printf("\033]4;10;?\033\\");
    printf("\033]4;11;?\033\\");
    printf("\033]4;12;?\033\\");
    printf("\033]4;13;?\033\\");
    printf("\033]4;14;?\033\\");
    printf("\033]4;15;?\033\\");
    printf("\033[c");
    printf("\033[0m");
    fflush(stdout);
    do {
        inamount += read(STDIN_FILENO, buffer + inamount, sizeof(buffer) - 1);
    } while (buffer[inamount - 1] != 'c');
    fflush(stdin);
    tcsetattr(STDIN_FILENO, TCSANOW, &term_old);
    #ifdef TRUE_BW
    int bestblack = 765, bestwhite = 0;
    #endif
    while (buffer[1] == ']' && buffer[0] != '\0'){
        int i, offset = 10;
        if (buffer[5] == ';'){
            i = (int)buffer[4] - 48;
        }
        else {
            i = (int)buffer[5] -38;
            offset++;
        }
        char temp[3];
        strncpy(temp, buffer + offset, 2);
        palette[i][0] = (unsigned char)strtol(temp, NULL, 16);
        offset += 5;
        strncpy(temp, buffer + offset, 2);
        palette[i][1] = (unsigned char)strtol(temp, NULL, 16);
        offset += 5;
        strncpy(temp, buffer + offset, 2);
        palette[i][2] = (unsigned char)strtol(temp, NULL, 16);
        next_entry(buffer);
        #ifdef TRUE_BW
        int lightlevel = palette[i] + palette[i + 1], palette[i + 2];
        if (lightlevel < bestblack){
            bestblack = lightlevel;
            palette_bw[0] = i;
        }
        if (lightlevel > bestwhite){
            bestwhite = lightlevel;
            palette_bw[1] = i;
        }
        #endif
    }
}

struct winsize get_term_size(){
    struct winsize term;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &term) == -1){
        errno_safe = errno;
        term.ws_row = 1;
    }
    term.ws_row -= 1;
    return term;
}

void next_entry(char *buffer){
    int index = 0, nextindex = 0;
    for (int i = 0; buffer[i] != '\0'; i++){
        if (buffer[i] == '\\' && buffer[i + 1] == '\033'){
            nextindex = i;
            break;
        }
        else if (buffer[i] == '\007' && buffer[i + 1] == '\033'){
            nextindex = i;
            break;
        }
    }
    if (index == nextindex){
        buffer[0] = '\0';
        return;
    }
    do {
        buffer[index++] = buffer[++nextindex];
    } while(buffer[nextindex] != '\0');
}

error_t parse_opt(int key, char *arg, struct argp_state *state){
    struct arguments *arguments = state->input;
    switch (key){
    case 'f':
        arguments->fast = 1;
        break;
    case 'n':
        arguments->no_scaling = 1;
        break;
    case 'v':
        arguments->no_vert_scaling = 1;
        break;
    case 'c':
        if (arg[0] > 57 || arg[0] < 48){
            printf("tpv: c: no color depth defined\n\n");
            argp_usage(state);
        }
        int depth = atoi(arg);
        switch (depth){
        case 24:
        case 8:
        case 7:
        case 4:
        case 3:
        case 1:
            arguments->color_depth = depth;
            break;
        default:
            printf("tpv: c %d: invalid color depth\n\n", depth);
            argp_usage(state);
        }
        break;
    case 'g':
        arguments->grayscale = 1;
        break;
    case 'i':
        arguments->arg_inline = 1;
        break;
    case 'p':
        arguments->progress = 1;
        break;
    case 'h':
        argp_state_help(state, stdout, ARGP_HELP_STD_HELP);
        break;
    case ARGP_KEY_ARG:
        arguments->filepath = arg;
        break;
    case ARGP_KEY_END:
        if (!arguments->filepath){
            argp_usage(state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

void print_clear(){
    printf("\033[2J");
    printf("\033[H");
}

void print_picture(ImageInfo *img, const struct winsize *term, int fast, int colordepth){
    int offset = 1;
    if (fast){
        if (img->w / term->ws_col > img->h / (term->ws_row * 2)){
            offset += img->w / term->ws_col;
        }
        else {
            offset += img->h / (term->ws_row * 2);
        }
    }
    
    for (int row = 0; row < img->h; row += (offset * 2)){
        int col_out = 0;
        for (int col = 0; col < img->w; col += offset){
            if (col_out >= term->ws_col){
                break;
            }
            unsigned char color_top[3], color_bot[3];
            if (img->c < 3){
                color_top[0] = img->data[(row * img->w * img->c) + (col * img->c)];
                color_top[1] = img->data[(row * img->w * img->c) + (col * img->c)];
                color_top[2] = img->data[(row * img->w * img->c) + (col * img->c)];
                color_bot[0] = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                color_bot[1] = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                color_bot[2] = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
            }
            else {
                color_top[0] = img->data[(row * img->w * img->c) + (col * img->c)];
                color_top[1] = img->data[(row * img->w * img->c) + (col * img->c) + 1];
                color_top[2] = img->data[(row * img->w * img->c) + (col * img->c) + 2];
                color_bot[0] = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                color_bot[1] = img->data[((row + offset) * img->w * img->c) + (col * img->c) + 1];
                color_bot[2] = img->data[((row + offset) * img->w * img->c) + (col * img->c) + 2];
            }
            if (colordepth == 24){
                print_pixel_24bit(color_top, color_bot);
            }
            else if (colordepth == 8){
                print_pixel_8bit(convert_256(color_top), convert_256(color_bot));
            }
            else if (colordepth == 7){
                print_pixel_8bit(convert_88(color_top), convert_88(color_bot));
            }
            else if (colordepth <= 4){
                int extended = 0;
                if (colordepth == 4){
                    extended = 1;
                }
                if (img->c < 3 || colordepth == 1){
                    print_pixel_bw(convert_bw(color_top), convert_bw(color_bot));
                }
                else {
                    print_pixel_systemcolor(convert_systemcolor(color_top, extended), convert_systemcolor(color_bot, extended));
                }
            }
            col_out++;
        }
        printf("\033[0m\n");
    }
    printf("\033[0m");
}

void print_pixel_8bit(unsigned char top, unsigned char bot){
    printf("\033[38;5;%dm", top);
    printf("\033[48;5;%dm\u2580", bot);
}

void print_pixel_24bit(unsigned char *top, unsigned char *bot){
    printf("\033[38;2;%d;%d;%dm", top[0], top[1], top[2]);
    printf("\033[48;2;%d;%d;%dm\u2580", bot[0], bot[1], bot[2]);
}

void print_pixel_bw(unsigned char top, unsigned char bot){
    if (top + bot == 2){
        printf("\033[7m \033[27m");
    }
    else if (top + bot == 0){
        printf(" ");
    }
    else if (top == 1){
        printf("\u2580");
    }
    else if (bot == 1){
        printf("\033[7m\u2580\033[27m");
    }
}

void print_pixel_systemcolor(unsigned char top, unsigned char bot){
    if (top < 8){
        printf("\033[%dm", top + 30);
    }
    else {
        printf("\033[%dm", top + 82);
    }
    if (bot < 8){
        printf("\033[%dm\u2580", bot + 40);
    }
    else {
        printf("\033[%dm\u2580", bot + 92);
    }
}

void set_term_mode_raw(struct termios *term_current){
    struct termios term_raw;
    if (tcgetattr(STDIN_FILENO, term_current) == -1){
        errno_safe = errno;
        printf("tpv: could not get terminal attributes");
        return;
    }
    term_raw = *term_current;
    term_raw.c_lflag &= ~(ICANON | ECHO);
    term_raw.c_cc[VMIN] = 0;
    term_raw.c_cc[VTIME] = 5;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_raw) == -1){
        errno_safe = errno;
        printf("tpv: could not set terminal attributes");
        return;
    }
}