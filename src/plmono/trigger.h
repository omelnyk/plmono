#ifndef _PLMONO_TRIGGER_H
#define _PLMONO_TRIGGER_H

MonoClass* plmono_trigger_data_get_class(void);
MonoObject* plmono_trigdata_get_columns(MonoClass *trigdata);
void plmono_trigger_build_args(TriggerData *trigdata, MonoObject *cols);
Datum plmono_trigger_build_result(TriggerData *trigdata, MonoObject *cols);
Datum plmono_trigger_handler(PG_FUNCTION_ARGS);

#endif
