#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 64
#define MAX_TOKEN 256

// Search path configurable
char *search_path[64] = {"/bin", NULL};
int path_count = 1;

// Function declarations
int tokenize(char *line, char *args[]) {
    int argc = 0;
    int pos = 0;
    char buf[MAX_TOKEN];
    bool in_quotes = false;

    for (int i = 0; line[i] && argc < MAX_ARGS - 1; i++) {
        char c = line[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if ((c == ' ' || c == '\t') && !in_quotes) {
            if (pos > 0) {
                buf[pos] = '\0';
                args[argc++] = strdup(buf);
                pos = 0;
            }
        } else {
            if (pos < MAX_TOKEN - 1) {
                buf[pos++] = c;
            }
        }
    }

    // último token pendiente
    if (pos > 0 && argc < MAX_ARGS - 1) {
        buf[pos] = '\0';
        args[argc++] = strdup(buf);
    }

    args[argc] = NULL;
    return argc;
}

void free_args(char *args[], int argc) {
    for (int i = 0; i < argc; i++) {
        free(args[i]);
    }
}

char *parse_redirect(char *args[], int nargs, int *redir_pos) {
    *redir_pos = -1;

    for (int i = 0; i < nargs; i++) {
        if (strcmp(args[i], ">") == 0) {
            // Validar: no puede haber otro >, debe haber exactamente 1 archivo después
            if (i == 0 || i + 1 != nargs - 1) {
                return NULL; // error: sin comando antes, o múltiples archivos
            }
            // Verificar que no haya otro > después
            *redir_pos = i;
            return args[i + 1];
        }
    }
    return NULL; // no hay redirección
}

char *resolve_path(char *cmd) {
    if (strchr(cmd, '/')) return strdup(cmd);

    char fullpath[512];
    for (int i = 0; i < path_count; i++) {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", search_path[i], cmd);
        if (access(fullpath, X_OK) == 0) {
            return strdup(fullpath);
        }
    }
    return NULL;
}

void execute_command(char *args[], char *outfile) {
    char *fullpath = resolve_path(args[0]);
    if (!fullpath) {
        fprintf(stderr, "wish: command not found: %s\n", args[0]);
        return;
    }
 
    pid_t pid = fork();
 
    if (pid == 0) {
        if (outfile) {
            int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        execv(fullpath, args);
        perror("execv");
        exit(1);
    } else if (pid > 0) {
        free(fullpath);
        waitpid(pid, NULL, 0);
    } else {
        free(fullpath);
        perror("fork");
    }
}

int main(int argc, char *argv[]){
    // Ruta para integrar comandos Unix
    search_path[0] = strdup("/bin");
    path_count = 1;
    char cwd[512]; // Util para tomar ruta actual

    if (argc > 2) {
        fprintf(stderr, "usage: wish [batch_file]\n");
        exit(1);
    }

    // Puntero para argumentos (file en caso de batch mode)
    FILE *input = stdin;
    bool interactive = (argc == 1); // Modo ejecución

    if (!interactive) {
        input = fopen(argv[1], "r");
        if (!input){
            fprintf(stderr, "Error: cannot open %s\n", argv[1]);
            exit(1);
        }
    }

    char *line = NULL;
    size_t len = 0;

    while (true) {
        // Validación de modo interactivo
        if (interactive){
            if (getcwd(cwd, sizeof(cwd)) != NULL){
                printf("wish> %s $ ", cwd);
            } else {
                printf("wish> ");
            }
            fflush(stdout);
        }

        // Lectura de lineas, aplica para ambos modos
        if (getline(&line, &len, input) == -1) exit(0); //EOF
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '\0') continue;

        char *args[MAX_ARGS];
        int nargs = tokenize(line, args); // Tokenizar los argumentos

        // Builtins
        if (strcmp(args[0], "exit") == 0) {
            free_args(args, nargs);
            exit(0);
        }

        if (strcmp(args[0], "chd") == 0) {
            if (nargs != 2) {
                fprintf(stderr, "cd: expected 1 argument\n");
            } else if (chdir(args[1]) != 0) {
                perror("cd");
            }
            free_args(args, nargs);
            continue;
        }

        if (strcmp(args[0], "route") == 0) {
            for (int i = 0; i < path_count; i++) {
                free(search_path[i]);
            }
            path_count = 0;
            for (int i = 1; i < nargs; i++) {
                search_path[path_count++] = strdup(args[i]);
            }
            search_path[path_count] = NULL;
            free_args(args, nargs);
            continue;
        }

        int redir_pos = -1;
        char *outfile = parse_redirect(args, nargs, &redir_pos);

        if (redir_pos == -1 && outfile == NULL) {
            // Sin redirección
            execute_command(args, NULL);
        } else if (redir_pos > 0) {
            // Redirección válida
            char *file = strdup(outfile);
            free(args[redir_pos]);      // liberar ">"
            free(args[redir_pos + 1]);  // liberar nombre de archivo
            args[redir_pos] = NULL;
            int original_nargs = nargs;
            nargs = redir_pos;
            execute_command(args, file);
            free(file);
        } else {
            // Error de redirección
            fprintf(stderr, "wish: invalid redirection\n");
        }

        free_args(args, redir_pos > 0 ? redir_pos : nargs);
    }

    free(line);
    if (!interactive) fclose(input);

    return 0;
}