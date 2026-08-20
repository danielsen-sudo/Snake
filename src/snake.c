#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../include/sodium_compat.h"

#define GAME_VERSION "1.3"
#define SCORE_FILE "data/toppliste.dat"
#define LEGACY_SCORE_FILE "toppliste.txt"
#define MAX_SCORES 10
#define MAX_NAME_CHARS 50
#define MAX_NAME_BYTES (MAX_NAME_CHARS * 4)
#define START_DELAY_MS 200
#define MIN_DELAY_MS 50
#define SPEED_STEP_MS 10
#define SCORE_LIFETIME_SECONDS (15LL * 24LL * 60LL * 60LL)

#define ANSI_CLEAR "\033[2J\033[H"
#define ANSI_HOME "\033[H"
#define ANSI_WHITE "\033[37m"
#define ANSI_RED "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RESET "\033[0m"
#define ANSI_HIDE_CURSOR "\033[?25l"
#define ANSI_SHOW_CURSOR "\033[?25h"
#define ANSI_ALT_ON "\033[?1049h"
#define ANSI_ALT_OFF "\033[?1049l"

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    char name[MAX_NAME_BYTES + 1];
    int score;
    int64_t created_at;
} Score;

typedef struct {
    int width;
    int height;
    const char *name;
} BoardPreset;

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef enum {
    END_COLLISION,
    END_ESCAPE,
    END_BOARD_FULL
} EndReason;

static const BoardPreset PRESETS[] = {
    {40, 20, "Standard"},
    {30, 15, "Kompakt"},
    {60, 25, "Stor"},
    {80, 35, "Ekstra stor"}
};

static struct termios saved_termios;
static volatile sig_atomic_t terminal_active = 0;

static void restore_terminal(void)
{
    if (terminal_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
        if (terminal_active == 1) {
            fputs(ANSI_RESET ANSI_SHOW_CURSOR ANSI_ALT_OFF, stdout);
            fflush(stdout);
        }
        terminal_active = 0;
    }
}

static void handle_signal(int signal_number)
{
    restore_terminal();
    _exit(128 + signal_number);
}

static bool enable_game_terminal(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &saved_termios) == -1) {
        return false;
    }
    raw = saved_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return false;
    }

    terminal_active = 1;
    fputs(ANSI_ALT_ON ANSI_HIDE_CURSOR ANSI_CLEAR, stdout);
    fflush(stdout);
    return true;
}

static void install_signal_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
}

static void trim_line(char *text)
{
    size_t length = strlen(text);

    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[--length] = '\0';
    }
}

static int utf8_character_count(const char *text)
{
    const unsigned char *bytes = (const unsigned char *)text;
    int count = 0;

    while (*bytes != '\0') {
        int continuation;
        int needed;
        if (*bytes < 0x80) {
            needed = 0;
        } else if (*bytes >= 0xc2 && *bytes <= 0xdf) {
            needed = 1;
        } else if (*bytes >= 0xe0 && *bytes <= 0xef) {
            needed = 2;
        } else if (*bytes >= 0xf0 && *bytes <= 0xf4) {
            needed = 3;
        } else {
            return -1;
        }
        for (continuation = 1; continuation <= needed; ++continuation) {
            if ((bytes[continuation] & 0xc0) != 0x80) {
                return -1;
            }
        }
        if ((needed == 2 && bytes[0] == 0xe0 && bytes[1] < 0xa0) ||
            (needed == 2 && bytes[0] == 0xed && bytes[1] >= 0xa0) ||
            (needed == 3 && bytes[0] == 0xf0 && bytes[1] < 0x90) ||
            (needed == 3 && bytes[0] == 0xf4 && bytes[1] >= 0x90)) {
            return -1;
        }
        bytes += needed + 1;
        ++count;
    }
    return count;
}

static void discard_line_rest(void)
{
    int character;

    while ((character = getchar()) != '\n' && character != EOF) {
    }
}

static bool read_line(char *buffer, size_t size)
{
    if (fgets(buffer, (int)size, stdin) == NULL) {
        return false;
    }
    if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
        discard_line_rest();
    }
    trim_line(buffer);
    return true;
}

