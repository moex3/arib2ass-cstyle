#ifdef __linux__
#include "platform.h"
#include "util.h"
#include <assert.h>
#include <sys/stat.h>


/* https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950 */
/* Make a directory; already existing dir okay */
static int maybe_mkdir(const pchar* path, mode_t mode)
{
    struct stat st;
    errno = 0;

    /* Try to make the directory */
    if (mkdir(path, mode) == 0)
        return 0;

    /* If it fails for any reason but EEXIST, fail */
    if (errno != EEXIST)
        return -1;

    /* Check if the existing path is a directory */
    if (stat(path, &st) != 0)
        return -1;

    /* If not, fail with ENOTDIR */
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    errno = 0;
    return 0;
}

int mkdir_p(const pchar *path)
{
    /* Adapted from http://stackoverflow.com/a/2336245/119527 */
    char *_path = NULL;
    char *p; 
    int result = -1;
    mode_t mode = 0775;

    errno = 0;

    /* Copy string so it's mutable */
    _path = strdup(path);
    if (_path == NULL)
        goto out;

    /* Iterate the string */
    for (p = _path + 1; *p; p++) {
        if (*p == '/') {
            /* Temporarily truncate */
            *p = '\0';

            if (maybe_mkdir(_path, mode) != 0)
                goto out;

            *p = '/';
        }
    }   

    if (maybe_mkdir(_path, mode) != 0)
        goto out;

    result = 0;

out:
    free(_path);
    return result;
}

#if 0
void utf8_to_pchar(char *in_ascii, int in_ascii_clen, pchar *out_pchar, int out_pchar_csize)
{
	/* Don't need to do anything */

    assert(in_ascii_clen > 0);
    assert(out_pchar_csize > in_ascii_clen);

	memcpy(out_pchar, in_ascii, in_ascii_clen);
	out_pchar[in_ascii_clen] = '\0';
}

void font_ucs_to_pchar(wchar_t *in_wchar, int in_wchar_clen, pchar *out_pchar, int out_pchar_csize)
{
    /* I will fix this when I find a font that breaks it */
    assert(out_pchar_csize > in_wchar_clen / sizeof(*in_wchar));
    int i;
    for (i = 0; i < in_wchar_clen; i++) {
        assert(in_wchar[i] == 0);

        out_pchar[i] = (pchar)((in_wchar[i] >> 8) & 0xFF);
    }
    out_pchar[i] = '\0';
}
#endif

void unicode_to_pchar(char32_t cp, pchar outs[8])
{
    unicode_to_utf8(cp, outs);
}


#endif /* __linux__ */
