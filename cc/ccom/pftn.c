#if 0
static char *sccsid ="@(#)pftn.c	1.29 (Berkeley) 6/18/90";
#endif

# include "pass1.h"

# include <stdlib.h>

unsigned int offsz;

struct symtab *schain[MAXSCOPES];	/* sym chains for clearst */
int chaintop;				/* highest active entry */
static int strunem;			/* currently parsed member */

struct instk {
	int in_sz;   /* size of array element */
	int in_x;    /* current index for structure member in structure initializations */
	int in_n;    /* number of initializations seen */
	int in_s;    /* sizoff */
	int in_d;    /* dimoff */
	TWORD in_t;    /* type */
	int in_id;   /* stab index */
	int in_fl;   /* flag which says if this level is controlled by {} */
	OFFSZ in_off;  /* offset of the beginning of this level */
} instack[10], *pstk;

/* defines used for getting things off of the initialization stack */


struct symtab *relook(struct symtab *);
struct symtab * mknonuniq(int *);
void fixtype(NODE *p, int class);
int fixclass(int class, TWORD type);
int falloc(struct symtab *p, int w, int new, NODE *pty);
void psave(int);
int oalloc(struct symtab *p, int *poff);
int hide(struct symtab *p);
void inforce(OFFSZ n);
void vfdalign(int n);
void instk(int id, TWORD t, int d, int s, OFFSZ off);
void gotscal(void);

int ddebug = 0;

void
defid(NODE *q, int class)
{
	struct symtab *p;
	int idp;
	TWORD type;
	TWORD stp;
	int scl;
	int dsym, ddef;
	int slev, temp;
	int changed;

	if( q == NIL ) return;  /* an error was detected */

	if( q < node || q >= &node[TREESZ] ) cerror( "defid call" );

	idp = q->tn.rval;

	if( idp < 0 ) cerror( "tyreduce" );
	p = &stab[idp];

# ifndef BUG1
	if (ddebug) {
		printf("defid(%s (%d), ", p->sname, idp);
		tprint(q->in.type);
		printf(", %s, (%d,%d)), level %d\n", scnames(class),
		    q->fn.cdim, q->fn.csiz, blevel);
	}
# endif

	fixtype( q, class );

	type = q->in.type;
	class = fixclass( class, type );

	stp = p->stype;
	slev = p->slevel;

# ifndef BUG1
	if (ddebug) {
		printf("	modified to ");
		tprint(type);
		printf(", %s\n", scnames(class));
		printf("	previous def'n: ");
		tprint(stp);
		printf(", %s, (%d,%d)), level %d\n",
		    scnames(p->sclass), p->dimoff, p->sizoff, slev);
	}
# endif

	if( stp == FTN && p->sclass == SNULL )goto enter;
	if (blevel==1 && stp!=FARG)
		switch( class ){
		default:
			if (!(class&FIELD))
				uerror( "declared argument %s is missing",
				    p->sname );
		case MOS:
		case STNAME:
		case MOU:
		case UNAME:
		case MOE:
		case ENAME:
		case TYPEDEF:
			;
	}
	if( stp == UNDEF|| stp == FARG ) goto enter;

	if( type != stp ) goto mismatch;
	if( blevel > slev && (class == AUTO || class == REGISTER) )
		/* new scope */
		goto mismatch;

	/* test (and possibly adjust) dimensions */
	dsym = p->dimoff;
	ddef = q->fn.cdim;
	changed = 0;
	for( temp=type; temp&TMASK; temp = DECREF(temp) ){
		if( ISARY(temp) ){
			if (dimtab[dsym] == 0) {
				dimtab[dsym] = dimtab[ddef];
				changed = 1;
				}
			else if (dimtab[ddef]!=0&&dimtab[dsym]!=dimtab[ddef]) {
				goto mismatch;
				}
			++dsym;
			++ddef;
			}
		}

	if (changed) {
		FIXDEF(p);
		}

	/* check that redeclarations are to the same structure */
	if( (temp==STRTY||temp==UNIONTY||temp==ENUMTY) && p->sizoff != q->fn.csiz
		 && class!=STNAME && class!=UNAME && class!=ENAME ){
		goto mismatch;
		}

	scl = ( p->sclass );

# ifndef BUG1
	if( ddebug ){
		printf( "	previous class: %s\n", scnames(scl) );
		}
# endif

	if( class&FIELD ){
		/* redefinition */
		if( !falloc( p, class&FLDSIZ, 1, NIL ) ) {
			/* successful allocation */
			psave( idp );
			return;
			}
		/* blew it: resume at end of switch... */
		}

	else switch( class ){

	case EXTERN:
		switch( scl ){
		case STATIC:
		case USTATIC:
			if( slev==0 ) return;
			break;
		case EXTDEF:
		case EXTERN:
		case FORTRAN:
		case UFORTRAN:
			return;
			}
		break;

	case STATIC:
		if( scl==USTATIC || (scl==EXTERN && blevel==0) ){
			p->sclass = STATIC;
			if( ISFTN(type) ) curftn = idp;
			return;
			}
		break;

	case USTATIC:
		if( scl==STATIC || scl==USTATIC ) return;
		break;

	case TYPEDEF:
		if( scl == class ) return;
		break;

	case UFORTRAN:
		if( scl == UFORTRAN || scl == FORTRAN ) return;
		break;

	case FORTRAN:
		if( scl == UFORTRAN ){
			p->sclass = FORTRAN;
			if( ISFTN(type) ) curftn = idp;
			return;
			}
		break;

	case MOU:
	case MOS:
		if( scl == class ) {
			if( oalloc( p, &strucoff ) ) break;
			if( class == MOU ) strucoff = 0;
			psave( idp );
			return;
			}
		break;

	case MOE:
		break;

	case EXTDEF:
		switch (scl) {
		case EXTERN:
			p->sclass = EXTDEF;
			if (ISFTN(type))
				curftn = idp;
			return;
		case USTATIC:
			p->sclass = STATIC;
			if (ISFTN(type))
				curftn = idp;
			return;
		}
		break;

	case STNAME:
	case UNAME:
	case ENAME:
		if( scl != class ) break;
		if( dimtab[p->sizoff] == 0 ) return;  /* previous entry just a mention */
		break;

	case PARAM:
	case AUTO:
	case REGISTER:
		;  /* mismatch.. */

		}

	mismatch:
	/* allow nonunique structure/union member names */

	if( class==MOU || class==MOS || class & FIELD ){/* make a new entry */
		int *memp;
		p->sflags |= SNONUNIQ;  /* old entry is nonunique */
		/* determine if name has occurred in this structure/union */
		if (paramno > 0) for( memp = &paramstk[paramno-1];
			/* while */ *memp>=0 && stab[*memp].sclass != STNAME
				&& stab[*memp].sclass != UNAME;
			/* iterate */ --memp){ char *cname, *oname;
			if( stab[*memp].sflags & SNONUNIQ ){
				cname=p->sname;
				oname=stab[*memp].sname;
				if (cname != oname) goto diff;
				uerror("redeclaration of: %s",p->sname);
				break;
				diff: continue;
				}
			}
		p = mknonuniq( &idp ); /* update p and idp to new entry */
		goto enter;
		}
	if (blevel > slev && class != EXTERN && class != FORTRAN &&
	    class != UFORTRAN) {
		q->tn.rval = idp = hide( p );
		p = &stab[idp];
		goto enter;
	}
	uerror( "redeclaration of %s", p->sname );
	if( class==EXTDEF && ISFTN(type) ) curftn = idp;
	return;

	enter:  /* make a new entry */

# ifndef BUG1
	if( ddebug ) printf( "	new entry made\n" );
# endif
	if( type == UNDEF ) uerror("void type for %s",p->sname);
	p->stype = type;
	p->sclass = class;
	p->slevel = blevel;
	p->offset = NOOFFSET;
	p->suse = lineno;
	if( class == STNAME || class == UNAME || class == ENAME ) {
		p->sizoff = curdim;
		dstash( 0 );  /* size */
		dstash( -1 ); /* index to members of str or union */
		dstash( ALSTRUCT );  /* alignment */
		dstash( idp );
		}
	else {
		switch( BTYPE(type) ){
		case STRTY:
		case UNIONTY:
		case ENUMTY:
			p->sizoff = q->fn.csiz;
			break;
		default:
			p->sizoff = BTYPE(type);
			}
		}

	/* copy dimensions */

	p->dimoff = q->fn.cdim;

	/* allocate offsets */
	if( class&FIELD ){
		(void) falloc( p, class&FLDSIZ, 0, NIL );  /* new entry */
		psave( idp );
		}
	else switch( class ){

	case AUTO:
		(void) oalloc( p, &autooff );
		break;
	case STATIC:
	case EXTDEF:
		p->offset = getlab();
		if( ISFTN(type) ) curftn = idp;
		break;

	case EXTERN:
	case UFORTRAN:
	case FORTRAN:
		p->offset = getlab();
		p->slevel = 0;
		break;
	case MOU:
	case MOS:
		(void) oalloc( p, &strucoff );
		if( class == MOU ) strucoff = 0;
		psave( idp );
		break;

	case MOE:
		p->offset = strucoff++;
		psave( idp );
		break;
	case REGISTER:
		p->offset = regvar--;
		if( blevel == 1 ) p->sflags |= SSET;
		if( regvar < minrvar ) minrvar = regvar;
		break;
		}

	{
		int l = p->slevel;

		if( l >= MAXSCOPES )
			cerror( "scopes nested too deep" );

		p->snext = schain[l];
		schain[l] = p;
		if( l >= chaintop )
			chaintop = l + 1;
		}

	/* user-supplied routine to fix up new definitions */

	FIXDEF(p);

# ifndef BUG1
	if( ddebug ) printf( "	dimoff, sizoff, offset: %d, %d, %d\n", p->dimoff, p->sizoff, p->offset );
# endif

	}

