
#ifndef ___egg_marshal_MARSHAL_H__
#define ___egg_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:OBJECT,OBJECT (eggmarshalers.list:1) */
extern void _egg_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:OBJECT,STRING,LONG,LONG (eggmarshalers.list:2) */
extern void _egg_marshal_VOID__OBJECT_STRING_LONG_LONG (GClosure     *closure,
                                                        GValue       *return_value,
                                                        guint         n_param_values,
                                                        const GValue *param_values,
                                                        gpointer      invocation_hint,
                                                        gpointer      marshal_data);

/* VOID:OBJECT,LONG (eggmarshalers.list:3) */
extern void _egg_marshal_VOID__OBJECT_LONG (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:OBJECT,STRING,STRING (eggmarshalers.list:4) */
extern void _egg_marshal_VOID__OBJECT_STRING_STRING (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

/* BOOLEAN:VOID (eggmarshalers.list:5) */
extern void _egg_marshal_BOOLEAN__VOID (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);

/* OBJECT:VOID (eggmarshalers.list:6) */
extern void _egg_marshal_OBJECT__VOID (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

G_END_DECLS

#endif /* ___egg_marshal_MARSHAL_H__ */

