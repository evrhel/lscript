#if !defined(DATAU_H)
#define DATAU_H

#include "../lscript.h"

typedef union data_u data_t;
union data_u
{
	lchar cvalue;
	luchar ucvalue;
	lshort svalue;
	lushort usvalue;
	lint ivalue;
	luint uivalue;
	llong lvalue;
	lulong ulvalue;
	lbool bvalue;
	lfloat fvalue;
	ldouble dvalue;
	lobject ovalue;

	lchararray cavalue;
	luchararray ucavalue;
	lshortarray savalue;
	lushortarray usavalue;
	lintarray iavalue;
	luintarray uiavalue;
	llongarray lavalue;
	lulongarray ulavalue;
	lboolarray bavalue;
	lobjectarray oavalue;
};

#endif