void
psave(int i)
{
	if( paramno >= PARAMSZ ){
		cerror( "parameter stack overflow");
	}
	paramstk[ paramno++ ] = i;
}

/*
 * end of function
 */
void
ftnend()
{
	if( retlab != NOLAB && nerrors == 0 ){ /* inside a real function */
		efcode();
	}
	checkst(0);
	retstat = 0;
	tcheck();
	brklab = contlab = retlab = NOLAB;
	flostat = 0;
	if( nerrors == 0 ){
		if( psavbc != & asavbc[0] ) cerror("bcsave error");
		if( paramno != 0 ) cerror("parameter reset error");
		if( swx != 0 ) cerror( "switch error");
		}
	psavbc = &asavbc[0];
	paramno = 0;
	autooff = AUTOINIT;
	minrvar = regvar = MAXRVAR;
	reached = 1;
	swx = 0;
	swp = swtab;
	(void) locctr(DATA);
}

void
dclargs()
{
	int i, j;
	struct symtab *p;
	NODE *q;
	argoff = ARGINIT;

# ifndef BUG1
	if (ddebug > 2)
		printf("dclargs()\n");
# endif
	for (i = 0; i < paramno; ++i) {
		if ((j = paramstk[i]) < 0)
			continue;
		p = &stab[j];
# ifndef BUG1
		if (ddebug > 2) {
			printf("\t%s (%d) ",p->sname, j);
			tprint(p->stype);
			printf("\n");
		}
# endif
		if (p->stype == FARG) {
			q = block(FREE,NIL,NIL,INT,0,INT);
			q->tn.rval = j;
			defid(q, PARAM);
		}
		FIXARG(p); /* local arg hook, eg. for sym. debugger */
	  /* always set aside space, even for register arguments */
		oalloc(p, &argoff);
	}
	cendarg();
	(void) locctr(PROG);
	defalign(ALINT);
	ftnno = getlab();
	bfcode(paramstk, paramno);
	paramno = 0;
}

/*
 * reference to a structure or union, with no definition
 */
NODE *
rstruct(int idn, int soru)
{
	struct symtab *p;
	NODE *q;
	p = &stab[idn];
	switch (p->stype) {

	case UNDEF:
	def:
		q = block(FREE, NIL, NIL, 0, 0, 0);
		q->tn.rval = idn;
		q->in.type = (soru&INSTRUCT) ? STRTY :
		    ((soru&INUNION) ? UNIONTY : ENUMTY);
		defid(q, (soru&INSTRUCT) ? STNAME :
		    ((soru&INUNION) ? UNAME : ENAME));
		break;

	case STRTY:
		if (soru & INSTRUCT)
			break;
		goto def;

	case UNIONTY:
		if (soru & INUNION)
			break;
		goto def;

	case ENUMTY:
		if (!(soru&(INUNION|INSTRUCT)))
			break;
		goto def;

	}
	stwart = instruct;
	return(mkty(p->stype, 0, p->sizoff));
}

void
moedef(int idn)
{
	NODE *q;

	q = block( FREE, NIL, NIL, MOETY, 0, 0 );
	q->tn.rval = idn;
	if( idn>=0 ) defid( q, MOE );
}

/*
 * begining of structure or union declaration
 */
int
bstruct(int idn, int soru)
{
	NODE *q;

	psave(instruct);
	psave(strunem);
	psave(strucoff);
	strucoff = 0;
	instruct = soru;
	q = block(FREE, NIL, NIL, 0, 0, 0);
	q->tn.rval = idn;
	if (instruct==INSTRUCT) {
		strunem = MOS;
		q->in.type = STRTY;
		if (idn >= 0)
			defid(q, STNAME);
	} else if(instruct == INUNION) {
		strunem = MOU;
		q->in.type = UNIONTY;
		if (idn >= 0)
			defid(q, UNAME);
	} else { /* enum */
		strunem = MOE;
		q->in.type = ENUMTY;
		if (idn >= 0)
			defid(q, ENAME);
	}
	psave(idn = q->tn.rval);
	/* the "real" definition is where the members are seen */
	if (idn >= 0)
		stab[idn].suse = lineno;
	return(paramno-4);
}

/*
 * Called after a struct is declared to restore the environment.
 */
