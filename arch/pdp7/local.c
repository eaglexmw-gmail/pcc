/*	$Id$	*/
/*
 * Copyright (c) 2017 Anders Magnusson (ragge@ludd.luth.se).
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


#include "pass1.h"

#undef NIL
#define	NIL NULL

#ifdef LANG_CXX
#define	P1ND NODE
#define	p1nfree nfree
#define	p1fwalk fwalk
#define	p1tcopy tcopy
#endif

/*	this file contains code which is dependent on the target machine */

/* clocal() is called to do local transformations on
 * an expression tree preparitory to its being
 * written out in intermediate code.
 *
 * the major essential job is rewriting the
 * automatic variables and arguments in terms of
 * REG and OREG nodes
 * conversion ops which are not necessary are also clobbered here
 * in addition, any special features (such as rewriting
 * exclusive or) are easily handled here as well
 */
P1ND *
clocal(P1ND *p)
{

	register struct symtab *q;
	register int o;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal: %p\n", p);
		p1fwalk(p, eprint, 0);
	}
#endif
	switch( o = p->n_op ){

	case NAME:
		if ((q = p->n_sp) == NULL)
			return p; /* Nothing to care about */

		switch (q->sclass) {

		case AUTO:
			fixdef(q);
			break;

		case PARAM:
			cerror("PARAM");
			break;

		case USTATIC:
			break;

		case STATIC:
			break;

		case REGISTER:
			p->n_op = REG;
			slval(p, 0);
			p->n_rval = q->soffset;
			break;

		case EXTERN:
		case EXTDEF:
			break;
		}
		break;

	case PCONV:
		break;

	case SCONV:
		break;

	case FORCE:
		/* put return value in return reg */
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, 0, 0);
		p->n_left->n_rval = p->n_left->n_type == BOOL ? 
		    RETREG(CHAR) : RETREG(p->n_type);
		break;

	}
#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal end: %p\n", p);
		p1fwalk(p, eprint, 0);
	}
#endif
	return(p);
}

void
myp2tree(P1ND *p)
{
}

/*ARGSUSED*/
int
andable(P1ND *p)
{
	return(1);	/* all names can have & taken on them */
}

/*
 * Return 1 if a variable of type type is OK to put in register.
 * No registers on pdp7.
 */
int
cisreg(TWORD t)
{
	return 0;
}

/*
 * Allocate off bits on the stack.  p is a tree that when evaluated
 * is the multiply count for off, t is a storeable node where to write
 * the allocated address.
 */
void
spalloc(P1ND *t, P1ND *p, OFFSZ off)
{
	P1ND *sp;

	p = buildtree(MUL, p, bcon(off/SZCHAR)); /* XXX word alignment? */

	/* sub the size from sp */
	sp = block(REG, NIL, NIL, p->n_type, 0, 0);
	slval(sp, 0);
	sp->n_rval = STKREG;
	ecomp(buildtree(MINUSEQ, sp, p));

#ifdef MACHOABI	
	/* align to 16 bytes */
	sp = block(REG, NIL, NIL, p->n_type, 0, 0);
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	ecomp(buildtree(PLUSEQ, sp, bcon(15)));
	
	sp = block(REG, NIL, NIL, p->n_type, 0, 0);
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	ecomp(buildtree(RSEQ, sp, bcon(4)));
	
	sp = block(REG, NIL, NIL, p->n_type, 0, 0);
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	ecomp(buildtree(LSEQ, sp, bcon(4)));
#endif
	

	/* save the address of sp */
	sp = block(REG, NIL, NIL, PTR+INT, t->n_df, t->n_ap);
	slval(sp, 0);
	sp->n_rval = STKREG;
	t->n_type = sp->n_type;
	ecomp(buildtree(ASSIGN, t, sp)); /* Emit! */

}

/*
 * print out a constant node, may be associated with a label.
 * Do not free the node after use.
 * off is bit offset from the beginning of the aggregate
 * fsz is the number of bits this is referring to
 */
