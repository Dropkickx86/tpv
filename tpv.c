/*BSD 3-Clause License

Copyright (c) 2026, Emil Stenström

Full license information in LICENSE file*/
//System libraries
#define _POSIX_C_SOURCE 200809L
#include <argp.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>

//STB_IMAGE settings and include
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#define STBI_NO_GIF
#include "stb_image.h"

//Global defines
#ifndef WB_SIZE
#define WB_SIZE 16*1024
#endif
#if WB_SIZE < 39
#undef WB_SIZE
#define WB_SIZE 39
#endif

//Local defines
#define true 1
#define false 0

//Structs
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
typedef struct {
    char *buffer;
    char *cursor;
} WriteBuffer;

//Functions
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
void dest_wb();
unsigned char get_ansi_level_88(unsigned char value);
unsigned char get_ansi_level_256(unsigned char);
float get_euclidean_distance_3D(unsigned char *p1, unsigned char *p2);
void get_term_bg(unsigned char *bg_col);
int get_term_fd();
int get_term_colordepth();
void get_term_colorpalette();
struct winsize get_term_size();
int init_wb();
void next_entry(char *buffer);
error_t parse_opt(int key, char *arg, struct argp_state *state);
void print_clear();
void print_picture(ImageInfo *img, const struct winsize *term, int fast, int colordepth);
void print_pixel_8bit(unsigned char top, unsigned char bot);
void print_pixel_24bit(unsigned char *top, unsigned char *bot);
void print_pixel_bw(unsigned char top, unsigned char bot);
void print_pixel_systemcolor(unsigned char top, unsigned char bot);
void print_wb(char *string);
int set_term_mode_raw(struct termios *term_current, int fd);

//Global vars and arrays
WriteBuffer wb;
char doc[] = "CLI tool to print images in terminal";
char args_doc[] = "filepath";
struct argp_option options[] = {
    {"color-depth", 'c', "depth", 0, "Choose color depth", 0},
    {"fast", 'f', 0, 0, "Print image by skipping pixels instead of scaling", 0},
    {"grayscale", 'g', 0, 0, "Print in grayscale", 0},
    {"help", 'h', 0, OPTION_HIDDEN, "Show this help list", 0},
    {"inline", 'i', 0, 0, "Print without clearing the screen", 0},
    {"no-scaling", 'n', 0, 0, "Print image in full resolution", 0},
    {"progress", 'p', 0, 0, "Show progress when scaling image", 0},
    {"no-vert-scaling", 'v', 0, 0, "Scale only to terminal width", 0},
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
    struct arguments arguments = {false};
    #ifdef DEFAULT_INLINE
    arguments.arg_inline = true;
    #endif
    argp_parse(&argp, argc, argv, ARGP_NO_HELP, 0, &arguments);

    //Check for pipe, can terminate execution
    char pipebuffer[128] = {0};
    if (!arguments.filepath){
        struct stat statinfo;
        if (fstat(STDIN_FILENO, &statinfo) == -1){
            printf("tpv: issue reading from pipe\n");
            return EXIT_FAILURE;
        }
        if (S_ISFIFO(statinfo.st_mode)){
            int inamount = 0, prevamount = 0;
            do {
                prevamount = inamount;
                inamount += read(STDIN_FILENO, pipebuffer, 127);
            } while(prevamount != inamount);
            if (inamount == 0){
                printf("tpv: issue reading from pipe\n");
                return EXIT_FAILURE;
            }
            if (pipebuffer[inamount - 1] == '\n'){
                if (pipebuffer[inamount - 2] == '\r'){
                    inamount -= 2;
                }
                else {
                    inamount -=1;
                }
            }
            pipebuffer[inamount] = '\0';
        }
        else if (S_ISREG(statinfo.st_mode)){
            ssize_t pathlength = readlink("/proc/self/fd/0", pipebuffer, 127);
            if (pathlength == -1){
                printf("tpv: issue reading from pipe\n");
                return EXIT_FAILURE;
            }
            pipebuffer[pathlength] = '\0';
            close(STDIN_FILENO);
        }
        arguments.filepath = pipebuffer;
    }