static void wait_for_enter(void)
{
    char line[8];

    fputs("\nTrykk Enter for å fortsette...", stdout);
    fflush(stdout);
    (void)read_line(line, sizeof(line));
}

static const uint8_t SCORE_MAGIC[8] = {'S', 'N', 'K', '1', '2', 'T', 'E', 'A'};

static bool ensure_data_directory(void)
{
    if (mkdir("data", 0700) == 0 || errno == EEXIST) {
        return true;
    }
    fprintf(stderr, "Kunne ikke opprette datamappen: %s\n", strerror(errno));
    return false;
}

static uint32_t read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static void write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static uint64_t read_u64_be(const uint8_t *bytes)
{
    return ((uint64_t)read_u32_be(bytes) << 32) | read_u32_be(bytes + 4);
}

static void derive_key(const char *purpose, uint32_t key[4])
{
    const char *secret = "CodeX";
    int part;

    for (part = 0; part < 4; ++part) {
        uint32_t hash = 2166136261u ^ ((uint32_t)part * 0x9e3779b9u);
        const unsigned char *cursor;
        for (cursor = (const unsigned char *)secret; *cursor != '\0'; ++cursor) {
            hash = (hash ^ *cursor) * 16777619u;
        }
        for (cursor = (const unsigned char *)purpose; *cursor != '\0'; ++cursor) {
            hash = (hash ^ *cursor) * 16777619u;
        }
        key[part] = hash;
    }
}

static void tea_encrypt_block(uint8_t block[8], const uint32_t key[4])
{
    uint32_t left = read_u32_be(block);
    uint32_t right = read_u32_be(block + 4);
    uint32_t sum = 0;
    int round;

    for (round = 0; round < 32; ++round) {
        sum += 0x9e3779b9u;
        left += ((right << 4) + key[0]) ^ (right + sum) ^
                ((right >> 5) + key[1]);
        right += ((left << 4) + key[2]) ^ (left + sum) ^
                 ((left >> 5) + key[3]);
    }
    write_u32_be(block, left);
    write_u32_be(block + 4, right);
}

static void tea_decrypt_block(uint8_t block[8], const uint32_t key[4])
{
    uint32_t left = read_u32_be(block);
    uint32_t right = read_u32_be(block + 4);
    uint32_t sum = 0xc6ef3720u;
    int round;

    for (round = 0; round < 32; ++round) {
        right -= ((left << 4) + key[2]) ^ (left + sum) ^
                 ((left >> 5) + key[3]);
        left -= ((right << 4) + key[0]) ^ (right + sum) ^
                ((right >> 5) + key[1]);
        sum -= 0x9e3779b9u;
    }
    write_u32_be(block, left);
    write_u32_be(block + 4, right);
}

static void score_mac(const uint8_t *data, size_t length, uint8_t tag[8])
{
    uint32_t key[4];
    size_t offset;

    derive_key("integrity", key);
    memset(tag, 0, 8);
    for (offset = 0; offset < length; offset += 8) {
        int byte;
        for (byte = 0; byte < 8; ++byte) {
            tag[byte] ^= data[offset + (size_t)byte];
        }
        tea_encrypt_block(tag, key);
    }
}

