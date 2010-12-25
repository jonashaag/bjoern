/**
 * request.h: Request types and functions declarations.
 * Copyright (c) 2010 Jonas Haag <jonas@lophus.org> and contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BJOERN_REQUEST_H_
#define BJOERN_REQUEST_H_ 1

#include <ev.h>
#include "http_parser.h"
#include "common.h"

void _request_module_initialize(const char* host, const int port);

typedef struct {
    unsigned error_code : 2;
    unsigned error : 1;
    unsigned parse_finished : 1;
    unsigned start_response_called : 1;
    unsigned headers_sent : 1;
} request_state;

typedef struct {
    http_parser parser;
    const char* field_start;
    size_t field_len;
    const char* value_start;
    size_t value_len;
} bj_parser;

typedef struct {
#ifdef DEBUG
    unsigned long id;
#endif
    request_state state;
    int client_fd;
    ev_io ev_watcher;

    bj_parser parser;
    PyObject* headers; /* rm. */
    PyObject* body;

    PyObject* current_chunk;
    Py_ssize_t current_chunk_p;
    PyObject* iterable;
    PyObject* status;
} Request;

Request* Request_new(int client_fd);
void Request_parse(Request*, const char*, const size_t);
void Request_free(Request*);


static PyObject
    * _PATH_INFO,
    * _QUERY_STRING,
    * _REQUEST_URI,
    * _HTTP_FRAGMENT,
    * _REQUEST_METHOD,
    * _wsgi_input,
    * _SERVER_PROTOCOL,
    * _GET,
    * _POST,
    * _CONTENT_LENGTH,
    * _CONTENT_TYPE,
    * _HTTP_1_1,
    * _HTTP_1_0,
    * _empty_string
;

#endif /* !BJOERN_REQUEST_H_ */
