/**
 * common.h: common declarations.
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

#ifndef BJOERN_COMMON_H_
#define BJOERN_COMMON_H_ 1

#include <Python.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define GIL_LOCK(n) PyGILState_STATE _gilstate_##n = PyGILState_Ensure()
#define GIL_UNLOCK(n) PyGILState_Release(_gilstate_##n)

#define ADDR_FROM_MEMBER(ptr, strct, mem) (strct*)((size_t)ptr - (size_t)(&(((strct*)NULL)->mem)));

#define TYPECHECK2(what, check_type, print_type, errmsg_name, failure_retval) \
    if(!what || !check_type##_Check(what)) { \
        assert(Py_TYPE(what ? what : Py_None)->tp_name); \
        PyErr_Format(\
            PyExc_TypeError, \
            errmsg_name " must be of type %s, not %s", \
            print_type##_Type.tp_name, \
            Py_TYPE(what ? what : Py_None)->tp_name \
        ); \
        return failure_retval; \
    }
#define TYPECHECK(what, type, ...) TYPECHECK2(what, type, type, __VA_ARGS__);

typedef PyObject* PyKeywordFunc(PyObject* self, PyObject* args, PyObject *kwargs);

typedef enum {
    HTTP_BAD_REQUEST, HTTP_LENGTH_REQUIRED, HTTP_SERVER_ERROR
} http_status;

#ifdef DEBUG
    #define DBG_REQ(request, ...) \
        do { \
            printf("[DEBUG Req %ld] ", request->id); \
            DBG(__VA_ARGS__); \
        } while(0)
    #define DBG(...) \
        do { \
            printf(__VA_ARGS__); \
            printf("\n"); \
        } while(0)
#else
    #define DBG(...) do{}while(0)
    #define DBG_REQ(...) DBG(__VA_ARGS__)
#endif

#define DBG_REFCOUNT(obj) \
    DBG(#obj "->obj_refcnt: %d", obj->ob_refcnt)

#define DBG_REFCOUNT_REQ(request, obj) \
    DBG_REQ(request, #obj "->ob_refcnt: %d", obj->ob_refcnt)

#endif /* !BJOERN_COMMON_H_ */