static bool write_scores(const Score scores[MAX_SCORES], int count)
{
    uint8_t plaintext[1024];
    uint8_t file_data[8 + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES +
                      sizeof(plaintext) + crypto_aead_xchacha20poly1305_ietf_ABYTES];
    uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    static const uint8_t magic[8] = {'S', 'N', 'K', '1', '3', 'X', 'C', 'P'};
    size_t plain_length = 0;
    unsigned long long cipher_length = 0;
    size_t file_length;
    int index;
    FILE *file;

    if (!ensure_data_directory()) {
        return false;
    }
    for (index = 0; index < count; ++index) {
        int written = snprintf((char *)plaintext + plain_length,
                               sizeof(plaintext) - plain_length,
                               "%lld\t%d\t%s\n",
                               (long long)scores[index].created_at,
                               scores[index].score, scores[index].name);
        if (written < 0 || (size_t)written >= sizeof(plaintext) - plain_length) {
            fputs("Topplisten er for stor til å lagres.\n", stderr);
            return false;
        }
        plain_length += (size_t)written;
    }

    memcpy(file_data, magic, sizeof(magic));
    randombytes_buf(file_data + 8, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    if (crypto_generichash(key, sizeof(key), (const unsigned char *)"CodeX", 5,
                           (const unsigned char *)"Snake scores 1.3", 16) != 0 ||
        crypto_aead_xchacha20poly1305_ietf_encrypt(
            file_data + 8 + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES,
            &cipher_length, plaintext, plain_length, magic, sizeof(magic), NULL,
            file_data + 8, key) != 0) {
        fputs("Kunne ikke kryptere topplisten.\n", stderr);
        return false;
    }
    file_length = 8 + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES +
                  (size_t)cipher_length;

    file = fopen(SCORE_FILE ".tmp", "wb");
    if (file == NULL) {
        fprintf(stderr, "Kunne ikke lagre %s: %s\n", SCORE_FILE,
                strerror(errno));
        return false;
    }
    if (fwrite(file_data, 1, file_length, file) != file_length) {
        fprintf(stderr, "Kunne ikke lagre %s: %s\n", SCORE_FILE,
                strerror(errno));
        fclose(file);
        return false;
    }
    if (fflush(file) == EOF || fsync(fileno(file)) == -1 || fclose(file) == EOF) {
        fprintf(stderr, "Kunne ikke fullføre lagring av %s.\n", SCORE_FILE);
        return false;
    }
    if (rename(SCORE_FILE ".tmp", SCORE_FILE) == -1) {
        fprintf(stderr, "Kunne ikke erstatte %s: %s\n", SCORE_FILE,
                strerror(errno));
        return false;
    }
    return true;
}

static int parse_score_text(char *text, Score scores[MAX_SCORES],
                            int64_t legacy_time, bool legacy)
{
    char *line;
    char *save_line = NULL;
    int count = 0;

    for (line = strtok_r(text, "\n", &save_line);
         line != NULL && count < MAX_SCORES;
         line = strtok_r(NULL, "\n", &save_line)) {
        char *first = strchr(line, '\t');
        char *second;
        char *end;
        long score_value;
        long long timestamp = legacy_time;

        if (first == NULL) {
            continue;
        }
        *first++ = '\0';
        if (legacy) {
            second = first;
            errno = 0;
            score_value = strtol(line, &end, 10);
        } else {
            errno = 0;
            timestamp = strtoll(line, &end, 10);
            if (errno != 0 || *end != '\0' || timestamp <= 0) {
                continue;
            }
            second = strchr(first, '\t');
            if (second == NULL) {
                continue;
            }
            *second++ = '\0';
            errno = 0;
            score_value = strtol(first, &end, 10);
        }
        if (errno != 0 || *end != '\0' || score_value < 1 ||
            score_value > 1000000 || *second == '\0' ||
            strlen(second) > MAX_NAME_BYTES || utf8_character_count(second) < 1 ||
            utf8_character_count(second) > MAX_NAME_CHARS ||
            strchr(second, '\t') != NULL) {
            continue;
        }
        scores[count].created_at = (int64_t)timestamp;
        scores[count].score = (int)score_value;
        strcpy(scores[count].name, second);
        ++count;
    }
    return count;
}

static int remove_expired_scores(Score scores[MAX_SCORES], int count)
{
    int64_t now = (int64_t)time(NULL);
    int source;
    int destination = 0;

    for (source = 0; source < count; ++source) {
        int64_t age = now - scores[source].created_at;
        if (age >= 0 && age < SCORE_LIFETIME_SECONDS) {
            scores[destination++] = scores[source];
        }
    }
    return destination;
}

static int load_legacy_scores(Score scores[MAX_SCORES])
{
    struct stat information;
    FILE *file = fopen(LEGACY_SCORE_FILE, "rb");
    uint8_t *data;
    long size;
    int count = 0;

    if (file == NULL) {
        if (errno != ENOENT) {
            fprintf(stderr, "Advarsel: Kunne ikke åpne %s: %s\n", LEGACY_SCORE_FILE,
                    strerror(errno));
        }
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    data = malloc((size_t)size + 1);
    if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return 0;
    }
    fclose(file);
    data[size] = '\0';

    if (size >= 40 && memcmp(data, SCORE_MAGIC, 8) == 0 &&
        ((size - 32) % 8) == 0) {
        size_t padded_length = (size_t)size - 32;
        uint64_t plain_length = read_u64_be(data + 16);
        uint8_t expected_tag[8];
        uint8_t difference = 0;
        uint8_t previous[8];
        uint32_t key[4];

        score_mac(data, (size_t)size - 8, expected_tag);
        for (int byte = 0; byte < 8; ++byte) {
            difference |= expected_tag[byte] ^ data[size - 8 + byte];
        }
        if (difference != 0 || plain_length >= padded_length) {
            fprintf(stderr, "Advarsel: %s er endret eller ugyldig og ble avvist.\n",
                    LEGACY_SCORE_FILE);
            free(data);
            return 0;
        }

        derive_key("encryption", key);
        memcpy(previous, data + 8, 8);
        for (size_t offset = 0; offset < padded_length; offset += 8) {
            uint8_t current[8];
            int byte;
            memcpy(current, data + 24 + offset, 8);
            tea_decrypt_block(data + 24 + offset, key);
            for (byte = 0; byte < 8; ++byte) {
                data[24 + offset + (size_t)byte] ^= previous[byte];
            }
            memcpy(previous, current, 8);
        }
        data[24 + plain_length] = '\0';
        count = parse_score_text((char *)data + 24, scores, 0, false);
        {
            int active_count = remove_expired_scores(scores, count);
            if (active_count != count) {
                count = active_count;
                (void)write_scores(scores, count);
            }
        }
    } else {
        int64_t legacy_time = (int64_t)time(NULL);
        if (stat(LEGACY_SCORE_FILE, &information) == 0) {
            legacy_time = (int64_t)information.st_mtime;
        }
        count = parse_score_text((char *)data, scores, legacy_time, true);
        count = remove_expired_scores(scores, count);
        (void)write_scores(scores, count);
    }
    free(data);
    return count;
}

static int load_scores(Score scores[MAX_SCORES])
{
    static const uint8_t magic[8] = {'S', 'N', 'K', '1', '3', 'X', 'C', 'P'};
    FILE *file;
    uint8_t *data;
    uint8_t *plaintext;
    uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    unsigned long long plain_length = 0;
    long size;
    int count;

    if (!ensure_data_directory()) {
        return 0;
    }
    file = fopen(SCORE_FILE, "rb");
    if (file == NULL) {
        if (errno != ENOENT) {
            fprintf(stderr, "Advarsel: Kunne ikke åpne %s: %s\n", SCORE_FILE,
                    strerror(errno));
            return 0;
        }
        count = load_legacy_scores(scores);
        if (write_scores(scores, count)) {
            (void)unlink(LEGACY_SCORE_FILE);
        }
        return count;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 48 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "Advarsel: %s er ugyldig og ble avvist.\n", SCORE_FILE);
        return 0;
    }
    data = malloc((size_t)size);
    plaintext = malloc((size_t)size);
    if (data == NULL || plaintext == NULL ||
        fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        free(plaintext);
        fclose(file);
        return 0;
    }
    fclose(file);

    if (memcmp(data, magic, sizeof(magic)) != 0 ||
        crypto_generichash(key, sizeof(key), (const unsigned char *)"CodeX", 5,
                           (const unsigned char *)"Snake scores 1.3", 16) != 0 ||
        crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext, &plain_length, NULL,
            data + 8 + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES,
            (unsigned long long)size - 8 -
                crypto_aead_xchacha20poly1305_ietf_NPUBBYTES,
            magic, sizeof(magic), data + 8, key) != 0 ||
        plain_length >= (unsigned long long)size) {
        fprintf(stderr, "Advarsel: %s er endret eller ugyldig og ble avvist.\n",
                SCORE_FILE);
        free(data);
        free(plaintext);
        return 0;
    }
    plaintext[plain_length] = '\0';
    count = parse_score_text((char *)plaintext, scores, 0, false);
    {
        int active_count = remove_expired_scores(scores, count);
        if (active_count != count) {
            count = active_count;
            (void)write_scores(scores, count);
        }
    }
    free(data);
    free(plaintext);
    return count;
}