NODE *
dclstruct(int oparam)
{
	struct symtab *p;
	int i, al, sa, j, sz, szindex;
	TWORD temp;
	int high, low;

	/*
	 * paramstk contains:
	 *	paramstk[oparam] = previous instruct
	 *	paramstk[oparam+1] = previous class
	 *	paramstk[oparam+2] = previous strucoff
	 *	paramstk[oparam+3] = structure name
	 *
	 *	paramstk[oparam+4, ...]  = member stab indices
	 */


	if ((i = paramstk[oparam+3]) < 0) {
		szindex = curdim;
		dstash(0);  /* size */
		dstash(-1);  /* index to member names */
		dstash(ALSTRUCT);  /* alignment */
		dstash(-lineno);	/* name of structure */
	} else
		szindex = stab[i].sizoff;

# ifndef BUG1
	if (ddebug) {
		printf("dclstruct( %s ), szindex = %d\n",
		    (i>=0)? stab[i].sname : "??", szindex);
	}
# endif
	temp = (instruct&INSTRUCT)?STRTY:((instruct&INUNION)?UNIONTY:ENUMTY);
	stwart = instruct = paramstk[oparam];
	strunem = paramstk[oparam+1];
	dimtab[szindex+1] = curdim;
	al = ALSTRUCT;

	high = low = 0;

	for( i = oparam+4;  i< paramno; ++i ){
		dstash( j=paramstk[i] );
		if( j<0 || j>= SYMTSZ ) cerror( "gummy structure member" );
		p = &stab[j];
		if( temp == ENUMTY ){
			if( p->offset < low ) low = p->offset;
			if( p->offset > high ) high = p->offset;
			p->sizoff = szindex;
			continue;
			}
		sa = talign( p->stype, p->sizoff );
		if (p->sclass & FIELD) {
			sz = p->sclass&FLDSIZ;
		} else {
			sz = tsize(p->stype, p->dimoff, p->sizoff);
		}
#if 0
		if (sz == 0) {
			werror("illegal zero sized structure member: %s", p->sname );
		}
#endif
		if (sz > strucoff)
			strucoff = sz;  /* for use with unions */
		/*
		 * set al, the alignment, to the lcm of the alignments
		 * of the members.
		 */
		SETOFF(al, sa);
	}
	dstash(-1);  /* endmarker */
	SETOFF( strucoff, al );

	if( temp == ENUMTY ){
		TWORD ty;

# ifdef ENUMSIZE
		ty = ENUMSIZE(high,low);
# else
		if( (char)high == high && (char)low == low ) ty = ctype( CHAR );
		else if( (short)high == high && (short)low == low ) ty = ctype( SHORT );
		else ty = ctype(INT);
#endif
		strucoff = tsize( ty, 0, (int)ty );
		dimtab[ szindex+2 ] = al = talign( ty, (int)ty );
	}

#if 0
	if (strucoff == 0)
		uerror( "zero sized structure" );
#endif
	dimtab[szindex] = strucoff;
	dimtab[szindex+2] = al;
	dimtab[szindex+3] = paramstk[ oparam+3 ];  /* name index */

	FIXSTRUCT(szindex, oparam); /* local hook, eg. for sym debugger */
# ifndef BUG1
	if (ddebug>1) {
		printf("\tdimtab[%d,%d,%d] = %d,%d,%d\n",
		    szindex,szindex+1,szindex+2,
		    dimtab[szindex],dimtab[szindex+1],dimtab[szindex+2] );
		for (i = dimtab[szindex+1]; dimtab[i] >= 0; ++i) {
			printf("\tmember %s(%d)\n",
			    stab[dimtab[i]].sname, dimtab[i]);
		}
	}
# endif

	strucoff = paramstk[ oparam+2 ];
	paramno = oparam;

	return( mkty( temp, 0, szindex ) );
}

/*
 * error printing routine in parser
 */
void yyerror(char *s);
void
yyerror(char *s)
{
	uerror(s);
}

void yyaccpt(void);
void
yyaccpt(void)
{
	ftnend();
}

void
ftnarg(int idn)
{
	switch( stab[idn].stype ){

	case UNDEF:
		/* this parameter, entered at scan */
		break;
	case FARG:
		uerror("redeclaration of formal parameter, %s",
			stab[idn].sname);
		/* fall thru */
	case FTN:
		/* the name of this function matches parm */
		/* fall thru */
	default:
		idn = hide( &stab[idn]);
		break;
	case TNULL:
		/* unused entry, fill it */
		;
		}
	stab[idn].stype = FARG;
	stab[idn].sclass = PARAM;
	psave( idn );
}

/*
 * compute the alignment of an object with type ty, sizeoff index s
 */
int
talign(unsigned int ty, int s)
{
	int i;

	if( s<0 && ty!=INT && ty!=CHAR && ty!=SHORT && ty!=UNSIGNED && ty!=UCHAR && ty!=USHORT 
					){
		return( fldal( ty ) );
		}

	for( i=0; i<=(SZINT-BTSHIFT-1); i+=TSHIFT ){
		switch( (ty>>i)&TMASK ){

		case FTN:
			uerror( "can't assign to function" );
			return( ALCHAR );
		case PTR:
			return( ALPOINT );
		case ARY:
			continue;
		case 0:
			break;
			}
		}

	switch( BTYPE(ty) ){

	case UNIONTY:
	case ENUMTY:
	case STRTY:
		return( (unsigned int) dimtab[ s+2 ] );
	case CHAR:
	case UCHAR:
		return (ALCHAR);
	case FLOAT:
		return (ALFLOAT);
	case DOUBLE:
		return (ALDOUBLE);
	case LONGLONG:
	case ULONGLONG:
		return (ALLONGLONG);
	case LONG:
	case ULONG:
		return (ALLONG);
	case SHORT:
	case USHORT:
		return (ALSHORT);
	default:
		return (ALINT);
		}
	}

OFFSZ
tsize( ty, d, s )  TWORD ty; {
	/* compute the size associated with type ty,
	    dimoff d, and sizoff s */
	/* BETTER NOT BE CALLED WHEN t, d, and s REFER TO A BIT FIELD... */

	int i;
	OFFSZ mult;

	mult = 1;

	for( i=0; i<=(SZINT-BTSHIFT-1); i+=TSHIFT ){
		switch( (ty>>i)&TMASK ){

		case FTN:
			/* cerror( "compiler takes size of function"); */
			uerror( "can't take size of function" );
			return( SZCHAR );
		case PTR:
			return( SZPOINT * mult );
		case ARY:
			mult *= (unsigned int) dimtab[ d++ ];
			continue;
		case 0:
			break;

			}
		}

	if (ty != STRTY && ty != UNIONTY) {
		if (dimtab[s] == 0) {
			uerror("unknown size");
			return(SZINT);
		}
	} else {
		if (dimtab[s+1] == -1)
			uerror("unknown structure/union");
	}

	return( (unsigned int) dimtab[ s ] * mult );
}

/*
 * force inoff to have the value n
 */
void
inforce(OFFSZ n)
{
	/* inoff is updated to have the value n */
	OFFSZ wb;
	int rest;
	/* rest is used to do a lot of conversion to ints... */

	if( inoff == n ) return;
	if (inoff > n)
		cerror("initialization alignment error: inoff %lld n %lld",
		    inoff, n);

	wb = inoff;
	SETOFF( wb, SZINT );

	/* wb now has the next higher word boundary */

	if( wb >= n ){ /* in the same word */
		rest = n - inoff;
		vfdzero( rest );
		return;
		}

	/* otherwise, extend inoff to be word aligned */

	rest = wb - inoff;
	vfdzero( rest );

	/* now, skip full words until near to n */

	rest = (n-inoff)/SZINT;
	zecode( rest );

	/* now, the remainder of the last word */

	rest = n-inoff;
	vfdzero( rest );
	if( inoff != n ) cerror( "inoff error");

	}

