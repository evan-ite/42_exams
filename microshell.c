/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   microshell.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: evan-ite <evan-ite@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/29 17:05:27 by evan-ite          #+#    #+#             */
/*   Updated: 2024/06/03 14:44:52 by evan-ite         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#define READ 0
#define WRITE 1

/* Prints error message, dependent on boolean newline prints newline
and dependent on exit code exits the program */
static void	handle_error(char *msg, int newline, int exit_code)
{
	int	i;

	i = -1;
	while (msg[++i])
		write(2, &msg[i], 1);
	if (newline)
		write(2, "\n", 1);
	if (exit_code >= 0)
		exit(exit_code);
}

/* Function to change directory, prints error on failure */
static void	cd(int start, int i, char **args)
{
	if (i - start != 2)
		handle_error("error: cd: bad arguments", 1, -1);
	else if (chdir(args[i - 1]) != 0) {
		handle_error("error: cd: cannot change directory to ", 0, -1);
		handle_error(args[i - 1], 1, -1);
	}
}

static void	exec(int start, int *pipes, int has_pipe, char **args, char **envp)
{
	// Check if output should be redirected
	if (has_pipe) {
		dup2(pipes[WRITE], 1);
		close(pipes[WRITE]), close(pipes[READ]);
	}
	if (execve(args[start], &args[start], envp) == -1) {
		close(pipes[WRITE]), close(pipes[READ]);
		handle_error("error: cannot execute ", 0, -1);
		handle_error(args[start], 1, 1);
	}
}

static void	command(int start, int i, char **args, char **envp)
{
	pid_t	pid;
	int	pipes[2];
	int	has_pipe = args[i] && !strcmp(args[i], "|");

	// Check if command is cd builtin
	if (!strcmp(args[start], "cd"))
		return (cd(start, i, args));
	if (has_pipe && pipe(pipes) == -1)
		handle_error("error: fatal", 1, 1);
	pid = fork();
	if (pid == -1) {
		if (has_pipe)
			close(pipes[READ]), close(pipes[WRITE]);
		handle_error("error: fatal", 1, 1);
	}
	else if (!pid) { // execute command in child process
		args[i] = NULL;
		exec(start, pipes, has_pipe, args, envp);
	}
	waitpid(pid, NULL, 0);
	if (has_pipe) { // redirect for next command
		dup2(pipes[READ], 0);
		close(pipes[WRITE]), close(pipes[READ]);
	}
}


int	main(int argc, char **argv, char **envp)
{
	int	i;
	int	start;

	if (argc < 2)
		handle_error("error: provide arguments", 1, 1);
	i = 1;
	start = 1;
	while (i < argc && argv[i])
	{
		while (argv[i] && strcmp(argv[i], "|") && strcmp(argv[i], ";"))
			i++;
		if (start != i)
			command(start, i, argv, envp);
		i++;
		start = i;
	}
	return (0);
}

