#include <lua.h>
#include <lauxlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imageinfo.h"

static int index_resolve(ImageInfo *img, int row, int col, int ch, int w_target) {
    if (w_target == 0) {
        w_target = img->w;
    }
    return ((((row - 1) * w_target * img->c) + ((col - 1) * img->c) + ch) - 1);
}

static int l_load(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    ImageInfo *img = (ImageInfo *)lua_newuserdata(L, sizeof(ImageInfo));
    img->data = stbi_load(path, &img->w, &img->h, &img->c, 0);

    if (!img->data) {
        luaL_error(L, "File could not be read");
    }

    luaL_getmetatable(L, "ImageInfo");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_index(lua_State *L) {
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, IMAGEINFO_MT);

    if (lua_type(L, 2) == LUA_TNUMBER) {
        int i = lua_tointeger(L, 2);
        luaL_argcheck(L, i >= 1 && i <= (img->w * img->h * img->c), 2, "OOB");
        lua_pushinteger(L, img->data[i - 1]);
    }
    else if (lua_type(L, 2) == LUA_TSTRING) {
        const char *key = luaL_checkstring(L, 2);
        if (strcmp(key, "w") == 0) {
            lua_pushinteger(L, img->w);
        }
        else if (strcmp(key, "h") == 0) {
            lua_pushinteger(L, img->h);
        }
        else if (strcmp(key, "c") == 0) {
            lua_pushinteger(L, img->c);
        }
        else {
            lua_pushnil(L);
        }
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static int l_newindex(lua_State *L) {
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, IMAGEINFO_MT);

    if (lua_type(L, 2) == LUA_TNUMBER) {
        int i = lua_tointeger(L, 2), v = lua_tointeger(L, 3);
        luaL_argcheck(L, i >= 1 && i <= (img->h * img->w * img->c), 2, "OOB");
        luaL_argcheck(L, v >= 0 && v <= 255, 3, "Invalid value");
        img->data[i - 1] = (unsigned char)v;
    }
    else if (lua_type(L, 2) == LUA_TSTRING) {
        const char *key = luaL_checkstring(L, 2);
        luaL_argcheck(L, strcmp(key, "w") == 0 || strcmp(key, "h") == 0 || strcmp(key, "c") == 0, 2, "Invalid key" );
        if (strcmp(key, "w") == 0) {
            img->w = lua_tointeger(L, 3);
        }
        else if (strcmp(key, "h") == 0) {
            img->h = lua_tointeger(L, 3);
        }
        else if (strcmp(key, "c") == 0) {
            img->c = lua_tointeger(L, 3);
        }
    }
    else {
        luaL_argerror(L, 2, "Invalid type");
    }
    return 1;
}

static int l_set(lua_State *L) {
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, IMAGEINFO_MT);
    int row = lua_tointeger(L, 2);
    int col = lua_tointeger(L, 3);
    int ch = lua_tointeger(L, 4);
    int val = lua_tointeger(L, 5);
    int w_target = lua_tointeger(L, 6);
    luaL_argcheck(L, row > 0 || row <= img->h, 2, "OOB");
    luaL_argcheck(L, col > 0 || col <= img->w, 3, "OOB");
    luaL_argcheck(L, ch > 0 || ch <= img->c, 4, "OOB");
    luaL_argcheck(L, val >= 0 || val <= 255, 5, "Invalid value");
    luaL_argcheck(L, w_target >= 0 || w_target <= img->w, 6, "Invalid value");
    img->data[index_resolve(img, row, col, ch, w_target)] = val;
    return 0;
}

int l_get(lua_State *L) {
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, IMAGEINFO_MT);
    int row = lua_tointeger(L, 2);
    int col = lua_tointeger(L, 3);
    int ch = lua_tointeger(L, 4);
    luaL_argcheck(L, row > 0 || row <= img->h, 2, "OOB");
    luaL_argcheck(L, col > 0 || col <= img->w, 3, "OOB");
    luaL_argcheck(L, ch > 0 || ch <= img->c, 4, "OOB");
    lua_pushinteger(L, img->data[index_resolve(img, row, col, ch, 0)]);
    return 1;
}

static int l_gc(lua_State *L) {
    ImageInfo *img = (ImageInfo *)luaL_checkudata(L, 1, IMAGEINFO_MT);
    stbi_image_free(img->data);
    return 0;
}

static const luaL_Reg meta[] = {
    {"__index", l_index},
    {"__newindex", l_newindex},
    {"__gc", l_gc},
    {NULL, NULL}
};

static const luaL_Reg funcs[] = {
    {"load", l_load},
    {"set", l_set},
    {"get", l_get},
    {NULL, NULL}
};

int luaopen_imageinfo(lua_State *L) {
    luaL_newmetatable(L, IMAGEINFO_MT);
    luaL_setfuncs(L, meta, 0);
    luaL_newlib(L, funcs);
    return 1;
}