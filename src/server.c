/*
 * Copyright (c) 2016 Sean Davis <dive@endersgame.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Adjust these as necessary.
 */
#define PYTB_MAXLEN         16384               /* maximum message size to accept */
#define PYTB_SOCKETPATH     "/tmp/pytb.sock"    /* path to our socket */
#define PYTB_QLEN           5                   /* queue 5 connections before dropping further attempts */

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 600
#endif /* _XOPEN_SOURCE */

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

/*
 * The maximum address length (file path length) of a unix domain socket is not
 * defined by POSIX. 4.4BSD uses 104 bytes, glibc uses 108, and the documentation
 * mentions other systems which use 92. Use the smallest of the three.
 */
#define MAX_SUN_LEN         92
/*
 * Linux doesn't expose this macro from sys/un.h; use NetBSD's version.
 */
#ifndef SUN_LEN
#define SUN_LEN(su) \
    (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif /* SUN_LEN */

struct pytb_header     /* our header */
{
    int32_t  tb_pid;   /* PID of connecting process */
    uint64_t tb_len;   /* Traceback length in bytes */
} __attribute__ ((packed));
/*
 * This should always be 12 bytes, but lets be nice and let the preprocessor figure that out.
 */
#define PYTB_HEADERSIZE sizeof(struct pytb_header)

static size_t pytb_killnewline(char *buf);

static void pytb_handle_client(int client_fd);

static int pytb_server(const char *path);

static void pytb_main_loop(void);

/*
 * pytb_killnewline(buf)
 *
 * Replace any trailing line endings with NULs.
 *
 * This function was written by Jens Steube for the Hashcat project, and modified
 * (renamed, blank lines removed) by Sean Davis as part of adding it to TG.
 *
 * This code is used under a BSD license with permission from Jens; the purpose
 * of this is simply to avoid any confusion from mixing licenses.
 */
static size_t pytb_killnewline(char *buf)
{
    size_t len = strlen(buf);

    while (len)
    {
        if (buf[len - 1] == '\n')
        {
            len--;
            continue;
        }
        if (buf[len - 1] == '\r')
        {
            len--;
            continue;
        }
        break;
    }
    buf[len] = 0;
    return len;
}

/*
 * pytb_handle_client(client_fd)
 *
 * This function reads a header and then message from the specified client.
 * Connections with invalid headers are silently dropped, as are messages that
 * are not properly NUL-terminated.
 */
static void pytb_handle_client(int client_fd)
{
    size_t             items_read;
    size_t             buflen;
    FILE               *client;
    char               *buf;
    struct pytb_header hdr;

    client = fdopen(client_fd, "r");
    if (setvbuf(client, NULL, _IONBF, 0) != 0)
    {
        /* Don't trust things to work if we can't disable buffering */
        goto nodata;
    }
    /* read the header */
    items_read = fread(&hdr, PYTB_HEADERSIZE, 1, client);
    if (items_read != 1)
    {
        /*
         * Cannot continue if we didn't get one and exactly one item of PYTB_HEADERSIZE bytes.
         */
        goto nodata;
    }
    else if (hdr.tb_len > PYTB_MAXLEN || hdr.tb_len < 3)
    {
        /*
         * Cannot continue with an invalid length.
         * Minimum set to 3 so we don't break when we try to look at buf[buflen-2].
         * (a one or two character message would be useless, but not invalid)
         */
        goto nodata;
    }
    else if (hdr.tb_pid < 1)
    {
        /* valid PIDs are never negative. */
        goto nodata;
    }

    buflen = hdr.tb_len;
    buf    = calloc(1, buflen + 1);
    if (buf == NULL)
    {
        /*
         * Cannot continue if we cannot allocate memory.
         */
        goto nodata;
    }

    items_read = fread(buf, hdr.tb_len, 1, client);         /* read the message */
    if (items_read == 1)                                    /* got expected number of bytes */
    {
        if (buf[buflen - 1] == 0)                           /* message is properly terminated */
        {
            pytb_killnewline(buf);
            printf("VALID message from PID %d\n", hdr.tb_pid);
            printf("\"%s\"\n", buf);
        }
    }

    /* clean up */
    memset(buf, 0, buflen);
    free(buf);

    nodata:
    fclose(client);     /* this closes client_fd as well */
}

/*
 * pytb_server(path)
 *
 * Open and listen on a UNIX domain socket for connections.
 *
 * Returns a file descriptor corresponding to the open socket,
 * or -1 if an error was detected.
 */
static int pytb_server(const char *path)
{
    struct stat        st;
    struct sockaddr_un sun;
    int                server;
    int                ret;

    if (strlen(path) > MAX_SUN_LEN || strlen(path) == 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (stat(path, &st) == 0)
    {
        if (S_ISSOCK(st.st_mode))   /* the path exists and IS a socket */
        {
            if (unlink(path) == -1)
            {
                return -1;          /* if we can't remove the old socket, we can't continue. */
            }
        }
        else                        /* the path exists and IS NOT a socket */
        {
            errno = EINVAL;
            return -1;              /* leave it alone and return an error */
        }
    }
    else                            /* stat() returns nonzero... */
    {
        if (errno == ENOENT)        /* ...we expect ENOENT if the socket does not exist */
        {
            errno = 0;              /* ...so clear the error and proceed */
        }
        else
        {
            return -1;              /* anything else is unhandled and thus fatal */
        }
    }

    memset(&sun, 0, SUN_LEN(&sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, path, sizeof(sun.sun_path));

    server = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server == -1)
    {
        return -1;
    }

    ret = bind(server, (const struct sockaddr *) &sun, SUN_LEN(&sun));
    if (ret == -1)
    {
        close(server);
        return -1;
    }

    ret = listen(server, PYTB_QLEN);
    if (ret == -1)
    {
        close(server);
        return -1;
    }

    return server;
}

/*
 * pytb_main_loop(path,handler)
 *
 * Create a socket at the specified path, and accept connections.
 * pytb_handle_client is called with the fd of the new connection, and is
 * expected to take care of closing the descriptor properly.
 */
static void pytb_main_loop()
{
    int                fd;
    struct sockaddr_un sa;
    socklen_t          sa_len;

    sa_len = sizeof(sa);
    fd     = pytb_server(PYTB_SOCKETPATH);
    if (fd < 0)
    {
        errx(EXIT_FAILURE, "pytb_server(): %s", strerror(errno));
    }

    while (1)
    {
        int client;
        memset(&sa, 0, sa_len);
        sa_len = sizeof(struct sockaddr_un);
        client = accept(fd, (struct sockaddr *) &sa, &sa_len);
        if (client > -1)
        {
            pytb_handle_client(client);
        }
        else
        {
            break;
        }
    }
}

#ifdef DEBUG

int main(void);

#endif /* DEBUG */

int main()
{
    pytb_main_loop();
}
