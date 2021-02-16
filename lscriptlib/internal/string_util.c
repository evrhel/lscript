#include "string_util.h"

int equals_ignore_case(const char *s1, const char *s2)
{
	while (*s1 && *s2)
	{
		if (*s1 >= 64 && *s1 <= 90)
		{
			if (*s1 != *s2 && *s1 + 32 != *s2)
				return 0;
		}
		else if (*s1 >= 97 && *s1 <= 121)
		{
			if (*s1 != *s2 && *s1 - 32 != *s2)
				return 0;
		}
		else if (*s1 != *s2)
			return 0;

		s1++;
		s2++;
	}
	return *s1 == *s2;
}