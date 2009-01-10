/*      $Id$     */
/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Routines to support some of the gcc extensions to C.
 */
#ifdef GCC_COMPAT

#include "pass1.h"
#include "cgram.h"

#include <string.h>

/* Remove heading and trailing __ */
static char *
decap(char *s)
{
	if (s[0] == '_' && s[1] == '_') {
		int len = strlen(s);

		if (s[len-1] == '_' && s[len-2] == '_') {
			s = tmpstrdup(s); /* will trash */
			s[len-2] = 0;
		}
		s += 2;
	}
	return s;
}

static struct kw {
	char *name, *ptr;
	int rv;
} kw[] = {
/*
 * Do NOT change the order of these entries unless you know 
 * what you're doing!
 */
/* 0 */	{ "__asm", NULL, C_ASM },
/* 1 */	{ "__signed", NULL, 0 },
/* 2 */	{ "__inline", NULL, C_FUNSPEC },
/* 3 */	{ "__const", NULL, 0 },
/* 4 */	{ "__asm__", NULL, C_ASM },
/* 5 */	{ "__inline__", NULL, C_FUNSPEC },
/* 6 */	{ "__thread", NULL, 0 },
/* 7 */	{ "__FUNCTION__", NULL, 0 },
/* 8 */	{ "__volatile", NULL, 0 },
/* 9 */	{ "__volatile__", NULL, 0 },
/* 10 */{ "__restrict", NULL, -1 },
/* 11 */{ "__typeof__", NULL, C_TYPEOF },
/* 12 */{ "typeof", NULL, C_TYPEOF },
/* 13 */{ "__extension__", NULL, -1 },
/* 14 */{ "__signed__", NULL, 0 },
/* 15 */{ "__attribute__", NULL, 0 },
/* 16 */{ "__attribute", NULL, 0 },
	{ NULL, NULL, 0 },
};

void
gcc_init()
{
	struct kw *kwp;

	for (kwp = kw; kwp->name; kwp++)
		kwp->ptr = addname(kwp->name);

}

#define	TS	"\n#pragma tls\n# %d\n"
#define	TLLEN	sizeof(TS)+10
/*
 * See if a string matches a gcc keyword.
 */
int
gcc_keyword(char *str, NODE **n)
{
	extern int inattr, parlvl, parbal;
	char tlbuf[TLLEN], *tw;
	struct kw *kwp;
	int i;

	for (i = 0, kwp = kw; kwp->name; kwp++, i++)
		if (str == kwp->ptr)
			break;
	if (kwp->name == NULL)
		return 0;
	if (kwp->rv)
		return kwp->rv;
	switch (i) {
	case 1:  /* __signed */
	case 14: /* __signed__ */
		*n = mkty((TWORD)SIGNED, 0, MKSUE(SIGNED));
		return C_TYPE;
	case 3: /* __const */
		*n = block(QUALIFIER, NIL, NIL, CON, 0, 0);
		return C_QUALIFIER;
	case 6: /* __thread */
		snprintf(tlbuf, TLLEN, TS, lineno);
		tw = &tlbuf[strlen(tlbuf)];
		while (tw > tlbuf)
			cunput(*--tw);
		return -1;
	case 7: /* __FUNCTION__ */
		if (cftnsp == NULL) {
			uerror("__FUNCTION__ outside function");
			yylval.strp = "";
		} else
			yylval.strp = cftnsp->sname; /* XXX - not C99 */
		return C_STRING;
	case 8: /* __volatile */
	case 9: /* __volatile__ */
		*n = block(QUALIFIER, NIL, NIL, VOL, 0, 0);
		return C_QUALIFIER;
	case 15: /* __attribute__ */
	case 16: /* __attribute */
		inattr = 1;
		parlvl = parbal;
		return C_ATTRIBUTE;
	}
	cerror("gcc_keyword");
	return 0;
}

#ifndef TARGET_ATTR
#define	TARGET_ATTR(p, sue)		1
#endif
#ifndef	ALMAX
#define	ALMAX (ALLDOUBLE > ALLONGLONG ? ALLDOUBLE : ALLONGLONG)
#endif

