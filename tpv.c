#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct arguments {
    char *args[6];
    int fast;
    int no_scaling;
    int no_vert_scaling;
    int arg_inline;
    int progress;
    char *filepath;
};

typedef struct {
    int w, h, c;
    unsigned char *data;
} ImageInfo;

void alpha_apply(unsigned char *color, unsigned char *termbg, unsigned char a);
void endprint();
unsigned char* gettermbg();
struct winsize getterminfo();
void image_scale(ImageInfo *img, int scalefactor);
error_t parse_opt(int key, char *arg, struct argp_state *state);
void pictureprint(ImageInfo *img, int fast);
void pixel_print(unsigned char *top, unsigned char *bot);
int scrclear();

char doc[] = "CLI tool to print images in terminal";
char args_doc[] = "filepath";
struct argp_option options[] = {
    {"fast", 'f', 0, 0, "Prints image by skipping pixels instead of scaling. Fast but inaccurate", 0},
    {"no-scaling", 'n', 0, 0, "Prints image without scaling it down to fit the terminal window", 0},
    {"no-vert-scaling", 'v', 0, 0, "Prints image after scaling it down to fit on width, ignoring image height", 0},
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
    arguments.arg_inline = 0;
    arguments.progress = 0;
    arguments.filepath = "";
    argp_parse(&argp, argc, argv, ARGP_NO_HELP, 0, &arguments);

    ImageInfo img;
    img.data = stbi_load(arguments.filepath, &img.w, &img.h, &img.c, 0);
    struct winsize term = getterminfo();
    if (arguments.fast){
        goto render;
    }
    if (!arguments.no_scaling){
        int scalefactor;
        if (!arguments.no_vert_scaling && (img.h / (term.ws_row * 2)) > (img.w / term.ws_col)){
            scalefactor = (img.h / (term.ws_row * 2)) + 1;
        }
        else {
            scalefactor = (img.w / term.ws_col) + 1;
        }
        if (scalefactor > 1){
            image_scale(&img, scalefactor);
        }
    }

    render:
    if (!arguments.arg_inline){
        scrclear();
    }
    pictureprint(&img, arguments.fast);
    return 0;
}

void alpha_apply(unsigned char *color, unsigned char *termbg, unsigned char a){
    if (a == 255){
        return;
    }
    float alpha = (float)a / 255;
    color[0] = (color[0] * alpha) + (*termbg * (1.0 - alpha));
    color[1] = (color[1] * alpha) + (*(termbg + 1) * (1.0 - alpha));
    color[2] = (color[2] * alpha) + (*(termbg + 2) * (1.0 - alpha));
}

void endprint(){
    printf("%c[0m", 0x1B);
}

unsigned char* gettermbg(){
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
        unsigned char *result = malloc(3 * sizeof(char));
        unsigned int r, g, b;
        sscanf(buf, "\033]11;rgb:%x/%x/%x", &r, &g, &b);
        result[0] = (unsigned char)(r >> 8);
        result[1] = (unsigned char)(g >> 8);
        result[2] = (unsigned char)(b >> 8);
        return result;
    }
    else {
        return 0;
    }
}

struct winsize getterminfo(){
    struct winsize term;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &term);
    return term;
}

void image_scale(ImageInfo *img, int scalefactor){
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
            }
        }
    }
    float h = ceil(img->h / scalefactor);
    float w = ceil(img->w / scalefactor);
    img->h = (int) h;
    img->w = (int) w;
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

void pictureprint(ImageInfo *img, int fast){
    unsigned char *termbg = gettermbg();
    struct winsize term = getterminfo();
    int offset = 1;
    if (fast){
        if (img->w / term.ws_col > img->h / term.ws_row){
            offset += img->w / term.ws_col;
        }
        else {
            offset += img->h / term.ws_row;
        }
    }
    
    for (int row = 0; row < img->h; row += (offset * 2)){
        int col_out = 0;
        for (int col = 0; col < img->w; col += offset){
            if (col_out >= term.ws_col) {
                break;
            }
            unsigned char top_rgb[3];
            unsigned char bot_rgb[3];
            if (img->c >= 3){
                top_rgb[0] = *(img->data + (row * img->w * img->c) + (col * img->c));
                top_rgb[1] = *(img->data + (row * img->w * img->c) + (col * img->c) + 1);
                top_rgb[2] = *(img->data + (row * img->w * img->c) + (col * img->c) + 2);
                if (row + 1 == img->h){
                    bot_rgb[0] = *(termbg);
                    bot_rgb[1] = *(termbg + 1);
                    bot_rgb[2] = *(termbg + 2);
                }
                else {
                    bot_rgb[0] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c));
                    bot_rgb[1] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c) + 1);
                    bot_rgb[2] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c) + 2);
                }
            }
            else {
                top_rgb[0] = *(img->data + (row * img->w * img->c) + (col * img->c));
                top_rgb[1] = *(img->data + (row * img->w * img->c) + (col * img->c));
                top_rgb[2] = *(img->data + (row * img->w * img->c) + (col * img->c));
                bot_rgb[0] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c));
                bot_rgb[1] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c));
                bot_rgb[2] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c));
            }
            if (img->c == 4 || img->c == 2){
                alpha_apply(top_rgb, termbg, *(img->data + (row * img->w * img->c) + (col * img->c) + img->c - 1));
                alpha_apply(bot_rgb, termbg, *(img->data + ((row + 1) * img->w * img->c) + (col * img->c) + img->c - 1));
            }
            pixel_print(top_rgb, bot_rgb);
            col_out++;
        }
        printf("%c[0m\n", 0x1B);
    }
    endprint();
    free(termbg);
}

void pixel_print(unsigned char *top, unsigned char *bot){
    printf("%c[38;2;%d;%d;%dm", 0x1B, *top, *(top + 1), *(top + 2));
    printf("%c[48;2;%d;%d;%dm\u2580", 0x1B, *bot, *(bot + 1), *(bot + 2));
}

int scrclear(){
    printf("%c[2J", 0x1B);
    printf("%c[H", 0x1B);
    return 0;
}