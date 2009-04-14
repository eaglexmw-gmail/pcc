/*	$Id$	*/

/*
 * Copyright (c) 2004,2009 Anders Magnusson. All rights reserved.
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>

#include "compat.h"
#include "cpp.h"
#include "y.tab.h"

static void cvtdig(int rad);
static int charcon(usch *);
static void elsestmt(void);
static void ifdefstmt(void);
static void ifndefstmt(void);
static void endifstmt(void);
static void ifstmt(void);
static void cpperror(void);
static void pragmastmt(void);
static void undefstmt(void);
static void cpperror(void);
static void elifstmt(void);
static void storepb(void);
static void badop(const char *);
void  include(void);
void  define(void);

extern int yyget_lineno (void);
extern void yyset_lineno (int);

static int inch(void);

static int scale, gotdef, contr;
int inif;

#undef input
#undef unput
#define input() inch()
#define unput(ch) unch(ch)
#define PRTOUT(x) if (YYSTATE || slow) return x; if (!flslvl) putstr((usch *)yytext);
/* protection against recursion in #include */
#define MAX_INCLEVEL	100
static int inclevel;

#define	IFR	1
#define	CONTR	2
#define	DEF	3
#define	COMMENT	4
static int state;
#define	BEGIN state =
#define	YYSTATE	state

#ifdef YYTEXT_POINTER
static char buf[CPPBUF];
char *yytext = buf;
#else
char yytext[CPPBUF];
#endif

static int owasnl, wasnl = 1;

static void
unch(int c)
{
		
	--ifiles->curptr;
	if (ifiles->curptr < ifiles->bbuf)
		error("pushback buffer full");
	*ifiles->curptr = c;
}


