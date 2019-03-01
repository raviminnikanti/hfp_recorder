/*
 * utils.h
 */

#ifndef UTILS_H_
#define UTILS_H_

char *strchr_multi_byte(const char *str, const char *delim);
char *strip_spaces(char *str);
void util_strstrip(char *str);
void util_charstrip(char *str, int c);
#endif /* UTILS_H_ */
