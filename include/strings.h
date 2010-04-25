#ifdef INIT_PYSTRINGS

    #define INIT_PYSTRING(s) PY_STRING_##s = PyString(#s); Py_INCREF(PY_STRING_##s)
    INIT_PYSTRING(   GET                 );
    INIT_PYSTRING(   POST                );
    INIT_PYSTRING(   REQUEST_METHOD      );
    INIT_PYSTRING(   PATH_INFO           );
    INIT_PYSTRING(   QUERY_STRING        );
    INIT_PYSTRING(   HTTP_CONTENT_TYPE   );
    INIT_PYSTRING(   CONTENT_TYPE        );
    INIT_PYSTRING(   SERVER_NAME         );
    INIT_PYSTRING(   SERVER_PORT         );
    INIT_PYSTRING(   SERVER_PROTOCOL     );

    PY_STRING_Content_Type   = PyString("Content-Type");
    PY_STRING_Content_Length = PyString("Content-Length");
    PY_STRING_start_response = PyString("start_response");
    PY_STRING_DEFAULT_RESPONSE_CONTENT_TYPE = PyString(DEFAULT_RESPONSE_CONTENT_TYPE);

    Py_INCREF(PY_STRING_Content_Length);
    Py_INCREF(PY_STRING_Content_Type);
    Py_INCREF(PY_STRING_start_response);
    Py_INCREF(PY_STRING_DEFAULT_RESPONSE_CONTENT_TYPE);

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
    static PyObject* PY_STRING_Content_Type;    /* "Content-Type"   */
    static PyObject* PY_STRING_Content_Length;  /* "Content-Length" */
    static PyObject* PY_STRING_start_response;  /* "start_response" */
    static PyObject* PY_STRING_DEFAULT_RESPONSE_CONTENT_TYPE; /* DEFAULT_RESPONSE_CONTENT_TYPE */

#endif