int
yylex()
{
	int ch;
	int yyp;
	int os, mixed, haspmd;

zagain:
	yyp = 0;
	yytext[yyp++] = ch = inch();
	owasnl = wasnl;
	wasnl = 0;
	switch (ch) {
	case -1:
		return 0;
	case '\n':
		os = YYSTATE;

		wasnl = 1;
		if (os != IFR)
			BEGIN 0;
		ifiles->lineno++;
		if (flslvl == 0) {
			if (ifiles->lineno == 1)
				prtline();
			else
				putch('\n');
		}
		if ((os != 0 || slow) && !contr)
			goto yyret;
		contr = 0;
		break;

	case '\r': /* Ignore CR's */
		yyp = 0;
		break;

#define	CHK(x,y) if (state != IFR) goto any; \
	if ((ch = input()) != y) { unput(ch); ch = x; goto any; } \
	yytext[yyp++] = ch

	case '+': CHK('+','+'); badop("++"); break;
	case '-': CHK('-','-'); badop("--"); break;
	case '=': CHK('=','='); ch = EQ; goto yyret;
	case '!': CHK('!','='); ch = NE; goto yyret;
	case '|': CHK('|','|'); ch = OROR; goto yyret;
	case '&': CHK('&','&'); ch = ANDAND; goto yyret;
	case '<':
		if (state != IFR) goto any;
		if ((ch = inch()) == '=') {
			yytext[yyp++] = ch; ch = LE; goto yyret;
		}
		if (ch == '<') { yytext[yyp++] = ch; ch = LS; goto yyret; }
		unch(ch);
		ch = '<';
		goto any;
	case '>':
		if (state != IFR) goto any;
		if ((ch = inch()) == '=') {
			yytext[yyp++] = ch; ch = GE; goto yyret;
		}
		if (ch == '>') { yytext[yyp++] = ch; ch = RS; goto yyret; }
		unch(ch);
		ch = '>';
		goto any;


	case '0': case '1': case '2': case '3': case '4': case '5': 
	case '6': case '7': case '8': case '9':
		/* readin a "pp-number" */
		mixed = haspmd = 0;
ppnum:		for (;;) {
			ch = input();
			if (ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P') {
				yytext[yyp++] = ch;
				mixed = 1;
				ch = input();
				if (ch == '-' || ch == '+') {
					yytext[yyp++] = ch;
					haspmd = 1;
				} else
					unput(ch);
				continue;
			}
			if (isdigit(ch) || isalpha(ch) ||
			    ch == '_' || ch == '.') {
				yytext[yyp++] = ch;
				if (ch == '.')
					haspmd = 1;
				if (!isdigit(ch))
					mixed = 1;
				continue;
			} 
			break;
		}
		unput(ch);
		yytext[yyp] = 0;

		if (mixed == 1 && slow && (state == 0 || state == DEF))
			return IDENT;

		if (mixed == 0) {
			if (slow && !YYSTATE)
				return IDENT;
			scale = yytext[0] == '0' ? 8 : 10;
			goto num;
		} else if (yytext[0] == '0' &&
		    (yytext[1] == 'x' || yytext[1] == 'X')) {
			scale = 16;
num:			if (YYSTATE == IFR)
				cvtdig(scale);
			PRTOUT(NUMBER);
		} else if (yytext[0] == '0' && isdigit(yytext[1])) {
			scale = 8; goto num;
		} else if (haspmd) {
			PRTOUT(FPOINT);
		} else {
			scale = 10; goto num;
		}
		goto zagain;


	case '\'':
chlit:		if (tflag && !(YYSTATE || slow))
			goto any;
		for (;;) {
			if ((ch = input()) == '\\') {
				yytext[yyp++] = ch;
				yytext[yyp++] = input();
				continue;
			} else if (ch == '\n') {
				/* not a constant */
				while (yyp > 1)
					unput(yytext[--yyp]);
				ch = '\'';
				goto any;
			} else
				yytext[yyp++] = ch;
			if (ch == '\'')
				break;
		}
		yytext[yyp] = 0;

		if (YYSTATE || slow) {
			yylval.node.op = NUMBER;
			yylval.node.nd_val = charcon((usch *)yytext);
			return (NUMBER);
		}
		if (!flslvl)
			putstr((usch *)yytext);
		goto zagain;

	case ' ':
	case '\t':
		if (state == IFR)
			goto zagain;

		while ((ch = input()) == ' ' || ch == '\t')
			yytext[yyp++] = ch;
		if (owasnl == 0) {
b1:			unput(ch);
			yytext[yyp] = 0;
			PRTOUT(WSPACE);
		} else if (ch != '#') {
			goto b1;
		} else {
			extern int inmac;

contr:			while ((ch = input()) == ' ' || ch == '\t')
				;
			unch(ch);
			if (inmac)
				error("preprocessor directive found "
				    "while expanding macro");
			contr = 1;
			BEGIN CONTR;

		}
		goto zagain;

	case '/':
		if ((ch = input()) == '/') {
			do {
				yytext[yyp++] = ch;
				ch = input();
			} while (ch && ch != '\n');
			yytext[yyp] = 0;
			unch(ch);
			if (Cflag && !flslvl && !slow)
				putstr((usch *)yytext);
			else if (!flslvl)
				putch(' ');
			goto zagain;
		} else if (ch == '*') {
			int c, wrn;
			int prtcm = Cflag && !flslvl && !slow;
			extern int readmac;

			if (Cflag && !flslvl && readmac)
				return CMNT;

			if (prtcm)
				putstr((usch *)yytext);
			wrn = 0;
		more:	while ((c = input()) && c != '*') {
				if (c == '\n')
					putch(c), ifiles->lineno++;
				else if (c == 1) /* WARN */
					wrn = 1;
				else if (prtcm)
					putch(c);
			}
			if (c == 0)
				return 0;
			if (prtcm)
				putch(c);
			if ((c = input()) && c != '/') {
				unput(c);
				goto more;
			}
			if (prtcm)
				putch(c);
			if (c == 0)
				return 0;
			if (!tflag && !Cflag && !flslvl)
				unput(' ');
			if (wrn)
				unput(1);
			goto zagain;
		}
		unch(ch);
		ch = '/';
		goto any;

	case '#':
		if (state != DEF) {
			if (owasnl)
				goto contr;
			goto any;
		}
		if ((ch = input()) == '#') {
			yytext[yyp++] = ch;
			ch = CONCAT;
		} else {
			unput(ch);
			ch = MKSTR;
		}
		goto yyret;
		
	case '.':
		if (state != DEF) {
			ch = input();
			if (isdigit(ch)) {
				yytext[yyp++] = ch;
				mixed = haspmd = 1;
				goto ppnum;
			} else {
				unput(ch);
				ch = '.';
			}
			goto any;
		}
		if ((ch = input()) != '.') {
			unput(ch);
			ch = '.';
			goto any;
		}
		if ((ch = input()) != '.') {
			unput(ch);
			unput('.');
			ch = '.';
			goto any;
		}
		yytext[yyp++] = ch;
		yytext[yyp++] = ch;
		ch = ELLIPS;
		goto yyret;


	case '\"':
	strng:
		for (;;) {
			if ((ch = input()) == '\\') {
				yytext[yyp++] = ch;
				yytext[yyp++] = input();
				continue;
			} else 
				yytext[yyp++] = ch;
			if (ch == '\"')
				break;
		}
		yytext[yyp] = 0;
		{ PRTOUT(STRING); }
		break;

	case 'L':
		if ((ch = input()) == '\"') {
			yytext[yyp++] = ch;
			goto strng;
		} else if (ch == '\'') {
			yytext[yyp++] = ch;
			goto chlit;
		}
		unput(ch);
		/* FALLTHROUGH */

	/* Yetch, all identifiers */
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': 
	case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': 
	case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': 
	case 's': case 't': case 'u': case 'v': case 'w': case 'x': 
	case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': 
	case 'G': case 'H': case 'I': case 'J': case 'K':
	case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': 
	case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': 
	case 'Y': case 'Z':
	case '_': { /* {L}({L}|{D})* */
		struct symtab *nl;

		/* Special hacks */
		for (;;) { /* get chars */
			ch = input();
			if (isalpha(ch) || isdigit(ch) || ch == '_') {
				yytext[yyp++] = ch;
			} else {
				unput(ch);
				break;
			}
		}
		yytext[yyp] = 0; /* need already string */

		switch (state) {
		case DEF:
			if (strcmp(yytext, "__VA_ARGS__") == 0)
				return VA_ARGS;
			break;
		case CONTR:
#define	CC(s)	if (strcmp(yytext, s) == 0)
			CC("ifndef") {
				contr = 0; ifndefstmt();
				goto zagain;
			} else CC("ifdef") {
				contr = 0; ifdefstmt();
				goto zagain;
			} else CC("if") {
				contr = 0; storepb(); BEGIN IFR;
				ifstmt(); BEGIN 0;
				goto zagain;
			} else CC("include") {
				contr = 0; BEGIN 0; include(); prtline();
				goto zagain;
			} else CC("else") {
				contr = 0; elsestmt();
				goto zagain;
			} else CC("endif") {
				contr = 0; endifstmt();
				goto zagain;
			} else CC("error") {
				contr = 0; if (slow) return IDENT;
				cpperror(); BEGIN 0;
				goto zagain;
			} else CC("define") {
				contr = 0; BEGIN DEF; define(); BEGIN 0;
				goto zagain;
			} else CC("undef") {
				contr = 0; if (slow) return IDENT; undefstmt();
				goto zagain;
			} else CC("line") {
				contr = 0; storepb(); BEGIN 0; line();
				goto zagain;
			} else CC("pragma") {
				contr = 0; pragmastmt(); BEGIN 0;
				goto zagain;
			} else CC("elif") {
				contr = 0; storepb(); BEGIN IFR;
				elifstmt(); BEGIN 0;
				goto zagain;
			}
			break;
		case  IFR:
			CC("defined") {
				int p, c;
				gotdef = 1;
				if ((p = c = yylex()) == '(')
					c = yylex();
				if (c != IDENT || (p != IDENT && p != '('))
					error("syntax error");
				if (p == '(' && yylex() != ')')
					error("syntax error");
				return NUMBER;
			} else {
				yylval.node.op = NUMBER;
				if (gotdef) {
					yylval.node.nd_val
					    = lookup((usch *)yytext, FIND) != 0;
					gotdef = 0;
					return IDENT;
				}
				yylval.node.nd_val = 0;
				return NUMBER;
			}
		}

		/* end special hacks */

		if (slow)
			return IDENT;
		if (YYSTATE == CONTR) {
			if (flslvl == 0) {
				/*error("undefined control");*/
				while (input() != '\n')
					;
				unput('\n');
				BEGIN 0;
				goto xx;
			} else {
				BEGIN 0; /* do nothing */
			}
		}
		if (flslvl) {
			; /* do nothing */
		} else if (isdigit((int)yytext[0]) == 0 &&
		    (nl = lookup((usch *)yytext, FIND)) != 0) {
			usch *op = stringbuf;
			putstr(gotident(nl));
			stringbuf = op;
		} else
			putstr((usch *)yytext);
		xx:
		goto zagain;
	}

	default:
	any:
		if (state == IFR)
			goto yyret;
		yytext[yyp] = 0;
		if (contr) {
			while (input() != '\n')
				;
			unput('\n');
			BEGIN 0;
			contr = 0;
			goto yy;
		}
		if (YYSTATE || slow)
			return yytext[0];
		if (yytext[0] == 6) { /* PRAGS */
			usch *obp = stringbuf;
			extern usch *prtprag(usch *);
			*stringbuf++ = yytext[0];
			do {
				*stringbuf = input();
			} while (*stringbuf++ != 14);
			prtprag(obp);
			stringbuf = obp;
		} else {
			PRTOUT(yytext[0]);
		}
		yy:
		yyp = 0;
		break;

	} /* endcase */
	goto zagain;

yyret:
	yytext[yyp] = 0;
	return ch;
}

