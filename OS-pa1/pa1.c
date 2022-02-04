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
#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "types.h"
#include "list_head.h"
#include "parser.h"

void push_history(char *const command);
void dump_history(void);
char *history_to_command(int index);
static void append_history(char *const command);
int num_to_tokens(int index, int *nr_tokens, char **tokens);
void init_tokens(int nr_tokens, char **tokens);
int check_pipe(int *pipe_where, int nr_tokens, char **tokens);
void concat_command(char *command, int nr_tokens, char **tokens);
/********************************************************
 * ***************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */

static int run_command(int nr_tokens, char *tokens[])
{ //이는 이미 __process_command를 통해서 nr_tokens와 *tokens[]는 수정되어있다.
	if (strcmp(tokens[0], "exit") == 0)
		return 0;
	int status;
	pid_t pid;

	//cd를 자식프로레스에서 처리하게 되면 이는 부모에게 영향을 줄 수 없다.
	//하지만 부모 프로세스에서 변경을 한 것은 자식에게도 그대로 들어가기 때문에
	//fork를 하기 전에 cd 명령어를 처리해주면 정상적으로 directory를 이동할 수 있게 된다.

	if (strcmp(tokens[0], "!") == 0)
	{
		num_to_tokens(atoi(tokens[1]), &nr_tokens, tokens);
	}
	//commend로 받은 것들 중에서 |가 있는 지 확인 하는 함수.

	int pipe_where = 0;
	int pipe_state = check_pipe(&pipe_where, nr_tokens, tokens);

	if (strcmp("cd", tokens[0]) == 0)
	{
		if (nr_tokens == 1)
		{
			chdir(getenv("HOME"));
		}
		else
		{
			if (strcmp("~", tokens[1]) == 0)
			{
				strcpy(tokens[1], getenv("HOME"));
			}
			chdir(tokens[1]);
		}
	}
	else if (strcmp(tokens[0], "history") == 0)
	{
		dump_history();
	}
	else if ((pid = fork()) == 0)
	{
		//자식 프로세스라면.
		//tokens라는 배열로 인자를 받기 때문에 execv 형태로 받아내야한다. execl은
		//list 형태로 test 받을 때 사용된다. 예를 들어 "ls -al /tmp"이런식으로!
		//p는 환경변수에 접근해서 이를 읽어 올 수 있도록 하는 suffix.

		if (pipe_state == 1)
		{
			// char **parent_tokens;
			char **child_tokens;

			pid_t pid2;
			int fds[2];
			int status_child;
			pipe(fds);
			if ((pid2 = fork()) == 0)
			{
				char *command = (char *)malloc(sizeof(char) * MAX_COMMAND_LEN);
				child_tokens = (char **)malloc((pipe_where + 1) * sizeof(char *));
				for (int i = 0; i < pipe_where; i++)
				{
					child_tokens[i] = (char *)malloc((strlen(tokens[i])) * sizeof(char));
				}
				// child_tokens[pipe_where] = NULL;
				// devide_token(pipe_where, nr_tokens, tokens, child_tokens, 1); //토큰을 자를 인덱스, 넣을 토큰, 넣어질 토큰.
				concat_command(command, pipe_where, tokens);
				parse_command(command, &pipe_where, child_tokens);
				dup2(fds[1], STDOUT_FILENO);
				close(fds[1]);
				close(fds[0]);
				if (execvp(child_tokens[0], child_tokens) == -1)
				{
					fprintf(stderr, "child : Unable to execute %s\n", child_tokens[0]);
					exit(0);
				}
			}
			wait(&status_child);
			dup2(fds[0], STDIN_FILENO);
			close(fds[0]);
			close(fds[1]);
			if (execvp(tokens[pipe_where + 1], &tokens[pipe_where + 1]) == -1)
			{
				exit(0);
			}
		}
		else
		{
			//external command가 정상적일땐 0이 리턴됨.
			if (execvp(tokens[0], tokens) != 0)
			{
				//에러가 발생했을 때
				fprintf(stderr, "Unable to execute %s\n", tokens[0]); //이는 토큰화된 명령어들이 제대로 실행 될 수 없으면!
				exit(0);
			}
			//에러가 발생하지 않았을 때}
		}
	}
	else
	{
		wait(&status); //자식 프로세스가 끝날때까지 기다린다.그리고 나서 return
	}

	return -EINVAL;
}
/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */

