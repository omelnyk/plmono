/*-------------------------------------------------------------------------
 *
 * core.c
 *     various routines used by function and trigger call handlers 
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

/*
 * AppDomain of PL/Mono backend
 */
static MonoDomain *domain = NULL;

/*
 * Image of PLMono assembly 
 */
static MonoImage *plmono_image = NULL;

/*
 * TODO: Remove this!
 */
static char *plmono_assembly = "/home/kynlem/Projects/PLMono/src/classes/PLMono/PLMono.dll";

/*
 * plmono_get_domain
 *
 *     Get AppDomain of PL/Mono backend
 */
MonoDomain*
plmono_get_domain(void)
{
	return domain;
}

/*
 * plmono_get_plmono_image
 *
 *     Get image of PLMono assembly
 */
MonoImage*
plmono_get_plmono_image(void)
{
	return plmono_image;
}

/*
 * plmono_get_corlib_image
 *
 *     Get image of corlib assembly
 */
MonoImage*
plmono_get_corlib_image(void)
{
	return mono_get_corlib();
}

/*
 * plmono_warm_up
 *
 *     Warm up PL/Mono backend: initialize Mono JIT and load images of required
 *     assemblies
 */
void
plmono_warm_up(void)
{
	if (!domain)
		domain = mono_jit_init_version("plmono", "v2.0.50727");

	if (!domain)
		elog(ERROR, "Cannot initialize Mono JIT");

	if (!plmono_image)
		plmono_image = plmono_image_open(plmono_assembly);
}

/*
 * plmono_image_open
 *
 *     Try to load specified assembly and get its image, or report error on
 *     failure
 */
MonoImage*
plmono_image_open(const char *filename)
{
	MonoAssembly *assembly;
	MonoImageOpenStatus status;

	assembly = mono_assembly_open(filename, &status);
	if (!assembly)
   		elog(ERROR, "Assembly %s not found", filename);

	return mono_assembly_get_image(assembly);
}

/*
 * plmono_class_from_name
 *
 *     Get class by its namespace and name, or report error if such class
 *     doesn't exist
 */
MonoClass*
plmono_class_from_name(MonoImage *image, const char *namespace, const char *name)
{
	MonoClass *klass;

	if (!(klass = mono_class_from_name(image, namespace, name)))
   		elog(ERROR, "Class %s not found", name);

	return klass;
}

/*
 * plmono_class_find
 *
 *     Get class by its signature, or report error if such class doesn't exist
 */
MonoClass*
plmono_class_find(MonoImage *image, char *sig)
{
	MonoClass *klass;

	if (!(klass = mono_class_find(image, sig)))
   		elog(ERROR, "Class %s not found", sig);

	return klass;
}

/*
 * plmono_method_find
 *
 *     Get method by its name and argument types, or report error if such method
 *     doesn't exist
 */
MonoMethod*
plmono_method_find(MonoClass *klass, char *name, MonoType **params, int nparams)
{
	MonoMethod *method;

	if (!(method = mono_method_find(klass, name, params, nparams)))
   		elog(ERROR, "Method %s with specified signature not found", name);

	return method;
}

/*
 * plmono_parse_function_body
 *
 *     Extract assembly name, class signature and method name from function body
 */
void
plmono_parse_function_body(char *body, char **passembly, char **psig, char **pmethod)
{
	char *p;

	/*
     * Extract assembly name
     */

	if (!(p = strchr(body, ',')))
		elog(ERROR, "No assembly name specified");

	if (!(*passembly = (char*) palloc(p - body + 1)))
		elog(ERROR, "Not enough memory");

	strncpy(*passembly, body, p - body);
	(*passembly)[p - body] = '\0';

	/*
     * Extract class signature and method name
     */

	body = p + 1;
	while (*body == ' ')
		body++;

	if (!(p = strchr(body, ':')))
		elog(ERROR, "No method name specified");
		
	if (!(*psig = (char*) palloc(p - body + 1)))
		elog(ERROR, "Not enough memory");

	if (!(*pmethod = (char*) palloc(strlen(p))))
		elog(ERROR, "Not enough memory");

	strncpy(*psig, body, p - body);
	(*psig)[p - body] = '\0';
	strcpy(*pmethod, p + 1);
}

/*
 * plmono_lookup_pg_function
 *
 *     Get information about function by its Oid
 */
