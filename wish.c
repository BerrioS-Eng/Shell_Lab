#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 64
#define MAX_TOKEN 256

char error_message[30] = "An error has occurred\n";
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
    if(path_count == 0) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    char *fullpath = resolve_path(args[0]);
    if (!fullpath) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
 
    if (outfile != NULL) {
        int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            free(fullpath);
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    } 
    execv(fullpath, args);
    write(STDERR_FILENO, error_message, strlen(error_message));
    free(fullpath);
    exit(1);
}

int main(int argc, char *argv[]){
    // Ruta para integrar comandos Unix
    search_path[0] = strdup("/bin");
    path_count = 1;
    char cwd[512]; // Util para tomar ruta actual

    if (argc > 2) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    // Puntero para argumentos (file en caso de batch mode)
    FILE *input = stdin;
    bool interactive = (argc == 1); // Modo ejecución

    if (!interactive) {
        input = fopen(argv[1], "r");
        if (!input){
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    }

    char *line = NULL;
    size_t len = 0;

    while (true) {
        // Validación de modo interactivo
        if (interactive){
            if (getcwd(cwd, sizeof(cwd)) != NULL){
                printf("wish> ");
            } else {
                printf("wish> ");
            }
            fflush(stdout);
        }

        // Lectura de lineas, aplica para ambos modos
        if (getline(&line, &len, input) == -1){
            break;
        }; //EOF
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '\0') continue;

        //Detectar errores de sintaxis relacionados con '&' o '&&'
        if(strstr(line, "&&") != NULL || line[0] == '&' || line[strlen(line)-1] == '&') {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }

        char *args[MAX_ARGS];
        int nargs = tokenize(line, args); // Tokenizar los argumentos

        char *temp = line;
        char *cmd;

        char *commands[64];
        int cmd_count = 0;

        while ((cmd = strsep(&temp, "&")) != NULL) {
            //limpia los espacios al inicio
            while (*cmd == ' ' || *cmd == '\t') cmd++;
            //valida comando vacío
            if (*cmd == '\0') {
                write(STDERR_FILENO, error_message, strlen(error_message));
                cmd_count=0;
                break;
            }
            commands[cmd_count++] = cmd;
        }

        if (cmd_count == 0) continue;

        pid_t pids[64];
        int pid_count =0;

        for(int i=0; i<cmd_count; i++) {
            char *args[MAX_ARGS];
            char *outfile = NULL;

            //Valida que no haya más de una redirección '>'
            int count_redir = 0;
            for (int k = 0; commands[i][k]; k++) {
                if (commands[i][k] == '>') count_redir++;
            }

            if(count_redir > 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }

            //Redirección
            char *redir = strchr(commands[i], '>');
            if (redir != NULL) {
                *redir = '\0';
                redir++;

                while (*redir == ' ' || *redir == '\t') redir++;

                if (*redir == '\0') {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    continue;
                }

                char *extra = strchr(redir, ' ');
                if (extra != NULL) {
                    *extra = '\0';
                    extra++;
                    if(strlen(extra)>0){
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        continue;
                    }
                }
                outfile = redir;
            }

            int nargs = tokenize(commands[i], args);
            if (nargs == 0) continue;

        // Builtins
        if (strcmp(args[0], "exit") == 0) {
            if(nargs != 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            } else {
                exit(0);
            }
            continue;
        }

        if (strcmp(args[0], "chd") == 0) {
            if (nargs != 2) {
                write(STDERR_FILENO, error_message, strlen(error_message));
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

        pid_t pid = fork();
        if (pid == 0) {
            execute_command(args, outfile);
        } else if (pid > 0) {
            pids[pid_count++] = pid;
        } else {
            // Error de redirección
            write(STDERR_FILENO, error_message, strlen(error_message));
        }

        free_args(args, nargs);
    }

    for (int i = 0; i < pid_count; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

    return 0;
}