LIST_HEAD(history);
//listhead로 구현된 history 를 이용해서 appen history를 구현하면 된다.

/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
struct entry
{
	struct list_head list;
	char *command;
};

struct entry *element;
void push_history(char *const command)
{
	element = (struct entry *)malloc(sizeof(struct entry));
	//entry 구조체의 메모리를 할당해 주었다.
	element->command = malloc(sizeof(char) * (strlen(command) + 1));
	strcpy(element->command, command);
	//entry로 들어올 data인 string을 저장해줌.
	list_add_tail(&(element->list), &history);
	//stack이 head가 된다.
}

void dump_history(void)
{
	int i = 0;
	// element->string = (char *)malloc(sizeof(char) * 80);
	list_for_each_entry(element, &history, list)
	{
		fprintf(stderr, "%2d: %s", i++, element->command);
	}
}
char *history_to_command(int index)
{
	//인덱스 값을 받고 이 인덱스에 해당하는 값을 문자열로 출력해준다.
	int i = 0;

	list_for_each_entry(element, &history, list)
	{
		if (i == index)
		{
			//인자로 주어받은 인덱스와 동일한 element라면 해당 문자열을 반환한다.
			return element->command;
		}
		i++;
	}
	return NULL;
}
void init_tokens(int nr_tokens, char **tokens)
{
	while (nr_tokens--)
	{
		tokens[nr_tokens] = NULL;
	}
}
int num_to_tokens(int index, int *nr_tokens, char **tokens)
{
	char *commend_temp = (char *)malloc(sizeof(char) * MAX_COMMAND_LEN);
	strcpy(commend_temp, history_to_command(index));
	//commend_temd에는 history에 등록된 번호에 해당하는 문자열이 들어가게 된다.

	init_tokens(*nr_tokens, tokens);
	parse_command(commend_temp, nr_tokens, tokens);
	//이 parse_command이후에는 tokens와 nr_tokens가 수정됨을 알 수 있다.
	if (strchr(tokens[0], '!') != NULL)
	{
		num_to_tokens(atoi(tokens[1]), nr_tokens, tokens);
		return 0;
	}

	free(commend_temp);
	return EXIT_SUCCESS;
}

static void append_history(char *const command)
{
	push_history(command);
}
int check_pipe(int *pipe_where, int nr_tokens, char **tokens)
{
	for (int i = 0; i < nr_tokens; i++)
	{
		if (strcmp(tokens[i], "|") == 0)
		{
			*pipe_where = i;
			return 1; //있을 경우에 1을 리턴!
		}
	}
	return 0; // 없을 경우에 0을 리턴
}
void concat_command(char *command, int nr_tokens, char **tokens)
{

	for (int i = 0; i < nr_tokens; i++)
	{
		if (i == 0)
		{
			strcpy(command, tokens[i]);
			strcat(command, " ");
			continue;
		}
		strcat(command, tokens[i]);
		strcat(command, " ");
	}
}
/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char *const argv[])
{
	return 0;
}

/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char *const argv[])
{
}

/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
static int __process_command(char *command)
{
	char *tokens[MAX_NR_TOKENS] = {NULL};
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	//위의 if문을 들어가지않았다는 것은 커맨드가 있다는 것이자나.

	return run_command(nr_tokens, tokens);
}

static bool __verbose = true;
static const char *__color_start = "[0;31;40m";
static const char *__color_end = "[0m";

static void __print_prompt(void)
{
	char *prompt = "$";
	if (!__verbose)
		return;

	fprintf(stderr, "%s%s%s ", __color_start, prompt, __color_end);
}

/***********************************************************************
 * main() of this program.
 */
int main(int argc, char *const argv[])
{
	char command[MAX_COMMAND_LEN] = {'\0'};
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1)
	{
		switch (opt)
		{
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv)))
		return EXIT_FAILURE;

	/**
	 * Make stdin unbuffered to prevent ghost (buffered) inputs during
	 * abnormal exit after fork()
	 */
	setvbuf(stdin, NULL, _IONBF, 0);

	while (true)
	{
		__print_prompt();

		if (!fgets(command, sizeof(command), stdin))
			break;

		append_history(command);
		ret = __process_command(command);
		//process_command는 run_command의 값을 반환한다. exit가
		//입력될 때 ret은 0이 된다.
		if (!ret)
			break;
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}