static void refresh_expired_scores(Score scores[MAX_SCORES], int *count)
{
    int active_count = remove_expired_scores(scores, *count);

    if (active_count != *count) {
        *count = active_count;
        (void)write_scores(scores, *count);
    }
}

static void show_scores(const Score scores[MAX_SCORES], int count)
{
    int index;

    puts("\n========== TOPPLISTE ==========");
    if (count == 0) {
        puts("Ingen resultater ennå.");
    } else {
        for (index = 0; index < count; ++index) {
            printf("%2d. %-50s %5d\n", index + 1, scores[index].name,
                   scores[index].score);
        }
    }
    puts("===============================");
}

static bool save_score(Score scores[MAX_SCORES], int *count,
                       const char *name, int value)
{
    int position = 0;
    int index;

    *count = remove_expired_scores(scores, *count);

    /* Bruk >, ikke >=: eldre resultat blir først ved lik poengsum. */
    while (position < *count && scores[position].score >= value) {
        ++position;
    }
    if (position >= MAX_SCORES) {
        return true;
    }

    if (*count < MAX_SCORES) {
        ++*count;
    }
    for (index = *count - 1; index > position; --index) {
        scores[index] = scores[index - 1];
    }
    scores[position].score = value;
    scores[position].created_at = (int64_t)time(NULL);
    strcpy(scores[position].name, name);
    return write_scores(scores, *count);
}

