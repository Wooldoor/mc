#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "parse.h"
#include "mi.h"
#include "asm.h"
#include "../config.h"

Mode regmodes[] = {
#define Reg(r, gasname, p9name, mode) mode,
#include "regs.def"
#undef Reg
};

Loc **locmap = NULL;
size_t maxregid = 0;

int
isintmode(Mode m)
{
	return m == ModeB || m == ModeW || m == ModeL || m == ModeQ;
}

int
isfloatmode(Mode m)
{
	return m == ModeF || m == ModeD;
}

Loc *
locstrlbl(char *lbl)
{
	Loc *l;

	l = zalloc(sizeof(Loc));
	l->type = Loclbl;
	l->mode = ModeQ;
	l->lbl = strdup(lbl);
	return l;
}

Loc *
loclitl(char *lbl)
{
	Loc *l;

	l = zalloc(sizeof(Loc));
	l->type = Loclitl;
	l->mode = ModeQ;
	l->lbl = strdup(lbl);
	return l;
}

Loc *
loclbl(Node *e)
{
	Node *lbl;
	assert(e->type == Nexpr);
	lbl = e->expr.args[0];
	assert(lbl->type == Nlit);
	assert(lbl->lit.littype == Llbl);
	return locstrlbl(lbl->lit.lblval);
}

void
resetregs()
{
	maxregid = Nreg;
}

static Loc *
locregid(regid id, Mode m)
{
	Loc *l;

	l = zalloc(sizeof(Loc));
	l->type = Locreg;
	l->mode = m;
	l->reg.id = id;
	locmap = xrealloc(locmap, maxregid * sizeof(Loc*));
	locmap[l->reg.id] = l;
	return l;
}

Loc *
locreg(Mode m)
{
	return locregid(maxregid++, m);
}

Loc *
locphysreg(Reg r)
{
	static Loc *physregs[Nreg] = {0,};

	if (physregs[r])
		return physregs[r];
	physregs[r] = locreg(regmodes[r]);
	physregs[r]->reg.colour = r;
	return physregs[r];
}

Loc *
locmem(long disp, Loc *base, Loc *idx, Mode mode)
{
	Loc *l;

	l = zalloc(sizeof(Loc));
	l->type = Locmem;
	l->mode = mode;
	l->mem.constdisp = disp;
	l->mem.base = base;
	l->mem.idx = idx;
	l->mem.scale = 1;
	return l;
}

Loc *
locmems(long disp, Loc *base, Loc *idx, int scale, Mode mode)
{
	Loc *l;

	l = locmem(disp, base, idx, mode);
	l->mem.scale = scale;
	return l;
}

Loc *
locmeml(char *disp, Loc *base, Loc *idx, Mode mode)
{
	Loc *l;

	l = zalloc(sizeof(Loc));
	l->type = Locmeml;
	l->mode = mode;
	l->mem.lbldisp = strdup(disp);
	l->mem.base = base;
	l->mem.idx = idx;
	l->mem.scale = 1;
	return l;
}

Loc *
locmemls(char *disp, Loc *base, Loc *idx, int scale, Mode mode)
{
	Loc *l;

	l = locmeml(disp, base, idx, mode);
	l->mem.scale = scale;
	return l;
}


/* val needs to be a long long on 32-bit due to usage as loclit(1LL << 63) 
 * in isel */
Loc *
loclit(long long val, Mode m)
{
	Loc *l;

	l = zalloc(sizeof(Loc));
	l->type = Loclit;
	l->mode = m;
	l->lit = val;
	return l;
}

