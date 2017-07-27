#define Py_TYPE(ob) (((PyObject*)(ob))->ob_type)
#define NOP do{}while(0)
#define PyFile_IncUseCount(file) NOP
#define PyFile_DecUseCount(file) NOP
#if PY_MAJOR_VERSION >= 3
#else
#define PyVarObject_HEAD_INIT(type, size) PyObject_HEAD_INIT(type) size,
#endif
