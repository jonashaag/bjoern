#include "stream.h"

static PyObject*
_Stream_read(Stream* stream, size_t howmuch)
{
  size_t bytesleft;
  PyObject* chunk;
  if(stream->data.data == NULL)
    return NULL;
  bytesleft = stream->data.len - stream->pos;
  if(howmuch > bytesleft)
    howmuch = bytesleft;
  if(howmuch == 0)
    return NULL;
  chunk = PyString_FromStringAndSize(stream->data.data + stream->pos, howmuch);
  stream->pos += howmuch;
  return chunk;
}

static PyObject*
Stream_read(PyObject* self, PyObject* args)
{
  size_t howmuch;
  if(!PyArg_ParseTuple(args, "I", &howmuch))
    return NULL;
  PyObject* chunk = _Stream_read((Stream*)self, howmuch);
  if(chunk == NULL) {
    Py_INCREF(_empty_string);
    chunk = _empty_string;
  }
  return chunk;
}

static PyObject*
Stream_notimplemented(PyObject* self, PyObject* args)
{
  PyErr_SetString(PyExc_NotImplementedError,
    "Nobody would ever use readline(s) on a stream of random bytes, right?");
  return NULL;
}

static PyObject*
Stream_iternew(PyObject* self) {
  Py_INCREF(self);
  return self;
}

static PyObject*
Stream_iternext(PyObject* self)
{
  return _Stream_read((Stream*)self, STREAM_CHUNK_SIZE);
}

static void
Stream_dealloc(PyObject* self)
{
  free(((Stream*)self)->data.data);
  self->ob_type->tp_free(self);
}

static PyMethodDef Stream_methods[] = {
  {"read", Stream_read, METH_VARARGS, NULL},
  {"readline", Stream_notimplemented, METH_NOARGS, NULL},
  {"readlines", Stream_notimplemented, METH_VARARGS, NULL},
  {NULL, NULL, 0, NULL}
};

PyTypeObject StreamType = {
  PyObject_HEAD_INIT(NULL)
  0,
  "StringStream",
  sizeof(Stream),
  0,
  Stream_dealloc,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  Py_TPFLAGS_HAVE_CLASS | Py_TPFLAGS_HAVE_ITER,
  0, 0, 0, 0, 0,
  Stream_iternew,
  Stream_iternext,
  Stream_methods
};
