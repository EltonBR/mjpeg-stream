#include "ini.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char *trim(char *s)
{
    char *end;

    while (isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '\0') {
        return s;
    }

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

int ini_parse_file(const char *path, ini_handler handler, void *user)
{
    FILE *fp;
    char line[512];
    char section[64] = "";
    unsigned line_no = 0;

    fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *s;
        char *comment;
        char *eq;

        line_no++;
        s = trim(line);
        if (*s == '\0' || *s == '#' || *s == ';') {
            continue;
        }

        comment = strchr(s, '#');
        if (comment) {
            *comment = '\0';
        }
        comment = strchr(s, ';');
        if (comment) {
            *comment = '\0';
        }
        s = trim(s);

        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end) {
                fprintf(stderr, "%s:%u: secao invalida\n", path, line_no);
                fclose(fp);
                return -1;
            }
            *end = '\0';
            snprintf(section, sizeof(section), "%s", trim(s + 1));
            continue;
        }

        eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "%s:%u: esperado chave=valor\n", path, line_no);
            fclose(fp);
            return -1;
        }
        *eq = '\0';
        if (handler(user, section, trim(s), trim(eq + 1)) < 0) {
            fprintf(stderr, "%s:%u: valor invalido para %s\n",
                    path, line_no, trim(s));
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}
