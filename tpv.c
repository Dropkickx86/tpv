#include <lauxlib.h>
#include <lualib.h>
//#include <stdlib.h>
//#include <stdio.h>
#include <string.h>
#include <argp.h>

struct arguments {
    char *args[3];
    int no_scaling;
    int no_vert_scaling;
    char *filepath;
};

static char doc[] = "CLI tool to print images in terminal";
static char args_doc[] = "filepath";
static struct argp_option options[] = {
    {"no-scaling", 'n', 0, 0, "Prints image without scaling it down to fit the terminal window", 0},
    {"no-vert-scaling", 'v', 0, 0, "Prints image after scaling it down to fit on width, ignoring image height", 0},
    {"help", 'h', 0, 0, "Shows this help list", 0},
    {0},
};
static error_t parse_opt(int key, char *arg, struct argp_state *state){
    struct arguments *arguments = state->input;

    switch (key) {
    case 'n':
        arguments->no_scaling = 1;
        break;
    case 'v':
        arguments->no_vert_scaling = 1;
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
static struct argp argp = {options, parse_opt, args_doc, doc, 0, 0, 0};

int luaopen_imageinfo(lua_State *L);
int luaopen_drw(lua_State *L);

int main(int argc, char **argv) {
    struct arguments arguments;
    arguments.no_scaling = 0;
    arguments.no_vert_scaling = 0;
    arguments.filepath = "";
    argp_parse(&argp, argc, argv, ARGP_NO_HELP, 0, &arguments);

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_requiref(L, "imageinfo", luaopen_imageinfo, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "drw", luaopen_drw, 1);
    lua_pop(L, 1);

    luaL_loadfile(L, "main.lua");
    lua_pushstring(L, arguments.filepath);
    lua_pushinteger(L, arguments.no_scaling);
    lua_pushinteger(L, arguments.no_vert_scaling);
    lua_pcall(L, 3, 0, 0);

    lua_close(L);
    return 0;
}