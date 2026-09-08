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

void pixel_print(unsigned char *fg, unsigned char *bg){
    printf("%c[38;2;%d;%d;%dm", 0x1B, *fg, *(fg + 1), *(fg + 2));
    printf("%c[48;2;%d;%d;%dm\u2580", 0x1B, *bg, *(bg + 1), *(bg + 2));
}

void pixel_print_4ch(unsigned char *fg, unsigned char *bg, unsigned char *termbg){
    if (*(fg + 3) != 255){
        float alpha = (float)*(fg + 3) / 255;
        *fg = (*fg * alpha) + (*termbg * (1.0 - alpha));
        *(fg + 1) = (*(fg + 1) * alpha) + (*(termbg + 1) * (1.0 - alpha));
        *(fg + 2) = (*(fg + 2) * alpha) + (*(termbg + 2) * (1.0 - alpha));
    }
    if (*(bg + 3) != 255){
        float alpha = (float)*(bg + 3) / 255;
        *bg = (*bg * alpha) + (*termbg * (1.0 - alpha));
        *(bg + 1) = (*(bg + 1) * alpha) + (*(termbg + 1) * (1.0 - alpha));
        *(bg + 2) = (*(bg + 2) * alpha) + (*(termbg + 2) * (1.0 - alpha));
    }
    pixel_print(fg ,bg);
}

int l_pictureprint(lua_State *L){
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, IMAGEINFO_MT);
    unsigned char *termbg = gettermbg();
    
    for (int row = 0; row < img->h; row += 2){
        for (int col = 0; col < img->w; col++){
            if (img->c == 4){
                pixel_print_4ch(img->data + (row * img->w * img->c) + (col * img->c), img->data + ((row + 1) * img->w * img->c) + (col * img->c), termbg);
            }
            else if (img->c == 3){
                pixel_print(img->data + (row * img->w * img->c) + (col * img->c), img->data + ((row + 1) * img->w * img->c) + (col * img->c));
            }
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