usch *yyp, yybuf[CPPBUF];

int yylex(void);
int yywrap(void);

static int
inpch(void)
{
	int len;

	if (ifiles->curptr < ifiles->maxread)
		return *ifiles->curptr++;

	if ((len = read(ifiles->infil, ifiles->buffer, CPPBUF)) < 0)
		error("read error on file %s", ifiles->orgfn);
	if (len == 0)
		return -1;
	ifiles->curptr = ifiles->buffer;
	ifiles->maxread = ifiles->buffer + len;
	return inpch();
}

static int
inch(void)
{
	int c;

again:	switch (c = inpch()) {
	case '\\': /* continued lines */
msdos:		if ((c = inpch()) == '\n') {
			ifiles->lineno++;
			putch('\n');
			goto again;
		} else if (c == '\r')
			goto msdos;
		unch(c);
		return '\\';
	case '?': /* trigraphs */
		if ((c = inpch()) != '?') {
			unch(c);
			return '?';
		}
		switch (c = inpch()) {
		case '=': c = '#'; break;
		case '(': c = '['; break;
		case ')': c = ']'; break;
		case '<': c = '{'; break;
		case '>': c = '}'; break;
		case '/': c = '\\'; break;
		case '\'': c = '^'; break;
		case '!': c = '|'; break;
		case '-': c = '~'; break;
		default:
			unch(c);
			unch('?');
			return '?';
		}
		unch(c);
		goto again;
	default:
		return c;
	}
}

