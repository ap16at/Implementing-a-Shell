#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

typedef struct {
	int size;
	char **items;
} tokenlist;

typedef struct {
	char *path;
	char **args;
} command;

typedef struct {
	char *in_file;
	char *out_file;
	bool input;
	bool output;
} redirection;

char *get_input(void);
tokenlist *get_tokens(char *input);

tokenlist *new_tokenlist(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);

command *new_command(char *path, char **args);
void free_command(command *cmd);

redirection *new_redirection(void);
void add_file(redirection *red, char *file, bool input);
void free_redirection(redirection *red);

void print_prompt(void);
void convert_to_env_var(char *token);
void tilde_expansion(char* path);
void execute_command(command *cmd);
void redirect(command *cmd, redirection *red);
command *fetch_command(tokenlist *tokens);
command *path_search(tokenlist *tokens);
redirection *convert_expand_detect_io(tokenlist *tokens);

void pipe1(command *cmd1, command *cmd2);
void pipe2(command *cmd1, command *cmd2, command *cmd3);
char* piping(char *input, int p_count);
int pipe_count(char* argv, char* str);

int main(int argc, char **argv, char **envp)
{
	while(1)
	{
		print_prompt();
		printf(" > ");

		char *input = get_input();
		tokenlist *tokens= get_tokens(input);
		int p_count = 0;																														// used to keep track of how many pipes are needed

		if(strcmp(input, "exit") == 0)
			break;
		else if(strlen(input) == 0)
			continue;

		// iterate through tokens to convert env vars, expand tilde, and check for io redirection
		redirection *red = convert_expand_detect_io(tokens);

		// iterate through tokens to check for piping
		for(int i = 0; i < tokens->size; i++)
		{
			char *token = tokens->items[i];
			if(token[0] == '|')
				p_count++;
		}

		// io needed
		if(red->input || red->output)
			redirect(fetch_command(tokens), red);
		// piping needed
		else if(p_count > 0)
			piping(input, p_count);
		else
			execute_command(fetch_command(tokens));

		free(input);
		free_tokens(tokens);
	}

	return 0;
}

command *fetch_command(tokenlist *tokens)
{
	if(tokens->items[0][0] != '/')
		return path_search(tokens);
	else
		return new_command(tokens->items[0], tokens->items);
}

void redirect(command *cmd, redirection *red)
{
	if(access(cmd->path, F_OK) == 0)
	{
		int in, out;
		in = open(red->in_file, O_RDONLY);
		out = creat(red->out_file, 0666);
		// error checking for file opening
		if(red->input)
		{
			if(in == -1)
			{
				printf("%s: Error accessing file.\n", red->in_file);
				free_command(cmd);
				free_redirection(red);
				close(in);
				return;
			}
		}
		if(red->output)
		{
			if(out == -1)
			{
				printf("%s: Error accessing/creating file.\n", red->out_file);
				free_command(cmd);
				free_redirection(red);
				close(out);
				return;
			}
		}

		// execute with directed i/o
		int pid = fork();
		if(pid == 0)
		{
			if(red->input)
			{
				close(0);
				dup(in);
				close(in);
			}
			if(red->output)
			{
				close(1);
				dup(out);
				close(out);
			}
			execv(cmd->path, cmd->args);
		}
		else
		{
			if(red->input)
				close(in);
			if(red->output)
				close(out);
			waitpid(pid, NULL, 0);
		}
	} else
		printf("%s: Command not found.\n", cmd->path);

	free_redirection(red);
	free_command(cmd);
}

void execute_command(command *cmd)
{
	// check if valid command
	if(access(cmd->path, F_OK) == 0)
	{
		int pid = fork();
		if(pid == 0)
			execv(cmd->path, cmd->args);
		else
			waitpid(pid, NULL, 0);
	} else
		printf("%s: Command not found.\n", cmd->path);

	free_command(cmd);
}

redirection *convert_expand_detect_io(tokenlist *tokens)
{
	redirection *red = new_redirection();

	for(int i = 0; i < tokens->size; i++)
	{
		char *token = tokens->items[i];
		char *temp = token + 1;

		// environmental variables
		if(token[0] == '$')
		{
			char *converted = getenv(temp);
			if(converted != NULL)
			{
				tokens->items[i] = (char *)realloc(tokens->items[i], strlen(converted)+1);
				strcpy(tokens->items[i], converted);
			}
			else
				printf("%s: Undefined variable\n", token+1);
		}
		// tilde expansion
		else if(token[0] == '~')																										
		{
			// HOME vasriable for the home directory
			char *home = getenv("HOME");
			char *home_c = (char*)malloc(strlen(home)+1);
			strcpy(home_c, home);
			home_c = (char *)realloc(home_c, strlen(home_c)+strlen(temp)+1);
			strcat(home_c, temp);
			tokens->items[i] = (char *)realloc(tokens->items[i], strlen(home_c)+1);
			strcpy(tokens->items[i], home_c);
			free(home_c);
		}
		else if(token[0] == '>')
			add_file(red, tokens->items[i+1], false);
		else if(token[0] == '<')
			add_file(red, tokens->items[i+1], true);
	}

	return red;
}

