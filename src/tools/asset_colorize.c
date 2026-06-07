#include <gdk-pixbuf/gdk-pixbuf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Uso: %s --color green|amber|#rrggbb input.png output.png\n"
            "Converte PNG para tons da cor escolhida preservando alpha.\n",
            argv0);
}

static int parse_hex_color(const char *text, double *r, double *g, double *b)
{
    unsigned int rv;
    unsigned int gv;
    unsigned int bv;

    if (!text || strlen(text) != 7 || text[0] != '#') {
        return -1;
    }
    if (sscanf(text + 1, "%02x%02x%02x", &rv, &gv, &bv) != 3) {
        return -1;
    }

    *r = rv / 255.0;
    *g = gv / 255.0;
    *b = bv / 255.0;
    return 0;
}

static int parse_color(const char *text, double *r, double *g, double *b)
{
    if (strcmp(text, "green") == 0 || strcmp(text, "verde") == 0) {
        *r = 0.0;
        *g = 1.0;
        *b = 0.0;
        return 0;
    }
    if (strcmp(text, "amber") == 0 || strcmp(text, "ambar") == 0) {
        *r = 1.0;
        *g = 0.62;
        *b = 0.0;
        return 0;
    }
    return parse_hex_color(text, r, g, b);
}

static double luminance(const guchar *px)
{
    return (0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2]) / 255.0;
}

static void analyze_luminance(GdkPixbuf *pixbuf, double *avg, double *min,
                              double *max)
{
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    double sum = 0.0;
    unsigned count = 0;
    int y;

    *min = 1.0;
    *max = 0.0;

    for (y = 0; y < height; y++) {
        guchar *row = pixels + y * rowstride;
        int x;
        for (x = 0; x < width; x++) {
            guchar *px = row + x * channels;
            double gray;

            if (channels >= 4 && px[3] == 0) {
                continue;
            }

            gray = luminance(px);
            if (gray < *min) {
                *min = gray;
            }
            if (gray > *max) {
                *max = gray;
            }
            sum += gray;
            count++;
        }
    }

    if (count == 0) {
        *avg = 0.0;
        *min = 0.0;
        *max = 0.0;
    } else {
        *avg = sum / count;
    }
}

static void colorize(GdkPixbuf *pixbuf, double r, double g, double b)
{
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    double avg;
    double min;
    double max;
    int use_white_as_strong;
    int y;

    analyze_luminance(pixbuf, &avg, &min, &max);
    use_white_as_strong = avg >= 0.5;

    for (y = 0; y < height; y++) {
        guchar *row = pixels + y * rowstride;
        int x;
        for (x = 0; x < width; x++) {
            guchar *px = row + x * channels;
            double gray;
            double strength;

            if (channels >= 4 && px[3] == 0) {
                continue;
            }

            gray = luminance(px);
            if (max - min < 0.02) {
                strength = 1.0;
            } else if (use_white_as_strong) {
                strength = gray;
            } else {
                strength = 1.0 - gray;
            }

            if (strength < 0.0) {
                strength = 0.0;
            } else if (strength > 1.0) {
                strength = 1.0;
            }

            px[0] = (guchar)(r * strength * 255.0);
            px[1] = (guchar)(g * strength * 255.0);
            px[2] = (guchar)(b * strength * 255.0);
        }
    }
}

int main(int argc, char **argv)
{
    const char *color = NULL;
    const char *input = NULL;
    const char *output = NULL;
    double r;
    double g;
    double b;
    GError *err = NULL;
    GdkPixbuf *loaded;
    GdkPixbuf *rgba;

    if (argc != 5 || strcmp(argv[1], "--color") != 0) {
        usage(argv[0]);
        return 2;
    }

    color = argv[2];
    input = argv[3];
    output = argv[4];

    if (parse_color(color, &r, &g, &b) < 0) {
        fprintf(stderr, "cor invalida: %s\n", color);
        usage(argv[0]);
        return 2;
    }

    loaded = gdk_pixbuf_new_from_file(input, &err);
    if (!loaded) {
        fprintf(stderr, "%s: %s\n", input, err ? err->message : "erro desconhecido");
        g_clear_error(&err);
        return 1;
    }

    rgba = gdk_pixbuf_get_has_alpha(loaded)
               ? gdk_pixbuf_copy(loaded)
               : gdk_pixbuf_add_alpha(loaded, FALSE, 0, 0, 0);
    g_object_unref(loaded);

    if (!rgba) {
        fprintf(stderr, "falha ao converter %s para RGBA\n", input);
        return 1;
    }

    colorize(rgba, r, g, b);

    if (!gdk_pixbuf_save(rgba, output, "png", &err, NULL)) {
        fprintf(stderr, "%s: %s\n", output, err ? err->message : "erro desconhecido");
        g_clear_error(&err);
        g_object_unref(rgba);
        return 1;
    }

    g_object_unref(rgba);
    return 0;
}