/*
 * Let the command-line args be faked defines at beginning of file.
 */
static void
prinit(struct initar *it, struct includ *ic)
{
	char *a, *pre, *post;

	if (it->next)
		prinit(it->next, ic);
	pre = post = NULL; /* XXX gcc */
	switch (it->type) {
	case 'D':
		pre = "#define ";
		if ((a = strchr(it->str, '=')) != NULL) {
			*a = ' ';
			post = "\n";
		} else
			post = " 1\n";
		break;
	case 'U':
		pre = "#undef ";
		post = "\n";
		break;
	case 'i':
		pre = "#include \"";
		post = "\"\n";
		break;
	default:
		error("prinit");
	}
	strlcat((char *)ic->buffer, pre, CPPBUF+1);
	strlcat((char *)ic->buffer, it->str, CPPBUF+1);
	if (strlcat((char *)ic->buffer, post, CPPBUF+1) >= CPPBUF+1)
		error("line exceeds buffer size");

	ic->lineno--;
	while (*ic->maxread)
		ic->maxread++;
}

/*
 * A new file included.
 * If ifiles == NULL, this is the first file and already opened (stdin).
 * Return 0 on success, -1 if file to be included is not found.
 */
int
pushfile(usch *file)
{
	extern struct initar *initar;
	struct includ ibuf;
	struct includ *ic;
	int c, otrulvl;

	ic = &ibuf;
	ic->next = ifiles;

	slow = 0;
	if (file != NULL) {
		if ((ic->infil = open((char *)file, O_RDONLY)) < 0)
			return -1;
		ic->orgfn = ic->fname = file;
		if (++inclevel > MAX_INCLEVEL)
			error("Limit for nested includes exceeded");
	} else {
		ic->infil = 0;
		ic->orgfn = ic->fname = (usch *)"<stdin>";
	}
	ic->buffer = ic->bbuf+NAMEMAX;
	ic->curptr = ic->buffer;
	ifiles = ic;
	ic->lineno = 1;
	ic->maxread = ic->curptr;
	prtline();
	if (initar) {
		*ic->maxread = 0;
		prinit(initar, ic);
		if (dMflag)
			write(ofd, ic->buffer, strlen((char *)ic->buffer));
		initar = NULL;
	}

	otrulvl = trulvl;

	if ((c = yylex()) != 0)
		error("yylex returned %d", c);

wasnl = owasnl;
	if (otrulvl != trulvl || flslvl)
		error("unterminated conditional");

	ifiles = ic->next;
	close(ic->infil);
	inclevel--;
	return 0;
}

