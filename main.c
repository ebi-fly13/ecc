#include "ecc.h"

static bool option_cc1;
static char *option_o;

static void help_command(int status) {
    fprintf(stderr, "ecc [ -o path] <file>\n");
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

        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            error("unknown argument: %s", argv[i]);
        }

        filename = argv[i];
    }
    if (filename == NULL) {
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

static void run_cc1(int argc, char **argv) {
    char **args = calloc(argc + 2, sizeof(char *));
    memcpy(args, argv, sizeof(char *) * argc);
    args[argc] = "-cc1";
    run_subprocess(args);
}

static void cc1() {
    struct Token *token = tokenize_file(filename);
    token = preprocess(token);
    program(token);

    FILE *out = open_file(option_o);

    codegen(out);
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    if (option_cc1) {
        cc1();
        return 0;
    }

    run_cc1(argc, argv);

    return 0;
}