/*
 * Parse attributes from an argument list.
 */
static void
gcc_attribs(NODE *p, void *arg)
{
	gcc_ap_t *gap = arg;
	char *n2, *name = NULL;
	int num;

	if (p->n_op == NAME) {
		name = (char *)p->n_sp;
	} else if (p->n_op == CALL || p->n_op == UCALL) {
		name = (char *)p->n_left->n_sp;
	} else
		cerror("bad variable attribute");

	n2 = name;
	name = decap(name);
	num = gap->num;
	if (strcmp(name, "aligned") == 0) {
		/* Align the variable to a given max alignment */
		gap->ga[num].atype = GCC_ATYP_ALIGNED;
		if (p->n_op == CALL) {
			gap->ga[num].a1.iarg = icons(eve(p->n_right)) * SZCHAR;
			p->n_op = UCALL;
		} else
			gap->ga[num].a1.iarg = ALMAX;
	} else if (strcmp(name, "section") == 0) {
		if (p->n_right->n_op != STRING)
			uerror("bad section");
		gap->ga[num].atype = GCC_ATYP_SECTION;
		gap->ga[num].a1.sarg = p->n_right->n_name;
	} else if (strcmp(name, "packed") == 0) {
		/* pack members of a struct */
		gap->ga[num].atype = GCC_ATYP_PACKED;
		if (p->n_op != NAME)
			uerror("packed takes no args");
		gap->ga[num].a1.iarg = SZCHAR; /* specify pack size? */
	} else if (TARGET_ATTR(p, gap) == 0)
		werror("unsupported attribute %s", n2);
	gap->num++;
}

/*
 * Extract type attributes from a node tree and create a gcc_attr_pack
 * struct based on its contents.
 */
gcc_ap_t *
gcc_attr_parse(NODE *p)
{
	gcc_ap_t *gap;
	NODE *q;
	int i, sz;

	/* count number of elems */
	for (q = p, i = 1; q->n_op == CM; q = q->n_left, i++)
		;

	/* get memory for struct */
	sz = sizeof(struct gcc_attr_pack) + sizeof(struct gcc_attrib) * i;
	if (blevel == 0)
		gap = memset(permalloc(sz), 0, sz);
	else
		gap = tmpcalloc(sz);

	flist(p, gcc_attribs, gap);
	if (gap->num != i)
		cerror("attribute sync error");

	tfree(p);
	return gap;
}

/*
 * Fixup struct/unions depending on attributes.
 */
void
gcc_tcattrfix(NODE *p, NODE *q)
{
	struct symtab *sp;
	struct suedef *sue;
	gcc_ap_t *gap;
	int align = 0;
	int i, sz, coff, sa;

	gap = gcc_attr_parse(q);
	sue = p->n_sue;

	/* must know about align first */
	for (i = 0; i < gap->num; i++)
		if (gap->ga[i].atype == GCC_ATYP_ALIGNED)
			align = gap->ga[i].a1.iarg;

	/* Check following attributes */
	for (i = 0; i < gap->num; i++) {
		switch (gap->ga[i].atype) {
		case GCC_ATYP_PACKED:
			/* Must repack struct */
			/* XXX - aligned types inside? */
			coff = 0;
			for (sp = sue->suem; sp; sp = sp->snext) {
				sa = talign(sp->stype, sp->ssue);
				if (sp->sclass & FIELD)
					sz = sp->sclass&FLDSIZ;
				else
					sz = tsize(sp->stype, sp->sdf, sp->ssue);
				sp->soffset = coff;
				if (p->n_type == STRTY)
					coff += sz;
				else if (sz > coff)
					coff = sz;
			}
			sue->suesize = coff;
			sue->suealign = ALCHAR;
			break;

		case GCC_ATYP_ALIGNED:
			break;

		default:
			werror("unsupported attribute %d", gap->ga[i].atype);
		}
	}
	if (align) {
		sue->suealign = align;
		SETOFF(sue->suesize, sue->suealign);
	}
}

#endif
