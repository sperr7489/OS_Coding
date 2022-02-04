/**********************************************************************
 * Copyright (c) 2020
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

#include <string.h>
#include <ctype.h>

#include "types.h"
#include "parser.h"

int parse_command(char *command, int *nr_tokens, char *tokens[])
{
	char *curr = command;
	int token_started = false;
	*nr_tokens = 0;

	while (*curr != '\0')
	{ //널문자가 아니라면 이 반복문을 실행한다.
		if (isspace(*curr))
		{ //공백일때!
			//isspace함수는 공백이 아니라면 0을 리턴한다. 공백이면 0이 아닌수.
			*curr = '\0'; //이 부분이 실행된다는 것은 공백이라는 것을 의미한다.
			token_started = false;
		}
		else
		{ //공백이 아닐때!
			if (!token_started)
			{
				tokens[*nr_tokens] = curr;
				*nr_tokens += 1;
				token_started = true;
			}
		}

		curr++;
	}

	return (*nr_tokens > 0); //토큰이 1개이상 존재하면 true를 반환하겠군.
}
