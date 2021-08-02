////////////////////////////////////////////////////////////////////////
// COMP1521 21t2 -- Assignment 2 -- shuck, A Simple Shell
// <https://www.cse.unsw.edu.au/~cs1521/21T2/assignments/ass2/index.html>
//
// Written by YOUR-NAME-HERE (z5555555) on INSERT-DATE-HERE.
//
// 2021-07-12    v1.0    Team COMP1521 <cs1521@cse.unsw.edu.au>
// 2021-07-21    v1.1    Team COMP1521 <cs1521@cse.unsw.edu.au>
//     * Adjust qualifiers and attributes in provided code,
//       to make `dcc -Werror' happy.
//

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// [[ TODO: put any extra `#include's here ]]
#include <glob.h>
#include <spawn.h>
// [[ TODO: put any `#define's here ]]

//
// Interactive prompt:
//     The default prompt displayed in `interactive' mode --- when both
//     standard input and standard output are connected to a TTY device.
//
static const char *const INTERACTIVE_PROMPT = "shuck& ";

//
// Default path:
//     If no `$PATH' variable is set in Shuck's environment, we fall
//     back to these directories as the `$PATH'.
//
static const char *const DEFAULT_PATH = "/bin:/usr/bin";

//
// Default history shown:
//     The number of history items shown by default; overridden by the
//     first argument to the `history' builtin command.
//     Remove the `unused' marker once you have implemented history.
//
static const int DEFAULT_HISTORY_SHOWN __attribute__((unused)) = 10;

//
// Input line length:
//     The length of the longest line of input we can read.
//
static const size_t MAX_LINE_CHARS = 1024;

//
// Special characters:
//     Characters that `tokenize' will return as words by themselves.
//
static const char *const SPECIAL_CHARS = "!><|";

//
// Word separators:
//     Characters that `tokenize' will use to delimit words.
//
static const char *const WORD_SEPARATORS = " \t\r\n";

// [[ TODO: put any extra constants here ]]
char home_path[30];
char U_name[10];
// [[ TODO: put any type definitions (i.e., `typedef', `struct', etc.) here ]]

static void execute_command(char **words, char **path, char **environment);
static void do_exit(char **words);
static int is_executable(char *pathname);
static char **tokenize(char *s, char *separators, char *special_chars);
static void free_tokens(char **tokens);

// [[ TODO: put any extra function prototypes here ]]
//cd function
static void do_cd(char **words);
static void do_pwd(char **words);
static void find_path();
static void save_history(char *shuru);
static void view_history(char **words);
static void redict1(char **words);
int main(void)
{
    // Ensure `stdout' is line-buffered for autotesting.
    setlinebuf(stdout);

    // Environment variables are pointed to by `environ', an array of
    // strings terminated by a NULL value -- something like:
    //     { "VAR1=value", "VAR2=value", NULL }
    extern char **environ;

    // Grab the `PATH' environment variable for our path.
    // If it isn't set, use the default path defined above.
    char *pathp;
    if ((pathp = getenv("PATH")) == NULL)
    {
        pathp = (char *)DEFAULT_PATH;
    }
    char **path = tokenize(pathp, ":", "");

    // Should this shell be interactive?
    bool interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    // Main loop: print prompt, read line, execute command
    while (1)
    {
        // If `stdout' is a terminal (i.e., we're an interactive shell),
        // print a prompt before reading a line of input.
        if (interactive)
        {
            fputs(INTERACTIVE_PROMPT, stdout);
            fflush(stdout);
        }

        char line[MAX_LINE_CHARS];
        if (fgets(line, MAX_LINE_CHARS, stdin) == NULL)
            break;

        // Tokenise and execute the input line.
        char **command_words =
            tokenize(line, (char *)WORD_SEPARATORS, (char *)SPECIAL_CHARS);
        execute_command(command_words, path, environ);
        free_tokens(command_words);
    }

    free_tokens(path);
    return 0;
}

