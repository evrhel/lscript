#if !defined(STRUTIL_H)
#define STRUTIL_H

/**
 * Tests string equality.
 * 
 * @param s1 The first string.
 * @param s2 The second string.
 * 
 * @return nonzero if the strings are equal.
 */
inline int str_equals(const char *s1, const char *s2)
{
	while (*s1 && *s2)
	{
		if (*s1 != *s2) return 0;
	}
	return *s1 == *s2;
}

/**
 * Tests string equality, ignoring case.
 * 
 * @param s1 The first string.
 * @param s2 The second string.
 * 
 * @return nonzero if the strings are case-insensitively equal.
 */ 
inline int str_equals_ignore_case(const char *s1, const char *s2)
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

#endif