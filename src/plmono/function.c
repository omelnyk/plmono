/*-------------------------------------------------------------------------
 *
 * function.c
 *     non-trigger function call handler and routines used exclusively by it 
 *
 * Copyright (c) 2009, Olexandr Melnyk <me@omelnyk.net>
 *
 *------------------------------------------------------------------------- 
 */

#include "postgres.h"
#include "executor/spi.h"
#include "commands/trigger.h"
#include "fmgr.h"
#include "access/heapam.h"
#include "utils/syscache.h"
#include "utils/builtins.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "string.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/appdomain.h>

#include "helpers.h"
#include "core.h"
#include "function.h"

/*
 * plmono_func_build_param_types
 *
 *     Build an array of function argument classes 
 */
MonoType**
plmono_func_build_param_types(FunctionCallInfo fcinfo, Oid *argtypes, char *argmodes, int argcount)
{
	MonoType** params;
	int i;

	params = (MonoType**) palloc(fcinfo->nargs * sizeof(MonoType*));
	for (i = 0; i < argcount; i++)
	{
		MonoClass *klass = plmono_typeoid_to_class(argtypes[i]);
		if (argmodes == NULL || argmodes[i] == PROARGMODE_IN)
			params[i] = mono_class_get_type(klass);
		else /* PROARGMODE_INOUT or PROARGMODE_OUT */
			params[i] = mono_class_get_byref_type(klass);
	}

	return params;
}

/*
 * plmono_func_build_args
 *
 *     Convert function arguments into a form suitable for calling a Mono method
 */
void
plmono_func_build_args(FunctionCallInfo fcinfo, Oid *argtypes, char *argmodes, int nparams, gpointer **pparams)
{
	void **p;
	int i;

	if (!(*pparams = palloc(nparams * sizeof(gpointer))))
		elog(ERROR, "Not enough memory");

	for (i = 0; i < nparams; i++)
	{
		(*pparams)[i] = plmono_datum_to_obj(fcinfo->arg[i], argtypes[i]);

		if (argmodes == NULL)
			continue;

		if ((argmodes[i] != PROARGMODE_IN) && (argtypes[i] == TEXTOID))
		{
			if (!(p = palloc(sizeof(void*))))
				elog(ERROR, "Not enough memory");

			*p = (*pparams)[i];
			(*pparams)[i] = p;
		}
	}
}

/*
 * plmono_func_build_out_args
 *
 *     Build a Datum based on INOUT and OUT argument values of a function. If
 *     there is only one such argument, its value will be returned; else, all
 *     INOUT and OUT arguments will be grouped in a tuple
 */
Datum
plmono_func_build_out_args(FunctionCallInfo fcinfo, TupleDesc resultTupleDesc, Oid *argtypes, char *argmodes, int argcount, gpointer *params)
{
	HeapTuple rettuple;
	Datum *retvals;
	bool *nulls;
	int nretvals, i;
	void *p;

	if (!(retvals = palloc(resultTupleDesc->natts * sizeof(Datum))))
		elog(ERROR, "Not enough memory");

	if (!(nulls = palloc(resultTupleDesc->natts * sizeof(bool))))
		elog(ERROR, "Not enough memory");

	nretvals = 0;
	for (i = 0; i < argcount; i++)
	{
		if (argmodes[i] != PROARGMODE_IN)
		{
			if (argtypes[i] != TEXTOID)
				p = params[i];
			else
				p = *((void**) params[i]);

			retvals[nretvals] = plmono_obj_to_datum(p, argtypes[i]);
			nulls[nretvals] = 0;
			nretvals++;
		}
	}
	
	if (nretvals == 1)
		return retvals[0];

	rettuple = heap_form_tuple(resultTupleDesc, retvals, nulls);
	return HeapTupleGetDatum(rettuple);
}

/*
 * plmono_func_build_result
 *
 *     Build function's result. If function has INOUT or OUT arguments, result
 *     is based on values of corresponding ref parameters; otherwise, result is
 *     set to function's return value.  
 */
Datum
plmono_func_build_result(FunctionCallInfo fcinfo, Form_pg_proc procStruct, Oid *argtypes, char *argmodes, int argcount, gpointer *params, MonoObject *result)
{
	TupleDesc resultTupleDesc;
	TypeFuncClass call_res_type = get_call_result_type(fcinfo, NULL, &resultTupleDesc);

	if (argmodes == NULL)
	{
		if (call_res_type == TYPEFUNC_SCALAR)
			return plmono_obj_to_datum(mono_object_unbox(result), procStruct->prorettype);
		else
			elog(ERROR, "Multiple values can be returned only using OUT arguments");
	}

	return plmono_func_build_out_args(fcinfo, resultTupleDesc, argtypes, argmodes, argcount, params);
}

/*
 * plmono_func_handler
 *
 *     Non-trigger function call hanler
 */
Datum
plmono_func_handler(PG_FUNCTION_ARGS)
{
	MonoImage *image;
	MonoMethod *method;
	MonoObject *result;
	MonoType **paramtypes;
	Form_pg_proc procStruct;
	MonoClass *klass;
	char *assembly, *method_name, *sig;

	Oid *argtypes;
	char **argnames;
	char *argmodes;
	int argcount;
	
	gpointer *args;
	char *source;

	/*
     * Get characteristics of called function and parse its body
     */
	plmono_lookup_pg_function(fcinfo->flinfo->fn_oid, &procStruct, &source, 
	    &argtypes, &argnames, &argmodes, &argcount);
	plmono_parse_function_body(source, &assembly, &sig, &method_name);

	/*
     * Find corresponding Mono method
     */
	paramtypes = plmono_func_build_param_types(fcinfo, argtypes, argmodes, argcount);
	image = plmono_image_open(assembly);
	klass = plmono_class_find(image, sig);
	method = plmono_method_find(klass, method_name, paramtypes, argcount);

	/*
     * Prepare arguments for method invokation
     */
	plmono_func_build_args(fcinfo, argtypes, argmodes, argcount, &args);

	/*
     * Invoke method
     */
	result = mono_runtime_invoke(method, NULL, args, NULL);

	/*
     * Return method's return value or arguments passed by reference
     */
	return plmono_func_build_result(fcinfo, procStruct, argtypes, argmodes, argcount, args, result);
}

