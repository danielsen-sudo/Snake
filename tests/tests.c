#define main snake_game_main
#include "../src/snake.c"
#undef main

#include <assert.h>

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

    file = fopen(SCORE_FILE, "r+b");
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
    assert(chdir(temporary) == 0);
    assert(sodium_init() >= 0);

    test_sorting_and_equal_scores();
    test_expiration();
    test_encryption_and_tamper_detection();

    (void)unlink(SCORE_FILE);
    (void)unlink(SCORE_FILE ".tmp");
    assert(rmdir("data") == 0);
    assert(chdir(original_directory) == 0);
    assert(rmdir(temporary) == 0);
    puts("Alle tester bestått.");
    return 0;
}
