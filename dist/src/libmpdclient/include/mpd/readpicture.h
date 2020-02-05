/* libmpdclient
   (c) 2003-2019 The Music Player Daemon Project
   This project's homepage is: http://www.musicpd.org

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Music Player Daemon nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*! \file
 * \brief MPD client library
 *
 * Do not include this header directly.  Use mpd/client.h instead.
 */

#ifndef MPD_READPICTURE_H
#define MPD_READPICTURE_H

#include "compiler.h"

#include <stdbool.h>
#include <stddef.h>

#include <mpd/binary.h>

struct mpd_connection;

struct mpd_readpicture {
        /** fixed size binary data buffer*/
        unsigned char data[MPD_BINARY_CHUNK_SIZE];

        /** the size of the picture */
        size_t size;
        
        /** bytes in the binary data buffer*/
        size_t data_length;
        
        /** optional mime_type*/
        char *mime_type;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Frees the "readpicture" struct
 *
 * @param buffer a allocated struct mpd_readpicture
 */
void
mpd_free_readpicture(struct mpd_readpicture *buffer);

/**
 * Sends the "readpicture" command to MPD.  Call mpd_recv_readpicture() to
 * read response lines. 
 *
 * @param connection a valid and connected #mpd_connection
 * @param uri the URI of the song
 * @param offset to read from
 * @return true on success
 */
bool
mpd_send_readpicture(struct mpd_connection *connection, 
                                   const char *uri, 
                                   unsigned offset);

/**
 * Receives the "readpicture" response
 *
 * @param connection a valid and connected #mpd_connection
 * @param buffer a allocated struct mpd_readpicture
 * @return true on success
 */
bool
mpd_recv_readpicture(struct mpd_connection *connection, struct mpd_readpicture *buffer);

/**
 * Shortcut for mpd_send_readpicture(), mpd_recv_readpicture() and
 * mpd_response_finish().
 *
 * @param connection a valid and connected #mpd_connection
 * @param uri the URI of the song
 * @param offset to read from
 * @param buffer a allocated struct mpd_readpicture
 * @return true on success
 */
bool
mpd_run_readpicture(struct mpd_connection *connection,
				   const char *uri,
				   unsigned offset,
				   struct mpd_readpicture *buffer);

#ifdef __cplusplus
}
#endif

#endif