static bool get_terminal_size(int *columns, int *rows)
{
    struct winsize size;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1 ||
        size.ws_col == 0 || size.ws_row == 0) {
        return false;
    }
    *columns = size.ws_col;
    *rows = size.ws_row;
    return true;
}

static int terminal_columns(void)
{
    int columns;
    int rows;

    return get_terminal_size(&columns, &rows) ? columns : 80;
}

static void print_centered_yellow(const char *text)
{
    int padding = (terminal_columns() - (int)strlen(text)) / 2;

    if (padding < 0) {
        padding = 0;
    }
    printf("%*s%s%s%s\n", padding, "", ANSI_YELLOW, text, ANSI_RESET);
}

static void print_menu_box_line(const char *text)
{
    enum { INNER_WIDTH = 32 };
    int text_length = (int)strlen(text);
    int left_inside = (INNER_WIDTH - text_length) / 2;
    int right_inside = INNER_WIDTH - text_length - left_inside;
    int outside = (terminal_columns() - (INNER_WIDTH + 2)) / 2;

    if (outside < 0) {
        outside = 0;
    }
    printf("%*s%s|%*s%s%*s|%s\n", outside, "", ANSI_YELLOW,
           left_inside, "", text, right_inside, "", ANSI_RESET);
}

static void show_main_menu_box(void)
{
    char title[32];

    snprintf(title, sizeof(title), "SNAKE %s", GAME_VERSION);
    print_centered_yellow("+--------------------------------+");
    print_menu_box_line("");
    print_menu_box_line(title);
    print_menu_box_line("");
    print_menu_box_line("1. Nytt spill");
    print_menu_box_line("2. Avslutt");
    print_menu_box_line("");
    print_menu_box_line("Trykk 1 eller 2");
    print_menu_box_line("");
    print_centered_yellow("+--------------------------------+");
}

static int read_single_choice(int minimum, int maximum)
{
    struct termios raw;
    unsigned char character;

    if (tcgetattr(STDIN_FILENO, &saved_termios) == -1) {
        return maximum;
    }
    raw = saved_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return maximum;
    }
    terminal_active = 2;

    for (;;) {
        ssize_t result = read(STDIN_FILENO, &character, 1);
        if (result == 1 && character >= (unsigned char)('0' + minimum) &&
            character <= (unsigned char)('0' + maximum)) {
            int choice = character - '0';
            restore_terminal();
            return choice;
        }
        if (result == 0 || (result < 0 && errno != EINTR)) {
            restore_terminal();
            return maximum;
        }
    }
}

static bool preset_fits(const BoardPreset *preset, int columns, int rows)
{
    /* Én statuslinje under brettet. */
    return preset->width <= columns && preset->height + 1 <= rows;
}