/*
 * make inoff have the offset the next alignment of n
 */
void
vfdalign(int n)
{
	OFFSZ m;

	m = inoff;
	SETOFF( m, n );
	inforce( m );
}


int idebug = 0;

int ibseen = 0;  /* the number of } constructions which have been filled */

int ifull = 0; /* 1 if all initializers have been seen */

int iclass;  /* storage class of thing being initialized */

int ilocctr = 0;  /* location counter for current initialization */

/*
 * beginning of initilization; set location ctr and set type
 */
void
beginit(int curid, int class)
{
	struct symtab *p;

# ifndef BUG1
	if( idebug >= 3 ) printf( "beginit(), curid = %d\n", curid );
# endif

	p = &stab[curid];

	iclass = p->sclass;
	if( class == EXTERN || class == FORTRAN )
		iclass = EXTERN;
	switch( iclass ){

	case UNAME:
	case EXTERN:
		return;
	case AUTO:
	case REGISTER:
		break;
	case EXTDEF:
	case STATIC:
		ilocctr = ISARY(p->stype)?ADATA:DATA;
		if( nerrors == 0 ){
			(void) locctr( ilocctr );
			defalign( talign( p->stype, p->sizoff ) );
			defnam( p );
		}
	}

	inoff = 0;
	ibseen = 0;
	ifull = 0;

	pstk = 0;

	instk( curid, p->stype, p->dimoff, p->sizoff, inoff );

}

/*
 * make a new entry on the parameter stack to initialize id
 */
void
instk(int id, TWORD t, int d, int s, OFFSZ off)
{
	struct symtab *p;

	for (;;) {
# ifndef BUG1
		if (idebug)
			printf("instk((%d, %o,%d,%d, %lld)\n",
			    id, t, d, s, (long long)off);
# endif

		/* save information on the stack */

		if (!pstk)
			pstk = instack;
		else
			++pstk;

		pstk->in_fl = 0;	/* { flag */
		pstk->in_id = id;
		pstk->in_t = t;
		pstk->in_d = d;
		pstk->in_s = s;
		pstk->in_n = 0;  /* number seen */
		pstk->in_x = (t == STRTY || t == UNIONTY) ? dimtab[s+1] : 0 ;
		pstk->in_off = off;/* offset at the beginning of this element */

		/* if t is an array, DECREF(t) can't be a field */
		/* in_sz has size of array elements, and -size for fields */
		if (ISARY(t)) {
			pstk->in_sz = tsize(DECREF(t), d+1, s);
		} else if (stab[id].sclass & FIELD){
			pstk->in_sz = - (stab[id].sclass & FLDSIZ);
		} else {
			pstk->in_sz = 0;
		}

		if ((iclass==AUTO || iclass == REGISTER) &&
		    (ISARY(t) || t==STRTY))
			uerror("no automatic aggregate initialization");

		/* now, if this is not a scalar, put on another element */

		if (ISARY(t)) {
			t = DECREF(t);
			++d;
			continue;
		} else if (t == STRTY || t == UNIONTY) {
			if (dimtab[pstk->in_s] == 0) {
				uerror("can't initialize undefined %s",
				    t == STRTY ? "structure" : "union");
				iclass = -1;
				return;
			}
			id = dimtab[pstk->in_x];
			p = &stab[id];
			if (((p->sclass != MOS && t == STRTY) ||
			    (p->sclass != MOU && t == UNIONTY)) &&
			    !(p->sclass&FIELD))
				cerror("insane %s member list",
				    t == STRTY ? "structure" : "union");
			t = p->stype;
			d = p->dimoff;
			s = p->sizoff;
			off += p->offset;
			continue;
		} else
			return;
	}
}

#define	MAXNSTRING	1000
static char *strarray[MAXNSTRING];
static int labarray[MAXNSTRING];
static int nstring;

/*
 * Write last part of string.
 */
NODE *
strend(char *str)
{
	int lxarg, i, val, strtemp, strlab;
	char *wr = str;
	NODE *p;

	i = 0;
	if ((iclass == EXTDEF || iclass==STATIC) &&
	    (pstk->in_t == CHAR || pstk->in_t == UCHAR) &&
	    pstk != instack && ISARY(pstk[-1].in_t)) {
		/* treat "abc" as { 'a', 'b', 'c', 0 } */
		ilbrace();  /* simulate { */
		inforce(pstk->in_off);
		/*
		 * if the array is inflexible (not top level),
		 * pass in the size and be prepared to throw away
		 * unwanted initializers
		 */

		lxarg = (pstk-1) != instack ? dimtab[(pstk-1)->in_d] : 0;
		while (*wr != 0) {
			if (*wr++ == '\\')
				val = esccon(&wr);
			else
				val = wr[-1];
			if (lxarg == 0 || i < lxarg)
				putbyte(val);
			else if (i == lxarg)
				werror("non-null byte ignored in string"
				    "initializer");
			i++;
		}

		if (lxarg == 0 || i < lxarg)
			putbyte(0);
		irbrace();  /* simulate } */
		free(str);
		return(NIL);
	}
	/* make a label, and get the contents and stash them away */
	if (iclass != SNULL) { /* initializing */
		/* fill out previous word, to permit pointer */
		vfdalign(ALPOINT);
	}

	if (isinlining)
		goto inl;

	/* If an identical string is already emitted, just forget this one */
	for (i = 0; i < nstring; i++) {
		if (strarray[i][0] == *wr && strcmp(strarray[i], wr) == 0)
			break;
	}
	if (i == nstring) { /* No string */
		if (nstring == MAXNSTRING) {
			cerror("out of string space");
			nstring = 0;
		}
		 /* set up location counter */
inl:		strtemp = locctr(blevel==0 ? ISTRNG : STRNG);
		deflab(strlab = getlab());
		if (isinlining == 0) {
			strarray[nstring] = str;
			labarray[nstring] = strlab;
		}
		i = 0;
		while (*wr != 0) {
			if (*wr++ == '\\')
				val = esccon(&wr);
			else
				val = wr[-1];
			bycode(val, i);
			i++;
		}
		bycode(0, i++);
		bycode(-1, i);
		(void) locctr(blevel==0 ? ilocctr : strtemp);
		if (isinlining == 0)
			nstring++;
	} else {
		strlab = labarray[i];
		i = strlen(strarray[i]);
		free(str);
	}

	dimtab[curdim] = i; /* in case of later sizeof ... */
	p = buildtree(STRING, NIL, NIL);
	p->tn.rval = -strlab;
	return(p);
}

/*
 * simulate byte v appearing in a list of integer values
 */
void
putbyte(int v)
{
	NODE *p;
	p = bcon(v);
	incode( p, SZCHAR );
	tfree( p );
	gotscal();
}