//
// Execute a command, and wait until it finishes.
//
//  * `words': a NULL-terminated array of words from the input command line
//  * `path': a NULL-terminated array of directories to search in;
//  * `environment': a NULL-terminated array of environment variables.
//
static void execute_command(char **words, char **path, char **environment)
{
    assert(words != NULL);
    assert(path != NULL);
    assert(environment != NULL);

    char *program = words[0];

    char *pattern = words[1];

    if (program == NULL)
    {
        // nothing to do
        return;
    }

    if (pattern != NULL)
    {
        //judge the pattern
        char *pattern1 = strrchr(pattern, '*');
        char *pattern2 = strrchr(pattern, '?');
        char *pattern3 = strrchr(pattern, '~');
        char *pattern4 = strrchr(pattern, '[');

        if (pattern1 != NULL || pattern2 != NULL || pattern3 != NULL || pattern4 != NULL)
        {

            glob_t matches; // holds pattern expansion
            int result = glob(pattern, GLOB_NOCHECK | GLOB_TILDE, NULL, &matches);

            if (result != 0)
            {
                //printf("glob returns %d\n", result);
            }
            else
            {
                //printf("%d matches\n", (int)matches.gl_pathc);
                int i = 0;
                for (i = 0; i < matches.gl_pathc; i++)
                {
                    words[1] = matches.gl_pathv[i];
                    execute_command(words, path, environment);
                    //printf("\t%s\n", matches.gl_pathv[i]);
                }
            }
        }
    }

    //judge redirec
    if (strcmp(program, "<") == 0)
    {
        redict1(words);
        return;
    }

    int redirecFLag;
    for (redirecFLag = 0; words[redirecFLag] != NULL; redirecFLag++)
    {
        if (strcmp(words[redirecFLag], ">") == 0)
        {
            if (words[redirecFLag + 1] != NULL && strcmp(words[redirecFLag + 1], ">") == 0)
            {
                // create a pipe
                int pipe_file_descriptors[2];
                if (pipe(pipe_file_descriptors) == -1)
                {
                    perror("pipe");
                    return;
                }

                // create a list of file actions to be carried out on spawned process
                posix_spawn_file_actions_t actions;
                if (posix_spawn_file_actions_init(&actions) != 0)
                {
                    perror("posix_spawn_file_actions_init");
                    return;
                }

                // tell spawned process to close unused read end of pipe
                // without this - spawned process would not receive EOF
                // when read end of the pipe is closed below,
                if (posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[0]) != 0)
                {
                    perror("posix_spawn_file_actions_init");
                    return;
                }

                // tell spawned process to replace file descriptor 1 (stdout)
                // with write end of the pipe
                if (posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[1], 1) != 0)
                {
                    perror("posix_spawn_file_actions_adddup2");
                    return;
                }

                pid_t pid;
                extern char **environ;
                char *date_argv[] = {"/bin/date", "--utc", NULL};
                if (posix_spawn(&pid, "/bin/date", &actions, NULL, date_argv, environ) != 0)
                {
                    perror("spawn");
                    return;
                }
                // close unused write end of pipe
                // in some case processes will deadlock without this
                // not in this case, but still good practice
                close(pipe_file_descriptors[1]);

                // create a stdio stream from read end of pipe
                FILE *f = fdopen(pipe_file_descriptors[0], "r");
                if (f == NULL)
                {
                    perror("fdopen");
                    return;
                }

                // read a line from read-end of pipe
                char line[256];
                if (fgets(line, sizeof line, f) == NULL)
                {
                    fprintf(stderr, "no output from date\n");
                    return;
                }

                printf("output captured from /bin/date was: '%s'\n", line);

                // close read-end of the pipe
                // spawned process will now receive EOF if attempts to read input
                fclose(f);

                int exit_status;
                if (waitpid(pid, &exit_status, 0) == -1)
                {
                    perror("waitpid");
                    return;
                }
                printf("/bin/date exit status was %d\n", exit_status);

                // free the list of file actions
                posix_spawn_file_actions_destroy(&actions);
            }
            else
            {
            }
        }

        if (strcmp(words[redirecFLag], "<") == 0)
        {
        }
    }

    if (strcmp(program, "history") == 0)
    {
        //view the history command
        view_history(words);
        return;
    }

    if (strcmp(program, "!") == 0)
    {
        //exec the history command
    }

    if (strcmp(program, "exit") == 0)
    {
        do_exit(words);
        // `do_exit' will only return if there was an error.
        return;
    }

    // [[ TODO: add code here to implement subset 0 ]]
    if (strcmp(program, "cd") == 0)
    {
        do_cd(words);
        return;
    }

    if (strcmp(program, "pwd") == 0)
    {
        do_pwd(words);
        return;
    }

    // [[ TODO: change code below here to implement subset 1 ]]

    if (strrchr(program, '/') == NULL)
    {
        fprintf(stderr, "--- UNIMPLEMENTED: searching for a program to run\n");
    }

    //save the history command
    save_history(program);

    //is_executable(program)
    //need add the path
    int pathIndex;
    for (pathIndex = 0; path[pathIndex] != NULL; pathIndex++)
    {
        strcat(path[pathIndex], "/");
        strcat(path[pathIndex], program);
        printf("path id %s \n", path[pathIndex]);
        if (is_executable(path[pathIndex]))
        {
            //do it
            pid_t pid;
            extern char **environ;

            // char *date_argv[10];
            // int i=1;
            // while(strcmp(words[i], '\n')!=0){
            //     date_argv[i-1]=words[i];
            //     i++;
            // }
            // date_argv[i]='\n';
            //char *date_argv[] = {"/bin/date", "--utc", NULL};

            // char *date_argv;
            // date_argv=(*words)+1

            // spawn "/bin/date" as a separate process
            if (posix_spawn(&pid, path[pathIndex], NULL, NULL, words, environ) != 0)
            {
                perror("spawn");
                exit(1);
            }

            // wait for spawned processes to finish
            int exit_status;
            if (waitpid(pid, &exit_status, 0) == -1)
            {
                perror("waitpid");
                exit(1);
            }

            printf(" %s exit status was %d\n", path[pathIndex], exit_status);
            return;
            //fprintf(stderr, "--- UNIMPLEMENTED: running a program\n");
        }
    }

    printf("%S: command not found\n",program);
}

