/*
 * utils.c
 */

#include "main.h"

/**
 * strchr_multi_byte:
 * @str: String that contains delimiter characters
 * @delim: String with has delimiter characters
 *
 * This function is similar to string library function strchr,
 * but it works on multiple delimiter functions.
 *
 * @Returns: Address of any of first delimiter character found.
 */
char *strchr_multi_byte(const char *str, const char *delim)
{
	bool found = false;
	char *tmp1, *tmp2;

	if (!str || !delim)
		return NULL;

	for (tmp1 = (char *)str; *tmp1 != '\0' ; ++tmp1) {

		for (tmp2 = (char *)delim; *tmp2 != '\0'; ++tmp2) {
			if (*tmp1 == *tmp2) {
				found = true;
				goto done;
			}
		}

	}

done:
	return (char *)(found ? tmp1 : NULL);
}

/**
 * strip_spaces:
 * @str: String that contain spaces.
 * Strips space characters from the string and creates a newly allocated string.
 * Original string is unchanged.
 * Returns: a newly allocated string with out space characters.
 */
char *strip_spaces(char *str)
{
	char *p;
	int i = 0, j = 0;

	p = malloc(strlen(str));
	if (!p)
		return NULL;

	while(*(str + i) != '\0') {
		if (*(str + i) != ' ') {
			*(p + j) = *(str + i);
			j++;
		}

		++i;
	}

	*(p + j) = '\0';
	return p;
}
/**
 * util_strstrip:
 * @str: String that contain spaces.
 * This function strips spaces from the string passed.
 * No new string is created.
 */
void util_strstrip(char *str)
{
	int i = 0, j = 0;
	int len = strlen(str);
	/* Linux kernel banned use of variable length arrays in kernel code.*/
	char temp[len];
	memset(temp, 0, len);

	while(*(str + i) != '\0') {
		if (*(str + i) != ' ') {
			*(temp + j) = *(str + i);
			j++;
		}

		++i;
	}

	*(temp + j) = '\0';
	memcpy(str, temp, j + 1);
}

void util_charstrip(char *str, int c)
{
	int i = 0 , j = 0;
	int len = strlen(str);
	char temp[len];
	memset(temp, 0, len);

	while (*(str + i) != '\0') {
		if (*(str + i) != c) {
			*(temp + j) = *(str + i);
			++j;
		}
		++i;
	}

	*(temp + j) = '\0';
	memcpy(str, temp, j + 1);
}
