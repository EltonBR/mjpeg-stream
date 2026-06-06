#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <jpeglib.h>

#define MAX_TCP_CLIENTS 32

enum protocol {
    PROTO_TCP,
    PROTO_UDP,
    PROTO_HTTP
};

struct client {
    int fd;
};

struct buffer {
    void *start;
    size_t length;
};

struct config {
    const char *device;
    const char *host;
    const char *port;
    enum protocol protocol;
    unsigned width;
    unsigned height;
    unsigned fps;
    int quality;
};

static volatile sig_atomic_t running = 1;

static void on_signal(int signo)
{
    (void)signo;
    running = 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Uso: %s --device /dev/video0 --host 0.0.0.0 --port 5000 [--tcp|--udp|--http]\n"
            "       [--width 640 --height 480 --fps 30 --quality 80]\n"
            "       --http expoe MJPEG compativel com navegador em http://host:port/\n"
            "       Tenta MJPEG nativo primeiro; quality so afeta fallback YUYV/RGB24.\n",
            argv0);
}

static int xioctl(int fd, unsigned long request, void *arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r < 0 && errno == EINTR);
    return r;
}

static int parse_args(int argc, char **argv, struct config *cfg)
{
    int i;
    cfg->device = "/dev/video0";
    cfg->host = "0.0.0.0";
    cfg->port = "5000";
    cfg->protocol = PROTO_TCP;
    cfg->width = 640;
    cfg->height = 480;
    cfg->fps = 30;
    cfg->quality = 80;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            cfg->device = argv[++i];
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            cfg->host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            cfg->port = argv[++i];
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            cfg->width = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            cfg->height = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            cfg->fps = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--quality") == 0 && i + 1 < argc) {
            cfg->quality = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--udp") == 0) {
            cfg->protocol = PROTO_UDP;
        } else if (strcmp(argv[i], "--tcp") == 0) {
            cfg->protocol = PROTO_TCP;
        } else if (strcmp(argv[i], "--http") == 0) {
            cfg->protocol = PROTO_HTTP;
        } else {
            return -1;
        }
    }

    if (cfg->width == 0 || cfg->height == 0 || cfg->fps == 0 ||
        cfg->quality < 1 || cfg->quality > 100) {
        return -1;
    }
    return 0;
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void yuyv_to_rgb(const unsigned char *src, unsigned char *dst,
                        unsigned width, unsigned height)
{
    size_t pixels = (size_t)width * height;
    size_t i;
    for (i = 0; i < pixels; i += 2) {
        int y0 = src[0];
        int u = src[1] - 128;
        int y1 = src[2];
        int v = src[3] - 128;
        int r, g, b;

#define CLAMP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
        r = y0 + ((359 * v) >> 8);
        g = y0 - ((88 * u + 183 * v) >> 8);
        b = y0 + ((454 * u) >> 8);
        dst[0] = (unsigned char)CLAMP(r);
        dst[1] = (unsigned char)CLAMP(g);
        dst[2] = (unsigned char)CLAMP(b);

        r = y1 + ((359 * v) >> 8);
        g = y1 - ((88 * u + 183 * v) >> 8);
        b = y1 + ((454 * u) >> 8);
        dst[3] = (unsigned char)CLAMP(r);
        dst[4] = (unsigned char)CLAMP(g);
        dst[5] = (unsigned char)CLAMP(b);
#undef CLAMP

        src += 4;
        dst += 6;
    }
}

static int encode_jpeg(const unsigned char *rgb, unsigned width, unsigned height,
                       int quality, unsigned char **out, unsigned long *out_len)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, out, out_len);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = (JSAMPROW)&rgb[cinfo.next_scanline * width * 3u];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return 0;
}

static int send_all_nosigpipe(int fd, const void *buf, size_t len)
{
    const unsigned char *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int send_tcp_frame_to_client(int fd, const unsigned char *jpeg,
                                    unsigned long len)
{
    uint32_t nlen;
    if (len > MJPEG_MAX_FRAME) {
        return -1;
    }
    nlen = htonl((uint32_t)len);
    if (send_all_nosigpipe(fd, &nlen, sizeof(nlen)) < 0 ||
        send_all_nosigpipe(fd, jpeg, (size_t)len) < 0) {
        return -1;
    }
    return 0;
}

static int send_http_stream_header(int fd)
{
    static const char header[] =
        "HTTP/1.0 200 OK\r\n"
        "Server: mjpeg_tx\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "Connection: close\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=mjpegframe\r\n"
        "\r\n";
    return send_all_nosigpipe(fd, header, sizeof(header) - 1u);
}

static int send_http_frame_to_client(int fd, const unsigned char *jpeg,
                                     unsigned long len)
{
    char header[160];
    int header_len;
    if (len > MJPEG_MAX_FRAME) {
        return -1;
    }

    header_len = snprintf(header, sizeof(header),
                          "--mjpegframe\r\n"
                          "Content-Type: image/jpeg\r\n"
                          "Content-Length: %lu\r\n"
                          "\r\n",
                          len);
    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
        return -1;
    }

    if (send_all_nosigpipe(fd, header, (size_t)header_len) < 0 ||
        send_all_nosigpipe(fd, jpeg, (size_t)len) < 0 ||
        send_all_nosigpipe(fd, "\r\n", 2) < 0) {
        return -1;
    }
    return 0;
}

