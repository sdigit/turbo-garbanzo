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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tg.h"

#define TB_LEN_MAX  16384UL /* sending process needs to respect this too */

static void handle_client_message(int client_fd);

int main(void);

static void handle_client_message(int client_fd)
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
    else if (hdr.tb_len > TB_LEN_MAX || hdr.tb_len < 3)
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
        if (buf[buflen - 1] == 0)        /* message is properly terminated */
        {
            pytb_killnewline(buf);
            printf("VALID message from PID %d\n", hdr.tb_pid);
            printf("\"%s\"\n", buf);
        }
        else
        {
            printf("improperly terminated message was discarded.\n");
        }
    }
    else
    {
        printf("short message was discarded.\n");
    }
    /*
     * If the lengths do not match, fall through since we're cleaning up anyway.
     */
    /* clean up */
    memset(buf, 0, buflen);
    free(buf);

    nodata:
    fclose(client);     /* this closes client_fd as well */
}

int main()
{
    unix_socket_server("/tmp/test.sock", handle_client_message);
    exit(0);
}