//
// Implement the `exit' shell built-in, which exits the shell.
//
// Synopsis: exit [exit-status]
// Examples:
//     % exit
//     % exit 1
//
static void do_exit(char **words)
{
    assert(words != NULL);
    assert(strcmp(words[0], "exit") == 0);

    int exit_status = 0;

    if (words[1] != NULL && words[2] != NULL)
    {
        // { "exit", "word", "word", ... }
        fprintf(stderr, "exit: too many arguments\n");
    }
    else if (words[1] != NULL)
    {
        // { "exit", something, NULL }
        char *endptr;
        exit_status = (int)strtol(words[1], &endptr, 10);
        if (*endptr != '\0')
        {
            fprintf(stderr, "exit: %s: numeric argument required\n", words[1]);
        }
    }

    exit(exit_status);
}

static void do_cd(char **words)
{
    assert(words != NULL);
    assert(strcmp(words[0], "cd") == 0);
    //printf("cd is '%s'\n", words[1]);

    //printf("path is '%s'\n", words[1]);
    if (words[1] == NULL)
    {
        char *value = getenv("HOME");
        if (chdir(value) != 0)
        {
            perror("chdir");
            return;
        }
    }
    else
    {
        //do cd and save path to env
        if (chdir(words[1]) != 0)
        {
            perror("chdir");
            return;
        }
    }

    char pathname[20];
    if (getcwd(pathname, sizeof pathname) == NULL)
    {
        perror("getcwd");
        return;
    }
    //setenv("STATUS", "great", 1);
    setenv("STATUS", pathname, 1);
}

static void do_pwd(char **words)
{
    assert(words != NULL);
    assert(strcmp(words[0], "pwd") == 0);

    char *value = getenv("STATUS");
    printf("current directory is '%s'\n", value);
}

static void find_path(void)
{
    memset(home_path, 30, 0);
    if (strcmp(U_name, "root") == 0)
    {
        strcpy(home_path, "/root/");
    }
    else
    {
        strcpy(home_path, "/home/");
        strcat(home_path, U_name);
        strcat(home_path, "/");
    }
}
static void save_history(char *shuru)
{
    int fd;
    find_path();
    strcat(home_path, ".shuck_history");
    if ((fd = open(home_path, O_WRONLY | O_APPEND)) == -1)
    {
        fd = creat(home_path, 0644);
        close(fd);
        fd = open(home_path, O_WRONLY | O_APPEND);
    }
    write(fd, shuru, strlen(shuru));
    write(fd, "\n", 1);
    close(fd);
}

