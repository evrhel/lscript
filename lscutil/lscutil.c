#include "lscutil.h"

#include <Windows.h>

typedef struct lscu_context_s
{
    char *classpaths[LSCU_MAX_CLASSPATHS];
    char *imports[LSCU_MAX_IMPORTS];
    char *package;
} lscu_context_t;

static int class_exists_on_path(lscu_context_t *__restrict ctx, const char *__restrict classname);

static inline void package_to_filepath(char *in)
{
    while (*in)
    {
        if (*in == '.')
            *in = '\\';
        in++;
    }
}

static inline int file_exists(const char *path)
{
    DWORD dwAttrib = GetFileAttributesA(path);
    return dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}

LSCUEXPORT LSCUCONTEXT lscu_init()
{
    lscu_context_t *ctx = (lscu_context_t *)calloc(1, sizeof(lscu_context_t));
    return ctx;
}

LSCUEXPORT void lscu_destroy(LSCUCONTEXT context)
{
    lscu_remove_all_classpaths(context);
    lscu_remove_all_imports(context);
    lscu_set_package(context, NULL);
    free(context);
}

LSCUEXPORT int lscu_add_classpath(LSCUCONTEXT context, const char *__restrict path)
{
    int i;
    size_t len;
    lscu_context_t *ctx = (lscu_context_t *)context;

    if (!path) return 0;

    for (i = LSCU_MAX_CLASSPATHS - 1; i >= 0; i--)
    {
        if (!ctx->classpaths[i])
        {
            len = strlen(path) + 1;
            ctx->classpaths[i] = (char *)malloc(len);
            if (!ctx->classpaths[i]) return 0;
            strcpy_s(ctx->classpaths[i], len, path);
            return 1;
        }
    }

    return 0;
}

LSCUEXPORT int lscu_add_unimportant_classpath(LSCUCONTEXT context, const char *__restrict path)
{
    int i;
    size_t len;
    lscu_context_t *ctx = (lscu_context_t *)context;

    if (!path) return 0;

    for (i = 0; i < LSCU_MAX_CLASSPATHS; i++)
    {
        if (!ctx->classpaths[i])
        {
            len = strlen(path) + 1;
            ctx->classpaths[i] = (char *)malloc(len);
            if (!ctx->classpaths[i]) return 0;
            strcpy_s(ctx->classpaths[i], len, path);
            return 1;
        }
    }

    return 0;
}

LSCUEXPORT void lscu_remove_all_classpaths(LSCUCONTEXT context)
{
    size_t i;
    lscu_context_t *ctx = (lscu_context_t *)context;

    for (i = 0; i < LSCU_MAX_CLASSPATHS; i++)
    {
        if (ctx->classpaths[i])
        {
            free(ctx->classpaths[i]);
            ctx->classpaths[i] = NULL;
        }
    }
}

LSCUEXPORT void lscu_set_package(LSCUCONTEXT context, const char *__restrict package)
{
    size_t len;
    lscu_context_t *ctx = (lscu_context_t *)context;

    if (ctx->package)
    {
        free(ctx->package);
        ctx->package = NULL;
    }

    if (package)
    {
        len = strlen(package) + 2;
        ctx->package = (char *)malloc(len);
        if (ctx->package)
        {
            strcpy_s(ctx->package, len - 1, package);
            ctx->package[len - 2] = '.';
            ctx->package[len - 1] = 0;
        }
    }
}

LSCUEXPORT int lscu_add_import(LSCUCONTEXT context, const char *__restrict package)
{
    size_t i, len;
    lscu_context_t *ctx = (lscu_context_t *)context;

    if (!package) return 0;

    for (i = 0; i < LSCU_MAX_IMPORTS; i++)
    {
        if (!ctx->imports[i])
        {
            len = strlen(package) + 2;
            ctx->imports[i] = (char *)malloc(len);
            if (!ctx->imports[i]) return 0;
            strcpy_s(ctx->imports[i], len - 1, package);
            ctx->imports[i][len - 2] = '.';
            ctx->imports[i][len - 1] = 0;
            return 1;
        }
    }

    return 0;
}

LSCUEXPORT void lscu_remove_all_imports(LSCUCONTEXT context)
{
    size_t i;
    lscu_context_t *ctx = (lscu_context_t *)context;

    for (i = 0; i < LSCU_MAX_IMPORTS; i++)
    {
        if (ctx->imports[i])
        {
            free(ctx->imports[i]);
            ctx->imports[i] = NULL;
        }
    }
}

LSCUEXPORT int lscu_resolve_class(LSCUCONTEXT context, const char *__restrict classname, char *__restrict fullname, size_t bufsize)
{
    size_t i;
    char tocheck[MAX_PATH];
    lscu_context_t *ctx = (lscu_context_t *)context;

    tocheck[0] = 0;
    if (ctx->package)
        strcpy_s(tocheck, sizeof(tocheck), ctx->package);
    strcat_s(tocheck, sizeof(tocheck), classname);

    if (class_exists_on_path(ctx, tocheck))
    {
        if (fullname) strcpy_s(fullname, bufsize, tocheck);
        return 1;
    }

    for (i = 0; i < LSCU_MAX_IMPORTS; i++)
    {
        if (!ctx->imports[i]) break;

        tocheck[0] = 0;
        strcpy_s(tocheck, sizeof(tocheck), ctx->imports[i]);
        strcat_s(tocheck, sizeof(tocheck), classname);

        if (class_exists_on_path(ctx, tocheck))
        {
            if (fullname) strcpy_s(fullname, bufsize, tocheck);
            return 1;
        }
    }

    return 0;
}

int class_exists_on_path(lscu_context_t *__restrict ctx, const char *__restrict classname)
{
    int i, end, len;
    char fullpath[MAX_PATH];

    for (i = LSCU_MAX_CLASSPATHS - 1; i >= 0; i--)
    {
        if (!ctx->classpaths[i]) continue;

        len = strlen(ctx->classpaths[i]);
        strcpy_s(fullpath, sizeof(fullpath), ctx->classpaths[i]);
        fullpath[len] = '\\';
        strcpy_s(fullpath + len + 1, sizeof(fullpath) - len - 1, classname);
        package_to_filepath(fullpath + len + 1);

        end = strlen(fullpath);
        fullpath[end + 0] = '.';
        fullpath[end + 1] = 'l';
        fullpath[end + 2] = 'b';
        fullpath[end + 3] = 0;

        if (file_exists(fullpath)) return 1;

        fullpath[end + 0] = '.';
        fullpath[end + 1] = 'l';
        fullpath[end + 2] = 'a';
        fullpath[end + 3] = 's';
        fullpath[end + 4] = 'm';
        fullpath[end + 5] = 0;

        if (file_exists(fullpath)) return 1;
    }

    return 0;
}
