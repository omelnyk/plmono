/*-------------------------------------------------------------------------
 *
 * trigger.c
 *     trigger function call handler and routines used exclusively by it 
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
#include "trigger.h"

/*
 * plmono_trigger_data_get_class
 *
 *     Get TriggerData class
 */
MonoClass*
plmono_trigger_data_get_class(void)
{
	MonoImage *image  = plmono_get_plmono_image();
	return plmono_class_from_name(image, "PLMono", "TriggerData");
}

/*
 * plmono_trigdata_get_columns
 *
 *     Get Columns property value of TriggerData class
 */
MonoObject*
plmono_trigdata_get_columns(MonoClass *trigdata)
{
	MonoVTable *vt;
	MonoProperty *prop;

	vt = mono_class_vtable(plmono_get_domain(), trigdata);
	mono_runtime_class_init(vt);
	prop = mono_class_get_property_from_name(trigdata, "Columns");
	return mono_property_get_value(prop, NULL, NULL, NULL);
}

/*
 * plmono_trigger_build_args
 *
 *     Create TableRow object based trigger's "OLD" row tuple
 */
void
plmono_trigger_build_args(TriggerData *trigdata, MonoObject *cols)
{
	MonoClass *rowklass;
	MonoClass *objklass;
	MonoMethod* additem;
	TupleDesc resdesc;
	gpointer kvpair[2];
	char *attname;
	Oid atttype;
	Datum *atts;
	bool *nulls;
	gpointer val;
	int i;

	resdesc = trigdata->tg_relation->rd_att;

	if (!(atts = palloc(resdesc->natts * sizeof(Datum))))
		elog(ERROR, "Not enough memory");

	if (!(nulls = palloc(resdesc->natts * sizeof(bool))))
		elog(ERROR, "Not enough memory");

	heap_deform_tuple(trigdata->tg_trigtuple, resdesc, atts, nulls);

	rowklass = mono_object_get_class(cols);
	additem = mono_class_get_method_from_name(rowklass, "Add", 2);

	for (i = 0; i < resdesc->natts; i++)
	{
		attname = NameStr(resdesc->attrs[i]->attname);
		atttype = resdesc->attrs[i]->atttypid;
		val = plmono_datum_to_obj(atts[i], atttype);

		objklass = plmono_typeoid_to_class(atttype);
		kvpair[0] = mono_string_new(plmono_get_domain(), attname); /* attribute name */
		//elog(ERROR, "Val %p ObjClass %p", val, objklass);

		if (atttype != TEXTOID)
			kvpair[1] = mono_value_box(plmono_get_domain(), objklass, val); /* attribute value */
		else
			kvpair[1] = val;

		mono_runtime_invoke(additem, cols, kvpair, NULL);
	}
}

/*
 * plmono_trigger_build_result
 *
 *     Build trigger's "NEW" row tuple based on TableRow object content
 */
Datum
plmono_trigger_build_result(TriggerData *trigdata, MonoObject *cols)
{
	MonoClass *rowklass;
	MonoProperty *item;
	TupleDesc resdesc;
	gpointer key, val;
	char *attname;
	Oid atttype;
	Datum *atts;
	bool *nulls;
	int i;

	resdesc = trigdata->tg_relation->rd_att;

	if (!(atts = palloc(resdesc->natts * sizeof(Datum))))
		elog(ERROR, "Not enough memory");

	if (!(nulls = palloc(resdesc->natts * sizeof(bool))))
		elog(ERROR, "Not enough memory");

	rowklass = mono_object_get_class(cols);
	item = mono_class_get_property_from_name(rowklass, "Item");

	for (i = 0; i < resdesc->natts; i++)
	{
		attname = NameStr(resdesc->attrs[i]->attname);
		atttype = resdesc->attrs[i]->atttypid;
		key = mono_string_new(plmono_get_domain(), attname); /* attribute name */

		val = mono_property_get_value(item, cols, &key, NULL);
		if (atttype != TEXTOID)
			val = mono_object_unbox(val);
		atts[i] = plmono_obj_to_datum(val, atttype);
		nulls[i] = 0;
	}

	return PointerGetDatum(heap_form_tuple(resdesc, atts, nulls));
}

/*
 * plmono_trigger_handler
 *
 *     Trigger function call handler
 */
Datum
plmono_trigger_handler(PG_FUNCTION_ARGS)
{
	TriggerData *trigdata = (TriggerData*) fcinfo->context;
	char *source, *assembly, *sig, *method_name;
	int argcount;

	MonoImage *image;
	MonoClass *trigklass, *klass;
	MonoMethod *method;
	MonoObject *cols;

	/*
     * Get characteristics of called function and parse its body
     */
	plmono_lookup_pg_function(fcinfo->flinfo->fn_oid, NULL, &source, NULL, NULL, NULL, &argcount);
	if (argcount)
		elog(ERROR, "PL/Mono trigger function cannot have explicit arguments");
	plmono_parse_function_body(source, &assembly, &sig, &method_name);

	/*
     * Find corresponding Mono method
     */
	image = plmono_image_open(assembly);
	klass = plmono_class_find(image, sig);
	method = plmono_method_find(klass, method_name, NULL, 0);

	/*
     * Get instance of PLMono.TriggerData.Columns
     */
	trigklass = plmono_trigger_data_get_class();
	cols = plmono_trigdata_get_columns(trigklass);

	/*
     * Prepare Columns object for trigger method invokation
     */
	plmono_trigger_build_args(trigdata, cols);

	/*
     * Invoke method
     */
	mono_runtime_invoke(method, NULL, NULL, NULL);

	/*
     * Return method's return value or arguments passed by reference
     */
	return plmono_trigger_build_result(trigdata, cols);
}

