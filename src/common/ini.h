#ifndef MJPEG_INI_H
#define MJPEG_INI_H

typedef int (*ini_handler)(void *user, const char *section,
                           const char *key, const char *value);

int ini_parse_file(const char *path, ini_handler handler, void *user);

#endif
