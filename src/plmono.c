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

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum plmono_call_handler(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(plmono_call_handler);

extern Datum plmono_validator(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(plmono_validator);

Datum plmono_regular_handler(PG_FUNCTION_ARGS);
Form_pg_type getTypeFromOid(Oid type_oid);
void getFunctionFromOid(Oid fn_oid, Form_pg_proc *p_procStruct, Oid **p_argtypes, char ***p_argnames, char **p_argmodes, int *argcount);
Datum getFunctionSourceFromOid(Oid fn_oid);
MonoClass* getMonoClass(char *class_name);
MonoMethod* getMonoMethod(MonoClass *klass, char *method_name, MonoType **argTypes, int nargs);
void* convertDatumToMonoType(Datum val, Oid type_oid);
void releaseMonoValue(void *val);
Datum convertMonoTypeToDatum(void *mono_val, Oid type_oid);
MonoClass* typeOidToMonoClass(Oid type_oid);
MonoMethod* findMonoMethod(char *sig, MonoType **argTypes, int nargs);

static MonoDomain *domain = NULL;
char *assembly_filename = "/home/kynlem/Projects/PLMono/debug/Test.dll";

Form_pg_type
getTypeFromOid(Oid type_oid)
{
	HeapTuple typeTup;
	Form_pg_type typeStruct;

	typeTup = SearchSysCache(TYPEOID, ObjectIdGetDatum(type_oid), 0, 0, 0);
	if (!HeapTupleIsValid(typeTup))
		elog(ERROR, "cache lookup failed for type %u", type_oid);

	typeStruct = (Form_pg_type) GETSTRUCT(typeTup);
	ReleaseSysCache(typeTup);
	
	return typeStruct;
}

void
getFunctionFromOid(Oid fn_oid, Form_pg_proc *p_procStruct, Oid **p_argtypes, char ***p_argnames, char **p_argmodes, int *argcount)
{
	HeapTuple procTup;

	procTup = SearchSysCache(PROCOID, ObjectIdGetDatum(fn_oid), 0, 0, 0);
	if (!HeapTupleIsValid(procTup))
		elog(ERROR, "cache lookup failed for function %u", fn_oid);

	*p_procStruct = (Form_pg_proc) GETSTRUCT(procTup);
  	ReleaseSysCache(procTup);

	*argcount = get_func_arg_info(procTup, p_argtypes, p_argnames, p_argmodes);
}

Datum
getFunctionSourceFromOid(Oid fn_oid)
{
	HeapTuple procTup;
	Datum procSourceDatum;
	bool isnull;

	procTup = SearchSysCache(PROCOID, ObjectIdGetDatum(fn_oid), 0, 0, 0);
	if (!HeapTupleIsValid(procTup))
		elog(ERROR, "cache lookup failed for function %u", fn_oid);

	procSourceDatum = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_prosrc, &isnull);
	if (isnull)
	{
		elog(ERROR, "'AS' clause of Mono function cannot be NULL'");
	}

	// Sure?
  	ReleaseSysCache(procTup);

	return procSourceDatum;
}

MonoMethod*
getMonoMethod(MonoClass *klass, char *method_name, MonoType **argTypes, int nargs)
{
	MonoMethod *method = NULL;
	MonoType *param_type;
	MonoMethodSignature *method_sig;
	gpointer method_iter = NULL;
	gpointer param_iter = NULL;
	int param_count, i;

	while ((method = mono_class_get_methods(klass, &method_iter)))
	{
		if (strcmp(mono_method_get_name(method), method_name))
		{
			continue;
		}

		method_sig = mono_method_signature(method);

		param_count = mono_signature_get_param_count(method_sig);
		if (param_count != nargs)
		{
			continue;
		}

		i = 0;
		while ((param_type = mono_signature_get_params(method_sig, &param_iter)))
		{
			if (mono_class_from_mono_type(param_type) != mono_class_from_mono_type(argTypes[i]))
			{
				break;
			}

			if (mono_type_is_byref(param_type) != mono_type_is_byref(argTypes[i]))
			{
				break;
			}

			i++;
		}
		
		if (i == nargs)
		{
			return method;
		}
	}

	return NULL;
}

MonoClass*
getMonoClass(char *class_name)
{
	MonoAssembly *assembly;
	MonoImage *image;
	MonoClass *class;
	MonoImageOpenStatus status;
	char *filename = "/home/kynlem/Projects/PLMono/debug/Test.dll";
	char *assembly_name = "PLMono";

	if (domain == NULL)
	{
		domain = mono_jit_init(filename);
	}

	assembly = mono_assembly_open(filename, &status);
	if (assembly == NULL)
	{
   		elog(ERROR, "Assembly %s not found", filename);
	}

	image = mono_assembly_get_image(assembly);

	class = mono_class_from_name(image, assembly_name, class_name);
	if (class == NULL)
	{
   		elog(ERROR, "Class %s not found", class_name);
	}

	return class;
}

MonoMethod*
findMonoMethod(char *sig, MonoType **argTypes, int nargs)
{
	char *namespace_name = NULL;
	char *class_name = (char*) palloc(strlen(sig) + 1);
	char *method_name = (char*) palloc(strlen(sig) + 1);
	char *p = strchr(sig, '.') + 1;
	char *p2 = strchr(p + 1, '.') + 1;
	MonoClass *klass;
	MonoMethod *method;

	namespace_name = (char*) palloc(strlen(sig) + 1);
	class_name = (char*) palloc(strlen(sig) + 1);
	method_name = (char*) palloc(strlen(sig) + 1);

	strncpy(namespace_name, sig, p - sig);
	namespace_name[p - sig - 1] = '\0';
	strncpy(class_name, p, p2 - p - 1);
	class_name[p2 - p - 1] = '\0';
	strcpy(method_name, p2);

	klass = getMonoClass(class_name);
	return getMonoMethod(klass, method_name, argTypes, nargs);
}

void*
convertDatumToMonoType(Datum val, Oid type_oid)
{
	void *mono_val = NULL;

	switch (type_oid)
	{
		case BOOLOID:
			mono_val = (int32*) palloc(sizeof(int32));
			*((int32*) mono_val) = DatumGetBool(val);
			break;

		case INT2OID:
			mono_val = (int16*) palloc(sizeof(int16));
			*((int16*) mono_val) = DatumGetInt16(val);
			break;

		case INT4OID:
			mono_val = (int32*) palloc(sizeof(int32));
			*((int32*) mono_val) = DatumGetInt32(val);
			break;

		case INT8OID:
			mono_val = (int64*) palloc(sizeof(int64));
			*((int64*) mono_val) = DatumGetInt64(val);
			break;

		case FLOAT4OID:
			mono_val = (float4*) palloc(sizeof(float4));
			*((float4*) mono_val) = DatumGetFloat4(val);
			break;

		case FLOAT8OID:
			mono_val = (float8*) palloc(sizeof(float8));
			*((float8*) mono_val) = DatumGetFloat8(val);
			break;

		case TEXTOID:
			mono_val = (MonoString*) palloc(sizeof(MonoString));
			mono_val = mono_string_new(domain, TextDatumGetCString(val));
			//mono_gchandle_new(mono_val, true);
			break;
	}

	return mono_val;
}

void
releaseMonoValue(void *val)
{
	if (val != NULL)
		pfree(val);
}

Datum
convertMonoTypeToDatum(void *mono_val, Oid type_oid)
{
	Datum val = Int32GetDatum(0);

	switch (type_oid)
	{
		case BOOLOID:
			val = BoolGetDatum(*((int32*) mono_val));
			break;

		case INT2OID:
			val = Int16GetDatum(*((int16*) mono_val));
			break;

		case INT4OID:
			val = Int32GetDatum(*((int32*) mono_val));
			break;

		case INT8OID:
			val = Int64GetDatum(*((int64*) mono_val));
			break;

		case FLOAT4OID:
			val = Float4GetDatum(*((float4*) mono_val));
			break;

		case FLOAT8OID:
			val = Float8GetDatum(*((float8*) mono_val));
			break;

		case TEXTOID:
			val = CStringGetTextDatum(mono_string_to_utf8((MonoString*) mono_val));
			break;

		default:
			elog(ERROR, "TypeOID: %d CString: %s", type_oid, mono_string_to_utf8((MonoString*) mono_val));
	}

	return val;
}

MonoClass*
typeOidToMonoClass(Oid type_oid)
{
	MonoClass *mclass = NULL;

	switch (type_oid)
	{
		case BOOLOID:
			mclass = mono_get_boolean_class();
			break;

		case INT2OID:
			mclass = mono_get_int16_class();
			break;

		case INT4OID:
			mclass = mono_get_int32_class();
			//elog(ERROR, "Setting mclass to %p", mclass);
			break;

		case INT8OID:
			mclass = mono_get_int64_class();
			break;

		case FLOAT4OID:
			mclass = mono_get_single_class();
			break;

		case FLOAT8OID:
			mclass = mono_get_double_class();
			break;

		case TEXTOID:
			mclass = mono_get_string_class();
			break;
	}

	return mclass;
}

Datum
plmono_regular_handler(PG_FUNCTION_ARGS)
{
	Datum d;
	MonoMethod *method;
	MonoObject *result;
	MonoType **argTypes;
	Form_pg_proc procStruct;
	TupleDesc resultTupleDesc;

	Oid *argtypes;
	char **argnames;
	char *argmodes;
	int argcount;
	
	gpointer args[FUNC_MAX_ARGS];
	Datum *ret_vals;
	int retval_count;
	bool *nulls;
	HeapTuple ret_tuple;
	Datum retval;
	Datum procSource;
	TypeFuncClass call_res_type;
	char *procSourceChar;
	int i;
	void *p;

	getFunctionFromOid(fcinfo->flinfo->fn_oid, &procStruct, &argtypes, &argnames, &argmodes, &argcount);
	procSource = getFunctionSourceFromOid(fcinfo->flinfo->fn_oid);

	procSourceChar = pstrdup(DatumGetCString(DirectFunctionCall1(textout, procSource)));
	
	if (domain == NULL)
	{
		domain = mono_jit_init(assembly_filename);
	}

	argTypes = (MonoType**) palloc(fcinfo->nargs * sizeof(MonoType*));
	for (i = 0; i < argcount; i++)
	{
		if (argmodes != NULL && (argmodes[i] == PROARGMODE_INOUT || argmodes[i] == PROARGMODE_OUT))
		{
			argTypes[i] = mono_class_get_byref_type(typeOidToMonoClass(argtypes[i]));
		}
		else
		{
			argTypes[i] = mono_class_get_type(typeOidToMonoClass(argtypes[i]));
		}
	}

	method = findMonoMethod(procSourceChar, argTypes, argcount); //"PLMono.Test.Func"

	for (i = 0; i < argcount; i++)
	{
		args[i] = convertDatumToMonoType(fcinfo->arg[i], argtypes[i]);
		if ((argmodes != NULL) && (argmodes[i] != PROARGMODE_IN) && (argtypes[i] == TEXTOID)) //(i == 3)
		{
			p = palloc(sizeof(void*));
			p = args[i];
			args[i] = &p;
		}
	}

	result = mono_runtime_invoke(method, NULL, args, NULL);

	if (argmodes == NULL)
	{
		retval = convertMonoTypeToDatum(mono_object_unbox(result), procStruct->prorettype);
		return retval;
	}

	call_res_type = get_call_result_type(fcinfo, NULL, &resultTupleDesc);

	ret_vals = palloc(resultTupleDesc->natts * sizeof(Datum));
	nulls = palloc(resultTupleDesc->natts * sizeof(bool));

	retval_count = 0;
	for (i = 0; i < argcount; i++)
	{
		if (argmodes[i] == PROARGMODE_OUT || argmodes[i] == PROARGMODE_INOUT)
		{
			if (argtypes[i] != TEXTOID)
			{
				ret_vals[retval_count] = convertMonoTypeToDatum(args[i], argtypes[i]);
			}
			else
			{
				ret_vals[retval_count] = convertMonoTypeToDatum(*((void**) args[i]), argtypes[i]);
			}
			nulls[retval_count] = 0;
			retval_count++;
		}

		//
		// TODO: releaseMonoValue(args[i]);
		//
	}

	if (call_res_type == TYPEFUNC_SCALAR)
	{
		return ret_vals[0];
	}

	if (call_res_type != TYPEFUNC_COMPOSITE)
	{
  		elog(ERROR, "Function must have [IN]OUT arguments");
	}

	//BlessTupleDesc(resultTupleDesc);

	ret_tuple = heap_form_tuple(resultTupleDesc, ret_vals, nulls);
	d = HeapTupleGetDatum(ret_tuple);

	return d;
}

Datum
plmono_call_handler(PG_FUNCTION_ARGS)
{
	if (CALLED_AS_TRIGGER(fcinfo))
	{
  		elog(ERROR, "Feature not implemented");
	}

	return plmono_regular_handler(fcinfo);
}

Datum
plmono_validator(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(0);
}