    //Variable declarations, definitions, checks, can terminate execution
    const struct winsize term = get_term_size();
    if (term.ws_row == 0){
        printf("tpv: failed getting terminal size\n");
        return EXIT_FAILURE;
    }
    unsigned char termbg[3] = {0};
    get_term_bg(termbg);
    int colordepth = get_term_colordepth();
    if (colordepth < 0){
        return EXIT_FAILURE;
    }
    else if (!arguments.color_depth){
        arguments.color_depth = colordepth;
    }
    else if (arguments.color_depth > colordepth){
        printf("tpv: c %d: requested color depth is higher than maximum supported by terminal\n", arguments.color_depth);
        return EXIT_FAILURE;
    }
    if (arguments.color_depth <= 4){
        get_term_colorpalette();
        #ifndef TRUE_BW
        if (arguments.color_depth == 4){
            palette_bw[1] = 15;
        }
        #endif
    }
    ImageInfo img;
    if (arguments.progress){
        printf("Loading image...");
        fflush(stdout);
    }
    img.data = stbi_load(arguments.filepath, &img.w, &img.h, &img.c, 0);
    if (arguments.progress){
        printf("loaded\n");
    }
    if (!img.data){
        printf("%s\n", stbi_failure_reason());
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
    return EXIT_SUCCESS;
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
        setvbuf(stdout, NULL, _IONBF, 0);
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
                        #ifdef SCALING_PRECISE
                        coloffset++;
                        #else
                        coloffset += scalefactor / 2;
                        #endif
                    }
                    #ifdef SCALING_PRECISE
                    rowoffset++;
                    #else
                    rowoffset += scalefactor / 2;
                    #endif
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
        setvbuf(stdout, NULL, _IOLBF, 0);
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

void dest_wb(){
    if (!wb.buffer){
        fprintf(stderr, "tpv: write buffer not allocated");
    }
    else {
        if (wb.cursor != wb.buffer){
            int n = write(STDOUT_FILENO, wb.buffer, wb.cursor - wb.buffer);
            if (n < wb.cursor - wb.buffer){
                fprintf(stderr, "tpv: failed to write to stdout");
            }
        }
        free(wb.buffer);
        wb.buffer = NULL;
        wb.cursor = wb.buffer;
    }
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
    char inbuffer[64] = {0};
    ssize_t inamount = 0;
    int fd = get_term_fd();
    if (set_term_mode_raw(&term_old, fd) == EXIT_FAILURE){
        return;
    }
    ssize_t outamount = write(fd, "\033]11;?\007\033[c", 10);
    if (outamount == 10){
        do {
            inamount += read(fd, inbuffer + inamount, sizeof(inbuffer) - 1);
        } while (inbuffer[inamount - 1] != 'c');
    }
    tcsetattr(fd, TCSANOW, &term_old);
    close(fd);
    if (inbuffer[1] == ']'){
        unsigned int r, g, b;
        sscanf(inbuffer, "\033]11;rgb:%x/%x/%x", &r, &g, &b);
        bg_col[0] = (unsigned char)(r >> 8);
        bg_col[1] = (unsigned char)(g >> 8);
        bg_col[2] = (unsigned char)(b >> 8);
    }
}

int get_term_fd(){
    int fd = open("/dev/tty", O_RDWR);
    if (fd == -1){
        for (int std_fd = STDIN_FILENO; std_fd <= STDERR_FILENO; std_fd++){
            fd = open(ttyname(std_fd), O_RDWR | O_NOCTTY);
            if (fd != -1){
                break;
            }
        }
        
    }
    return fd;
}

int get_term_colordepth(){
    struct termios term_old;
    char inbuffer[256] = {0};
    const char *env_colorterm = getenv("COLORTERM");
    const char *env_term = getenv("TERM");
    ssize_t inamount = 0;
    int fd = get_term_fd();
    if (set_term_mode_raw(&term_old, fd) == EXIT_FAILURE){
        return -1;
    }
    char outbuffer[80] = {"\033[38;2;1;2;3m"
        "\033P$qm\033\\"
        "\033P+q524742\033\\"
        "\033]4;255;?\033\\"
        "\033]4;87;?\033\\"
        "\033]4;15;?\033\\"
        "\033]4;7;?\033\\"
        "\033[c"
        "\033[0m"};
    ssize_t outamount = write(fd, outbuffer, 79);
    if (outamount == 79){
        do {
            inamount += read(fd, inbuffer + inamount, sizeof(inbuffer) - 1);
        } while (inbuffer[inamount - 1] != 'c');
    }
    tcsetattr(fd, TCSANOW, &term_old);
    close(fd);
    while (inbuffer[1] == 'P' && inbuffer[0] != '\0'){ 
        if (inbuffer[2] == '1' && inbuffer[5] != '0'){
            return 24;
        }
        else {
            next_entry(inbuffer);
        }
    }
    if ((env_colorterm && (strcmp(env_colorterm, "truecolor") == 0 || strcmp(env_colorterm, "24bit") == 0)) || (env_term && (strcmp(env_term, "st-256color") == 0))){
        return 24;
    }
    if (inbuffer[1] == ']'){
        if (inbuffer[4] == '2'){
            return 8;
        }
        else if (inbuffer[4] == '8'){
            return 7;
        }
        else if (inbuffer[4] == '1'){
            return 4;
        }
        else if (inbuffer[4] == '7'){
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
    ssize_t inamount = 0;
    int fd = get_term_fd();
    if (set_term_mode_raw(&term_old, fd) == EXIT_FAILURE){
        return;
    }
    char outbuffer[157] = {"\033]4;0;?\033\\"
    "\033]4;1;?\033\\"
    "\033]4;2;?\033\\"
    "\033]4;3;?\033\\"
    "\033]4;4;?\033\\"
    "\033]4;5;?\033\\"
    "\033]4;6;?\033\\"
    "\033]4;7;?\033\\"
    "\033]4;8;?\033\\"
    "\033]4;9;?\033\\"
    "\033]4;10;?\033\\"
    "\033]4;11;?\033\\"
    "\033]4;12;?\033\\"
    "\033]4;13;?\033\\"
    "\033]4;14;?\033\\"
    "\033]4;15;?\033\\"
    "\033[c"
    "\033[0m"};
    ssize_t outamount = write(fd, outbuffer, 156);
    if (outamount == 156){
        do {
            inamount += read(fd, buffer + inamount, sizeof(buffer) - 1);
        } while (buffer[inamount - 1] != 'c');
    }
    tcsetattr(fd, TCSANOW, &term_old);
    close(fd);
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
        int lightlevel = palette[i][0] + palette[i][1] + palette[i][2];
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
    int fd = get_term_fd();
    if (ioctl(fd, TIOCGWINSZ, &term) == -1){
        term.ws_row = 1;
    }
    term.ws_row -= 1;
    close(fd);
    return term;
}

int init_wb(){
    if (!wb.buffer){
        wb.buffer = malloc(WB_SIZE);
        wb.cursor = wb.buffer;
        if (!wb.buffer){
            fprintf(stderr, "tpv: failed to allocate write buffer");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    else {
        fprintf(stderr, "tpv: double init of write buffer");
        return EXIT_SUCCESS;
    }
}

void itostr(char *output, int input){
    int index = 0;
    if (input > 99){
        output[index++] = (input / 100) + 48;
        input = input % 100;
    }
    if (input > 9){
        output[index++] = (input / 10) + 48;
        input = input % 10;
    }
    else if (index > 0) {
        output[index++] = '0';
    }
    output[index++] = input + 48;
    output[index] = '\0';
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
    case 'f':
        arguments->fast = true;
        break;
    case 'g':
        arguments->grayscale = true;
        break;
    case 'h':
        argp_state_help(state, stdout, ARGP_HELP_STD_HELP);
        break;
    case 'i':
        #ifndef DEFAULT_INLINE
        arguments->arg_inline = true;
        #else
        arguments->arg_inline = false;
        #endif
        break;
    case 'n':
        arguments->no_scaling = true;
        break;
    case 'p':
        arguments->progress = true;
        break;
    case 'v':
        arguments->no_vert_scaling = true;
        break;
    case ARGP_KEY_ARG:
        arguments->filepath = arg;
        break;
    case ARGP_KEY_END:
        if (!arguments->filepath){
            struct pollfd p = { .fd = STDIN_FILENO, .events = POLLIN};
            if (poll(&p, 1, 0) == 0){
                argp_usage(state);
            }
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return EXIT_SUCCESS;
}

void print_clear(){
    printf("\033[2J");
    printf("\033[H");
}

void print_picture(ImageInfo *img, const struct winsize *term, int fast, int colordepth){
    if (init_wb() == EXIT_FAILURE){
        return;
    }
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
                if (row + offset <= img->h){
                    color_bot[0] = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                    color_bot[1] = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                    color_bot[2] = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                }
                else {
                    color_bot[0] = color_top[0];
                    color_bot[1] = color_top[1];
                    color_bot[2] = color_top[2];
                }
            }
            else {
                color_top[0] = img->data[(row * img->w * img->c) + (col * img->c)];
                color_top[1] = img->data[(row * img->w * img->c) + (col * img->c) + 1];
                color_top[2] = img->data[(row * img->w * img->c) + (col * img->c) + 2];
                if (row + offset <= img->h){
                    color_bot[0] = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                    color_bot[1] = img->data[((row + offset) * img->w * img->c) + (col * img->c) + 1];
                    color_bot[2] = img->data[((row + offset) * img->w * img->c) + (col * img->c) + 2];
                }
                else {
                    color_bot[0] = color_top[0];
                    color_bot[1] = color_top[1];
                    color_bot[2] = color_top[2];
                }
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
                int extended = false;
                if (colordepth == 4){
                    extended = true;
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
        print_wb("\033[0m\n");
    }
    print_wb("\033[0m");
    dest_wb();
}

void print_pixel_8bit(unsigned char top, unsigned char bot){
    char output[23] = {0};
    strcat(output, "\033[38;5;");
    char temp[4];
    itostr(temp, top);
    strcat(output, temp);
    strcat(output, ";48;5;");
    itostr(temp, bot);
    strcat(output, temp);
    strcat(output, "m\u2580");
    print_wb(output);
}

void print_pixel_24bit(unsigned char *top, unsigned char *bot){
    char output[40] = {0};
    strcat(output, "\033[38;2;");
    char temp[4];
    for (int i = 0; i < 3; i++){
        itostr(temp, top[i]);
        strcat(output, temp);
        output[strlen(output)] = ';';
    }
    strcat(output, "48;2;");
    for (int i = 0; i < 3; i++){
        itostr(temp, bot[i]);
        strcat(output, temp);
        if (i != 2){
            output[strlen(output)] = ';';
        }
        else {
            output[strlen(output)] = 'm';
        }
    }
    strcat(output, "\u2580");
    print_wb(output);
}

void print_pixel_bw(unsigned char top, unsigned char bot){
    if (top + bot == 2){
        print_wb("\033[7m \033[27m");
    }
    else if (top + bot == 0){
        print_wb(" ");
    }
    else if (top == 1){
        print_wb("\u2580");
    }
    else if (bot == 1){
        print_wb("\033[7m\u2580\033[27m");
    }
}

void print_pixel_systemcolor(unsigned char top, unsigned char bot){
    char output[14] = {0};
    strcat(output, "\033[");
    char temp[4];
    if (top < 8){
        itostr(temp, top + 30);
    }
    else {
        itostr(temp, top + 82);
    }
    strcat(output, temp);
    output[strlen(output)] = ';';
    if (bot < 8){
        itostr(temp, bot + 40);
    }
    else {
        itostr(temp, bot + 92);
    }
    strcat(output, temp);
    strcat(output, "m\u2580");
    print_wb(output);
}

void print_wb(char *string){
    int strlen = 0;
    do {
        strlen++;
    } while (string[strlen] != '\0');
    if (strlen > (wb.buffer + WB_SIZE) - wb.cursor){
        int written = 0, towrite = wb.cursor - wb.buffer;
        char *curwrite = wb.buffer;
        do {
            written += write(STDOUT_FILENO, curwrite, towrite);
            curwrite += written;
        } while (written < towrite);
        wb.cursor = wb.buffer;
    }
    for (int i = 0; i < strlen; i++){
        *wb.cursor++ = string[i];
    }
}

int set_term_mode_raw(struct termios *term_current, int fd){
    struct termios term_raw;
    if (tcgetattr(fd, term_current) == -1){
        printf("tpv: could not get terminal attributes\n");
        return EXIT_FAILURE;
    }
    term_raw = *term_current;
    term_raw.c_lflag &= ~(ICANON | ECHO);
    term_raw.c_cc[VMIN] = 0;
    term_raw.c_cc[VTIME] = 5;
    if (tcsetattr(fd, TCSANOW, &term_raw) == -1){
        printf("tpv: could not set terminal attributes\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}