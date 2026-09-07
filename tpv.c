#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <unistd.h>

int luaopen_imageinfo(lua_State *L);
int luaopen_drw(lua_State *L);

int main(int argc, char *argv[]) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_requiref(L, "imageinfo", luaopen_imageinfo, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "drw", luaopen_drw, 1);
    lua_pop(L, 1);

    if (luaL_loadfile(L, "main.lua") || lua_pcall(L, 0, 0, 0)){
        printf("%s", lua_tostring(L, -1));
    }

    lua_close(L);
    return 0;
}