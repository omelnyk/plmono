#ifndef _PLMONO_CORE_H
#define _PLMONO_CORE_H

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/appdomain.h>

void plmono_warm_up(void);
MonoDomain* plmono_get_domain(void);
MonoImage* plmono_get_plmono_image(void);
MonoImage* plmono_get_corlib_image(void);
void plmono_parse_function_body(char *body, char **passembly, char **psig, char **pmethod);
MonoImage* plmono_image_open(const char *filename);
MonoClass* plmono_class_from_name(MonoImage *image, const char *namespace, const char *name);
MonoClass* plmono_class_find(MonoImage *image, char *sig);
MonoMethod* plmono_method_find(MonoClass *klass, char *name, MonoType **params, int nparams);
void plmono_lookup_pg_function(Oid fn_oid, Form_pg_proc *p_procStruct, char **psource, Oid **p_argtypes, char ***p_argnames, char **p_argmodes, int *p_argcount);
void* plmono_datum_to_obj(Datum val, Oid type_oid);
Datum plmono_obj_to_datum(void *mono_val, Oid type_oid);
MonoClass* plmono_typeoid_to_class(Oid type_oid);

#endif
