#include <lua.h>
#include <lauxlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct {
    int w, h, c;
    unsigned char *data;
} ImageInfo;

static int l_load(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    ImageInfo *img = (ImageInfo *)lua_newuserdata(L, sizeof(ImageInfo));
    img->data = stbi_load(path, &img->w, &img->h, &img->c, 0);

    if (!img->data) {
        luaL_error(L, "kunde inte ladda bild: %s", path);
    }

    luaL_getmetatable(L, "ImageInfo");
    lua_setmetatable(L, -2);
    return 1;
}

// img.w / img.h / img.c / img[i] -> hit
static int l_index(lua_State *L) {
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, "ImageInfo");

    // Numeriskt index -> pixeldata
    if (lua_type(L, 2) == LUA_TNUMBER) {
        int i = lua_tointeger(L, 2);
        int total = img->w * img->h * img->c;
        luaL_argcheck(L, i >= 1 && i <= total, 2, "index utanför gränserna");
        lua_pushinteger(L, img->data[i - 1]);
        return 1;
    }

    // Strängnyckel -> namngivna fält
    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "w") == 0) {
        lua_pushinteger(L, img->w);
    } else if (strcmp(key, "h") == 0) {
        lua_pushinteger(L, img->h);
    } else if (strcmp(key, "c") == 0) {
        lua_pushinteger(L, img->c);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int l_gc(lua_State *L) {
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, "ImageInfo");
    stbi_image_free(img->data);
    return 0;
}

static const luaL_Reg meta[] = {
    {"__index", l_index},
    {"__gc", l_gc},
    {NULL, NULL}
};

static const luaL_Reg funcs[] = {
    {"load", l_load},
    {NULL, NULL}
};

int luaopen_imageinfo(lua_State *L) {
    luaL_newmetatable(L, "ImageInfo");
    luaL_setfuncs(L, meta, 0);
    luaL_newlib(L, funcs);
    return 1;
}