/*
 * main.c
 */

#include "utils.h"
#include "main.h"
#include "controller.h"

/**
 * Display command-line interface usage
 */
static void print_help(void)
{
    printf(
        "./dudley [options] -f [file]\n"
        "-f FILE    Path to dudley JSON formatted fuzzing model\n"
    );
}

/**
 * Parse arguments and execute a dudley file
 */
int main(int argc, char **argv)
{
    int opt = 0;
    char *filepath = NULL;
    dud_t *ctx = NULL;
    int ret = FAILURE;

    if (argc < 2) {
        print_help();
        return SUCCESS;
    }

    while ((opt = getopt(argc, argv, "hf:")) != -1) {
        switch (opt) {
        case 'h':
            print_help();
            return SUCCESS;

        case 'f':
            filepath = optarg;
            break;

        default:
            return FAILURE;
        }
    }

    if (!filepath) {
        duderr("you must supply a filepath");
        return ret;
    }

    ctx = dudley_load_file(filepath);
    if (!ctx) {
        return ret;
    }

    ret = dudley_iterate_actions(ctx);
    dudley_destroy(ctx);

    return ret;
}
