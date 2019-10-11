/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_TOOLS_HINAWA_FW_NODE_H__
#define __ALSA_TOOLS_HINAWA_FW_NODE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_NODE	(hinawa_fw_node_get_type())

#define HINAWA_FW_NODE(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_FW_NODE,	\
				    HinawaFwNode))
#define HINAWA_IS_FW_NODE(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_FW_NODE))

#define HINAWA_FW_NODE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_FW_NODE,		\
				 HinawaFwNodeClass))
#define HINAWA_IS_FW_NODE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_FW_NODE))
#define HINAWA_FW_NODE_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_FW_NODE,		\
				   HinawaFwNodeClass))

typedef struct _HinawaFwNode		HinawaFwNode;
typedef struct _HinawaFwNodeClass	HinawaFwNodeClass;
typedef struct _HinawaFwNodePrivate	HinawaFwNodePrivate;

struct _HinawaFwNode {
	GObject parent_instance;

	HinawaFwNodePrivate *priv;
};

struct _HinawaFwNodeClass {
	GObjectClass parent_class;
};

GType hinawa_fw_node_get_type(void) G_GNUC_CONST;

HinawaFwNode *hinawa_fw_node_new(void);

void hinawa_fw_node_open(HinawaFwNode *self, const gchar *path,
			 GError **exception);

G_END_DECLS

#endif