int
ninval(CONSZ off, int fsz, P1ND *p)
{
	struct symtab *sp = p->n_sp;
	long l = glval(p);

	switch (p->n_type) {
	case INT:
	case UNSIGNED:
		printf("0%o", (int)l);
		if (sp != NULL) {
			if ((sp->sclass == STATIC && sp->slevel > 0)) {
				printf("+" LABFMT, (int)sp->soffset);
			} else
				printf("+%s", getexname(sp));
		}
		printf("\n");
		break;
	default:
		return 0;
	}
	return 1;
}

void
instring(struct symtab *sp)
{
	char *s = sp->sname;
	unsigned short word;

	defloc(sp);
	printf("\n");
	for (; ; ) {
		word = (*s == '\\' ? esccon(&s) : (unsigned)*s++) << 9;
		if (word == 0 || *s == 0)
			break;
		word |= (*s == '\\' ? esccon(&s) : (unsigned)*s++);
		printf("	0%o\n", word);
	}
	printf("	0%o\n", word);
}

/* make a name look like an external name in the local machine */
char *
exname(char *p)
{
	return p;

}

/*
 * map types which are not defined on the local machine
 */
TWORD
ctype(TWORD type)
{
	switch (BTYPE(type)) {
	case SHORT:
		MODTYPE(type,INT);
		break;

	case USHORT:
		MODTYPE(type,UNSIGNED);

	}
	return (type);
}

void
calldec(P1ND *p, P1ND *q) 
{
}

void
extdec(struct symtab *q)
{
}

/* make a common declaration for id, if reasonable */
void
defzero(struct symtab *sp)
{
	int off;
	int al;
	char *name;

	name = getexname(sp);
	al = talign(sp->stype, sp->sap)/SZCHAR;
	off = (int)tsize(sp->stype, sp->sdf, sp->sap);
	SETOFF(off,SZCHAR);
	off /= SZCHAR;
	if (sp->sclass == STATIC) {
		if (sp->slevel == 0) {
			printf(PRTPREF "\t.local %s\n", name);
		} else
			printf(PRTPREF "\t.local " LABFMT "\n", sp->soffset);
	}
	if (sp->slevel == 0)
		printf(PRTPREF "\t.comm %s,0%o,%d\n", name, off, al);
	else
		printf(PRTPREF "\t.comm  " LABFMT ",0%o,%d\n", sp->soffset, off, al);
}

/*
 * Give target the opportunity of handling pragmas.
 */
int
mypragma(char *str)
{
	return 0;
}

struct lab { struct lab *next; int lab; } *lpole;
void dellab(int lab);
void printlab(void);
void addlab(int lab);

void
addlab(int lab)
{
	struct lab *l;

	for (l = lpole; l; l = l->next) {
		if (l->lab == lab)
			return;
	}
	l = tmpalloc(sizeof(struct lab));
	l->lab = lab;
	l->next = lpole;
	lpole = l;
}

void
dellab(int lab)
{
	struct lab *l;

	for (l = lpole; l; l = l->next) {
		if (l->lab == lab) {
			l->lab = 0;
			return;
		}
	}
}

void
printlab()
{
	struct lab *l;

	for (l = lpole; l; l = l->next) {
		if (l->lab != 0)
			printf(LABFMT ":	0\n", l->lab);
	}
}

/*
 * Called when a identifier has been declared.
 */
void
fixdef(struct symtab *sp)
{
	if (sp->sflags & STNODE)
		return;
	if (sp->sclass == AUTO) {
		sp->sclass = STATIC;
		sp->soffset = getlab();
		addlab(sp->soffset);
//		printf(LABFMT ":	0\n", sp->soffset);
	}
}

void
pass1_lastchance(struct interpass *ip)
{
}

#ifdef PASS1
void
mflags(char *s)
{
}
#endif