void
endinit(void)
{
	TWORD t;
	int d, s, n, d1;

# ifndef BUG1
	if (idebug)
		printf("endinit(), inoff = %lld\n", (long long)inoff);
# endif

	switch( iclass ){

	case EXTERN:
	case AUTO:
	case REGISTER:
	case -1:
		return;
		}

	pstk = instack;

	t = pstk->in_t;
	d = pstk->in_d;
	s = pstk->in_s;
	n = pstk->in_n;

	if( ISARY(t) ){
		d1 = dimtab[d];

		vfdalign( pstk->in_sz );  /* fill out part of the last element, if needed */
		n = inoff/pstk->in_sz;  /* real number of initializers */
		if( d1 >= n ){
			/* once again, t is an array, so no fields */
			inforce( tsize( t, d, s ) );
			n = d1;
			}
		if( d1!=0 && d1!=n ) uerror( "too many initializers");
		if( n==0 ) werror( "empty array declaration");
		dimtab[d] = n;
		if (d1==0) {
			FIXDEF(&stab[pstk->in_id]);
		}
	}

	else if( t == STRTY || t == UNIONTY ){
		/* clearly not fields either */
		inforce( tsize( t, d, s ) );
		}
	else if( n > 1 ) uerror( "bad scalar initialization");
	/* this will never be called with a field element... */
	else inforce( tsize(t,d,s) );

	paramno = 0;
	vfdalign( AL_INIT );
	inoff = 0;
	iclass = SNULL;

}

/*
 * called from the grammar if we must punt during initialization
 * stolen from endinit()
 */
void
fixinit(void)
{
	pstk = instack;
	paramno = 0;
	vfdalign( AL_INIT );
	inoff = 0;
	iclass = SNULL;
}

/*
 * take care of generating a value for the initializer p
 * inoff has the current offset (last bit written)
 * in the current word being generated
 */
void
doinit(NODE *p)
{
	int sz, d, s;
	TWORD t;
	int o;

	/* note: size of an individual initializer is assumed to fit into an int */

	if( iclass < 0 ) goto leave;
	if( iclass == EXTERN || iclass == UNAME ){
		uerror( "cannot initialize extern or union" );
		iclass = -1;
		goto leave;
		}

	if( iclass == AUTO || iclass == REGISTER ){
		/* do the initialization and get out, without regard 
		    for filing out the variable with zeros, etc. */
		bccode();
		idname = pstk->in_id;
		p = buildtree( ASSIGN, buildtree( NAME, NIL, NIL ), p );
		ecomp(p);
		return;
		}

	if( p == NIL ) return;  /* for throwing away strings that have been turned into lists */

	if( ifull ){
		uerror( "too many initializers" );
		iclass = -1;
		goto leave;
		}
	if( ibseen ){
		uerror( "} expected");
		goto leave;
		}

# ifndef BUG1
	if (idebug > 1)
		printf("doinit(%p)\n", p);
# endif

	t = pstk->in_t;  /* type required */
	d = pstk->in_d;
	s = pstk->in_s;
	if (pstk->in_sz < 0) {  /* bit field */
		sz = -pstk->in_sz;
	} else {
		sz = tsize( t, d, s );
	}

	inforce( pstk->in_off );

	p = buildtree( ASSIGN, block( NAME, NIL,NIL, t, d, s ), p );
	p->in.left->in.op = FREE;
	p->in.left = p->in.right;
	p->in.right = NIL;
	p->in.left = optim( p->in.left );
	o = p->in.left->in.op;
	if( o == UNARY AND ){
		o = p->in.left->in.op = FREE;
		p->in.left = p->in.left->in.left;
		}
	p->in.op = INIT;

	if( sz < SZINT ){ /* special case: bit fields, etc. */
		if( o != ICON || p->in.left->tn.rval != NONAME )
			uerror( "illegal initialization" );
		else
			incode( p->in.left, sz );
	} else if( o == FCON ){
		fincode( p->in.left->fpn.fval, sz );
	} else if( o == DCON ){
		fincode( p->in.left->dpn.dval, sz );
	} else {
		p = optim(p);
		if( p->in.left->in.op != ICON )
			uerror( "illegal initialization" );
		else
			cinit( p, sz );
	}

	gotscal();

	leave:
	tfree(p);
}

void
gotscal(void)
{
	int t, ix;
	int n, id;
	struct symtab *p;
	OFFSZ temp;

	for( ; pstk > instack; ) {

		if( pstk->in_fl ) ++ibseen;

		--pstk;
		
		t = pstk->in_t;

		if( t == STRTY || t == UNIONTY){
			ix = ++pstk->in_x;
			if( (id=dimtab[ix]) < 0 ) continue;

			/* otherwise, put next element on the stack */

			p = &stab[id];
			instk( id, p->stype, p->dimoff, p->sizoff, p->offset+pstk->in_off );
			return;
			}
		else if( ISARY(t) ){
			n = ++pstk->in_n;
			if( n >= dimtab[pstk->in_d] && pstk > instack ) continue;

			/* put the new element onto the stack */

			temp = pstk->in_sz;
			instk( pstk->in_id, (TWORD)DECREF(pstk->in_t), pstk->in_d+1, pstk->in_s,
				pstk->in_off+n*temp );
			return;
			}

		}
	ifull = 1;
	}

/*
 * process an initializer's left brace
 */
void
ilbrace()
{
	int t;
	struct instk *temp;

	temp = pstk;

	for (; pstk > instack; --pstk) {

		t = pstk->in_t;
		if (t != UNIONTY && t != STRTY && !ISARY(t))
			continue; /* not an aggregate */
		if (pstk->in_fl) { /* already associated with a { */
			if (pstk->in_n)
				uerror( "illegal {");
			continue;
		}

		/* we have one ... */
		pstk->in_fl = 1;
		break;
	}

	/* cannot find one */
	/* ignore such right braces */

	pstk = temp;
}

/*
 * called when a '}' is seen
 */
void
irbrace()
{
# ifndef BUG1
	if (idebug)
		printf( "irbrace(): paramno = %d on entry\n", paramno );
# endif

	if (ibseen) {
		--ibseen;
		return;
	}

	for (; pstk > instack; --pstk) {
		if(!pstk->in_fl)
			continue;

		/* we have one now */

		pstk->in_fl = 0;  /* cancel { */
		gotscal();  /* take it away... */
		return;
	}

	/* these right braces match ignored left braces: throw out */
	ifull = 1;
}

/*
 * update the offset pointed to by poff; return the
 * offset of a value of size `size', alignment `alignment',
 * given that off is increasing
 */
int
upoff(int size, int alignment, int *poff)
{
	int off;

	off = *poff;
	SETOFF(off, alignment);
	if ((offsz-off) <  size) {
		if (instruct != INSTRUCT)
			cerror("too many local variables");
		else
			cerror("Structure too large");
	}
	*poff = off+size;
	return (off);
}

/*
 * allocate p with offset *poff, and update *poff
 */