void
plmono_lookup_pg_function(Oid fn_oid, Form_pg_proc *p_procStruct, char **psource, Oid **p_argtypes, char ***p_argnames, char **p_argmodes, int *p_argcount)
{
	HeapTuple procTup;
	Datum sourceDatum;
	bool isnull;
	int nargs;

	procTup = SearchSysCache(PROCOID, ObjectIdGetDatum(fn_oid), 0, 0, 0);
	if (!HeapTupleIsValid(procTup))
		elog(ERROR, "Cache lookup failed for function %u", fn_oid);

	if (!p_argtypes)
		if (!(p_argtypes = palloc(sizeof(Oid**))))
			elog(ERROR, "Not enough memory");

	if (!p_argnames)
		if (!(p_argnames = palloc(sizeof(char**))))
			elog(ERROR, "Not enough memory");

	if (!p_argmodes)
		if (!(p_argmodes = palloc(sizeof(char**))))
			elog(ERROR, "Not enough memory");

	if (p_procStruct)
		*p_procStruct = (Form_pg_proc) GETSTRUCT(procTup);

	nargs = get_func_arg_info(procTup, p_argtypes, p_argnames, p_argmodes);

	if(p_argcount)
		*p_argcount = nargs;

	sourceDatum = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_prosrc, &isnull);
	if (isnull)
		elog(ERROR, "'AS' clause of Mono function cannot be NULL'");

	*psource = pstrdup(DatumGetCString(DirectFunctionCall1(textout, sourceDatum)));
	
  	ReleaseSysCache(procTup);
}

/*
 * plmono_datum_to_obj
 *
 *     Convert Datum to corresponding Mono data type 
 */
void*
plmono_datum_to_obj(Datum val, Oid typeoid)
{
	void *obj = NULL;

	switch (typeoid)
	{
		case BOOLOID:
			obj = (int32*) palloc(sizeof(int32));
			*((int32*) obj) = DatumGetBool(val);
			break;

		case INT2OID:
			obj = (int16*) palloc(sizeof(int16));
			*((int16*) obj) = DatumGetInt16(val);
			break;

		case INT4OID:
			obj = (int32*) palloc(sizeof(int32));
			*((int32*) obj) = DatumGetInt32(val);
			break;

		case INT8OID:
			obj = (int64*) palloc(sizeof(int64));
			*((int64*) obj) = DatumGetInt64(val);
			break;

		case FLOAT4OID:
			obj = (float4*) palloc(sizeof(float4));
			*((float4*) obj) = DatumGetFloat4(val);
			break;

		case FLOAT8OID:
			obj = (float8*) palloc(sizeof(float8));
			*((float8*) obj) = DatumGetFloat8(val);
			break;

		case TEXTOID:
			obj = (MonoString*) palloc(sizeof(MonoString));
			obj = mono_string_new(domain, TextDatumGetCString(val));
			break;

		default:
			elog(ERROR, "Data type with OID %d is not supported by PL/Mono", typeoid);
	}

	return obj;
}

/*
 * plmono_obj_to_datum
 *
 *     Convert Mono value or object to Datum 
 */
Datum
plmono_obj_to_datum(void *obj, Oid typeoid)
{
	Datum val = Int32GetDatum(0);

	switch (typeoid)
	{
		case BOOLOID:
			val = BoolGetDatum(*((int32*) obj));
			break;

		case INT2OID:
			val = Int16GetDatum(*((int16*) obj));
			break;

		case INT4OID:
			val = Int32GetDatum(*((int32*) obj));
			break;

		case INT8OID:
			val = Int64GetDatum(*((int64*) obj));
			break;

		case FLOAT4OID:
			val = Float4GetDatum(*((float4*) obj));
			break;

		case FLOAT8OID:
			val = Float8GetDatum(*((float8*) obj));
			break;

		case TEXTOID:
			val = CStringGetTextDatum(mono_string_to_utf8((MonoString*) obj));
			break;

		default:
			elog(ERROR, "Data type with OID %d is not supported by PL/Mono", typeoid);
	}

	return val;
}

/*
 * plmono_typeoid_to_class
 *
 *     Get Mono counterpart of Postgres data type 
 */
MonoClass*
plmono_typeoid_to_class(Oid typeoid)
{
	MonoClass *klass = NULL;

	switch (typeoid)
	{
		case BOOLOID:
			klass = mono_get_boolean_class();
			break;

		case INT2OID:
			klass = mono_get_int16_class();
			break;

		case INT4OID:
			klass = mono_get_int32_class();
			break;

		case INT8OID:
			klass = mono_get_int64_class();
			break;

		case FLOAT4OID:
			klass = mono_get_single_class();
			break;

		case FLOAT8OID:
			klass = mono_get_double_class();
			break;

		case TEXTOID:
			klass = mono_get_string_class();
			break;

		default:
			elog(ERROR, "Data type with OID %d is not supported by PL/Mono", typeoid);
	}

	return klass;
}

