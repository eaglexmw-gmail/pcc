/*	$Id$	*/
/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditionsand the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

# include "pass2.h"

int e2print(NODE *p, int down, int *a, int *b);
void prttype(int t);

int fldsz, fldshf;

int s2debug = 0;

/*
 * return true if shape is appropriate for the node p
 * side effect for SFLD is to set up fldsz, etc
 */
int
tshape(NODE *p, int shape)
{
	int o, mask;

	o = p->n_op;

#ifdef PCC_DEBUG
	if (s2debug)
		printf("tshape(%p, %s) op = %s\n", p, prcook(shape), opst[o]);
#endif

	if( shape & SPECIAL ){

		switch( shape ){
		case SZERO:
		case SONE:
		case SMONE:
		case SSCON:
		case SCCON:
			if( o != ICON || p->n_name[0] ) return(0);
			if( p->n_lval == 0 && shape == SZERO ) return(1);
			else if( p->n_lval == 1 && shape == SONE ) return(1);
			else if( p->n_lval == -1 && shape == SMONE ) return(1);
			else if( p->n_lval > -257 && p->n_lval < 256 && shape == SCCON ) return(1);
			else if( p->n_lval > -32769 && p->n_lval < 32768 && shape == SSCON ) return(1);
			else return(0);

		case SSOREG:	/* non-indexed OREG */
			if( o == OREG && !R2TEST(p->n_rval) ) return(1);
			else return(0);

		default:
			return( special( p, shape ) );
			}
		}

	if( shape & SANY ) return(1);

	if( (shape&INTEMP) && shtemp(p) ) return(1);

	if( (shape&SWADD) && (o==NAME||o==OREG) ){
		if( BYTEOFF(p->n_lval) ) return(0);
		}

	switch( o ){

	case NAME:
		return( shape&SNAME );
	case ICON:
		mask = SCON;
		return( shape & mask );

	case FLD:
		if( shape & SFLD ){
			if( !flshape( p->n_left ) ) return(0);
			/* it is a FIELD shape; make side-effects */
			o = p->n_rval;
			fldsz = UPKFSZ(o);
# ifdef RTOLBYTES
			fldshf = UPKFOFF(o);
# else
			fldshf = SZINT - fldsz - UPKFOFF(o);
# endif
			return(1);
			}
		return(0);

	case CCODES:
		return( shape&SCC );

	case REG:
		/* distinctions:
		SAREG	any scalar register
		STAREG	any temporary scalar register
		SBREG	any lvalue (index) register
		STBREG	any temporary lvalue register
		*/
		mask = isbreg(p->n_rval) ? SBREG : SAREG;
		if (istreg(p->n_rval) && busy[p->n_rval]<=1 )
			mask |= mask==SAREG ? STAREG : STBREG;
		return( shape & mask );

	case OREG:
		return( shape & SOREG );

	case UMUL:
		/* return STARNM or STARREG or 0 */
		return( shumul(p->n_left) & shape );

		}

	return(0);
	}

/*
 * does the type t match tword
 */
int
ttype(TWORD t, int tword)
{
	if (tword & TANY)
		return(1);

#ifdef PCC_DEBUG
	if (t2debug)
		printf("ttype(%o, %o)\n", t, tword);
#endif
	if (ISPTR(t) && (tword&TPTRTO)) {
		do {
			t = DECREF(t);
		} while (ISARY(t));
			/* arrays that are left are usually only
			 * in structure references...
			 */
		return (ttype(t, tword&(~TPTRTO)));
	}
	if (t != BTYPE(t))
		return (tword & TPOINT); /* TPOINT means not simple! */
	if (tword & TPTRTO)
		return(0);

	switch (t) {
	case CHAR:
		return( tword & TCHAR );
	case SHORT:
		return( tword & TSHORT );
	case STRTY:
	case UNIONTY:
		return( tword & TSTRUCT );
	case INT:
		return( tword & TINT );
	case UNSIGNED:
		return( tword & TUNSIGNED );
	case USHORT:
		return( tword & TUSHORT );
	case UCHAR:
		return( tword & TUCHAR );
	case ULONG:
		return( tword & TULONG );
	case LONG:
		return( tword & TLONG );
	case LONGLONG:
		return( tword & TLONGLONG );
	case ULONGLONG:
		return( tword & TULONGLONG );
	case FLOAT:
		return( tword & TFLOAT );
	case DOUBLE:
		return( tword & TDOUBLE );
	}

	return(0);
}

/*
 * called by: order, gencall
 * look for match in table and generate code if found unless
 * entry specified REWRITE.
 * returns MDONE, MNOPE, or rewrite specification from table
 */
