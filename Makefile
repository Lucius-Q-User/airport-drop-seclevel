#	$NetBSD: Makefile,v 1.12 2003/05/18 07:57:31 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	haxx
LDADD+=		-lcrypt -lutil
.include <bsd.prog.mk>
CFLAGS+= -Wno-strict-prototypes -Wno-missing-prototypes
LDFLAGS+= -static
