#include <argp.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
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

unsigned char alpha_apply(unsigned char color, unsigned char bg, unsigned char a);
unsigned char convert_8bit(unsigned char r, unsigned char g, unsigned char b);
unsigned char convert_color_8bit(unsigned char r, unsigned char g, unsigned char b);
unsigned char convert_grayscale_8bit(unsigned char value);
void endprint();
float euclidean_3D(unsigned char *p1, unsigned char *p2);
unsigned char get_ansi_level(unsigned char);
void gettermbg(unsigned char *bg_col);
struct winsize getterminfo();
void image_scale(ImageInfo *img, int scalefactor, int progress);
error_t parse_opt(int key, char *arg, struct argp_state *state);
void picture_print_8bit(ImageInfo *img, const struct winsize *term, int fast);
void picture_print_24bit(ImageInfo *img, const struct winsize *term, int fast);
void pixel_print_8bit(unsigned char top, unsigned char bot);
void pixel_print_24bit(unsigned char *top, unsigned char *bot, int ch_single);
int scrclear();

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
    arguments.color_depth = 24;
    arguments.arg_inline = 0;
    arguments.progress = 0;
    arguments.filepath = "";
    argp_parse(&argp, argc, argv, ARGP_NO_HELP, 0, &arguments);

    ImageInfo img;
    img.data = stbi_load(arguments.filepath, &img.w, &img.h, &img.c, 0);
    const struct winsize term = getterminfo();
    unsigned char termbg[3] = {0};
    gettermbg(termbg);

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
            image_scale(&img, scalefactor, arguments.progress);
        }
    }

    //alpha
    if (img.c == 2 || img.c == 4){
        for (int pixel = 0; pixel < (img.h * img.w * img.c); pixel += img.c){
            for (int offset = 0; offset < img.c - 1; offset++){
                img.data[pixel + offset] = alpha_apply(img.data[pixel + offset], termbg[offset], img.data[pixel + img.c - 1]);
            }
        }
    }

    //render
    if (!arguments.arg_inline){
        scrclear();
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
    if (arguments.color_depth == 8){
        picture_print_8bit(&img, &term, arguments.fast);
    }
    else if (arguments.color_depth == 24){
        picture_print_24bit(&img, &term, arguments.fast);
    }

    //cleanup
    free(img.data);
    return 0;
}

unsigned char alpha_apply(unsigned char color, unsigned char bg, unsigned char a){
    if (a != 255){
        float alpha = (float)a / 255;
        color = (color * alpha) + (bg * (1.0 - alpha));
    }
    return color;
}

unsigned char convert_8bit(unsigned char r, unsigned char g, unsigned char b){
    if (abs(r - g) < 12 && abs(g - b) < 12 && abs(r - b) < 12){
        unsigned char gray_index = convert_grayscale_8bit((unsigned char) (((int) r + (int) g + (int) b) / 3));
        unsigned char color_index = convert_color_8bit(r, g, b);
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
        float dist_gray = euclidean_3D(temp_color, val_gray);
        float dist_color = euclidean_3D(temp_color, val_color);
        if (dist_gray < dist_color){
            return gray_index;
        }
        else{
            return color_index;
        }
    }
    else{
        return convert_color_8bit(r, g, b);
    }
}

unsigned char convert_color_8bit(unsigned char r, unsigned char g, unsigned char b){
    return (unsigned char) 16 + (36 * get_ansi_level(r)) + (6 * get_ansi_level(g)) + get_ansi_level(b);
}

unsigned char convert_grayscale_8bit(unsigned char value){
    if (value < 8){
        return 16;
    }
    else if (value > 248){
        return 231;
    }
    else{
        return roundf(((float)value - 8) / 247 * 23) + 232;
    }
}

void endprint(){
    printf("%c[0m", 0x1B);
}

float euclidean_3D(unsigned char *p1, unsigned char *p2){
    int dist_r = (p1[0] - p2[0]) * (p1[0] - p2[0]);
    int dist_g = (p1[0] - p2[0]) * (p1[0] - p2[0]);
    int dist_b = (p1[0] - p2[0]) * (p1[0] - p2[0]);
    int dist_tot = dist_r + dist_g + dist_b;
    return sqrtf((float) dist_tot);
}

unsigned char get_ansi_level(unsigned char value){
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

void gettermbg(unsigned char *bg_col){
    struct termios old_term, raw_term;
    tcgetattr(STDIN_FILENO, &old_term);
    raw_term = old_term;
    raw_term.c_lflag &= ~(ICANON | ECHO);
    raw_term.c_cc[VMIN] = 0;
    raw_term.c_cc[VTIME] = 5;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);
    printf("\033]11;?\007");
    fflush(stdout);
    char buf[64] = {0};
    int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
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

struct winsize getterminfo(){
    struct winsize term;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &term);
    term.ws_row -= 1;
    return term;
}

void image_scale(ImageInfo *img, int scalefactor, int progress){
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
        if (atoi(arg) == 24 || atoi(arg) == 8){
            arguments->color_depth = atoi(arg);
        }
        else {
            printf("Invalid color depth\nValid options are 24 and 8\n");
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

void picture_print_8bit(ImageInfo *img, const struct winsize *term, int fast){
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
            if (img->c < 3){
                color_top = img->data[(row * img->w * img->c) + (col * img->c)];
                color_bot = img->data[((row + offset) * img->w * img->c) + (col * img->c)];
                color_top = convert_8bit(color_top, color_top, color_top);
                color_bot = convert_8bit(color_bot, color_bot, color_bot);
            }
            else{
                color_top = convert_8bit(img->data[(row * img->w * img->c) + (col * img->c)], img->data[(row * img->w * img->c) + (col * img->c) + 1], img->data[(row * img->w * img->c) + (col * img->c) + 2]);
                color_bot = convert_8bit(img->data[((row + offset) * img->w * img->c) + (col * img->c)], img->data[((row + offset) * img->w * img->c) + (col * img->c) + 1], img->data[((row + offset) * img->w * img->c) + (col * img->c) + 2]);
            }
            pixel_print_8bit(color_top, color_bot);
            col_out++;
        }
        printf("%c[0m\n", 0x1B);
    }
    endprint();
}

void picture_print_24bit(ImageInfo *img, const struct winsize *term, int fast){
    int offset = 1;
    int ch_single = 0;
    if (img->c < 3){
        ch_single = 1;
    }
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
            pixel_print_24bit(img->data + (row * img->w * img->c) + (col * img->c), img->data + ((row + offset) * img->w * img->c) + (col * img->c), ch_single);
            col_out++;
        }
        printf("%c[0m\n", 0x1B);
    }
    endprint();
}

void pixel_print_8bit(unsigned char top, unsigned char bot){
    printf("%c[38;5;%dm", 0x1B, top);
    printf("%c[48;5;%dm\u2580", 0x1B, bot);
}

void pixel_print_24bit(unsigned char *top, unsigned char *bot, int ch_single){
    if (ch_single){
        printf("%c[38;2;%d;%d;%dm", 0x1B, *top, *top, *top);
        printf("%c[48;2;%d;%d;%dm\u2580", 0x1B, *bot, *bot, *bot);
    }
    else{
        printf("%c[38;2;%d;%d;%dm", 0x1B, *top, *(top + 1), *(top + 2));
        printf("%c[48;2;%d;%d;%dm\u2580", 0x1B, *bot, *(bot + 1), *(bot + 2));
    }   
}

int scrclear(){
    printf("%c[2J", 0x1B);
    printf("%c[H", 0x1B);
    return 0;
}