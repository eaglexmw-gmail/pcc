#include "pass2.h"

# define ANYSIGNED TINT|TLONG|TCHAR
# define ANYUSIGNED TUNSIGNED|TULONG|TUCHAR
# define ANYFIXED ANYSIGNED|ANYUSIGNED
# define TL TLONG|TULONG
# define TWORD TUNSIGNED|TINT
# define TCH TCHAR|TUCHAR

struct optab table[] = {
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },

/* (signed) char -> int/pointer */
{ SCONV,	INTAREG,
	STAREG,		TCHAR,
	SANY,		TINT|TPOINT,
		0,	RLEFT,
		"	exts.b AL\n", },

{ SCONV,	INTAREG,
	STAREG,		TWORD,
	SANY,		TCH,
		0,	RLEFT,
		"", },

{ SCONV,	INTAREG,
	STAREG,		TPOINT,
	SANY,		TWORD,
		0,	RLEFT,
		"", },

{ OPSIMP,	INAREG|FOREFF,
	SAREG|STAREG,			TCH,
	SAREG|STAREG|SNAME|SOREG,	TCH,
		0,	RLEFT,
		"	Ob AR,AL\n", },

{ OPSIMP,	INAREG|FOREFF,
	SAREG|STAREG,			TWORD|TPOINT,
	SAREG|STAREG|SNAME|SOREG,	TWORD|TPOINT,
		0,	RLEFT,
		"	Ow AR,AL\n", },

/* signed integer division */
{ DIV,		INTAREG|FOREFF,
	SAREG|STAREG,			TINT,
	STAREG|SAREG|SNAME|SOREG,	TWORD,
		3*NAREG|NASL|NSPECIAL,		RESC1,
		"	xor.w r2\n	div.w AR\n", },

/* signed integer modulus, equal to above */
{ MOD,		INTAREG|FOREFF,
	SAREG|STAREG,			TINT,
	STAREG|SAREG|SNAME|SOREG,	TWORD,
		3*NAREG|NASL|NSPECIAL,		RESC1,
		"	xor.w r2\n	div.w AR\n", },

{ LS,		INTAREG,
	STAREG|SAREG,	TWORD,
	SCON,		TANY,
		0,	RLEFT,
		"	shl AR,AL\n", },

{ LS,		INTAREG,
	STAREG|SAREG,	TWORD,
	STAREG|SAREG,	TWORD,
		0,	RLEFT,
		"	shl AR,AL\n", },

{ RS,		INTAREG,
	STAREG|SAREG,	TUNSIGNED,
	SCON,		TANY,
		0,	RLEFT,
		"	shl ZA,AL\n", },

{ RS,		INTAREG,
	STAREG|SAREG,	TINT,
	SCON,		TANY,
		0,	RLEFT,
		"	sha ZA,AL\n", },

{ OPLOG,	FORCC,
	SBREG|STBREG|SOREG,	TWORD|TPOINT,
	SCON,			TWORD|TPOINT,
		0,	RESCC,
		"	cmp AR,AL\n", },

{ GOTO,		FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jmp ZC\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SCON|SNAME|SOREG|SAREG|STAREG,	TWORD|TPOINT,
		NAREG,	RESC1,
		"	mov.w AR,A1\n", },	

{ OPLTYPE,	INTBREG,
	SANY,	TANY,
	SCON|SNAME|SOREG|SAREG|STAREG,	TWORD|TPOINT,
		NBREG,	RESC1,
		"	mov.w AR,A1\n", },	

{ OPLTYPE,	INTAREG,
	SANY,		TANY,
	SCON|SNAME|SOREG,	TCH,
		NAREG,	RESC1,
		"	mov.b AR, A1\n", },

{ OPLTYPE,	INTBREG,
	SANY,			TANY,
	SCON|SNAME|SOREG,	TCHAR|TUCHAR,
		NBREG,	RESC1,
		"	mov.b AR,A1\n", },

{ COMPL,	INTAREG,
	SAREG|STAREG,	TWORD,
	SANY,		TANY,
		0,	RLEFT,
		"	not.w AL\n", },

{ FUNARG,	FOREFF,
	SCON|SAREG|STAREG|SNAME|SOREG,	TWORD|TPOINT,
	SANY,			TANY,
		0,	RNULL,
		"	push.w AL\n", },

{ FUNARG,	FOREFF,
	SAREG|STAREG|SNAME|SOREG,	TCHAR|TUCHAR,
	SANY,				TANY,
		0,	RNULL,
		"	push.b AL\n", },

{ ASSIGN,	INTAREG|INTBREG|FOREFF,
	SBREG|STBREG|SAREG|STAREG,	TWORD|TPOINT,
	SCON,				TANY,
		0,	RLEFT,
		"	mov.w AR,AL\n", },

{ ASSIGN,	FOREFF,
	SNAME|SOREG,	TWORD|TPOINT,
	SCON,		TANY,
		0,	0,
		"	mov.w AR,AL\n", },

/* char, oreg/name -> any reg */
{ ASSIGN,	FOREFF|INTAREG,
	SAREG|STAREG|SBREG|STBREG,	TCHAR|TUCHAR,
	SOREG|SNAME|SCON,		TCHAR|TUCHAR,
		0,	RLEFT,
		"	mov.b AR,AL\n", },

/* int, oreg/name -> any reg */
{ ASSIGN,	FOREFF|INTAREG,
	SAREG|STAREG|SBREG|STBREG,	TWORD|TPOINT,
	SOREG|SNAME,			TWORD|TPOINT,
		0,	RLEFT,
		"	mov.w AR,AL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	SOREG|SNAME,			TWORD|TPOINT,
	SAREG|STAREG|SBREG|STBREG,	TWORD|TPOINT,
		0,	RRIGHT,
		"	mov.w AR,AL\n", },

{ ASSIGN,	FOREFF|INTAREG,
	SOREG|SNAME,	TCHAR|TUCHAR,
	SAREG|STAREG,	TCHAR|TUCHAR,
		0,	RRIGHT,
		"	mov.b AR,AL\n", },

{ MOVE,		FOREFF|INTAREG,
	SAREG|STAREG|STBREG|SBREG,	TWORD|TPOINT,
	SAREG|STAREG,			TWORD|TPOINT,
		NAREG,	RESC1,
		"	mov.w AL, AR\n", },

{ UMUL, 	INTAREG,
	SBREG|STBREG,	TPOINT|TWORD,
	SANY,		TPOINT|TWORD,
		NAREG,	RESC1,
		"	mov.w [AL],A1\n", },

{ UMUL, 	INTBREG,
	SBREG|STBREG,	TPOINT|TWORD,
	SANY,		TPOINT|TWORD,
		NBREG|NBSL,	RESC1,
		"	mov.w [AL],A1\n", },

{ UMUL,		INTAREG,
	SBREG|STBREG,	TCHAR|TUCHAR|TPTRTO,
	SANY,		TCHAR|TUCHAR,
		NAREG,	RESC1,
		"	mov.b [AL], A1\n", }, 

{ UCALL,	FOREFF|INTAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG,	RESC1,
		"	jsr.w CL\nZB", },

{ UCALL,        INTAREG,
	SBREG|STBREG,   TANY,
	SANY,   TANY,
		NAREG|NASL,     RESC1,  /* should be 0 */
		"	jsri *AL\n", }, /* XXX - fun ptrs are 20 bits */

{ FREE, FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	"help; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);

