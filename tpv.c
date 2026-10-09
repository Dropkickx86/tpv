#include <argp.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct arguments {
    char *args[7];
    int fast;
    int no_scaling;
    int no_vert_scaling;
    int color_depth;
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
unsigned char convert_88(unsigned char r, unsigned char g, unsigned char b);
unsigned char convert_color_88(unsigned char r, unsigned char g, unsigned char b);
unsigned char convert_grayscale_88(unsigned char value);
unsigned char convert_256(unsigned char r, unsigned char g, unsigned char b);
unsigned char convert_color_256(unsigned char r, unsigned char g, unsigned char b);
unsigned char convert_grayscale_256(unsigned char value);
unsigned char get_ansi_level_88(unsigned char value);
unsigned char get_ansi_level_256(unsigned char);
float get_euclidean_distance_3D(unsigned char *p1, unsigned char *p2);
void get_term_bg(unsigned char *bg_col);
int get_term_colordepth();
struct winsize get_term_size();
void next_entry(char *buffer);
error_t parse_opt(int key, char *arg, struct argp_state *state);
void print_clear();
void print_picture(ImageInfo *img, const struct winsize *term, int fast, int colordepth);
void print_pixel_8bit(unsigned char top, unsigned char bot);
void print_pixel_24bit(unsigned char *top, unsigned char *bot, int ch_single);
void set_term_mode_raw(struct termios *term_current);

char doc[] = "CLI tool to print images in terminal";
char args_doc[] = "filepath";
struct argp_option options[] = {
    {"fast", 'f', 0, 0, "Prints image by skipping pixels instead of scaling. Fast but inaccurate", 0},
    {"no-scaling", 'n', 0, 0, "Prints image without scaling it down to fit the terminal window", 0},
    {"no-vert-scaling", 'v', 0, 0, "Prints image after scaling it down to fit on width, ignoring image height", 0},
    {"color-depth", 'c', "depth", 0, "Choose color depth. Defaults to highest capability of terminal", 0},
    {"inline", 'i', 0, 0, "Prints without clearing the screen", 0},
    {"progress", 'p', 0, 0, "Prints the progress when scaling images", 0},
    {"help", 'h', 0, 0, "Shows this help list", 0},
    {0},
};
struct argp argp = {options, parse_opt, args_doc, doc, 0, 0, 0};

int main(int argc, char **argv){
    struct arguments arguments;
    arguments.fast = 0;
    arguments.no_scaling = 0;
    arguments.no_vert_scaling = 0;
    arguments.color_depth = 0;
    arguments.arg_inline = 0;
    arguments.progress = 0;
    arguments.filepath = "";
    argp_parse(&argp, argc, argv, ARGP_NO_HELP, 0, &arguments);

    ImageInfo img;
    img.data = stbi_load(arguments.filepath, &img.w, &img.h, &img.c, 0);
    const struct winsize term = get_term_size();
    int colordepth = get_term_colordepth();
    if (arguments.color_depth == 0){
        arguments.color_depth = colordepth;
    }
    else if (arguments.color_depth > colordepth){
        printf("Requested color depth is higher than maximum supported by terminal\n");
        goto cleanup;
    }
    unsigned char termbg[3] = {0};
    get_term_bg(termbg);

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

    //alpha
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
                    else{
                        img.data[pixel + ch] = termbg[ch];
                    }
                }
            }
        }
    }
    print_picture(&img, &term, arguments.fast, arguments.color_depth);

    cleanup:
    free(img.data);
    return 0;
}

