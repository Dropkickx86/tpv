#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <lua.h>
#include <lauxlib.h>
#include <unistd.h>
#include "imageinfo.h"

int l_scrclear(){
    printf("%c[2J", 0x1B);
    printf("%c[H", 0x1B);
    return 0;
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

void pixel_print(unsigned char *top, unsigned char *bot){
    printf("%c[38;2;%d;%d;%dm", 0x1B, *top, *(top + 1), *(top + 2));
    printf("%c[48;2;%d;%d;%dm\u2580", 0x1B, *bot, *(bot + 1), *(bot + 2));
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

int l_pictureprint(lua_State *L){
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, IMAGEINFO_MT);
    unsigned char *termbg = gettermbg();
    
    for (int row = 0; row < img->h; row += 2){
        for (int col = 0; col < img->w; col++){
            unsigned char top_rgb[3];
            unsigned char bot_rgb[3];
            if (img->c >= 3){
                top_rgb[0] = *(img->data + (row * img->w * img->c) + (col * img->c));
                top_rgb[1] = *(img->data + (row * img->w * img->c) + (col * img->c) + 1);
                top_rgb[2] = *(img->data + (row * img->w * img->c) + (col * img->c) + 2);
                bot_rgb[0] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c));
                bot_rgb[1] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c) + 1);
                bot_rgb[2] = *(img->data + ((row + 1) * img->w * img->c) + (col * img->c) + 2);
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
        }
        printf("%c[0m\n", 0x1B);
    }
    endprint();
    free(termbg);
    return 0;
}

static const luaL_Reg funcs[] = {
    {"scrclear", l_scrclear},
    {"picture_print", l_pictureprint},
    {NULL, NULL}
};

int luaopen_drw(lua_State *L) {
    luaL_newlib(L, funcs);
    return 1;
}