int
oalloc(struct symtab *p, int *poff )
{
	int al, off, tsz;
	int noff;

	al = talign(p->stype, p->sizoff);
	noff = off = *poff;
	tsz = tsize(p->stype, p->dimoff, p->sizoff);
#ifdef BACKAUTO
	if (p->sclass == AUTO) {
		if ((offsz-off) < tsz)
			cerror("too many local variables");
		noff = off + tsz;
		SETOFF(noff, al);
		off = -noff;
	} else
#endif
#ifdef PARAMS_UPWARD
	if (p->sclass == PARAM) {
		if ((offsz-off) < tsz)
			cerror("too many parameters");
		noff = off + tsz;
		if (tsz < SZINT)
			al = ALINT;
		SETOFF(noff, al);
		off = -noff;

	} else
#endif
	if (p->sclass == PARAM && (tsz < SZINT)) {
		off = upoff(SZINT, ALINT, &noff);
#ifndef RTOLBYTES
		off = noff - tsz;
#endif
	} else {
		off = upoff(tsz, al, &noff);
	}

	if (p->sclass != REGISTER) {
	/* in case we are allocating stack space for register arguments */
		if (p->offset == NOOFFSET)
			p->offset = off;
		else if(off != p->offset)
			return(1);
	}

	*poff = noff;
	return(0);
}

/*
 * allocate a field of width w
 * new is 0 if new entry, 1 if redefinition, -1 if alignment
 */
int
falloc(struct symtab *p, int w, int new, NODE *pty)
{
	int al,sz,type;

	type = (new<0)? pty->in.type : p->stype;

	/* this must be fixed to use the current type in alignments */
	switch( new<0?pty->in.type:p->stype ){

	case ENUMTY:
		{
			int s;
			s = new<0 ? pty->fn.csiz : p->sizoff;
			al = dimtab[s+2];
			sz = dimtab[s];
			break;
			}

	case CHAR:
	case UCHAR:
		al = ALCHAR;
		sz = SZCHAR;
		break;

	case SHORT:
	case USHORT:
		al = ALSHORT;
		sz = SZSHORT;
		break;

	case INT:
	case UNSIGNED:
		al = ALINT;
		sz = SZINT;
		break;

	default:
		if( new < 0 ) {
			uerror( "illegal field type" );
			al = ALINT;
			}
		else {
			al = fldal( p->stype );
			sz =SZINT;
			}
		}

	if( w > sz ) {
		uerror( "field too big");
		w = sz;
		}

	if( w == 0 ){ /* align only */
		SETOFF( strucoff, al );
		if( new >= 0 ) uerror( "zero size field");
		return(0);
		}

	if( strucoff%al + w > sz ) SETOFF( strucoff, al );
	if( new < 0 ) {
		if( (offsz-strucoff) < w )
			cerror("structure too large");
		strucoff += w;  /* we know it will fit */
		return(0);
		}

	/* establish the field */

	if( new == 1 ) { /* previous definition */
		if( p->offset != strucoff || p->sclass != (FIELD|w) ) return(1);
		}
	p->offset = strucoff;
	if( (offsz-strucoff) < w ) cerror("structure too large");
	strucoff += w;
	p->stype = type;
	fldty( p );
	return(0);
}

/*
 * handle unitialized declarations
 * assumed to be not functions
 */
void
nidcl(NODE *p, int class)
{
	int commflag;  /* flag for labelled common declarations */

	commflag = 0;

	/* compute class */
	if (class == SNULL) {
		if (blevel > 1)
			class = AUTO;
		else if (blevel != 0 || instruct)
			cerror( "nidcl error" );
		else { /* blevel = 0 */
			class = noinit();
			if (class == EXTERN)
				commflag = 1;
		}
	}
#ifdef LCOMM
	/* hack so stab will come out as LCSYM rather than STSYM */
	if (class == STATIC) {
		extern int stabLCSYM;
		stabLCSYM = 1;
	}
#endif

	defid(p, class);

	/* if an array is not initialized, no empty dimension */
	if (class != EXTERN && class != TYPEDEF &&
	    ISARY(p->in.type) && dimtab[p->fn.cdim]==0)
		uerror("null storage definition");

#ifndef LCOMM
	if (class==EXTDEF || class==STATIC)
#else
	if (class==STATIC) {
		struct symtab *s = &stab[p->tn.rval];
		extern int stabLCSYM;
		int sz = tsize(s->stype, s->dimoff, s->sizoff)/SZCHAR;
		
		stabLCSYM = 0;
		if (sz % sizeof (int))
			sz += sizeof (int) - (sz % sizeof (int));
		if (s->slevel > 1)
			printf("	.lcomm	L%d,%d\n", s->offset, sz);
		else
			printf("	.lcomm	%s,%d\n", exname(s->sname), sz);
	} else if (class == EXTDEF)
#endif
	{
		/* simulate initialization by 0 */
		beginit(p->tn.rval, class);
		endinit();
	}
	if (commflag)
		commdec(p->tn.rval);
}

/*
 * Merges a type tree into one type. Returns one type node with merged types
 * and class stored in the su field. Frees all other nodes.
 * XXX - classes in typedefs?
 */
NODE *
typenode(NODE *p)
{
	int class = 0, adj, noun, sign;

	adj = INT;	/* INT, LONG or SHORT */
	noun = UNDEF;	/* INT, CHAR or FLOAT */
	sign = 0;	/* 0, SIGNED or UNSIGNED */

	/* Remove initial QUALIFIERs */
	if (p && p->in.op == QUALIFIER) {
		p->in.op = FREE;
		p = p->in.left;
	}

	/* Handle initial classes special */
	if (p && p->in.op == CLASS) {
		class = p->in.type;
		p->in.op = FREE;
		p = p->in.left;
	}

	/* Remove more QUALIFIERs */
	if (p && p->in.op == QUALIFIER) {
		p->in.op = FREE;
		p = p->in.left;
	}

	if (p && p->in.op == TYPE && p->in.left == NIL) {
#ifdef CHAR_UNSIGNED
		if (p->in.type == CHAR)
			p->in.type = UCHAR;
#endif
		p->in.su = class;
		return p;
	}

	while (p != NIL) { 
		if (p->in.op == QUALIFIER) /* Skip const/volatile */
			goto next;
		if (p->in.op == CLASS) {
			if (class != 0)
				uerror("too many storage classes");
			class = p->in.type;
			goto next;
		}
		if (p->in.op != TYPE)
			cerror("typenode got notype %d", p->in.op);
		switch (p->in.type) {
		case SIGNED:
		case UNSIGNED:
			if (sign != 0)
				goto bad;
			sign = p->in.type;
			break;
		case LONG:
			if (adj == LONG) {
				adj = LONGLONG;
				break;
			}
			/* FALLTHROUGH */
		case SHORT:
			if (adj != INT)
				goto bad;
			adj = p->in.type;
			break;
		case INT:
		case CHAR:
		case FLOAT:
			if (noun != UNDEF)
				goto bad;
			noun = p->in.type;
			break;
		default:
			goto bad;
		}
	next:
		p->in.op = FREE;
		p = p->in.left;
	}

#ifdef CHAR_UNSIGNED
	if (noun == CHAR && sign == 0)
		sign = UNSIGNED;
#endif
	if (noun == UNDEF) {
		noun = INT;
	} else if (noun == FLOAT) {
		if (sign != 0 || adj == SHORT)
			goto bad;
		noun = (adj == LONG ? DOUBLE : FLOAT);
	} else if (noun == CHAR && adj != INT)
		goto bad;

	if (adj != INT)
		noun = adj;
	if (sign == UNSIGNED)
		noun += (UNSIGNED-INT);

	p = block(TYPE, NIL, NIL, noun, 0, 0);
	if (strunem != 0)
		class = strunem;
	p->in.su = class;
	return p;

bad:	uerror("illegal type combination");
	return mkty(INT, 0, 0);
}