unsigned char apply_image_alpha(unsigned char color, unsigned char bg, unsigned char a){
    if (a != 255){
        float alpha = (float)a / 255;
        color = (color * alpha) + (bg * (1.0 - alpha));
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
        printf("%c[2G", 0x1B);
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
                    if (perc >= cursor) {
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

unsigned char convert_88(unsigned char r, unsigned char g, unsigned char b){
    if (abs(r - g) < 12 && abs(g - b) < 12 && abs(r - b) < 12){
        unsigned char gray_index = convert_grayscale_88((unsigned char) (((int) r + (int) g + (int) b) / 3));
        unsigned char color_index = convert_color_88(r, g, b);
        unsigned char val_color[3], val_gray[3];
        if (gray_index > 80){
            val_gray[0] = roundf(((float)gray_index - 81) * 23.2) + 92;
            val_gray[1] = val_gray[0];
            val_gray[2] = val_gray[0];
        }
        else{
            val_gray[0] = 46;
            val_gray[1] = 46;
            val_gray[2] = 46;
        }
        if (color_index == 16){
            val_color[0] = 0;
            val_color[1] = 0;
            val_color[2] = 0;
        }
        else{
            val_color[0] = 55 + (40 * ((color_index - 16) / 36));
            val_color[1] = 55 + (40 * (((color_index - 16) / 6) % 6));
            val_color[2] = 55 + (40 * ((color_index - 16) % 6));
        }
        unsigned char temp_color[3] = {r, g, b};
        float dist_gray = get_euclidean_distance_3D(temp_color, val_gray);
        float dist_color = get_euclidean_distance_3D(temp_color, val_color);
        if (dist_gray < dist_color){
            return gray_index;
        }
        else{
            return color_index;
        }
    }
    else{
        return convert_color_88(r, g, b);
    }
}

unsigned char convert_color_88(unsigned char r, unsigned char g, unsigned char b){
    return (unsigned char) 16 + (16 * get_ansi_level_88(r)) + (4 * get_ansi_level_88(g)) + get_ansi_level_88(b);
}

unsigned char convert_grayscale_88(unsigned char value){
    if (value < 29){
        return 16;
    }
    else if (value < 70){
        return 80;
    }
    else if (value > 243){
        return 79;
    }
    else{
        return roundf(((float)value - 92) / 23.2) + 81;
    }
}

unsigned char convert_256(unsigned char r, unsigned char g, unsigned char b){
    if (abs(r - g) < 12 && abs(g - b) < 12 && abs(r - b) < 12){
        unsigned char gray_index = convert_grayscale_256((unsigned char) (((int) r + (int) g + (int) b) / 3));
        unsigned char color_index = convert_color_256(r, g, b);
        unsigned char val_color[3], val_gray[3] = {8 + 10 * (gray_index - 232), 8 + 10 * (gray_index - 232), 8 + 10 * (gray_index - 232)};
        if (color_index == 16){
            val_color[0] = 0;
            val_color[1] = 0;
            val_color[2] = 0;
        }
        else{
            val_color[0] = 55 + (40 * ((color_index - 16) / 36));
            val_color[1] = 55 + (40 * (((color_index - 16) / 6) % 6));
            val_color[2] = 55 + (40 * ((color_index - 16) % 6));
        }
        unsigned char temp_color[3] = {r, g, b};
        float dist_gray = get_euclidean_distance_3D(temp_color, val_gray);
        float dist_color = get_euclidean_distance_3D(temp_color, val_color);
        if (dist_gray < dist_color){
            return gray_index;
        }
        else{
            return color_index;
        }
    }
    else{
        return convert_color_256(r, g, b);
    }
}

unsigned char convert_color_256(unsigned char r, unsigned char g, unsigned char b){
    return (unsigned char) 16 + (36 * get_ansi_level_256(r)) + (6 * get_ansi_level_256(g)) + get_ansi_level_256(b);
}

unsigned char convert_grayscale_256(unsigned char value){
    if (value < 5){
        return 16;
    }
    else if (value > 242){
        return 231;
    }
    else{
        return roundf(((float)value - 8) / 10) + 232;
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
    int dist_g = (p1[0] - p2[0]) * (p1[0] - p2[0]);
    int dist_b = (p1[0] - p2[0]) * (p1[0] - p2[0]);
    int dist_tot = dist_r + dist_g + dist_b;
    return sqrtf((float) dist_tot);
}

void get_term_bg(unsigned char *bg_col){
    struct termios term_old;
    set_term_mode_raw(&term_old);
    printf("\033]11;?\007");
    fflush(stdout);
    char buf[64] = {0};
    int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &term_old);
    if (n > 0){
        unsigned int r, g, b;
        sscanf(buf, "\033]11;rgb:%x/%x/%x", &r, &g, &b);
        bg_col[0] = (unsigned char)(r >> 8);
        bg_col[1] = (unsigned char)(g >> 8);
        bg_col[2] = (unsigned char)(b >> 8);
        return;
    }
    else {
        bg_col = NULL;
        return;
    }
}

int get_term_colordepth(){
    struct termios term_old;
    char buffer[128] = {0};
    const char *env_colorterm = getenv("COLORTERM");
    const char *env_term = getenv("TERM");
    int inamount = 0;
    set_term_mode_raw(&term_old);
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
    while (buffer[1] == 'P'){ 
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
        else if (buffer[4] == '1') {
            return 4;
        }
        else if (buffer[4] == '7') {
            return 3;
        }
    }
    if (env_term && strcmp(env_term, "linux")){
        return 3;
    }
    return 1;
}

struct winsize get_term_size(){
    struct winsize term;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &term);
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
        if (atoi(arg) == 24 || atoi(arg) == 8 || atoi(arg) == 7){
            arguments->color_depth = atoi(arg);
        }
        else {
            printf("Invalid color depth\nValid options are 24, 8 and 7\n");
            argp_usage(state);
        }
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
        if (strcmp(arguments->filepath, "") == 0){
            argp_usage(state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

void print_clear(){
    printf("%c[2J", 0x1B);
    printf("%c[H", 0x1B);
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
            if (col_out >= term->ws_col) {
                break;
            }
            unsigned char color_top, color_bot;
            if (colordepth == 24){
                if (img->c < 3){
                    print_pixel_24bit(img->data + (row * img->w * img->c) + (col * img->c), img->data + ((row + offset) * img->w * img->c) + (col * img->c), 1);
                }
                else{
                    print_pixel_24bit(img->data + (row * img->w * img->c) + (col * img->c), img->data + ((row + offset) * img->w * img->c) + (col * img->c), 0);
                }
            }
            else if (colordepth == 8){
                if (img->c < 3){
                    color_top = img->data[(row * img->w * img->c) + (col * img->c)];
                    color_bot = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                    color_top = convert_256(color_top, color_top, color_top);
                    color_bot = convert_256(color_bot, color_bot, color_bot);
                }
                else{
                    color_top = convert_256(img->data[(row * img->w * img->c) + (col * img->c)], img->data[(row * img->w * img->c) + (col * img->c) + 1], img->data[(row * img->w * img->c) + (col * img->c) + 2]);
                    color_bot = convert_256(img->data[((row + offset) * img->w * img->c) + (col * img->c)], img->data[((row + offset) * img->w * img->c) + (col * img->c) + 1], img->data[((row + offset) * img->w * img->c) + (col * img->c) + 2]);
                }
                print_pixel_8bit(color_top, color_bot);
            }
            else if (colordepth == 7) {
                if (img->c < 3){
                    color_top = img->data[(row * img->w * img->c) + (col * img->c)];
                    color_bot = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                    color_top = convert_88(color_top, color_top, color_top);
                    color_bot = convert_88(color_bot, color_bot, color_bot);
                }
                else{
                    color_top = convert_88(img->data[(row * img->w * img->c) + (col * img->c)], img->data[(row * img->w * img->c) + (col * img->c) + 1], img->data[(row * img->w * img->c) + (col * img->c) + 2]);
                    color_bot = convert_88(img->data[((row + offset) * img->w * img->c) + (col * img->c)], img->data[((row + offset) * img->w * img->c) + (col * img->c) + 1], img->data[((row + offset) * img->w * img->c) + (col * img->c) + 2]);
                }
                print_pixel_8bit(color_top, color_bot);
            }
            
            col_out++;
        }
        printf("%c[0m\n", 0x1B);
    }
    printf("%c[0m", 0x1B);
}

void print_pixel_8bit(unsigned char top, unsigned char bot){
    printf("%c[38;5;%dm", 0x1B, top);
    printf("%c[48;5;%dm\u2580", 0x1B, bot);
}

void print_pixel_24bit(unsigned char *top, unsigned char *bot, int ch_single){
    if (ch_single){
        printf("%c[38;2;%d;%d;%dm", 0x1B, *top, *top, *top);
        printf("%c[48;2;%d;%d;%dm\u2580", 0x1B, *bot, *bot, *bot);
    }
    else{
        printf("%c[38;2;%d;%d;%dm", 0x1B, *top, *(top + 1), *(top + 2));
        printf("%c[48;2;%d;%d;%dm\u2580", 0x1B, *bot, *(bot + 1), *(bot + 2));
    }   
}

void set_term_mode_raw(struct termios *term_current){
    struct termios term_raw;
    tcgetattr(STDIN_FILENO, term_current);
    term_raw = *term_current;
    term_raw.c_lflag &= ~(ICANON | ECHO);
    term_raw.c_cc[VMIN] = 0;
    term_raw.c_cc[VTIME] = 5;
    tcsetattr(STDIN_FILENO, TCSANOW, &term_raw);
}