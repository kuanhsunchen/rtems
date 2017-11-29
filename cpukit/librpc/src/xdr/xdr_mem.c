/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)xdr_mem.c 1.19 87/08/11 Copyr 1984 Sun Micro";*/
/*static char *sccsid = "from: @(#)xdr_mem.c	2.1 88/07/29 4.0 RPCSRC";*/
static char *rcsid = "$FreeBSD: src/lib/libc/xdr/xdr_mem.c,v 1.8 1999/08/28 00:02:56 peter Exp $";
#endif

/*
 * xdr_mem.h, XDR implementation using memory buffers.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <netinet/in.h>

static bool_t	xdrmem_getlong_aligned(XDR *xdrs, long *lp);
static bool_t	xdrmem_putlong_aligned(XDR *xdrs, const long *lp);
static bool_t	xdrmem_getlong_unaligned(XDR *xdrs, long *lp);
static bool_t	xdrmem_putlong_unaligned(XDR *xdrs, const long *lp);
static bool_t	xdrmem_getbytes(XDR *xdrs, caddr_t addr, u_int len);
static bool_t	xdrmem_putbytes(XDR *xdrs, const char *addr, u_int len);
static u_int	xdrmem_getpos(XDR *xdrs); /* XXX w/64-bit pointers, u_int not enough! */
static bool_t	xdrmem_setpos(XDR *xdrs, u_int pos);
static int32_t *xdrmem_inline_aligned(XDR *xdrs, u_int len);
static int32_t *xdrmem_inline_unaligned(XDR *xdrs, u_int len);
static void	xdrmem_destroy(XDR *);

static struct	xdr_ops xdrmem_ops_aligned = {
	xdrmem_getlong_aligned,
	xdrmem_putlong_aligned,
	xdrmem_getbytes,
	xdrmem_putbytes,
	xdrmem_getpos,
	xdrmem_setpos,
	xdrmem_inline_aligned,
	xdrmem_destroy
};

static struct	xdr_ops xdrmem_ops_unaligned = {
	xdrmem_getlong_unaligned,
	xdrmem_putlong_unaligned,
	xdrmem_getbytes,
	xdrmem_putbytes,
	xdrmem_getpos,
	xdrmem_setpos,
	xdrmem_inline_unaligned,
	xdrmem_destroy
};

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.
 */
void
xdrmem_create(
	XDR *xdrs,
	char * addr,
	u_int size,
	enum xdr_op op)
{

	xdrs->x_op = op;
	xdrs->x_ops = ((uintptr_t)addr & (sizeof(int32_t) - 1))
		? &xdrmem_ops_unaligned : &xdrmem_ops_aligned;
	xdrs->x_private = xdrs->x_base = addr;
	xdrs->x_handy = size;
}

static void
xdrmem_destroy(XDR *xdrs)
{

}

static bool_t
xdrmem_getlong_aligned(
	XDR *xdrs,
	long *lp)
{

	if ((xdrs->x_handy -= sizeof(int32_t)) < 0)
		return (FALSE);
	*lp = ntohl(*(int32_t *)(xdrs->x_private));
	xdrs->x_private += sizeof(int32_t);
	return (TRUE);
}

static bool_t
xdrmem_putlong_aligned(
	XDR *xdrs,
	const long *lp)
{

	if ((xdrs->x_handy -= sizeof(int32_t)) < 0)
		return (FALSE);
	*(int32_t *)xdrs->x_private = htonl(*lp);
	xdrs->x_private += sizeof(int32_t);
	return (TRUE);
}

static bool_t
xdrmem_getlong_unaligned(
	XDR *xdrs,
	long *lp)
{
	int32_t l;

	if ((xdrs->x_handy -= sizeof(int32_t)) < 0)
		return (FALSE);
	memcpy(&l, xdrs->x_private, sizeof(int32_t));
	*lp = ntohl(l);
	xdrs->x_private += sizeof(int32_t);
	return (TRUE);
}

static bool_t
xdrmem_putlong_unaligned(
	XDR *xdrs,
	const long *lp)
{
	int32_t l;

	if ((xdrs->x_handy -= sizeof(int32_t)) < 0)
		return (FALSE);
	l = htonl(*lp);
	memcpy(xdrs->x_private, &l, sizeof(int32_t));
	xdrs->x_private += sizeof(int32_t);
	return (TRUE);
}

static bool_t
xdrmem_getbytes(
	XDR *xdrs,
	caddr_t addr,
	u_int len)
{

	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	memcpy(addr, xdrs->x_private, len);
	xdrs->x_private += len;
	return (TRUE);
}

static bool_t
xdrmem_putbytes(
	XDR *xdrs,
	const char *addr,
	u_int len)
{

	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	memcpy(xdrs->x_private, addr, len);
	xdrs->x_private += len;
	return (TRUE);
}

static u_int
xdrmem_getpos(
	XDR *xdrs)
{

	/* XXX w/64-bit pointers, u_int not enough! */
	return ((uintptr_t)xdrs->x_private - (uintptr_t)xdrs->x_base);
}

static bool_t
xdrmem_setpos(
	XDR *xdrs,
	u_int pos)
{
	void *newaddr = xdrs->x_base + pos;
	void *lastaddr = xdrs->x_private + xdrs->x_handy;

	if (newaddr > lastaddr)
		return (FALSE);
	xdrs->x_private = newaddr;
	xdrs->x_handy = (intptr_t)lastaddr - (intptr_t)newaddr;
	return (TRUE);
}

static int32_t *
xdrmem_inline_aligned(
	XDR *xdrs,
	u_int len)
{
	int32_t *buf = 0;

	if (xdrs->x_handy >= len) {
		xdrs->x_handy -= len;
		buf = (int32_t *)xdrs->x_private;
		xdrs->x_private += len;
	}
	return (buf);
}

static int32_t *
xdrmem_inline_unaligned(
	XDR *xdrs,
	u_int len)
{
	
	return (0);
}