// commands are passed in and a fork() is done
void pipe1(command *cmd1, command *cmd2)
{
	int p1_to_p2[2];
	pipe(p1_to_p2);
	int pid1 = fork();

	if(pid1 == 0)
	{
		dup2(p1_to_p2[0], 0);
		close(p1_to_p2[1]);
		execv(cmd2->path, cmd2->args);
	}
	else
	{
		dup2(p1_to_p2[1], 1);
		close(p1_to_p2[0]);
		execv(cmd1->path, cmd1->args);
	}
}

// commands are passed in and a fork() is done
void pipe2(command *cmd1, command *cmd2, command *cmd3)
{
	int status;
	int p_fds[4];
	pipe(p_fds);
	pipe(p_fds + 2);

	if(fork() == 0)
	{
		dup2(p_fds[1], 1);
		close(p_fds[0]);
		close(p_fds[1]);
		close(p_fds[2]);
		close(p_fds[3]);
		execv(cmd1->path, cmd1->args);
	}
	else
	{
		if(fork() == 0)
		{
			dup2(p_fds[0], 0);
			dup2(p_fds[3], 1);
			close(p_fds[0]);
			close(p_fds[1]);
			close(p_fds[2]);
			close(p_fds[3]);
			execv(cmd2->path, cmd2->args);
		}
		else
		{
			if(fork() == 0)
			{
				dup2(p_fds[2], 0);
				close(p_fds[0]);
				close(p_fds[1]);
				close(p_fds[2]);
				close(p_fds[3]);
				execv(cmd3->path, cmd3->args);
			}
		}
	}
}

char* piping(char *input, int p_count)
{
	// input is tokenized with the ' | ' as a delimiter
	char *buf = (char *)malloc(strlen(input) + 1);
	strcpy(buf, input);

	tokenlist *tokens = new_tokenlist();

	char *tok = strtok(buf, " | ");
	while (tok != NULL) {
		add_token(tokens, tok);
		tok = strtok(NULL, " | ");
	}
	free(buf);

	// checks number of pipes that were found
	if(p_count == 1)
	{
		tokenlist *token1 = new_tokenlist();				// new token lists are created for storing
		tokenlist *token2 = new_tokenlist();				// one token each.

		char *token1_ = tokens->items[0];					// tokens from the old list are separated
		char *token2_ = tokens->items[1];

		add_token(token1, token1_);							// added to new lists
		add_token(token2, token2_);

		command *cmd1, *cmd2;

		cmd1 = fetch_command(token1);						// command struct is used
		cmd2 = fetch_command(token2);

		pipe1(cmd1, cmd2);									// function for a single pipe is called

		free_tokens(token1);
		free_tokens(token2);

	}
	// everything is repeated with the addition of one command
	else if(p_count == 2)	
	{
		tokenlist *token1 = new_tokenlist();
		tokenlist *token2 = new_tokenlist();
		tokenlist *token3 = new_tokenlist();

		char *token1_ = tokens->items[0];
		char *token2_ = tokens->items[1];
		char *token3_ = tokens->items[2];

		add_token(token1, token1_);
		add_token(token2, token2_);
		add_token(token3, token3_);

		command *cmd1, *cmd2, *cmd3;

		cmd1 = fetch_command(token1);
		cmd2 = fetch_command(token2);
		cmd3 = fetch_command(token3);

		pipe2(cmd1, cmd2, cmd3);

		free_tokens(token1);
		free_tokens(token2);
		free_tokens(token3);

	}
}

//gets the correct environment variables to be printed within main before the '>'
void print_prompt(void)
{
	char *user = getenv("USER");
	char *machine = getenv("MACHINE");
	char *pwd = getenv("PWD");
	printf("%s@%s : %s ", user,machine,pwd);
}

