/*	manifest.h	4.2	87/12/09	*/

#ifndef _MANIFEST_
#define	_MANIFEST_

#include <stdio.h>
#include "cgram.h"
#include "config.h"

#define DSIZE	(MAXOP+1)	/* DSIZE is the size of the dope array */

#define NOLAB	(-1)		/* no label with constant */

/*
 * Node types
 */
#define LTYPE	02		/* leaf */
#define UTYPE	04		/* unary */
#define BITYPE	010		/* binary */

/*
 * Bogus type values
 */
#define TNULL	INCREF(MOETY)	/* pointer to MOETY -- impossible type */
#define TVOID	FTN		/* function returning UNDEF (for void) */

/*
 * Type packing constants
 */
#define TMASK	0x60		/* mask for 1st component of compound type */
#define TMASK1	0x180		/* mask for 2nd component of compound type */
#define TMASK2	0x1e0		/* mask for 3rd component of compound type */
#define BTMASK	0x1f		/* basic type mask */
#define BTSHIFT	5		/* basic type shift */
#define TSHIFT	2		/* shift count to get next type component */

/*
 * Type manipulation macros
 */
#define MODTYPE(x,y)	x = ((x)&(~BTMASK))|(y)	/* set basic type of x to y */
#define BTYPE(x)	((x)&BTMASK)		/* basic type of x */
#define ISUNSIGNED(x)	((x)<=ULONGLONG&&(x)>=UCHAR)
#define UNSIGNABLE(x)	((x)<=LONGLONG&&(x)>=CHAR)
#define ENUNSIGN(x)	((x)+(UNSIGNED-INT))
#define DEUNSIGN(x)	((x)+(INT-UNSIGNED))
#define ISPTR(x)	(((x)&TMASK)==PTR)
#define ISFTN(x)	(((x)&TMASK)==FTN)	/* is x a function type */
#define ISARY(x)	(((x)&TMASK)==ARY)	/* is x an array type */
#define INCREF(x)	((((x)&~BTMASK)<<TSHIFT)|PTR|((x)&BTMASK))
#define DECREF(x)	((((x)>>TSHIFT)&~BTMASK)|( (x)&BTMASK))
/* advance x to a multiple of y */
#define SETOFF(x,y)	if ((x)%(y) != 0) (x) = (((x)/(y) + 1) * (y))
/* can y bits be added to x without overflowing z */
#define NOFIT(x,y,z)	(((x)%(z) + (y)) > (z))

/*
 * Pack and unpack field descriptors (size and offset)
 */
#define PKFIELD(s,o)	(((o)<<6)| (s))
#define UPKFSZ(v)	((v) &077)
#define UPKFOFF(v)	((v)>>6)

/*
 * Operator information
 */
#define TYFLG	016
#define ASGFLG	01
#define LOGFLG	020

#define SIMPFLG	040
#define COMMFLG	0100
#define DIVFLG	0200
#define FLOFLG	0400
#define LTYFLG	01000
#define CALLFLG	02000
#define MULFLG	04000
#define SHFFLG	010000
#define ASGOPFLG 020000

#define SPFLG	040000

#define optype(o)	(dope[o]&TYFLG)
#define asgop(o)	(dope[o]&ASGFLG)
#define logop(o)	(dope[o]&LOGFLG)
#define callop(o)	(dope[o]&CALLFLG)

/*
 * Macros from pcc.h, cooperates with cgram.y generated header file.
 */
#define	ASG		1+
#define	UNARY		2+
#define	NOASG		(-1)+
#define	NOUNARY		(-2)+

/*
 * Types, as encoded in intermediate file cookies.
 */
#define	UNDEF		0
#define	FARG		1	/* function argument */
#define	CHAR		2
#define	SHORT		3
#define	INT		4
#define	LONG		5
#define	LONGLONG	6	/* long long, per C99 */
#define	FLOAT		7
#define	DOUBLE		8
#define	STRTY		9
#define	UNIONTY		10
#define	ENUMTY		11
#define	MOETY		12	/* member of enum */
#define	UCHAR		13
#define	USHORT		14
#define	UNSIGNED	15
#define	ULONG		16      
#define	ULONGLONG	17
#define	SIGNED		18	/* Signed, per ANSI-C */
#define	SCHAR		19
#define	CONST		20	/* Not really a type */
#define	VOLATILE	21	/* Not really a type */

/* 
 * Type modifiers.
 */
#define	PTR		0x20
#define	FTN		0x40
#define	ARY		0x60
#define	BASETYPE	0x1f
#define	TYPESHIFT	2

/*
 * Node status.
 */
#define	ERROR		1	/* an error node */
#define	FREE		2	/* an unused node */

/*
 * External declarations, typedefs and the like
 */
char	*hash(char *s);
char	*savestr(char *cp);
char	*tstr(char *cp);
extern	int tstrused;
extern	char *tstrbuf[];
extern	char **curtstr;
#define	freetstr()	curtstr = tstrbuf, tstrused = 0

extern	int nerrors;		/* number of errors seen so far */
extern	int dope[];		/* a vector containing operator information */
extern	char *opst[];		/* a vector containing names for ops */

typedef	union ndu NODE;
typedef	unsigned int TWORD;
#define NIL	(NODE *)0

#include "onepass.h"
#include "main.h"
#endif