static void remove_client(struct client clients[], int *count, int index)
{
    close(clients[index].fd);
    if (index + 1 < *count) {
        memmove(&clients[index], &clients[index + 1],
                (size_t)(*count - index - 1) * sizeof(clients[0]));
    }
    (*count)--;
}

static void accept_stream_clients(int listen_fd, struct client clients[],
                                  int *count, enum protocol protocol)
{
    while (*count < MAX_TCP_CLIENTS) {
        int fd = accept(listen_fd, NULL, NULL);
        if (fd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                perror("accept");
            }
            return;
        }

        if (protocol == PROTO_HTTP && send_http_stream_header(fd) < 0) {
            close(fd);
            continue;
        }

        clients[*count].fd = fd;
        (*count)++;
        fprintf(stderr, "cliente %s conectado (%d/%d)\n",
                protocol == PROTO_HTTP ? "HTTP" : "TCP",
                *count, MAX_TCP_CLIENTS);
    }
}

static void send_frame_to_stream_clients(struct client clients[], int *count,
                                         enum protocol protocol,
                                         const unsigned char *jpeg,
                                         unsigned long jpeg_len)
{
    int i = 0;
    while (i < *count) {
        int rc = protocol == PROTO_HTTP
                     ? send_http_frame_to_client(clients[i].fd, jpeg, jpeg_len)
                     : send_tcp_frame_to_client(clients[i].fd, jpeg, jpeg_len);
        if (rc < 0) {
            fprintf(stderr, "cliente %s desconectado\n",
                    protocol == PROTO_HTTP ? "HTTP" : "TCP");
            remove_client(clients, count, i);
        } else {
            i++;
        }
    }
}

static int send_udp_frame(int fd, const unsigned char *jpeg, unsigned long len,
                          uint32_t frame_id)
{
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD];
    uint16_t chunks;
    uint16_t i;

    if (len > MJPEG_MAX_FRAME) {
        return -1;
    }

    chunks = (uint16_t)((len + MJPEG_UDP_PAYLOAD - 1u) / MJPEG_UDP_PAYLOAD);
    if (chunks == 0) {
        chunks = 1;
    }

    for (i = 0; i < chunks; i++) {
        struct udp_frame_header hdr;
        size_t off = (size_t)i * MJPEG_UDP_PAYLOAD;
        size_t left = (size_t)len - off;
        size_t payload = left < MJPEG_UDP_PAYLOAD ? left : MJPEG_UDP_PAYLOAD;

        hdr.magic = htonl(MJPEG_UDP_MAGIC);
        hdr.frame_id = htonl(frame_id);
        hdr.chunk_id = htons(i);
        hdr.chunk_count = htons(chunks);
        hdr.frame_size = htonl((uint32_t)len);

        memcpy(packet, &hdr, sizeof(hdr));
        memcpy(packet + sizeof(hdr), jpeg + off, payload);
        if (send(fd, packet, sizeof(hdr) + payload, 0) < 0) {
            perror("send udp");
            return -1;
        }
    }
    return 0;
}

static int setup_camera(struct config *cfg, int *fd_out,
                        struct buffer **buffers_out, unsigned *count_out,
                        uint32_t *pixfmt_out)
{
    int fd;
    struct v4l2_format fmt;
    struct v4l2_streamparm parm;
    struct v4l2_requestbuffers req;
    struct buffer *buffers;
    unsigned i;
    enum v4l2_buf_type type;

    fd = open(cfg->device, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror(cfg->device);
        return -1;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = cfg->width;
    fmt.fmt.pix.height = cfg->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0 ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
        if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0 ||
            fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        }
        if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV &&
            xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            perror("VIDIOC_S_FMT");
            close(fd);
            return -1;
        }
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG &&
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV &&
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
        fprintf(stderr, "camera retornou formato nao suportado: %.4s\n",
                (char *)&fmt.fmt.pix.pixelformat);
        close(fd);
        return -1;
    }

    cfg->width = fmt.fmt.pix.width;
    cfg->height = fmt.fmt.pix.height;

    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = cfg->fps;
    (void)xioctl(fd, VIDIOC_S_PARM, &parm);

    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return -1;
    }

    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        close(fd);
        return -1;
    }

    for (i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            close(fd);
            free(buffers);
            return -1;
        }
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            close(fd);
            free(buffers);
            return -1;
        }
        if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            close(fd);
            free(buffers);
            return -1;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        close(fd);
        free(buffers);
        return -1;
    }

    fprintf(stderr, "camera: %ux%u formato %.4s\n",
            fmt.fmt.pix.width, fmt.fmt.pix.height,
            (char *)&fmt.fmt.pix.pixelformat);

    *fd_out = fd;
    *buffers_out = buffers;
    *count_out = req.count;
    *pixfmt_out = fmt.fmt.pix.pixelformat;
    return 0;
}