command *path_search(tokenlist *tokens)					//path search function that locates file that holds info so command can execute
{
	char *path = getenv("PATH");
	char *path_c = (char*)malloc(strlen(path)+1);		//to store a copy of the $PATH 
	strcpy(path_c, path);

	int colon_count = 0;
	int i = 0;

	for(i = 0; i < strlen(path_c); i++)					//loop to count number of colons within $PATH
	{
		if(path_c[i] == ':')
			colon_count++;
	}

	char *tkn = strtok(path_c,":");
	char *locations[colon_count];

	//splits up the $PATH copy into sections, each containing sub directories
	for(i = 0; i < colon_count; i++)					//splits up the $PATH copy into sections, each containing sub directories
	{													//typically the /usr/bin is where the proper files are for most commands
		if(tkn == NULL) break;							//all the sub paths are stored inside the locations array
		locations[i] = tkn;
		tkn = strtok(NULL, ":");
	}

	char *token = tokens->items[0];
	char *command_list[tokens->size];

	//loop to gather commands passed in from input and store copy into command_list[] array
	for(i = 0; i < tokens->size; i++)
	{
		char *token = tokens->items[i];
		if(token[0] == '>' || token[0] == '<')	
			break;
		command_list[i] = token;
	}

	int size = i;
	char slash[] = "/";
	strcat(slash, command_list[0]);						//to concatenate a '/' onto the command passed into the function which will be used later to find correct path

	char temp[100];

	for(i = 0; i < colon_count; i++)					//this loop searches to find if the command passed in has a corresponding path that allows it to execute
	{													//if not found, returns 'command not found'
		strcpy(temp, locations[i]);
		strcat(temp, slash);

		if(access(temp, F_OK) == 0)
			break;
	}

	command_list[tokens->size] = NULL;
	return new_command(temp, command_list);
}

redirection *new_redirection(void)
{
	redirection *red = (redirection *)malloc(sizeof(redirection));
	red->input = false;
	red->output = false;
	red->in_file = NULL;
	red->out_file = NULL;
	return red;
}

void add_file(redirection *red, char *file, bool input)
{
	if(input)
	{
		red->input = true;
		if(red->in_file != NULL)
			red->in_file = (char *)realloc(red->in_file, strlen(file)+1);
		else
			red->in_file = (char *)malloc(strlen(file)+1);
		strcpy(red->in_file, file);
	}
	else
	{
		red->output = true;
		if(red->out_file != NULL)
			red->out_file = (char *)realloc(red->out_file, strlen(file)+1);
		else
			red->out_file = (char *)malloc(strlen(file)+1);
		strcpy(red->out_file, file);
	}
}

command *new_command(char *path, char **args)
{
	command *cmd = (command *)malloc(sizeof(command));
	cmd->path = (char *)malloc(strlen(path)+1);
	cmd->args = (char **)malloc(sizeof(char *));
	strcpy(cmd->path, path);
	cmd->args = args;
	return cmd;
}

tokenlist *new_tokenlist(void)
{
	tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = (char **)malloc(sizeof(char *));
	tokens->items[0] = NULL;
	return tokens;
}

void add_token(tokenlist *tokens, char *item)
{
	int i = tokens->size;

	tokens->items = (char **)realloc(tokens->items, (i+2)*sizeof(char *));
	tokens->items[i] = (char *)malloc(strlen(item)+1);
	tokens->items[i+1] = NULL;
	strcpy(tokens->items[i], item);

	tokens->size += 1;
}

char *get_input(void)
{
	char *buffer = NULL;
	int bufsize = 0;

	char line[5];
	while(fgets(line, 5, stdin) != NULL)
	{
		int addby = 0;
		char *newln = strchr(line, '\n');
		if(newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;

		buffer = (char *) realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;

		if(newln != NULL)
			break;
	}

	buffer = (char *) realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;

	return buffer;
}

tokenlist *get_tokens(char *input)
{
	char *buf = (char *)malloc(strlen(input) + 1);
	strcpy(buf, input);

	tokenlist *tokens = new_tokenlist();

	char *tok = strtok(buf, " ");
	while(tok != NULL) {
		add_token(tokens, tok);
		tok = strtok(NULL, " ");
	}
	free(buf);
	return tokens;
}

void free_redirection(redirection *red)
{
	red->input = false;
	red->output = false;
	free(red->in_file);
	free(red->out_file);
	free(red);
}

void free_command(command *cmd)
{
	free(cmd->path);
	free(cmd);
}

void free_tokens(tokenlist *tokens)
{
	for(int i = 0; i < tokens->size; i++)
		free(tokens->items[i]);
	free(tokens->items);
	free(tokens);
}
