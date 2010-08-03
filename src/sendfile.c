#include "sendfile.h"

bool
wsgi_sendfile_init(Request* request, PyFileObject* file)
{
    int file_descriptor;

    PyFile_IncUseCount(file);

    request->response_body = (PyObject*)file;
    file_descriptor = PyObject_AsFileDescriptor((PyObject*)file);

    struct stat file_stat;
    if(fstat(file_descriptor, &file_stat) == -1) {
        PyObject* file_str = PyObject_Str((PyObject*)file);
        PyErr_Format(
            PyExc_OSError,
            "failed to fstat %s",
            file_str ? PyString_AS_STRING(file_str) : "<open file>"
        );
        return false;
    }

    request->response_body_length = file_stat.st_size;

    /* Ensure the file's mime type is set. */
    if(!PyDict_Contains(request->headers, _(Content_Type)))
    {
        c_char* filename = PyString_AsString(PyFile_Name((PyObject*)file));
        c_char* mimetype = get_mimetype(filename);
        if(mimetype == NULL)
            return false;

        /* the following is equivalent to the Python expression
              headers = headers + (('Content-Type', the_mimetype),)
           - Hence, it concats a tuple containing 'Content-Type' as
           item 0 and the_mimetype as item 1 to the headers tuple.
        */
        PyObject* inner_tuple = PyTuple_Pack(/* size */ 2, _(Content_Type), PyString_FromString(mimetype));
        PyObject* outer_tuple = PyTuple_Pack(/* size */ 1, inner_tuple);
        request->headers = PyNumber_Add(request->headers, outer_tuple);
        /* `PyNumber_Add` isn't restricted to numbers at all but just represents
           a Python '+'.  Fuck the Python C API! */
        return request->headers != NULL;
    }

    return true;
}

int
wsgi_sendfile(Request* request)
{
    int file_descriptor;
    int return_value;

    GIL_LOCK();

    file_descriptor = PyObject_AsFileDescriptor(request->response_body);
    //DEBUG("Continue sendfile() , %d bytes left to send.", request->response_body_length);
    ssize_t bytes_sent = sendfile(
        request->client_fd,
        file_descriptor,
        NULL /* offset=NULL: let sendfile() manage the file offset */,
        request->response_body_length
    );
    //DEBUG("sendfile(): sent %d bytes.", bytes_sent);
    if(bytes_sent == -1) {
        /* socket error [note 1] */
        if(errno == EAGAIN) {
            return_value = RESPONSE_NOT_YET_FINISHED; /* Try again next time. */
            goto unlock_GIL_and_return;
        }
        else {
            return_value = RESPONSE_SOCKET_ERROR_OCCURRED;
            goto close_connection;
        }
    }

    request->response_body_length -= bytes_sent;

    if(bytes_sent == 0 || request->response_body_length == 0) {
        /* Everything sent */
        return_value = RESPONSE_FINISHED;
        goto close_connection;
    } else {
        /* There's data left to send */
        return_value = RESPONSE_NOT_YET_FINISHED;
        goto unlock_GIL_and_return;
    }

close_connection:
    PyFile_DecUseCount((PyFileObject*)request->response_body);
    goto unlock_GIL_and_return;

unlock_GIL_and_return:
    GIL_UNLOCK();
    return return_value;
}
