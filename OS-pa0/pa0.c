/**********************************************************************
 * Copyright (c) 2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/* Stack instance */
LIST_HEAD(stack);
//stack이라는 헤드포인터로 이는 empty list이다.
void push_stack(char *string);
int pop_stack(char *buffer);
void dump_stack(void);

#define MAX_BUFFER 80

unsigned int seed = 0xdfcc230;

static char *generate_string(char *buffer)
{
	char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int len = 1 + random() % (MAX_BUFFER - 2);
	//랜덤하게 문자열의 길이 뽑느것.
	/* +1 to prevent null string
			 * -1 to consider \0 termination char
			 */
	int i;

	for (i = 0; i < len; i++)
	{
		buffer[i] = chars[random() % strlen(chars)];
	} //buffer라는 것은 문자열로 되어있는것.
	buffer[len] = '\0';
	return buffer;
} //문자열을 생성해주는 함수.

int main(int argc, const char *argv[])
{
	int ret;
	char buffer[MAX_BUFFER];
	unsigned int i;

	if (argc != 1)
		seed = atoi(argv[1]);

	srandom(seed);

	/* Push 4 values */
	for (i = 0; i < 4; i++)
	{
		push_stack(generate_string(buffer));
		//buffer라는 문자열을 생성해서 push해준다.
		fprintf(stderr, "%s\n", buffer);
	}
	fprintf(stderr, "\n");

	/* Dump the current stack */
	dump_stack();
	fprintf(stderr, "\n");

	/* Pop 3 values */
	for (i = 0; i < 3; i++)
	{
		memset(buffer, 0x00, MAX_BUFFER);
		//memset함수로 buffer를 모두 0으로 초기화 해주었다.
		pop_stack(buffer);
		//pop_stack은 마지막 원소를 없애는 것이다. 어차피 buffer는 초기화
		//되어 있으므로 신경써줄 필요 없다.
		fprintf(stderr, "%s\n", buffer);
	}
	fprintf(stderr, "\n");

	/* Dump the current stack */
	dump_stack();
	fprintf(stderr, "\n");

	/* And so on ..... */
	for (i = 0; i < (1 << 12); i++)
	{
		push_stack(generate_string(buffer));
	}
	for (i = 0; i < (1 << 12) - 8; i++)
	{
		pop_stack(buffer);
	}

	dump_stack();
	fprintf(stderr, "\n");

	/* Empty the stack by popping out all entries */
	i = 0;
	while ((ret = pop_stack(buffer)) >= 0 && i++ < 1000)
	{
		fprintf(stderr, "%s\n", buffer);
		memset(buffer, 0x00, MAX_BUFFER);
	}

	return EXIT_SUCCESS;
}
