MODULE_big = plmono

PG_CPPFLAGS = `pkg-config --cflags --libs mono glib-2.0`
PG_LIBS = `pkg-config --cflags --libs mono glib-2.0`
SHLIB_LINK = `pkg-config --cflags --libs mono glib-2.0`
OBJS = plmono.o

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

