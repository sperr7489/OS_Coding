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

/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING FROM THIS LINE ******       */
#include <stdio.h>
#include "types.h"
#include "list_head.h"

/* Declaration for the stack instance defined in pa0.c */
extern struct list_head stack;
//list_head라는 타입의 stack을 불러온것. 전역변수로 설정해주었다. - qro

/* Entry for the stack 스택에 새롭게 들어온 것. */
struct entry
{
	struct list_head list;
	char *string;
};
/*          ****** DO NOT MODIFY ANYTHING ABOVE THIS LINE ******      */
/*====================================================================*/

/*====================================================================*
 * The rest of this file is all yours. This implies that you can      *
 * include any header files if you want to ...                        */

#include <stdlib.h> /* like this */
#include <string.h>

/**
 * push_stack()
 *
 * DESCRIPTION
 *   Push @string into the @stack. The @string should be inserted into the top
 *   of the stack. You may use either the head or tail of the list for the top.
 */
struct entry *element;
void push_stack(char *string) //string이라는 배열을 갖고 있다.
{
	/* TODO: Implement this function */
	//string이라는 것이 새롭게 들어왔다. 이를 malloc함수로 데이터를 만들어줘야 한다.

	element = (struct entry *)malloc(sizeof(struct entry));
	//entry 구조체의 메모리를 할당해 주었다.
	element->string = malloc(sizeof(char) * (strlen(string) + 1));
	strcpy(element->string, string);
	//entry로 들어올 data인 string을 저장해줌.
	list_add(&(element->list), &stack);
	//stack이 head가 된다.
}

/*
 * 
 * pop_stack()
 *
 * DESCRIPTION
 *   Pop a value from @stack and return it through @buffer. The value should
 *   come from the top of the stack, and the corresponding entry should be
 *   removed from @stack.
 *
 * RETURN
 *   If the stack is not empty, pop the top of @stack, and return 0
 *   If the stack is empty, return -1
 */
int pop_stack(char *buffer) //문자열이 pop이 되어야한다? 일단 해당 문자열이 pop될때 free도 해주어야 한다.
{
	/* TODO: Implement this function */
	//비어있는지를 조사해봐야한다.
	//마지막에 있는 주소를 찾고 그것에 대한 삭제 과정을 진행한다.
	if (!list_empty(&stack))
	{
		//비어있지 않다면 buffer string을 stack에서 없애버린다.

		struct entry *node;
		node = list_first_entry(&stack, struct entry, list);
		//마지막 element의 주소값을 반환한 것을 node 구조체 포인터에 넣는다.

		strcpy(buffer, node->string);
		list_del(&(node->list));
		free(node->string);
		free(node);
		return 0;
	}
	return -1; /* Must fix to return a proper value when @stack is not empty */
}

/**
 * dump_stack()
 *
 * DESCRIPTION
 *   Dump the contents in @stack. Print out @string of stack entries while
 *   traversing the stack from the bottom to the top. Note that the value
 *   should be printed out to @stderr to get properly graded in pasubmit.
 */
void dump_stack(void)
{
	/* TODO: Implement this function */

	// element->string = (char *)malloc(sizeof(char) * 80);
	list_for_each_entry_reverse(element, &stack, list)
	{
		fprintf(stderr, "%s\n", element->string);
	}
}