static int select_board(void)
{
    int columns = 0;
    int rows = 0;
    int choice;
    size_t index;
    bool known_size = get_terminal_size(&columns, &rows);

    puts("\nVelg brettstørrelse:");
    for (index = 0; index < sizeof(PRESETS) / sizeof(PRESETS[0]); ++index) {
        bool fits = !known_size || preset_fits(&PRESETS[index], columns, rows);
        printf("%zu. %-12s %2d x %-2d%s\n", index + 1, PRESETS[index].name,
               PRESETS[index].width, PRESETS[index].height,
               fits ? "" : " (får ikke plass)");
    }
    if (known_size) {
        printf("Terminalen er %d x %d tegn.\n", columns, rows);
    }

    for (;;) {
        fputs("Trykk 1, 2, 3 eller 4: ", stdout);
        fflush(stdout);
        choice = read_single_choice(1, 4) - 1;
        putchar('\n');
        if (!known_size || preset_fits(&PRESETS[choice], columns, rows)) {
            return choice;
        }
        puts("Dette brettet får ikke plass. Velg en mindre variant eller gjør terminalen større.");
    }
}

static bool point_equals(Point first, Point second)
{
    return first.x == second.x && first.y == second.y;
}

static bool snake_contains(const Point *snake, int length, Point point)
{
    int index;

    for (index = 0; index < length; ++index) {
        if (point_equals(snake[index], point)) {
            return true;
        }
    }
    return false;
}

static bool place_food(Point *food, const Point *snake, int length,
                       int width, int height)
{
    int free_cells = (width - 2) * (height - 2) - length;
    int target;
    int seen = 0;
    int x;
    int y;

    if (free_cells <= 0) {
        return false;
    }
    target = rand() % free_cells;
    for (y = 1; y < height - 1; ++y) {
        for (x = 1; x < width - 1; ++x) {
            Point candidate = {x, y};
            if (!snake_contains(snake, length, candidate)) {
                if (seen == target) {
                    *food = candidate;
                    return true;
                }
                ++seen;
            }
        }
    }
    return false;
}

static void draw_game(const Point *snake, int length, Point food,
                      int width, int height, int level, int delay_ms,
                      bool full_redraw)
{
    static char previous_cells[80 * 35];
    static char previous_status[96];
    char status[96];
    int columns = width;
    int rows = height + 1;
    int left;
    int status_left;
    int top;
    int x;
    int y;

    (void)get_terminal_size(&columns, &rows);
    left = (columns - width) / 2;
    top = (rows - (height + 1)) / 2;
    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }

    if (full_redraw) {
        fputs(ANSI_CLEAR, stdout);
        memset(previous_cells, 0, sizeof(previous_cells));
        previous_status[0] = '\0';
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            Point point = {x, y};
            char cell;
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1) {
                cell = '*';
            } else if (point_equals(point, food)) {
                cell = 'O';
            } else if (snake_contains(snake, length, point)) {
                cell = 'S';
            } else {
                cell = ' ';
            }
            if (previous_cells[y * width + x] != cell) {
                printf("\033[%d;%dH", top + y + 1, left + x + 1);
                if (cell == '*') {
                    fputs(ANSI_WHITE "*" ANSI_RESET, stdout);
                } else if (cell == 'O') {
                    fputs(ANSI_YELLOW "O" ANSI_RESET, stdout);
                } else if (cell == 'S') {
                    fputs(ANSI_RED "S" ANSI_RESET, stdout);
                } else {
                    putchar(' ');
                }
                previous_cells[y * width + x] = cell;
            }
        }
    }
    snprintf(status, sizeof(status), "Lengde:%d  Nivå:%d  %dms  Esc:avslutt",
             length, level, delay_ms);
    status_left = (columns - (int)strlen(status)) / 2;
    if (status_left < 0) {
        status_left = 0;
    }
    if (strcmp(status, previous_status) != 0) {
        printf("\033[%d;%dH%s\033[K", top + height + 1, status_left + 1, status);
        strcpy(previous_status, status);
    }
    fflush(stdout);
}

