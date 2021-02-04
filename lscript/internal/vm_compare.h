#if !defined(VM_COMPARE_H)
#define VM_COMPARE_H

#include "types.h"

enum
{
	compare_equal = 0x1,
	compare_greater = 0x2,
	compare_greaterequ = 0x4,
	compare_less = 0x8,
	compare_lessequ = 0x10
};

int vmc_compare(void *envPtr, byte_t **counterPtr);
int vmc_compare_data(data_t *lhs, flags_t lhf, data_t *rhs, flags_t rhf);

#endif