/*
 * Print current position to output file.
 */
void
prtline()
{
	usch *s, *os = stringbuf;

	if (Mflag) {
		if (dMflag)
			return; /* no output */
		if (ifiles->lineno == 1) {
			s = sheap("%s: %s\n", Mfile, ifiles->fname);
			write(ofd, s, strlen((char *)s));
		}
	} else if (!Pflag)
		putstr(sheap("# %d \"%s\"\n", ifiles->lineno, ifiles->fname));
	stringbuf = os;
}

void
cunput(int c)
{
#ifdef CPP_DEBUG
	extern int dflag;
	if (dflag)printf(": '%c'(%d)", c > 31 ? c : ' ', c);
#endif
	unput(c);
}

int yywrap(void) { return 1; }

static int
dig2num(int c)
{
	if (c >= 'a')
		c = c - 'a' + 10;
	else if (c >= 'A')
		c = c - 'A' + 10;
	else
		c = c - '0';
	return c;
}

/*
 * Convert string numbers to unsigned long long and check overflow.
 */
static void
cvtdig(int rad)
{
	unsigned long long rv = 0;
	unsigned long long rv2 = 0;
	char *y = yytext;
	int c;

	c = *y++;
	if (rad == 16)
		y++;
	while (isxdigit(c)) {
		rv = rv * rad + dig2num(c);
		/* check overflow */
		if (rv / rad < rv2)
			error("Constant \"%s\" is out of range", yytext);
		rv2 = rv;
		c = *y++;
	}
	y--;
	while (*y == 'l' || *y == 'L')
		y++;
	yylval.node.op = *y == 'u' || *y == 'U' ? UNUMBER : NUMBER;
	yylval.node.nd_uval = rv;
	if ((rad == 8 || rad == 16) && yylval.node.nd_val < 0)
		yylval.node.op = UNUMBER;
	if (yylval.node.op == NUMBER && yylval.node.nd_val < 0)
		/* too large for signed */
		error("Constant \"%s\" is out of range", yytext);
}