static bool directions_are_opposite(Direction first, Direction second)
{
    return (first == DIR_UP && second == DIR_DOWN) ||
           (first == DIR_DOWN && second == DIR_UP) ||
           (first == DIR_LEFT && second == DIR_RIGHT) ||
           (first == DIR_RIGHT && second == DIR_LEFT);
}

/* Returnerer -1 for Esc, 0 for timeout og 1 når retningen ble endret. */
static int read_game_input(int timeout_ms, Direction current, Direction *next)
{
    fd_set input_set;
    struct timeval timeout;
    unsigned char bytes[16];
    ssize_t length;
    int result;

    FD_ZERO(&input_set);
    FD_SET(STDIN_FILENO, &input_set);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    result = select(STDIN_FILENO + 1, &input_set, NULL, NULL, &timeout);
    if (result <= 0) {
        return 0;
    }
    length = read(STDIN_FILENO, bytes, sizeof(bytes));
    if (length <= 0) {
        return 0;
    }

    for (ssize_t index = 0; index < length; ++index) {
        Direction candidate;
        bool arrow = false;

        if (bytes[index] == 27) {
            if (index + 2 < length && bytes[index + 1] == '[') {
                switch (bytes[index + 2]) {
                    case 'A': candidate = DIR_UP; arrow = true; break;
                    case 'B': candidate = DIR_DOWN; arrow = true; break;
                    case 'C': candidate = DIR_RIGHT; arrow = true; break;
                    case 'D': candidate = DIR_LEFT; arrow = true; break;
                    default: break;
                }
                index += 2;
            } else {
                return -1;
            }
        }
        if (arrow && !directions_are_opposite(current, candidate)) {
            *next = candidate;
            return 1;
        }
    }
    return 0;
}

static EndReason play_game(const BoardPreset *board, int *final_length)
{
    int capacity = (board->width - 2) * (board->height - 2);
    Point *snake = malloc((size_t)capacity * sizeof(*snake));
    Point food = {0, 0};
    int length = 3;
    int eaten = 0;
    int last_columns = -1;
    int last_rows = -1;
    Direction direction = DIR_RIGHT;
    EndReason reason = END_ESCAPE;
    bool full_redraw = true;

    if (snake == NULL) {
        fputs("Kunne ikke reservere minne til spillebrettet.\n", stderr);
        *final_length = 0;
        return END_ESCAPE;
    }

    snake[0] = (Point){board->width / 2, board->height / 2};
    snake[1] = (Point){snake[0].x - 1, snake[0].y};
    snake[2] = (Point){snake[0].x - 2, snake[0].y};
    (void)place_food(&food, snake, length, board->width, board->height);

    if (!enable_game_terminal()) {
        fputs("Spillet krever en interaktiv terminal.\n", stderr);
        free(snake);
        *final_length = 0;
        return END_ESCAPE;
    }

    for (;;) {
        int delay_ms = START_DELAY_MS - eaten * SPEED_STEP_MS;
        Direction requested = direction;
        int input;
        Point new_head = snake[0];
        bool grows;
        int collision_length;
        int columns = board->width;
        int rows = board->height + 1;

        if (get_terminal_size(&columns, &rows) &&
            !preset_fits(board, columns, rows)) {
            fputs(ANSI_CLEAR, stdout);
            printf("\033[%d;1HTerminalen er for liten. Spillet er pauset.\n"
                   "Gjør vinduet minst %d x %d tegn. Esc avslutter.",
                   rows / 2, board->width, board->height + 1);
            fflush(stdout);
            do {
                input = read_game_input(250, direction, &requested);
                if (input < 0) {
                    reason = END_ESCAPE;
                    goto game_finished;
                }
            } while (!get_terminal_size(&columns, &rows) ||
                     !preset_fits(board, columns, rows));
            full_redraw = true;
        }
        if (columns != last_columns || rows != last_rows) {
            last_columns = columns;
            last_rows = rows;
            full_redraw = true;
        }

        if (delay_ms < MIN_DELAY_MS) {
            delay_ms = MIN_DELAY_MS;
        }
        draw_game(snake, length, food, board->width, board->height,
                  eaten + 1, delay_ms, full_redraw);
        full_redraw = false;
        input = read_game_input(delay_ms, direction, &requested);
        if (input < 0) {
            reason = END_ESCAPE;
            break;
        }
        direction = requested;

        switch (direction) {
            case DIR_UP: --new_head.y; break;
            case DIR_DOWN: ++new_head.y; break;
            case DIR_LEFT: --new_head.x; break;
            case DIR_RIGHT: ++new_head.x; break;
        }
        grows = point_equals(new_head, food);
        collision_length = grows ? length : length - 1;
        if (new_head.x <= 0 || new_head.x >= board->width - 1 ||
            new_head.y <= 0 || new_head.y >= board->height - 1 ||
            snake_contains(snake, collision_length, new_head)) {
            reason = END_COLLISION;
            break;
        }

        if (grows) {
            memmove(&snake[1], &snake[0], (size_t)length * sizeof(*snake));
            snake[0] = new_head;
            ++length;
            ++eaten;
            if (!place_food(&food, snake, length, board->width, board->height)) {
                reason = END_BOARD_FULL;
                break;
            }
        } else {
            memmove(&snake[1], &snake[0], (size_t)(length - 1) * sizeof(*snake));
            snake[0] = new_head;
        }
    }

game_finished:
    *final_length = length;
    free(snake);
    restore_terminal();
    return reason;
}

