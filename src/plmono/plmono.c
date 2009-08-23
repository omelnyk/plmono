/*-------------------------------------------------------------------------
 *
 * plmono.c
 *     PL/Mono: a procedural language for PostgreSQL based on Mono
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

#include "core.h"
#include "function.h"
#include "trigger.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum plmono_call_handler(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(plmono_call_handler);

extern Datum plmono_validator(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(plmono_validator);

Datum plmono_trigger_handler(PG_FUNCTION_ARGS);

/*
 * Call handler for both trigger and non-trigger functions
 */
Datum
plmono_call_handler(PG_FUNCTION_ARGS)
{
	plmono_warm_up();

	if (CALLED_AS_TRIGGER(fcinfo))
		return plmono_trigger_handler(fcinfo);
	else
		return plmono_func_handler(fcinfo);
}

/*
 * Function validator
 */
Datum
plmono_validator(PG_FUNCTION_ARGS)
{
	/*
     * All checks are deferred to method invokation
     */

	PG_RETURN_INT32(0);
}
