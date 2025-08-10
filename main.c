#include "ecc.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "引数の個数が正しくありません\n");
        return 1;
    }

    filename = argv[1];
    struct Token *token = tokenize_file(filename);
    token = preprocess(token);
    program(token);

    codegen();

    return 0;
}