NODE *
tymerge( typ, idp ) NODE *typ, *idp; {
	/* merge type typ with identifier idp  */

	unsigned int t;
	int i;

	if( typ->in.op != TYPE ) cerror( "tymerge: arg 1" );
	if(idp == NIL ) return( NIL );

# ifndef BUG1
	if( ddebug > 2 ) fwalk( idp, eprint, 0 );
# endif

	idp->in.type = typ->in.type;
	idp->fn.cdim = curdim;
	tyreduce( idp );
	idp->fn.csiz = typ->fn.csiz;

	for( t=typ->in.type, i=typ->fn.cdim; t&TMASK; t = DECREF(t) ){
		if( ISARY(t) ) dstash( dimtab[i++] );
		}

	/* now idp is a single node: fix up type */

	idp->in.type = ctype( idp->in.type );

	if( (t = BTYPE(idp->in.type)) != STRTY && t != UNIONTY && t != ENUMTY ){
		idp->fn.csiz = t;  /* in case ctype has rewritten things */
		}

	return( idp );
	}

/*
 * build a type, and stash away dimensions, from a parse tree of the declaration
 * the type is build top down, the dimensions bottom up
 */
void
tyreduce(NODE *p)
{
	int o, temp;
	unsigned int t;

	o = p->in.op;
	p->in.op = FREE;

	if( o == NAME ) return;

	t = INCREF( p->in.type );
	if( o == UNARY CALL ) t += (FTN-PTR);
	else if( o == LB ){
		t += (ARY-PTR);
		temp = p->in.right->tn.lval;
		p->in.right->in.op = FREE;
		if( temp == 0 && p->in.left->tn.op == LB )
			uerror( "null dimension" );
		}

	p->in.left->in.type = t;
	tyreduce( p->in.left );

	if( o == LB ) dstash( temp );

	p->tn.rval = p->in.left->tn.rval;
	p->in.type = p->in.left->in.type;

	}

void
fixtype(NODE *p, int class)
{
	unsigned int t, type;
	int mod1, mod2;
	/* fix up the types, and check for legality */

	if( (type = p->in.type) == UNDEF ) return;
	if ((mod2 = (type&TMASK))) {
		t = DECREF(type);
		while( mod1=mod2, mod2 = (t&TMASK) ){
			if( mod1 == ARY && mod2 == FTN ){
				uerror( "array of functions is illegal" );
				type = 0;
				}
			else if( mod1 == FTN && ( mod2 == ARY || mod2 == FTN ) ){
				uerror( "function returns illegal type" );
				type = 0;
				}
			t = DECREF(t);
			}
		}

	/* detect function arguments, watching out for structure declarations */
	/* for example, beware of f(x) struct { int a[10]; } *x; { ... } */
	/* the danger is that "a" will be converted to a pointer */

	if( class==SNULL && blevel==1 && !(instruct&(INSTRUCT|INUNION)) )
		class = PARAM;
	if( class == PARAM || ( class==REGISTER && blevel==1 ) ){
		if( type == FLOAT ) type = DOUBLE;
		else if( ISARY(type) ){
			++p->fn.cdim;
			type += (PTR-ARY);
			}
		else if( ISFTN(type) ){
			werror( "a function is declared as an argument" );
			type = INCREF(type);
			}

		}

	if( instruct && ISFTN(type) ){
		uerror( "function illegal in structure or union" );
		type = INCREF(type);
		}
	p->in.type = type;
}

/*
 * give undefined version of class
 */
int
uclass(int class)
{
	if (class == SNULL)
		return(EXTERN);
	else if (class == STATIC)
		return(USTATIC);
	else if (class == FORTRAN)
		return(UFORTRAN);
	else
		return(class);
}

int
fixclass(int class, TWORD type)
{
	/* first, fix null class */
	if( class == SNULL ){
		if( instruct&INSTRUCT ) class = MOS;
		else if( instruct&INUNION ) class = MOU;
		else if( blevel == 0 ) class = EXTDEF;
		else if( blevel == 1 ) class = PARAM;
		else class = AUTO;

		}

	/* now, do general checking */

	if( ISFTN( type ) ){
		switch( class ) {
		default:
			uerror( "function has illegal storage class" );
		case AUTO:
			class = EXTERN;
		case EXTERN:
		case EXTDEF:
		case FORTRAN:
		case TYPEDEF:
		case STATIC:
		case UFORTRAN:
		case USTATIC:
			;
			}
		}

	if( class&FIELD ){
		if( !(instruct&INSTRUCT) ) uerror( "illegal use of field" );
		return( class );
		}

	switch( class ){

	case MOU:
		if( !(instruct&INUNION) ) uerror( "illegal MOU class" );
		return( class );

	case MOS:
		if( !(instruct&INSTRUCT) ) uerror( "illegal MOS class" );
		return( class );

	case MOE:
		if( instruct & (INSTRUCT|INUNION) ) uerror( "illegal MOE class" );
		return( class );

	case REGISTER:
		if( blevel == 0 ) uerror( "illegal register declaration" );
		else if( regvar >= MINRVAR && cisreg( type ) ) return( class );
		if( blevel == 1 ) return( PARAM );
		else return( AUTO );

	case AUTO:
		if( blevel < 2 ) uerror( "illegal ULABEL class" );
		return( class );

	case PARAM:
		if( blevel != 1 ) uerror( "illegal PARAM class" );
		return( class );

	case UFORTRAN:
	case FORTRAN:
# ifdef NOFORTRAN
			NOFORTRAN;    /* a condition which can regulate the FORTRAN usage */
# endif
		if( !ISFTN(type) ) uerror( "fortran declaration must apply to function" );
		else {
			type = DECREF(type);
			if( ISFTN(type) || ISARY(type) || ISPTR(type) ) {
				uerror( "fortran function has wrong type" );
				}
			}
	case EXTERN:
	case STATIC:
	case EXTDEF:
	case TYPEDEF:
	case USTATIC:
		if( blevel == 1 ){
			uerror( "illegal USTATIC class" );
			return( PARAM );
			}
	case STNAME:
	case UNAME:
	case ENAME:
		return( class );

	default:
		cerror( "illegal class: %d", class );
		/* NOTREACHED */

	}
	return 0; /* XXX */
}

#define	MAXLABELS	100	/* per function */
static int labelno[MAXLABELS];
static char *labelname[MAXLABELS];
static int nlabels;

static int
lentry(int id)
{
	int l;

	/* Free symtab entry if unused */
	if (stab[id].stype == UNDEF)
		stab[id].stype = TNULL;

	for (l = 0; l < nlabels; l++)
		if (labelname[l] == stab[id].sname)
			break;

	if (l == nlabels) {
		if (nlabels == MAXLABELS) {
			uerror("too many labels");
			nlabels = 0;
			return 0;
		}
		labelno[nlabels] = -getlab();
		labelname[nlabels] = stab[id].sname;
		l = nlabels++;
	}
	return l;
}

