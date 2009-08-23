#ifndef _PLMONO_FUNCTION_H
#define _PLMONO_FUNCTION_H

MonoType** plmono_func_build_param_types(FunctionCallInfo fcinfo, Oid *argtypes, char *argmodes, int argcount);
void plmono_func_build_args(FunctionCallInfo fcinfo, Oid *argtypes, char *argmodes, int nparams, gpointer **pparams);
Datum plmono_func_build_result(FunctionCallInfo fcinfo, Form_pg_proc procStruct, Oid *argtypes, char *argmodes, int argcount, gpointer *params, MonoObject *result);
Datum plmono_func_build_out_args(FunctionCallInfo fcinfo, TupleDesc resultTupleDesc, Oid *argtypes, char *argmodes, int argcount, gpointer *params);
Datum plmono_func_handler(PG_FUNCTION_ARGS);

#endif
