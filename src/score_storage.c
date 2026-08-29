#define _POSIX_C_SOURCE 200809L

#include "../include/scores.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char score_path[PATH_MAX];
static char temp_path[PATH_MAX];
static char legacy_path[PATH_MAX];
static bool initialized;

/*
 * Stiene beregnes én gang per prosess. Miljøvariablene må derfor settes før
 * første kall til en score_*_path()-funksjon; dette er særlig viktig i tester.
 */

static bool join_path(char output[PATH_MAX], const char *directory,
                      const char *filename)
{
    size_t directory_length = strlen(directory);
    size_t filename_length = strlen(filename);

    if (directory_length + 1 + filename_length + 1 > PATH_MAX) return false;
    memcpy(output, directory, directory_length);
    output[directory_length] = '/';
    memcpy(output + directory_length + 1, filename, filename_length + 1);
    return true;
}

static bool make_directory(const char *path)
{
    char copy[PATH_MAX];
    char *cursor;

    if (strlen(path) >= sizeof(copy)) return false;
    strcpy(copy, path);
    for (cursor = copy + 1; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (mkdir(copy, 0700) != 0 && errno != EEXIST) return false;
            *cursor = '/';
        }
    }
    return mkdir(copy, 0700) == 0 || errno == EEXIST;
}

static void copy_existing_scores(void)
{
    FILE *source;
    FILE *destination;
    unsigned char buffer[4096];
    size_t length;

    source = fopen("data/toppliste.dat", "rb");
    if (source == NULL) return;
    destination = fopen(score_path, "wb");
    if (destination == NULL) {
        fclose(source);
        return;
    }
    while ((length = fread(buffer, 1, sizeof(buffer), source)) > 0)
        (void)fwrite(buffer, 1, length, destination);
    fclose(destination);
    fclose(source);
}

static void initialize_paths(void)
{
    const char *override = getenv("SNAKE_DATA_DIR");
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    char directory[PATH_MAX];

    if (initialized) return;
    if (override != NULL && override[0] != '\0')
        snprintf(directory, sizeof(directory), "%s", override);
    else if (xdg != NULL && xdg[0] != '\0')
        snprintf(directory, sizeof(directory), "%s/snake", xdg);
    else if (home != NULL && home[0] != '\0')
        snprintf(directory, sizeof(directory), "%s/.local/share/snake", home);
    else
        snprintf(directory, sizeof(directory), "data");

    if (!make_directory(directory))
        fprintf(stderr, "Kunne ikke opprette datamappen %s.\n", directory);
    if (!join_path(score_path, directory, "toppliste.dat") ||
        !join_path(temp_path, directory, "toppliste.dat.tmp") ||
        !join_path(legacy_path, directory, "toppliste.txt")) {
        fprintf(stderr, "Datastien er for lang. Bruker lokal datamappe.\n");
        strcpy(score_path, "data/toppliste.dat");
        strcpy(temp_path, "data/toppliste.dat.tmp");
        strcpy(legacy_path, "data/toppliste.txt");
    }
    initialized = true;

    if ((override == NULL || override[0] == '\0') &&
        strcmp(score_path, "data/toppliste.dat") != 0) {
        struct stat info;
        if (stat(score_path, &info) != 0 && errno == ENOENT)
            copy_existing_scores();
    }
}

const char *score_file_path(void)
{
    initialize_paths();
    return score_path;
}

const char *score_temp_file_path(void)
{
    initialize_paths();
    return temp_path;
}

const char *legacy_score_file_path(void)
{
    initialize_paths();
    return legacy_path;
}
