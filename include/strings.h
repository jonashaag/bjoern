#ifdef INIT_PYSTRINGS

    #define INIT_PYSTRING(s) PY_STRING_##s = PyString(#s); Py_INCREF(PY_STRING_##s)
    INIT_PYSTRING(  GET                 );
    INIT_PYSTRING(  POST                );
    INIT_PYSTRING(  REQUEST_METHOD      );
    INIT_PYSTRING(  PATH_INFO           );
    INIT_PYSTRING(  QUERY_STRING        );
    INIT_PYSTRING(  HTTP_CONTENT_TYPE   );
    INIT_PYSTRING(  CONTENT_TYPE        );
    INIT_PYSTRING(  SERVER_NAME         );
    INIT_PYSTRING(  SERVER_PORT         );
    INIT_PYSTRING(  SERVER_PROTOCOL     );
    INIT_PYSTRING(  start_response      );
    INIT_PYSTRING(  response_headers    );
    INIT_PYSTRING(  response_status     );

    PY_STRING_404_NOT_FOUND = PyString("404 Not Found");
    PY_STRING_500_INTERNAL_SERVER_ERROR = PyString("500 Internal Server Error");

    Py_INCREF(PY_STRING_start_response);

#else

    /* Some predefined, often-required objects. */
    static PyObject* PY_STRING_GET;
    static PyObject* PY_STRING_POST;
    static PyObject* PY_STRING_REQUEST_METHOD;
    static PyObject* PY_STRING_PATH_INFO;
    static PyObject* PY_STRING_QUERY_STRING;
    static PyObject* PY_STRING_HTTP_CONTENT_TYPE;
    static PyObject* PY_STRING_CONTENT_TYPE;
    static PyObject* PY_STRING_SERVER_NAME;
    static PyObject* PY_STRING_SERVER_PORT;
    static PyObject* PY_STRING_SERVER_PROTOCOL;
    static PyObject* PY_STRING_404_NOT_FOUND;
    static PyObject* PY_STRING_500_INTERNAL_SERVER_ERROR;
    static PyObject* PY_STRING_start_response;
    static PyObject* PY_STRING_response_headers;
    static PyObject* PY_STRING_response_status;

#endif