/* For register Rdn, register Rs(n*2) is low bits */
Loc *
coreg(Reg r, Mode m)
{
	Reg crtab[][Nmode + 1] = {
		[Rr0] = {Rnone, Rr0},
		[Rr1] = {Rnone, Rr1},
		[Rr2] = {Rnone, Rr2},
		[Rr3] = {Rnone, Rr3},
		[Rr4] = {Rnone, Rr4},
		[Rr5] = {Rnone, Rr5},
		[Rr6] = {Rnone, Rr6},
		[Rr7] = {Rnone, Rr7},
		[Rr8] = {Rnone, Rr8},
		[Rr9] = {Rnone, Rr9},
		[Rr10] = {Rnone, Rr10},
		[Rr11] = {Rnone, Rr11},
		[Rr12] = {Rnone, Rr12},
		[Rr13] = {Rnone, Rr13},
		[Rr14] = {Rnone, Rr14},
		[Rr15] = {Rnone, Rr15},

		[Rs0] = {[ModeF] = Rs0, [ModeD] = Rd0},
		[Rs1] = {[ModeF] = Rs1, [ModeD] = Rd0},
		[Rs2] = {[ModeF] = Rs2, [ModeD] = Rd1},
		[Rs3] = {[ModeF] = Rs3, [ModeD] = Rd1},
		[Rs4] = {[ModeF] = Rs4, [ModeD] = Rd2},
		[Rs5] = {[ModeF] = Rs5, [ModeD] = Rd2},
		[Rs6] = {[ModeF] = Rs6, [ModeD] = Rd3},
		[Rs7] = {[ModeF] = Rs7, [ModeD] = Rd3},
		[Rs8] = {[ModeF] = Rs8, [ModeD] = Rd4},
		[Rs9] = {[ModeF] = Rs9, [ModeD] = Rd4},
		[Rs10] = {[ModeF] = Rs10, [ModeD] = Rd5},
		[Rs11] = {[ModeF] = Rs11, [ModeD] = Rd5},
		[Rs12] = {[ModeF] = Rs12, [ModeD] = Rd6},
		[Rs13] = {[ModeF] = Rs13, [ModeD] = Rd6},
		[Rs14] = {[ModeF] = Rs14, [ModeD] = Rd7},
		[Rs15] = {[ModeF] = Rs15, [ModeD] = Rd7},
		[Rs16] = {[ModeF] = Rs16, [ModeD] = Rd8},
		[Rs17] = {[ModeF] = Rs17, [ModeD] = Rd8},
		[Rs18] = {[ModeF] = Rs18, [ModeD] = Rd9},
		[Rs19] = {[ModeF] = Rs19, [ModeD] = Rd9},
		[Rs20] = {[ModeF] = Rs20, [ModeD] = Rd10},
		[Rs21] = {[ModeF] = Rs21, [ModeD] = Rd10},
		[Rs22] = {[ModeF] = Rs22, [ModeD] = Rd11},
		[Rs23] = {[ModeF] = Rs23, [ModeD] = Rd11},
		[Rs24] = {[ModeF] = Rs24, [ModeD] = Rd12},
		[Rs25] = {[ModeF] = Rs25, [ModeD] = Rd12},
		[Rs26] = {[ModeF] = Rs26, [ModeD] = Rd13},
		[Rs27] = {[ModeF] = Rs27, [ModeD] = Rd13},
		[Rs28] = {[ModeF] = Rs28, [ModeD] = Rd14},
		[Rs29] = {[ModeF] = Rs29, [ModeD] = Rd14},
		[Rs30] = {[ModeF] = Rs30, [ModeD] = Rd15},
		[Rs31] = {[ModeF] = Rs31, [ModeD] = Rd15},

		[Rd0] = {[ModeF] = Rs0, [ModeD] = Rd0},
		[Rd1] = {[ModeF] = Rs2, [ModeD] = Rd1},
		[Rd2] = {[ModeF] = Rs4, [ModeD] = Rd2},
		[Rd3] = {[ModeF] = Rs6, [ModeD] = Rd3},
		[Rd4] = {[ModeF] = Rs8, [ModeD] = Rd4},
		[Rd5] = {[ModeF] = Rs10, [ModeD] = Rd5},
		[Rd6] = {[ModeF] = Rs12, [ModeD] = Rd6},
		[Rd7] = {[ModeF] = Rs14, [ModeD] = Rd7},
		[Rd8] = {[ModeF] = Rs16, [ModeD] = Rd8},
		[Rd9] = {[ModeF] = Rs18, [ModeD] = Rd9},
		[Rd10] = {[ModeF] = Rs20, [ModeD] = Rd10},
		[Rd11] = {[ModeF] = Rs22, [ModeD] = Rd11},
		[Rd12] = {[ModeF] = Rs24, [ModeD] = Rd12},
		[Rd13] = {[ModeF] = Rs26, [ModeD] = Rd13},
		[Rd14] = {[ModeF] = Rs28, [ModeD] = Rd14},
		[Rd15] = {[ModeF] = Rs30, [ModeD] = Rd15},
	};

	assert(crtab[r][m] != Rnone);
	return locphysreg(crtab[r][m]);
}
