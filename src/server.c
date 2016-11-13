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

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 600
#endif /* _XOPEN_SOURCE */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include "server.h"

#define SOCKET_BACKLOG      5   /* queue 5 connections before dropping further attempts */

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
#endif

static int unix_listen(const char *path);

/*
 * unix_listen(path)
 *
 * Utility function to open, bind, and listen on a UNIX domain socket.
 *
 * Returns a file descriptor corresponding to the open socket,
 * or -1 if an error was detected.
 */
static int unix_listen(const char *path)
{
    struct stat        st;
    struct sockaddr_un sun;
    int                s;
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

    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1)
    {
        return -1;
    }

    ret = bind(s, (const struct sockaddr *) &sun, SUN_LEN(&sun));
    if (ret == -1)
    {
        close(s);
        return -1;
    }

    ret = listen(s, SOCKET_BACKLOG);
    if (ret == -1)
    {
        close(s);
        return -1;
    }

    return s;
}

/*
 * unix_socket_server(path,handler)
 *
 * Create a socket at the specified path, and accept connections.
 * handler is called with the fd of the new connection, and is
 * expected to take care of closing the descriptor properly.
 */
void unix_socket_server(const char *path,
                        void (*handler)(int))
{
    int                fd;
    int                c;
    struct sockaddr_un sa;
    socklen_t          sa_len;

    sa_len = sizeof(sa);
    fd     = unix_listen(path);
    if (fd < 0)
    {
        errx(EXIT_FAILURE, "unix_listen() returned %d", fd);
    }

    memset(&sa, 0, sa_len);
    while (1)
    {
        c = accept(fd, (struct sockaddr *) &sa, &sa_len);
        if (c > -1)
        {
            handler(c);
        }
        else
        {
            break;
        }
    }
}
