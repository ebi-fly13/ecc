#include "ecc.h"

static bool option_cc1;
static bool option_S;
static bool option_E;
static char *option_o;

static char *input_path;

char **include_paths;
int count = 0;

void add_include_path(char *path) {
    include_paths = realloc(include_paths, sizeof(char *) * (count + 1));
    include_paths[count] = strdup(path);
    count++;
}

static char *replace_extn(char *path, char *extn) {
    char *filename = basename(strdup(path));
    char *dot = strrchr(filename, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    return format("%s%s", filename, extn);
}

static char *create_tmpfile() {
    char *path = strdup("/tmp/ecc-XXXXXX");
    int fd = mkstemp(path);
    if (fd == -1) {
        error("mkstemp failed: %s", strerror(errno));
    }
    close(fd);

    return path;
}

static void help_command(int status) {
    fprintf(stderr, "ecc [ -o path] [ -I<dir>] <file>\n");
    exit(status);
}

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) {
            help_command(0);
        }

        if (!strcmp(argv[i], "-cc1")) {
            option_cc1 = true;
            continue;
        }

        if (!strcmp(argv[i], "-S")) {
            option_S = true;
            continue;
        }

        if (!strcmp(argv[i], "-E")) {
            option_E = true;
            continue;
        }

        if (!strcmp(argv[i], "-o")) {
            if (argv[i + 1] == NULL) {
                help_command(1);
            }
            option_o = argv[i + 1];
            i++;
            continue;
        }

        if (!strncmp(argv[i], "-o", 2)) {
            option_o = argv[i] + 2;
            continue;
        }

        if (!strncmp(argv[i], "-I", 2)) {
            add_include_path(argv[i] + 2);
            continue;
        }

        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            error("unknown argument: %s", argv[i]);
        }

        input_path = argv[i];
    }
    if (input_path == NULL) {
        error("no input files");
    }
}

static FILE *open_file(char *path) {
    if (path == NULL)
        return stdout;

    FILE *out = fopen(path, "w");
    if (!out)
        error("cannot open output file: %s: %s", path, strerror(errno));
    return out;
}

static void run_subprocess(char **args) {
    if (fork() == 0) {
        execvp(args[0], args);
        fprintf(stderr, "exec failed: %s: %s\n", args[0], strerror(errno));
        _exit(1);
    }

    int status;
    while (wait(&status) > 0)
        ;
    if (status != 0) {
        exit(1);
    }
}

static void run_cc1(int argc, char **argv, char *input, char *output) {
    char **args = calloc(argc + 10, sizeof(char *));
    memcpy(args, argv, sizeof(char *) * argc);
    args[argc++] = "-cc1";
    if (input != NULL) {
        args[argc++] = input;
    }
    if (output != NULL) {
        args[argc++] = "-o";
        args[argc++] = output;
    }
    run_subprocess(args);
}

static void print_token(struct Token *token) {
    FILE *out = option_o == NULL ? stdout : open_file(option_o);
    for (; token != NULL && token->kind != TK_EOF; token = token->next) {
        if (token->line_number > 1 && token->is_begin) {
            fprintf(out, "\n");
        }
        if (!token->is_begin && token->has_space) {
            fprintf(out, " ");
        }
        fprintf(out, "%.*s", token->len, token->loc);
    }
    fprintf(out, "\n");
}

static void cc1() {
    struct Token *token = tokenize_file(input_path);
    token = preprocess(token);
    convert_keywords(token);
    if (option_E) {
        print_token(token);
        return;
    }
    program(token);

    FILE *out = open_file(option_o);

    codegen(out);
}

static void assemble(char *input, char *output) {
    char *cmd[] = {"as", "-c", input, "-o", output, NULL};
    run_subprocess(cmd);
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    if (option_cc1) {
        cc1();
        return 0;
    }

    char *output;
    if (option_o != NULL) {
        output = option_o;
    } else if (option_S) {
        output = replace_extn(input_path, ".s");
    } else {
        output = replace_extn(input_path, ".o");
    }

    if (option_S) {
        run_cc1(argc, argv, input_path, output);
        return 0;
    }

    if (option_E) {
        run_cc1(argc, argv, input_path, NULL);
        return 0;
    }

    char *tmp_file = create_tmpfile();
    run_cc1(argc, argv, input_path, tmp_file);
    assemble(tmp_file, output);
    unlink(tmp_file);

    return 0;
}