#include <lauxlib.h>
#include <lualib.h>

int luaopen_imageinfo(lua_State *L);
int luaopen_drw(lua_State *L);

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("Need file name\n");
        return 1;
    }
    else if (argc > 2) {
        printf("Only provide file name\n");
        return 1;
    }

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_requiref(L, "imageinfo", luaopen_imageinfo, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "drw", luaopen_drw, 1);
    lua_pop(L, 1);

    luaL_loadfile(L, "main.lua");
    lua_pushstring(L, argv[1]);
    lua_pcall(L, 1, 0, 0);

    lua_close(L);
    return 0;
}