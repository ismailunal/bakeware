#include "bakeware.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct bakeware bw;

static void process_arguments(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            argv[i] = 0;
            break;
        } else if (strcmp(argv[i], "--bw-gc") == 0) {
            bw.run_gc = true;
            argv[i] = 0;
            break;
        } else if (strcmp(argv[i], "--bw-install") == 0) {
            bw.install_only = true;
            argv[i] = 0;
            break;
        } else if (strcmp(argv[i], "--bw-system-install") == 0) {
            bw_warnx("Ignoring --bw-system-install");
            argv[i] = 0;
            break;
        } else if (strcmp(argv[i], "--bw-info") == 0) {
            bw.print_info = true;
            argv[i] = 0;
            break;
        }
    }
}

static void update_environment(int argc, char *argv[])
{
    bw_set_environment("BAKEWARE_EXECUTABLE", -1, bw.path);

    int arg_index = 0;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (*arg == 0)
            continue;

        arg_index++;
        bw_set_environment("BAKEWARE_ARG", arg_index, arg);
    }
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%d", arg_index);
    bw_set_environment("BAKEWARE_ARGC", -1, buffer);
}

static void print_trailer_info(const struct bakeware_trailer *trailer)
{
    printf("Trailer version: %d\n", trailer->trailer_version);
    printf("Compression: %d\n", trailer->compression);
    printf("Flags: 0x%04x\n", trailer->flags);
    printf("Contents offset: %d\n", (int) trailer->contents_offset);
    printf("Contents length: %d\n", (int) trailer->contents_length);
    printf("SHA256: %s\n", trailer->sha256_ascii);
}

// Initialize app state
static void init_bk(int argc, char *argv[])
{
    memset(&bw, 0, sizeof(bw));

    bw.argc = argc;
    bw.argv = argv;

    process_arguments(argc, argv);
    bw_find_executable_path(bw.path, sizeof(bw.path));
    bw_cache_directory(bw.cache_dir_base, sizeof(bw.cache_dir_base));
}

static void run_application()
{
    bw_debug("Running %s...", bw.app_path);
    update_environment(bw.argc, bw.argv);
    execl(bw.app_path, bw.app_path, NULL);
    bw_fatal("Failed to start application '%s'", bw.app_path);
}

int main(int argc, char *argv[])
{
    init_bk(argc, argv);

    bw_debug("starting '%s' (cachedir=%s)...", bw.path, bw.cache_dir_base);

    bw.fd = open(bw.path, O_RDONLY);
    if (bw.fd < 0)
        bw_fatal("Can't open '%s'", bw.path);

    if (bw_read_trailer(bw.fd, &bw.trailer) < 0)
        bw_fatalx("Error reading trailer!");

    if (bw.print_info) {
        print_trailer_info(&bw.trailer);
        exit(EXIT_SUCCESS);
    }

    if (bw.trailer.trailer_version != 1)
        bw_fatalx("Expecting trailer version 1");

    cache_init(&bw);

    if (cache_validate(&bw) < 0)
        bw_fatalx("Unrecoverable validation error");

    if (cache_read_app_data(&bw) < 0)
        bw_fatalx("Unrecoverable application data error");

    close(bw.fd);

    run_application();
}
