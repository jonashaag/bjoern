static bool
wsgi_sendfile_init(Transaction* transaction, PyFileObject* file)
{
    int file_descriptor;

    PyFile_IncUseCount(file);

    transaction->body = file;
    file_descriptor = PyObject_AsFileDescriptor((PyObject*)file);

    struct stat file_stat;
    if(fstat(file_descriptor, &file_stat) == -1) {
        /* an error occured */
        return false;
    } else {
        transaction->body_length = file_stat.st_size;

        /* Ensure the file's mime type is set. */
        if(true || !PyDict_Contains(transaction->headers, PYSTRING(Content_Type)))
        {
            const char* filename = PyString_AsString(PyFile_Name((PyObject*)file));
            const char* mimetype = get_mimetype(filename);
            if(mimetype == NULL)
                return false;
            /* the following is equivalent to the Python expression
                  headers = headers + (('Content-Type', the_mimetype),)
               hence, it concats a tuple containing 'Content-Type' as
               item 0 and the_mimetype as item 1 to the headers tuple.
            */
            PyObject* inner_tuple = PyTuple_Pack(/* size */ 2, PYSTRING(Content_Type), PyString_FromString(mimetype));
            PyObject* outer_tuple = PyTuple_Pack(/* size */ 1, inner_tuple);
            transaction->headers = PyNumber_Add(transaction->headers, outer_tuple);
            /* `PyNumber_Add` isn't restricted to numbers at all but just represents
               a Python '+'.  Fuck the Python C API! */
            return transaction->headers != NULL;
        }
        else {
            return true;
        }
    }
}

static response_status
wsgi_sendfile(Transaction* transaction)
{
    int file_descriptor;
    response_status return_value;

    GIL_LOCK();

    file_descriptor = PyObject_AsFileDescriptor(transaction->body);
    //DEBUG("Continue sendfile() , %d bytes left to send.", transaction->body_length);
    ssize_t bytes_sent = sendfile(
        transaction->client_fd,
        file_descriptor,
        NULL /* offset=NULL: let sendfile() manage the file offset */,
        transaction->body_length
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

    transaction->body_length -= bytes_sent;

    if(bytes_sent == 0 || transaction->body_length == 0) {
        /* Everything sent */
        return_value = RESPONSE_FINISHED;
        goto close_connection;
    } else {
        /* There's data left to send */
        return_value = RESPONSE_NOT_YET_FINISHED;
        goto unlock_GIL_and_return;
    }

close_connection:
    PyFile_DecUseCount(transaction->body);
    Py_DECREF((PyObject*)transaction->body);
    goto unlock_GIL_and_return;

unlock_GIL_and_return:
    GIL_UNLOCK();
    return return_value;
}