static void view_history(char **words)
{
    assert(words != NULL);
    assert(strcmp(words[0], "history") == 0);
    find_path();
    strcat(home_path, ".shuck_history");
    int fd;
    fd = open(home_path, O_RDONLY);

    char a[1];
    if (words[1] != NULL)
    {
        int num = atoi(words[1]);
        for (num; num >= 1; num--)
        {
            if (read(fd, a, 1) != 0)
            {
                printf("%c", a[0]);
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        int num = 10;
        for (num; num >= 1; num--)
        {
            if (read(fd, a, 1) != 0)
            {
                printf("%c", a[0]);
            }
            else
            {
                break;
            }
        }
    }
    close(fd);
}

static void redict1(char **words)
{
    printf("--1-\n");
    // create a pipe
    int pipe_file_descriptors[2];
    if (pipe(pipe_file_descriptors) == -1)
    {
        perror("pipe");
        return;
    }

    // create a list of file actions to be carried out on spawned process
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0)
    {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // tell spawned process to close unused write end of pipe
    // without this - spawned process will not receive EOF
    // when write end of the pipe is closed below,
    // because spawned process also has the write-end open
    // deadlock will result
    if (posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[1]) != 0)
    {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // tell spawned process to replace file descriptor 0 (stdin)
    // with read end of the pipe
    if (posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[0], 0) != 0)
    {
        perror("posix_spawn_file_actions_adddup2");
        return;
    }

    // create a process running /usr/bin/sort
    // sort reads lines from stdin and prints them in sorted order
    pid_t pid;
    extern char **environ;
    if (posix_spawn(&pid, words[2], &actions, NULL, words + 2, environ) != 0)
    {
        perror("spawn");
        return;
    }

    // close unused read end of pipe
    close(pipe_file_descriptors[0]);

    // create a stdio stream from write-end of pipe
    FILE *f = fdopen(pipe_file_descriptors[1], "w");
    if (f == NULL)
    {
        perror("fdopen");
        return;
    }

    // send some input to the /usr/bin/sort process
    //sort with will print the lines to stdout in sorted order
    // fprintf(f, words[1]);
    printf("%s\n", words[0]);
    printf("%s\n", words[1]);
    printf("%s\n", words[2]);

    printf("%s\n", words[3]);

    freopen(words[1], "r", f);

    // close write-end of the pipe
    // without this sort will hang waiting for more input
    fclose(f);

    int exit_status;
    if (waitpid(pid, &exit_status, 0) == -1)
    {
        perror("waitpid");
        return;
    }
    printf("%s exit status was %d\n", words[2], exit_status);

    // free the list of file actions
    posix_spawn_file_actions_destroy(&actions);
}

//
// Check whether this process can execute a file.  This function will be
// useful while searching through the list of directories in the path to
// find an executable file.
//
static int is_executable(char *pathname)
{
    struct stat s;
    return
        // does the file exist?
        stat(pathname, &s) == 0 &&
        // is the file a regular file?
        S_ISREG(s.st_mode) &&
        // can we execute it?
        faccessat(AT_FDCWD, pathname, X_OK, AT_EACCESS) == 0;
}

//
// Split a string 's' into pieces by any one of a set of separators.
//
// Returns an array of strings, with the last element being `NULL'.
// The array itself, and the strings, are allocated with `malloc(3)';
// the provided `free_token' function can deallocate this.
//
static char **tokenize(char *s, char *separators, char *special_chars)
{
    size_t n_tokens = 0;

    // Allocate space for tokens.  We don't know how many tokens there
    // are yet --- pessimistically assume that every single character
    // will turn into a token.  (We fix this later.)
    char **tokens = calloc((strlen(s) + 1), sizeof *tokens);
    assert(tokens != NULL);

    while (*s != '\0')
    {
        // We are pointing at zero or more of any of the separators.
        // Skip all leading instances of the separators.
        s += strspn(s, separators);

        // Trailing separators after the last token mean that, at this
        // point, we are looking at the end of the string, so:
        if (*s == '\0')
        {
            break;
        }

        // Now, `s' points at one or more characters we want to keep.
        // The number of non-separator characters is the token length.
        size_t length = strcspn(s, separators);
        size_t length_without_specials = strcspn(s, special_chars);
        if (length_without_specials == 0)
        {
            length_without_specials = 1;
        }
        if (length_without_specials < length)
        {
            length = length_without_specials;
        }

        // Allocate a copy of the token.
        char *token = strndup(s, length);
        assert(token != NULL);
        s += length;

        // Add this token.
        tokens[n_tokens] = token;
        n_tokens++;
    }

    // Add the final `NULL'.
    tokens[n_tokens] = NULL;

    // Finally, shrink our array back down to the correct size.
    tokens = realloc(tokens, (n_tokens + 1) * sizeof *tokens);
    assert(tokens != NULL);

    return tokens;
}

//
// Free an array of strings as returned by `tokenize'.
//
static void free_tokens(char **tokens)
{
    int i;
    for (i = 0; tokens[i] != NULL; i++)
    {
        free(tokens[i]);
    }
    free(tokens);
}
