#define _POSIX_C_SOURCE 200809L

#include "../include/scores.h"
#include "../include/sodium_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Testene bruker en midlertidig arbeidsmappe og berører aldri spillerens data. */

static void test_sorting_and_equal_scores(void)
{
    Score scores[MAX_SCORES] = {0};
    int count = 0;

    assert(save_score(scores, &count, "Først", 10));
    assert(save_score(scores, &count, "Lav", 5));
    assert(save_score(scores, &count, "Andre", 10));
    assert(count == 3);
    assert(strcmp(scores[0].name, "Først") == 0);
    assert(strcmp(scores[1].name, "Andre") == 0);
    assert(strcmp(scores[2].name, "Lav") == 0);
}

static void test_expiration(void)
{
    int64_t now = (int64_t)time(NULL);
    Score scores[MAX_SCORES] = {
        {"Aktiv", 9, now - SCORE_LIFETIME_SECONDS + 1},
        {"Utløpt", 8, now - SCORE_LIFETIME_SECONDS}
    };

    assert(remove_expired_scores(scores, 2) == 1);
    assert(strcmp(scores[0].name, "Aktiv") == 0);
}

static void test_score_validation(void)
{
    Score scores[MAX_SCORES] = {0};
    int count = 0;
    char too_long[MAX_NAME_BYTES + 2];

    memset(too_long, 'a', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    assert(!save_score(scores, &count, "", 10));
    assert(!save_score(scores, &count, "linje\nskift", 10));
    assert(!save_score(scores, &count, too_long, 10));
    assert(!save_score(scores, &count, "Navn", 0));
    assert(count == 0);
}

static void test_score_outside_top_ten_is_ignored(void)
{
    Score scores[MAX_SCORES] = {0};
    int count = MAX_SCORES;
    int index;

    for (index = 0; index < MAX_SCORES; ++index) {
        snprintf(scores[index].name, sizeof(scores[index].name),
                 "Spiller %d", index + 1);
        scores[index].score = MAX_SCORES - index + 1;
        scores[index].created_at = (int64_t)time(NULL);
    }
    assert(score_rank(scores, count, 1) == MAX_SCORES + 1);
    assert(save_score(scores, &count, "Utenfor", 1));
    assert(count == MAX_SCORES);
    assert(strcmp(scores[MAX_SCORES - 1].name, "Spiller 10") == 0);
}

static void test_legacy_file_is_preserved(void)
{
    Score loaded[MAX_SCORES] = {0};
    FILE *file = fopen(legacy_score_file_path(), "wb");

    assert(file != NULL);
    assert(fputs("7\tHistorisk\n", file) >= 0);
    assert(fclose(file) == 0);
    assert(load_scores(loaded) == 1);
    assert(strcmp(loaded[0].name, "Historisk") == 0);
    assert(access(legacy_score_file_path(), F_OK) == 0);
    assert(unlink(score_file_path()) == 0);
    assert(unlink(legacy_score_file_path()) == 0);
}

static void test_encryption_and_tamper_detection(void)
{
    Score original[MAX_SCORES] = {{"Åse", 42, (int64_t)time(NULL)}};
    Score loaded[MAX_SCORES] = {0};
    FILE *file;
    int byte;

    assert(write_scores(original, 1));
    assert(load_scores(loaded) == 1);
    assert(strcmp(loaded[0].name, "Åse") == 0);
    assert(loaded[0].score == 42);

    file = fopen(score_file_path(), "r+b");
    assert(file != NULL);
    assert(fseek(file, 40, SEEK_SET) == 0);
    byte = fgetc(file);
    assert(byte != EOF);
    assert(fseek(file, 40, SEEK_SET) == 0);
    assert(fputc(byte ^ 1, file) != EOF);
    assert(fclose(file) == 0);
    assert(load_scores(loaded) == 0);
}

int main(void)
{
    char original_directory[4096];
    char temporary[] = "/tmp/snake-tests-XXXXXX";

    assert(getcwd(original_directory, sizeof(original_directory)) != NULL);
    assert(mkdtemp(temporary) != NULL);
    assert(setenv("SNAKE_DATA_DIR", temporary, 1) == 0);
    assert(chdir(temporary) == 0);
    assert(sodium_init() >= 0);

    test_legacy_file_is_preserved();
    test_sorting_and_equal_scores();
    test_expiration();
    test_score_validation();
    test_score_outside_top_ten_is_ignored();
    test_encryption_and_tamper_detection();

    (void)unlink(score_file_path());
    (void)unlink(score_temp_file_path());
    assert(chdir(original_directory) == 0);
    assert(rmdir(temporary) == 0);
    puts("Alle tester bestått.");
    return 0;
}
