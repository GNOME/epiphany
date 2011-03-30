/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2011, Igalia S.L.
 */

#ifndef EPHY_REQUEST_ABOUT_H
#define EPHY_REQUEST_ABOUT_H 1

#define LIBSOUP_USE_UNSTABLE_REQUEST_API
#include <libsoup/soup-request.h>

#define EPHY_TYPE_REQUEST_ABOUT            (ephy_request_about_get_type ())
#define EPHY_REQUEST_ABOUT(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), EPHY_TYPE_REQUEST_ABOUT, EphyRequestAbout))
#define EPHY_REQUEST_ABOUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_REQUEST_ABOUT, EphyRequestAboutClass))
#define EPHY_IS_REQUEST_ABOUT(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), EPHY_TYPE_REQUEST_ABOUT))
#define EPHY_IS_REQUEST_ABOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_REQUEST_ABOUT))
#define EPHY_REQUEST_ABOUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_REQUEST_ABOUT, EphyRequestAboutClass))

#define EPHY_ABOUT_SCHEME "ephy-about"
#define EPHY_ABOUT_SCHEME_LEN 10

typedef struct _EphyRequestAboutPrivate EphyRequestAboutPrivate;

typedef struct {
	SoupRequest parent;

	EphyRequestAboutPrivate *priv;
} EphyRequestAbout;

typedef struct {
	SoupRequestClass parent;

} EphyRequestAboutClass;

GType ephy_request_about_get_type (void);

#endif /* EPHY_REQUEST_ABOUT_H */