int
match(NODE *p, int cookie)
{
	extern int *qtable[];
	struct optab *q;
	NODE *r;
	int i, rval, *ixp;

	rcount();

#ifdef PCC_DEBUG
	if (mdebug) {
		printf("match(%p, %s)\n", p, prcook(cookie));
		fwalk(p, e2print, 0);
	}
#endif

	ixp = qtable[p->n_op];
	for (i = 0; ixp[i] >= 0; i++) {
		q = &table[ixp[i]];

		/* Check if cookie matches this entry */
		if (!(q->visit & cookie))
			continue;

		/* see if left child matches */
		r = getlr(p, 'L');
		if (mdebug) {
			printf("matching left shape (%s) against (%s)\n",
			    opst[r->n_op], prcook(q->lshape));
			printf("matching left type (");
			tprint(stdout, r->n_type, r->n_qual);
			printf(") against (");
			prttype(q->ltype);
			printf(")\n");
		}
		if (!tshape( r, q->lshape))
			continue;
		if (!ttype(r->n_type, q->ltype))
			continue;

		/* see if right child matches */
		r = getlr(p, 'R');
		if (mdebug) {
			printf("matching right shape (%s) against (%s)\n",
			    opst[r->n_op], prcook(q->rshape));
			printf("matching right type (");
			tprint(stdout, r->n_type, r->n_qual);
			printf(") against (");
			prttype(q->rtype);
			printf(")\n");
		}
		if (!tshape(r, q->rshape))
			continue;
		if (!ttype(r->n_type, q->rtype))
			continue;

		/*
		 * REWRITE means no code from this match but go ahead
		 * and rewrite node to help future match
		 */
		if (q->needs & REWRITE) {
			rval = q->rewrite;
			goto leave;
		}
		if (!allo(p, q)) { /* if can't generate code, skip entry */
			if (mdebug)
				printf("allo(p, q) failed\n");
			continue;
		}

		/* resources are available */

		expand(p, cookie, q->cstring);		/* generate code */
		reclaim(p, q->rewrite, cookie);

		rval = MDONE;
		goto leave;

	}

	rval = MNOPE;
leave:
#ifdef PCC_DEBUG
	if (odebug)
		printf("leave match(%p, %s) == %s\n", p, prcook(cookie),
		    rval == MNOPE ? "MNOPE" : rval == MDONE ? "MDONE" :
		    prcook(cookie));
#endif

	return rval;
}

/*
 * generate code by interpreting table entry
 */
void
expand(NODE *p, int cookie, char *cp)
{
	CONSZ val;

	for( ; *cp; ++cp ){
		switch( *cp ){

		default:
			PUTCHAR( *cp );
			continue;  /* this is the usual case... */

		case 'Z':  /* special machine dependent operations */
			zzzcode( p, *++cp );
			continue;

		case 'F':  /* this line deleted if FOREFF is active */
			if( cookie & FOREFF ) while( *++cp != '\n' ) ; /* VOID */
			continue;

		case 'S':  /* field size */
			printf( "%d", fldsz );
			continue;

		case 'H':  /* field shift */
			printf( "%d", fldshf );
			continue;

		case 'M':  /* field mask */
		case 'N':  /* complement of field mask */
			val = 1;
			val <<= fldsz;
			--val;
			val <<= fldshf;
			adrcon( *cp=='M' ? val : ~val );
			continue;

		case 'L':  /* output special label field */
			if (*++cp == 'C')
				printf(LABFMT, p->n_label);
			else
				printf(LABFMT, (int)getlr(p,*cp)->n_lval);
			continue;

		case 'O':  /* opcode string */
			hopcode( *++cp, p->n_op );
			continue;

		case 'B':  /* byte offset in word */
			val = getlr(p,*++cp)->n_lval;
			val = BYTEOFF(val);
			printf( CONFMT, val );
			continue;

		case 'C': /* for constant value only */
			conput( getlr( p, *++cp ) );
			continue;

		case 'I': /* in instruction */
			insput( getlr( p, *++cp ) );
			continue;

		case 'A': /* address of */
			adrput(stdout, getlr( p, *++cp ) );
			continue;

		case 'U': /* for upper half of address, only */
			upput(getlr(p, *++cp), SZLONG);
			continue;

			}

		}

	}

NODE *
getlr(NODE *p, int c)
{
	NODE *q;

	/* return the pointer to the left or right side of p, or p itself,
	   depending on the optype of p */

	switch (c) {

	case '1':
	case '2':
	case '3':
		c -= '1';
		q = &resc[c];
		q->n_op = REG;
		q->n_type = p->n_type; /* ???? */
		q->n_rval = p->n_rall; /* Should be assigned by genregs() */
		q->n_rval += szty(q->n_type) * c;
		return q;

	case 'L':
		return( optype( p->n_op ) == LTYPE ? p : p->n_left );

	case 'R':
		return( optype( p->n_op ) != BITYPE ? p : p->n_right );

	}
	cerror( "bad getlr: %c", c );
	/* NOTREACHED */
	return NULL;
}

static char *tarr[] = {
	"CHAR", "SHORT", "INT", "LONG", "FLOAT", "DOUBLE", "POINT", "UCHAR",
	"USHORT", "UINT", "ULONG", "PTRTO", "ANY", "STRUCT", "LONGLONG",
	"ULONGLONG",
};

void
prttype(int t)
{
	int i, gone = 0;

	for (i = 0; i < 16; i++)
		if ((t >> i) & 1) {
			if (gone) putchar('|');
			gone++;
			printf("%s", tarr[i]);
		}
}
