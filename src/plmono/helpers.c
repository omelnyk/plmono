/*-------------------------------------------------------------------------
 *
 * helpers.c
 *     extensions to Mono Embedded API
 *
 * Copyright (c) 2009, Olexandr Melnyk <me@omelnyk.net>
 *
 *------------------------------------------------------------------------- 
 */

#include <mono/jit/jit.h>
#include <string.h>

#include "helpers.h"

/*
 * mono_class_find
 *
 *     Get class by its signature
 */
MonoClass*
mono_class_find(MonoImage *image, char *sig)
{
	MonoClass *klass = NULL;
	char *p = sig;

	while (!klass && (p = strchr(p, '.')))
	{
		*p = '\0';
		klass = mono_class_from_name(image, sig, p + 1);
		*p = '.';
	}

	return klass;
}

/*
 * mono_method_find
 *
 *     Get method by its name and argument types
 */
MonoMethod*
mono_method_find(MonoClass *klass, char *name, MonoType **params, int nparams)
{
	MonoMethod *method = NULL;
	MonoType *param_type;
	MonoMethodSignature *sig;
	gpointer method_iter = NULL;
	gpointer param_iter = NULL;
	int i;

	while ((method = mono_class_get_methods(klass, &method_iter)))
	{
		if (strcmp(mono_method_get_name(method), name))
			continue;

		sig = mono_method_signature(method);

		if (mono_signature_get_param_count(sig) != nparams)
			continue;

		i = 0;
		while ((param_type = mono_signature_get_params(sig, &param_iter)))
		{
			if (mono_class_from_mono_type(param_type) != mono_class_from_mono_type(params[i]))
				break;

			if (mono_type_is_byref(param_type) != mono_type_is_byref(params[i]))
				break;

			i++;
		}
		
		if (i == nparams)
		{
			return method;
		}
	}

	return NULL;
}

