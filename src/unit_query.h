#ifndef __ALSA_TOOLS_HINAWA_UNIT_QUERY_H__
#define __ALSA_TOOLS_HINAWA_UNIT_QUERY_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_UNIT_QUERY	(hinawa_unit_query_get_type())

#define HINAWA_UNIT_QUERY(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_UNIT_QUERY,	\
				    HinawaUnitQuery))
#define HINAWA_IS_UNIT_QUERY(obj)				\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_UNIT_QUERY))

#define HINAWA_UNIT_QUERY_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_UNIT_QUERY,	\
				 HinawaUnitQueryClass))
#define HINAWA_IS_UNIT_QUERY_CLASS(klass)			\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_UNIT_QUERY))
#define HINAWA_UNIT_QUERY_GET_CLASS(obj)			\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_UNIT_QUERY,	\
				   HinawaUnitQueryClass))

typedef struct _HinawaUnitQuery		HinawaUnitQuery;
typedef struct _HinawaUnitQueryClass	HinawaUnitQueryClass;

struct _HinawaUnitQuery {
	GObject parent_instance;
};

struct _HinawaUnitQueryClass {
	GObjectClass parent_class;
};

GType hinawa_unit_query_get_type(void) G_GNUC_CONST;

gint hinawa_unit_query_get_sibling(gint id, GError **exception);

gint hinawa_unit_query_get_unit_type(gint id, GError **exception);

#endif
