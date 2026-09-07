#include <stdlib.h>
#include <stdio.h>
//#include <string.h>
#include <termios.h>
#include <lua.h>
#include <lauxlib.h>
//#include <lualib.h>
#include <unistd.h>

int l_scrclear(lua_State *L){
    printf("%c[2J", 0x1B);
    printf("%c[H", 0x1B);
    return 0;
}

int lua_gettablevalue(lua_State *L, char *label){
    lua_pushstring(L, label);
    lua_gettable(L, -2);
    int result = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return result;
}

unsigned char* parsehex(unsigned long int hex){
    unsigned char *result = malloc(4 * sizeof(char));
    for(int bitshift = 24; bitshift >= 0; bitshift -= 8){
        *result = (hex >> bitshift) & 0xFF;
        result++;
    }
    return result - 4;
}

unsigned long int gettermbg(){
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

    unsigned long int hex = 0;

    if (n > 0){
        unsigned int r, g, b;
        if (sscanf(buf, "\033]11;rgb:%x/%x/%x", &r, &g, &b) == 3) {
            hex = ((unsigned long int)(r >> 8) << 24) | ((unsigned long int)(g >> 8) << 16) | ((unsigned long int)(b >> 8) << 8) | 0xFF;
        }
        return hex;
    }
    else {
        return 0;
    }
}

void pixel_print(unsigned long int colorcodefg, unsigned long int colorcodebg, unsigned long int termbg){
    unsigned char *fg = parsehex(colorcodefg);
    unsigned char *bg = parsehex(colorcodebg);
    unsigned char *term = parsehex(termbg);
    if (*(fg + 3) != 255){
        float alpha = (float)*(fg + 3) / 255;
        *fg = (*fg * alpha) + (*term * (1.0 - alpha));
        *(fg + 1) = (*(fg + 1) * alpha) + (*(term + 1) * (1.0 - alpha));
        *(fg + 2) = (*(fg + 2) * alpha) + (*(term + 2) * (1.0 - alpha));
    }
    if (*(bg + 3) != 255){
        float alpha = (float)*(bg + 3) / 255;
        *bg = (*bg * alpha) + (*term * (1.0 - alpha));
        *(bg + 1) = (*(bg + 1) * alpha) + (*(term + 1) * (1.0 - alpha));
        *(bg + 2) = (*(bg + 2) * alpha) + (*(term + 2) * (1.0 - alpha));
    }
    printf("%c[38;2;%d;%d;%dm", 0x1B, *fg, *(fg + 1), *(fg + 2));
    printf("%c[48;2;%d;%d;%dm\u2580", 0x1B, *bg, *(bg + 1), *(bg + 2));
    free(fg);
    free(bg);
    free(term);
}

void endprint(){
    printf("%c[0m", 0x1B);
}

int l_pictureprint(lua_State *L){
    const char *tablename = lua_tostring(L, 1);
    unsigned long int termbg = gettermbg();
    lua_getglobal(L, tablename);
    lua_pushinteger(L, 1);
    lua_gettable(L, -2);
    lua_pushinteger(L, 1);
    lua_gettable(L, -2);
    lua_pushinteger(L, 2);
    lua_gettable(L, -3);
    int height = (int)lua_tointeger(L, -2);
    int width = (int)lua_tointeger(L, -1);
    lua_pop(L, 3);
    int row = 2;
    lua_pushinteger(L, row);
    lua_gettable(L, -2);
    lua_pushinteger(L, row + 1);
    if (row <= height){
        lua_gettable(L, -3);
    }
    while(lua_istable(L, -2)) {
        int col = 1;
        lua_pushinteger(L, col);
        lua_gettable(L, -3);
        lua_pushinteger(L, col);
        if (row <= height){
            lua_gettable(L, -3);
        }
        unsigned long int colorcodefg, colorcodebg;
        while((colorcodefg = (unsigned long int)lua_tointeger(L, -2)) != 0){
            if(row <= height){
                colorcodebg = (unsigned long int)lua_tointeger(L, -1);
            }
            else {
                colorcodebg = termbg;
            }
            pixel_print(colorcodefg, colorcodebg, termbg);
            lua_pop(L, 2);
            lua_pushinteger(L, ++col);
            lua_gettable(L, -3);
            lua_pushinteger(L, col);
            if(row <= height){
                lua_gettable(L, -3);
            }
        }
        printf("\n");
        lua_pop(L, 4);
        row += 2;
        lua_pushinteger(L, row);
        lua_gettable(L, -2);
        lua_pushinteger(L, row + 1);
        lua_gettable(L, -3);
    }
    endprint();
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