static int
charcon(usch *p)
{
	int val, c;

	p++; /* skip first ' */
	val = 0;
	if (*p++ == '\\') {
		switch (*p++) {
		case 'a': val = '\a'; break;
		case 'b': val = '\b'; break;
		case 'f': val = '\f'; break;
		case 'n': val = '\n'; break;
		case 'r': val = '\r'; break;
		case 't': val = '\t'; break;
		case 'v': val = '\v'; break;
		case '\"': val = '\"'; break;
		case '\'': val = '\''; break;
		case '\\': val = '\\'; break;
		case 'x':
			while (isxdigit(c = *p)) {
				val = val * 16 + dig2num(c);
				p++;
			}
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
			p--;
			while (isdigit(c = *p)) {
				val = val * 8 + (c - '0');
				p++;
			}
			break;
		default: val = p[-1];
		}

	} else
		val = p[-1];
	return val;
}

static void
chknl(int ignore)
{
	int t;

	slow = 1;
	while ((t = yylex()) == WSPACE)
		;
	if (t != '\n') {
		if (ignore) {
			warning("newline expected, got \"%s\"", yytext);
			/* ignore rest of line */
			while ((t = yylex()) && t != '\n')
				;
		}
		else
			error("newline expected, got \"%s\"", yytext);
	}
	slow = 0;
}

static void
elsestmt(void)
{
	if (flslvl) {
		if (elflvl > trulvl)
			;
		else if (--flslvl!=0) {
			flslvl++;
		} else {
			trulvl++;
			prtline();
		}
	} else if (trulvl) {
		flslvl++;
		trulvl--;
	} else
		error("If-less else");
	if (elslvl==trulvl+flslvl)
		error("Too many else");
	elslvl=trulvl+flslvl;
	chknl(1);
}

static void
ifdefstmt(void)		 
{ 
	int t;

	if (flslvl) {
		/* just ignore the rest of the line */
		while (input() != '\n')
			;
		unput('\n');
		yylex();
		flslvl++;
		return;
	}
	slow = 1;
	do
		t = yylex();
	while (t == WSPACE);
	if (t != IDENT)
		error("bad ifdef");
	slow = 0;
	if (flslvl == 0 && lookup((usch *)yytext, FIND) != 0)
		trulvl++;
	else
		flslvl++;
	chknl(0);
}

static void
ifndefstmt(void)	  
{ 
	int t;

	slow = 1;
	do
		t = yylex();
	while (t == WSPACE);
	if (t != IDENT)
		error("bad ifndef");
	slow = 0;
	if (flslvl == 0 && lookup((usch *)yytext, FIND) == 0)
		trulvl++;
	else
		flslvl++;
	chknl(0);
}

static void
endifstmt(void)		 
{
	if (flslvl) {
		flslvl--;
		if (flslvl == 0)
			prtline();
	} else if (trulvl)
		trulvl--;
	else
		error("If-less endif");
	if (flslvl == 0)
		elflvl = 0;
	elslvl = 0;
	chknl(1);
}

/*
 * Note! Ugly!
 * Walk over the string s and search for defined, and replace it with 
 * spaces and a 1 or 0. 
 */