int main(int argc, char **argv)
{
    struct config cfg;
    int cam_fd = -1;
    int net_fd = -1;
    struct client stream_clients[MAX_TCP_CLIENTS];
    int stream_client_count = 0;
    struct buffer *buffers = NULL;
    unsigned buffer_count = 0;
    uint32_t pixfmt = 0;
    unsigned char *rgb = NULL;
    uint32_t frame_id = 1;

    if (parse_args(argc, argv, &cfg) < 0) {
        usage(argv[0]);
        return 2;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    if (setup_camera(&cfg, &cam_fd, &buffers, &buffer_count, &pixfmt) < 0) {
        return 1;
    }

    if (pixfmt != V4L2_PIX_FMT_MJPEG) {
        rgb = malloc((size_t)cfg.width * cfg.height * 3u);
        if (!rgb) {
            perror("malloc rgb");
            return 1;
        }
    }

    net_fd = cfg.protocol == PROTO_UDP ? udp_connect(cfg.host, cfg.port)
                                       : tcp_listen(cfg.host, cfg.port);
    if (net_fd < 0) {
        fprintf(stderr, "falha ao abrir %s em %s:%s\n",
                cfg.protocol == PROTO_UDP ? "UDP" :
                (cfg.protocol == PROTO_HTTP ? "servidor HTTP" : "servidor TCP"),
                cfg.host, cfg.port);
        return 1;
    }
    if (cfg.protocol != PROTO_UDP) {
        if (set_nonblocking(net_fd) < 0) {
            perror("fcntl listen");
            return 1;
        }
        fprintf(stderr, "servidor %s aguardando clientes em %s:%s\n",
                cfg.protocol == PROTO_HTTP ? "HTTP" : "TCP",
                cfg.host, cfg.port);
    }

    while (running) {
        fd_set fds;
        struct timeval tv;
        struct v4l2_buffer buf;
        unsigned char *jpeg = NULL;
        unsigned long jpeg_len = 0;

        FD_ZERO(&fds);
        FD_SET(cam_fd, &fds);
        if (cfg.protocol != PROTO_UDP) {
            FD_SET(net_fd, &fds);
        }
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        if (select((cam_fd > net_fd ? cam_fd : net_fd) + 1, &fds, NULL, NULL, &tv) <= 0) {
            continue;
        }
        if (cfg.protocol != PROTO_UDP && FD_ISSET(net_fd, &fds)) {
            accept_stream_clients(net_fd, stream_clients, &stream_client_count,
                                  cfg.protocol);
        }
        if (!FD_ISSET(cam_fd, &fds)) {
            continue;
        }

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(cam_fd, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            perror("VIDIOC_DQBUF");
            break;
        }

        if (pixfmt == V4L2_PIX_FMT_MJPEG) {
            jpeg = buffers[buf.index].start;
            jpeg_len = buf.bytesused;
            if (cfg.protocol == PROTO_UDP) {
                if (send_udp_frame(net_fd, jpeg, jpeg_len, frame_id++) < 0) {
                    break;
                }
            } else if (stream_client_count > 0) {
                send_frame_to_stream_clients(stream_clients, &stream_client_count,
                                             cfg.protocol, jpeg, jpeg_len);
            }
        } else {
            if (pixfmt == V4L2_PIX_FMT_YUYV) {
                yuyv_to_rgb(buffers[buf.index].start, rgb, cfg.width, cfg.height);
            } else if (pixfmt == V4L2_PIX_FMT_RGB24) {
                memcpy(rgb, buffers[buf.index].start, (size_t)cfg.width * cfg.height * 3u);
            } else {
                fprintf(stderr, "formato V4L2 nao suportado\n");
                break;
            }

            if (encode_jpeg(rgb, cfg.width, cfg.height, cfg.quality, &jpeg, &jpeg_len) == 0) {
                if (cfg.protocol == PROTO_UDP) {
                    if (send_udp_frame(net_fd, jpeg, jpeg_len, frame_id++) < 0) {
                        free(jpeg);
                        break;
                    }
                } else if (stream_client_count > 0) {
                    send_frame_to_stream_clients(stream_clients, &stream_client_count,
                                                 cfg.protocol, jpeg, jpeg_len);
                }
                free(jpeg);
            }
        }

        if (xioctl(cam_fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            break;
        }
    }

    if (cam_fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        (void)xioctl(cam_fd, VIDIOC_STREAMOFF, &type);
    }
    if (buffers) {
        unsigned i;
        for (i = 0; i < buffer_count; i++) {
            if (buffers[i].start && buffers[i].start != MAP_FAILED) {
                munmap(buffers[i].start, buffers[i].length);
            }
        }
    }
    free(buffers);
    free(rgb);
    if (net_fd >= 0) {
        close(net_fd);
    }
    while (stream_client_count > 0) {
        remove_client(stream_clients, &stream_client_count, 0);
    }
    if (cam_fd >= 0) {
        close(cam_fd);
    }
    return 0;
}
