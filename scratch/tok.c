#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <stdlib.h>

char *remove_regex_lines(const char *input, const char *pattern)
{
    regex_t regx;
    if (regcomp(&regx, pattern, REG_EXTENDED | REG_NOSUB) != 0)
        return NULL;

    const char *p = input;
    const char *newline;
    char *out = NULL;
    size_t out_size = 0;
    size_t out_used = 0;

    char line[4096];   // temp line buffer

    while (*p) {
        newline = strchr(p, '\n');
        size_t len = newline ? (size_t)(newline - p) : strlen(p);

        if (len >= sizeof(line))
            len = sizeof(line) - 1;

        memcpy(line, p, len);
        line[len] = '\0';

        int match = regexec(&regx, line, 0, NULL, 0);

        if (match == REG_NOMATCH) {
            size_t need = len + 1;  // +1 for newline
            if (out_used + need + 1 > out_size) {
                size_t new_size = (out_size == 0) ? 4096 : out_size * 2;
                char *tmp = realloc(out, new_size);
                if (!tmp) {
                    free(out);
                    regfree(&regx);
                    return NULL;
                }
                out = tmp;
                out_size = new_size;
            }

            memcpy(out + out_used, line, len);
            out_used += len;
            out[out_used++] = '\n';
            out[out_used] = '\0';
        }

        p = newline ? newline + 1 : p + len;
    }

    regfree(&regx);

    return out;  // caller must free()
}

int main(void)
{
    const char *input =
        "keep 1\n"
        "drop 123\n"
        "keep 2\n"
        "drop everything\n";

    char *filtered = remove_regex_lines(input, "^drop");
    if (!filtered) {
        fprintf(stderr, "error filtering\n");
        return 1;
    }

    printf("Filtered:\n%s", filtered);
    free(filtered);
    return 0;
}