static void
fixdefined(usch *s)
{
	usch *bc, oc;

	for (; *s; s++) {
		if (*s != 'd')
			continue;
		if (memcmp(s, "defined", 7))
			continue;
		/* Ok, got defined, can scratch it now */
		memset(s, ' ', 7);
		s += 7;
#define	WSARG(x) (x == ' ' || x == '\t')
		if (*s != '(' && !WSARG(*s))
			continue;
		while (WSARG(*s))
			s++;
		if (*s == '(')
			s++;
		while (WSARG(*s))
			s++;
#define IDARG(x) ((x>= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z') || (x == '_'))
#define	NUMARG(x) (x >= '0' && x <= '9')
		if (!IDARG(*s))
			error("bad defined arg");
		bc = s;
		while (IDARG(*s) || NUMARG(*s))
			s++;
		oc = *s;
		*s = 0;
		*bc = (lookup(bc, FIND) != 0) + '0';
		memset(bc+1, ' ', s-bc-1);
		*s = oc;
	}
}

/*
 * get the full line of identifiers after an #if, pushback a WARN and
 * the line and prepare for expmac() to expand.
 * This is done before switching state.  When expmac is finished,
 * pushback the expanded line, change state and call yyparse.
 */
static void
storepb(void)
{
	usch *opb = stringbuf;
	int c;

	while ((c = input()) != '\n') {
		if (c == '/') {
			 if ((c = input()) == '*') {
				/* ignore comments here whatsoever */
				usch *g = stringbuf;
				getcmnt();
				stringbuf = g;
				continue;
			} else if (c == '/') {
				while ((c = input()) && c != '\n')
					;
				break;
			}
			unput(c);
			c = '/';
		}
		savch(c);
	}
	cunput('\n');
	savch(0);
	fixdefined(opb); /* XXX can fail if #line? */
	cunput(1); /* WARN XXX */
	unpstr(opb);
	stringbuf = opb;
	slow = 1;
	expmac(NULL);
	slow = 0;
	/* line now expanded */
	while (stringbuf > opb)
		cunput(*--stringbuf);
}

static void
ifstmt(void)
{
	if (flslvl == 0) {
		slow = 1;
		if (yyparse())
			++trulvl;
		else
			++flslvl;
		slow = 0;
	} else
		++flslvl;
}

static void
elifstmt(void)
{
	if (flslvl == 0)
		elflvl = trulvl;
	if (flslvl) {
		if (elflvl > trulvl)
			;
		else if (--flslvl!=0)
			++flslvl;
		else {
			slow = 1;
			if (yyparse()) {
				++trulvl;
				prtline();
			} else
				++flslvl;
			slow = 0;
		}
	} else if (trulvl) {
		++flslvl;
		--trulvl;
	} else
		error("If-less elif");
}

static usch *
svinp(void)
{
	int c;
	usch *cp = stringbuf;

	while ((c = input()) && c != '\n')
		savch(c);
	savch('\n');
	savch(0);
	BEGIN 0;
	return cp;
}

static void
cpperror(void)
{
	usch *cp;
	int c;

	if (flslvl)
		return;
	c = yylex();
	if (c != WSPACE && c != '\n')
		error("bad error");
	cp = svinp();
	if (flslvl)
		stringbuf = cp;
	else
		error("%s", cp);
}

static void
undefstmt(void)
{
	struct symtab *np;

	slow = 1;
	if (yylex() != WSPACE || yylex() != IDENT)
		error("bad undef");
	if (flslvl == 0 && (np = lookup((usch *)yytext, FIND)))
		np->value = 0;
	slow = 0;
	chknl(0);
}

static void
pragmastmt(void)
{
	int c;

	slow = 1;
	if (yylex() != WSPACE)
		error("bad pragma");
	if (!flslvl)
		putstr((usch *)"#pragma ");
	do {
		c = input();
		if (!flslvl)
			putch(c);	/* Do arg expansion instead? */
	} while (c && c != '\n');
	if (c == '\n')
		unch(c);
	prtline();
	slow = 0;
}

static void
badop(const char *op)
{
	error("invalid operator in preprocessor expression: %s", op);
}

int
cinput()
{
	return input();
}