/*
 * Generates a goto statement; sets up label number etc.
 */
void
gotolabel(int id)
{
	int l = lentry(id);
	branch(labelno[l] < 0 ? -labelno[l] : labelno[l]);
}

/*
 * Sets a label for gotos.
 */
void
deflabel(int id)
{
	int l = lentry(id);

	if (labelno[l] > 0)
		uerror("label '%s' redefined", labelname[l]);
	labelno[l] = -labelno[l];
	locctr(PROG);
	deflab(labelno[l]);
}

/*
 * locate a symbol table entry for
 * an occurrence of a nonunique structure member name
 * or field
 */
struct symtab *
mknonuniq(int *idindex)
{
	extern int newsidx;
	int i;
	struct symtab * sp;
	char *q;

	sp = & stab[ i= *idindex ]; /* position search at old entry */
	while( sp->stype != TNULL ){ /* locate unused entry */
		if( ++i >= SYMTSZ ){/* wrap around symbol table */
			i = 0;
			sp = stab;
			}
		else ++sp;
		if( i == *idindex ) cerror("Symbol table full");
		}
	sp->sflags = SNONUNIQ | SMOS;
	q = stab[*idindex].sname; /* old entry name */
	sp->sname = stab[*idindex].sname;
# ifndef BUG1
	if( ddebug ){
		printf("\tnonunique entry for %s from %d to %d\n",
			q, *idindex, i );
		}
# endif
	*idindex = newsidx = i;
	return ( sp );
}

/*
 * look up name: must agree with s w.r.t. STAG, SMOS and SHIDDEN
 */
int
lookup(char *name, int s)
{ 
	char *p, *q;
	int i, ii;
	struct symtab *sp;

	/* compute initial hash index */
# ifndef BUG1
	if( ddebug > 2 ){
		printf( "lookup( %s, %d ), stwart=%d, instruct=%d\n", name, s, stwart, instruct );
		}
# endif

	i = 0;
	i = (int)name;
	i = i%SYMTSZ;
	sp = &stab[ii=i];

	for(;;){ /* look for name */

		if( sp->stype == TNULL ){ /* empty slot */
			sp->sflags = s;  /* set STAG, SMOS if needed, turn off all others */
			sp->sname = name;
			sp->stype = UNDEF;
			sp->sclass = SNULL;
			sp->s_argn = 0;
			return( i );
			}
		if( (sp->sflags & (STAG|SMOS|SHIDDEN)) != s ) goto next;
		p = sp->sname;
		q = name;
		if (p == q)
			return ( i );
	next:
		if( ++i >= SYMTSZ ){
			i = 0;
			sp = stab;
			}
		else ++sp;
		if( i == ii ) cerror( "symbol table full" );
		}
	}

#ifdef PCC_DEBUG
/* if not debugging, checkst is a macro */
void
checkst(int lev)
{
#if 0
	int i, j;
	struct symtab *p, *q;

	for (i=0, p=stab; i<SYMTSZ; ++i, ++p) {
		if (p->stype == TNULL)
			continue;
		j = lookup(p->sname, p->sflags&(SMOS|STAG));
		if (j != i) {
			q = &stab[j];
			if (q->stype == UNDEF || q->slevel <= p->slevel)
				cerror("check error: %s", q->sname);
		} else if (p->slevel > lev)
			cerror("%s check at level %d", p->sname, lev);
	}
#endif
}
#endif

/*
 * look up p again, and see where it lies
 */
struct symtab *
relook(struct symtab *p)
{
	struct symtab *q;

	/* I'm not sure that this handles towers of several hidden definitions in all cases */
	q = &stab[lookup( p->sname, p->sflags&(STAG|SMOS|SHIDDEN) )];
	/* make relook always point to either p or an empty cell */
	if( q->stype == UNDEF ){
		q->stype = TNULL;
		return(q);
		}
	while( q != p ){
		if( q->stype == TNULL ) break;
		if( ++q >= &stab[SYMTSZ] ) q=stab;
		}
	return(q);
}

void
clearst(int lev)
{
	struct symtab *p, *q;
	int i, temp;
	struct symtab *clist = 0;

	temp = lineno;
	aobeg();

	/* step 1: remove entries */
	while( chaintop-1 > lev ){
		p = schain[--chaintop];
		schain[chaintop] = 0;
		for( ; p; p = q ){
			q = p->snext;
			if( p->stype == TNULL || p->slevel <= lev )
				cerror( "schain botch" );
			lineno = p->suse < 0 ? -p->suse : p->suse;
			if (p->stype==UNDEF) {
				lineno = temp;
				uerror("%s undefined", p->sname);
			} else
				aocode(p);
# ifndef BUG1
			if( ddebug ){
				printf( "removing %s", p->sname );
				printf( " from stab[%d], flags %o level %d\n",
					p-stab, p->sflags, p->slevel);
				}
# endif
			if( p->sflags & SHIDES )unhide( p );
			p->stype = TNULL;
			p->snext = clist;
			clist = p;
			}
		}

	/* step 2: fix any mishashed entries */
	p = clist;
	while( p ){
		struct symtab *next, **t, *r;

		q = p;
		next = p->snext;
		for(;;){
			if( ++q >= &stab[SYMTSZ] )q = stab;
			if( q == p || q->stype == TNULL )break;
			if( (r = relook(q)) != q ) {
				/* move q in schain list */
				t = &schain[(int)q->slevel];
				while( *t && *t != q )
					t = &(*t)->snext;
				if( *t )
					*t = r;
				else
					cerror("schain botch 2");
				*r = *q;
				q->stype = TNULL;
				}
			}
		p = next;
		}
	/* step 3: check for undefined labels and reset label stack */
	if (lev == 0) {
		for (i = 0; i < nlabels; i++)
			if (labelno[i] < 0)
				uerror("label '%s' not defined", labelname[i]);
		nlabels = 0;
	}

	lineno = temp;
	aoend();
}

int
hide(struct symtab *p)
{
	struct symtab *q;
	for( q=p+1; ; ++q ){
		if( q >= &stab[SYMTSZ] ) q = stab;
		if( q == p ) cerror( "symbol table full" );
		if( q->stype == TNULL ) break;
		}
	*q = *p;
	p->sflags |= SHIDDEN;
	q->sflags = (p->sflags&(SMOS|STAG)) | SHIDES;
	if (p->slevel > 0)
		werror("%s redefinition hides earlier one", p->sname);
# ifndef BUG1
	if( ddebug ) printf( "	%d hidden in %d\n", p-stab, q-stab );
# endif
	return( idname = q-stab );
}

void
unhide(struct symtab *p)
{
	struct symtab *q;
	int s;

	s = p->sflags & (SMOS|STAG);
	q = p;

	for(;;){

		if( q == stab ) q = &stab[SYMTSZ-1];
		else --q;

		if( q == p ) break;

		if( (q->sflags&(SMOS|STAG)) == s ){
			if (p->sname == q->sname) {
				q->sflags &= ~SHIDDEN;
# ifndef BUG1
				if( ddebug ) printf( "unhide uncovered %d from %d\n", q-stab,p-stab);
# endif
				return;
				}
			}

		}
	cerror( "unhide fails" );
	}
