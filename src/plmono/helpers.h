#ifndef _PLMONO_HELPERS_H
#define _PLMONO_HELPERS_H

MonoClass* mono_class_find(MonoImage *image, char *sig);
MonoMethod* mono_method_find(MonoClass *klass, char *method_name, MonoType 
**argTypes, int nargs);

#endif