static bool prompt_player_name(char name[MAX_NAME_BYTES + 1])
{
    char input[MAX_NAME_BYTES + 2];

    for (;;) {
        printf("Spillernavn (1-%d tegn): ", MAX_NAME_CHARS);
        fflush(stdout);
        if (!read_line(input, sizeof(input))) {
            return false;
        }
        if (input[0] == '\0') {
            puts("Navnet kan ikke være tomt.");
        } else if (utf8_character_count(input) < 0) {
            puts("Navnet må være gyldig UTF-8.");
        } else if (utf8_character_count(input) > MAX_NAME_CHARS) {
            printf("Navnet kan ikke være lengre enn %d tegn.\n", MAX_NAME_CHARS);
        } else if (strchr(input, '\t') != NULL) {
            puts("Navnet kan ikke inneholde tabulatortegn.");
        } else {
            strcpy(name, input);
            return true;
        }
    }
}

int main(void)
{
    Score scores[MAX_SCORES];
    int score_count;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fputs("Snake må startes i en interaktiv terminal.\n", stderr);
        return EXIT_FAILURE;
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    if (sodium_init() < 0) {
        fputs("Kunne ikke initialisere libsodium.\n", stderr);
        return EXIT_FAILURE;
    }
    install_signal_handlers();
    atexit(restore_terminal);
    srand((unsigned int)(time(NULL) ^ (time_t)getpid()));
    printf("\033]0;Snake %s\007", GAME_VERSION);
    fflush(stdout);
    score_count = load_scores(scores);

    for (;;) {
        int choice;

        refresh_expired_scores(scores, &score_count);
        fputs(ANSI_CLEAR, stdout);
        show_scores(scores, score_count);
        putchar('\n');
        show_main_menu_box();
        choice = read_single_choice(1, 2);
        if (choice == 2) {
            break;
        }

        {
            char player_name[MAX_NAME_BYTES + 1];
            int board_index;
            int final_length;
            EndReason reason;

            if (!prompt_player_name(player_name)) {
                break;
            }
            board_index = select_board();
            reason = play_game(&PRESETS[board_index], &final_length);

            fputs(ANSI_CLEAR, stdout);
            if (reason == END_ESCAPE) {
                printf("Spillet ble avsluttet med Esc. Resultatet ble ikke lagret.\n");
            } else {
                if (reason == END_BOARD_FULL) {
                    puts("Gratulerer! Du fylte hele brettet.");
                } else {
                    puts("Game over!");
                }
                printf("%s fikk en sluttlengde på %d.\n", player_name, final_length);
                (void)save_score(scores, &score_count, player_name, final_length);
            }
            refresh_expired_scores(scores, &score_count);
            show_scores(scores, score_count);
            wait_for_enter();
        }
    }

    puts("Takk for spillet!");
    return EXIT_SUCCESS;
}
