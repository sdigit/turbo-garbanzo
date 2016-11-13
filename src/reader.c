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
#include "server.h"

#define TB_LEN_MAX  16384UL /* sending process needs to respect this too */

static void handle_client_message(int client_fd);

int main(void);

static void handle_client_message(int client_fd)
{
    size_t             items_read;
    FILE               *client;
    struct pytb_header hdr;
    char               *buf;

    client = fdopen(client_fd, "r");
    if (setvbuf(client, NULL, _IONBF, 0) != 0)              /* disable buffering */
    {
        /*
         * Don't trust things to work if we can't disable buffering
         */
        goto nodata;
    }

    items_read = fread(&hdr, PYTB_HEADERSIZE, 1, client);   /* read the header */
    if (items_read != 1)
    {
        /*
         * Cannot continue if we didn't get one and exactly one item of PYTB_HEADERSIZE bytes.
         */
        goto nodata;
    }
    else if (hdr.tb_len > TB_LEN_MAX || hdr.tb_len == 0 || hdr.tb_pid < 1)
    {
        /*
         * Cannot continue if the traceback length or PID are invalid.
         */
        goto nodata;
    }

    buf = calloc(1, hdr.tb_len);
    if (buf == NULL)
    {
        /*
         * Cannot continue if we cannot allocate memory.
         */
        goto nodata;
    }

    items_read = fread(buf, hdr.tb_len, 1, client);         /* read the message */
    if (items_read == 1)
    {
        printf("%d:%lu:\n", hdr.tb_pid, hdr.tb_len);
        printf("%s\n",buf);
    }

    /* clean up */
    memset(buf,0,hdr.tb_len);
    free(buf);

    nodata:
    fclose(client);     /* this closes client_fd as well */
}

int main()
{
    unix_socket_server("/tmp/test.sock", handle_client_message);
    exit(0);
}
