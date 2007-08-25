#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* include this first, before NO_IMPORT_PYGOBJECT is defined */
#include <pygobject.h>

void pygtkmozembed_register_classes (PyObject *d);
void pygtkmozembed_add_constants(PyObject *module, const gchar *strip_prefix);

extern PyMethodDef pygnomegeckoembed_functions[];

DL_EXPORT(void)
initgnomegeckoembed(void)
{
    PyObject *m, *d;

    init_pygobject ();

    m = Py_InitModule ("gnomegeckoembed", pygnomegeckoembed_functions);
    d = PyModule_GetDict (m);

    pygnomegeckoembed_register_classes (d);
    /*pygnomegeckoembed_add_constants(m, "GNOME_GECKO_EMBED_");*/

    if (PyErr_Occurred ()) {
        Py_FatalError ("can't initialise module gnomegeckoembed");
    }
}
