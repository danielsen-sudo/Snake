#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "../include/snake_shared.h"
#include "../include/sodium_compat.h"
#include "../include/game.h"
#include "../include/scores.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * SDL3-grensesnittet. Rendering bruker en fast logisk oppløsning slik at
 * layout og musekoordinater er de samme uansett faktisk vindusstørrelse.
 */

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 640

static TTF_Font *ui_font;

typedef enum {
    VIEW_MENU, VIEW_SCORES, VIEW_NAME, VIEW_BOARD, VIEW_GAME, VIEW_GAME_OVER
} View;

/*
 * Normal flyt: MENU -> NAME -> BOARD -> GAME -> GAME_OVER -> MENU.
 * SCORES er en modal avstikker fra MENU. Esc går ett nivå tilbake, mens Esc
 * under GAME avbryter runden uten lagring. R starter GAME på samme brett igjen.
 */

static void color(SDL_Renderer *r, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
}

static void text(SDL_Renderer *r, float x, float y, float scale,
                 const char *value)
{
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha;
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_FRect destination;

    if (ui_font == NULL) {
        SDL_SetRenderScale(r, scale, scale);
        SDL_RenderDebugText(r, x / scale, y / scale, value);
        SDL_SetRenderScale(r, 1.0f, 1.0f);
        return;
    }
    TTF_SetFontSize(ui_font, 12.0f * scale);
    SDL_GetRenderDrawColor(r, &red, &green, &blue, &alpha);
    surface = TTF_RenderText_Blended(ui_font, value, 0,
                                    (SDL_Color){red, green, blue, alpha});
    if (surface == NULL) return;
    texture = SDL_CreateTextureFromSurface(r, surface);
    destination = (SDL_FRect){x, y, (float)surface->w, (float)surface->h};
    SDL_DestroySurface(surface);
    if (texture == NULL) return;
    SDL_RenderTexture(r, texture, NULL, &destination);
    SDL_DestroyTexture(texture);
}

static void centered(SDL_Renderer *r, float y, float scale, const char *value)
{
    float width = (float)strlen(value) * 8.0f * scale;
    int measured_width;
    int measured_height;

    if (ui_font != NULL) {
        TTF_SetFontSize(ui_font, 12.0f * scale);
        if (TTF_GetStringSize(ui_font, value, 0, &measured_width,
                              &measured_height))
            width = (float)measured_width;
    }
    text(r, (WINDOW_WIDTH - width) / 2.0f, y, scale, value);
}

static TTF_Font *open_ui_font(void)
{
    const char *relative = "assets/fonts/NotoSans-Regular.ttf";
    TTF_Font *font = TTF_OpenFont(relative, 16.0f);

    if (font == NULL) {
        const char *base = SDL_GetBasePath();
        char path[4096];
        if (base != NULL &&
            snprintf(path, sizeof(path), "%s../%s", base, relative) > 0)
            font = TTF_OpenFont(path, 16.0f);
    }
    return font;
}

static void background(SDL_Renderer *r)
{
    SDL_FRect stripe = {0, 0, WINDOW_WIDTH, 6};
    color(r, 8, 12, 18);
    SDL_RenderClear(r);
    color(r, 30, 46, 58);
    SDL_RenderFillRect(r, &stripe);
}

static void panel(SDL_Renderer *r, SDL_FRect box)
{
    SDL_FRect shadow = {box.x + 8, box.y + 8, box.w, box.h};
    color(r, 2, 5, 9);
    SDL_RenderFillRect(r, &shadow);
    color(r, 17, 27, 36);
    SDL_RenderFillRect(r, &box);
    color(r, 68, 220, 132);
    SDL_RenderRect(r, &box);
}

static SDL_FRect button_box(int index)
{
    return (SDL_FRect){310, 330 + index * 66, 340, 48};
}

static void button(SDL_Renderer *r, int index, const char *label)
{
    SDL_FRect box = button_box(index);
    color(r, 25, 42, 53);
    SDL_RenderFillRect(r, &box);
    color(r, 68, 220, 132);
    SDL_RenderRect(r, &box);
    color(r, 226, 240, 235);
    centered(r, box.y + 15, 1.5f, label);
}

static void score_rows(SDL_Renderer *r, const Score scores[MAX_SCORES],
                       int count, int limit, float y)
{
    int shown = count < limit ? count : limit;
    int index;

    if (shown == 0) {
        color(r, 150, 165, 174);
        centered(r, y, 1.5f, "INGEN RESULTATER ENNA");
    }
    for (index = 0; index < shown; ++index) {
        char rank[12];
        char value[24];
        float row_y = y + index * 34;
        snprintf(rank, sizeof(rank), "%d.", index + 1);
        snprintf(value, sizeof(value), "%d", scores[index].score);
        color(r, index < 3 ? 255 : 174, index < 3 ? 204 : 190,
              index < 3 ? 80 : 198);
        text(r, 250, row_y, 1.5f, rank);
        color(r, 220, 232, 237);
        text(r, 300, row_y, 1.5f, scores[index].name);
        color(r, 68, 220, 132);
        text(r, 660, row_y, 1.5f, value);
    }
}

static void menu(SDL_Renderer *r, const Score scores[MAX_SCORES], int count)
{
    char title[64];

    background(r);
    panel(r, (SDL_FRect){220, 65, 520, 525});
    color(r, 68, 220, 132);
    snprintf(title, sizeof(title), "SNAKE %s", GAME_VERSION);
    centered(r, 96, 3, title);
    color(r, 137, 151, 160);
    centered(r, 140, 1.25f, "SDL3 EDITION");
    color(r, 255, 204, 80);
    centered(r, 190, 1.5f, "TOPP 3");
    score_rows(r, scores, count, 3, 226);
    button(r, 0, "1  NYTT SPILL");
    button(r, 1, "2  VIS TOPPLISTE");
    button(r, 2, "3  AVSLUTT");
    color(r, 112, 129, 139);
    centered(r, 554, 1, "BRUK TALLTASTENE ELLER MUSEN");
}

static void scores_view(SDL_Renderer *r, const Score scores[MAX_SCORES],
                        int count)
{
    background(r);
    panel(r, (SDL_FRect){185, 55, 590, 530});
    color(r, 255, 204, 80);
    centered(r, 88, 2.5f, "TOPPLISTE");
    score_rows(r, scores, count, MAX_SCORES, 155);
    color(r, 112, 129, 139);
    centered(r, 548, 1, "ENTER ELLER ESC: TILBAKE");
}

static void name_view(SDL_Renderer *r, const char *name)
{
    SDL_FRect field = {260, 285, 440, 58};
    background(r);
    panel(r, (SDL_FRect){205, 120, 550, 390});
    color(r, 68, 220, 132);
    centered(r, 165, 2.25f, "SPILLERNAVN");
    color(r, 150, 165, 174);
    centered(r, 225, 1.25f, "SKRIV NAVNET DITT");
    color(r, 25, 42, 53);
    SDL_RenderFillRect(r, &field);
    color(r, 68, 220, 132);
    SDL_RenderRect(r, &field);
    color(r, 226, 240, 235);
    if (name[0] == '\0') centered(r, 306, 1.5f, "_");
    else centered(r, 306, 1.5f, name);
    color(r, 112, 129, 139);
    centered(r, 405, 1, "ENTER: FORTSETT  |  ESC: TILBAKE");
}

static void board_view(SDL_Renderer *r)
{
    size_t index;
    background(r);
    panel(r, (SDL_FRect){215, 70, 530, 500});
    color(r, 68, 220, 132);
    centered(r, 105, 2.25f, "VELG BRETT");
    for (index = 0; index < SDL_arraysize(PRESETS); ++index) {
        char label[80];
        SDL_FRect box = {280, 180 + index * 72, 400, 50};
        snprintf(label, sizeof(label), "%zu  %-12s  %d x %d", index + 1,
                 PRESETS[index].name, PRESETS[index].width,
                 PRESETS[index].height);
        color(r, 25, 42, 53);
        SDL_RenderFillRect(r, &box);
        color(r, 68, 220, 132);
        SDL_RenderRect(r, &box);
        color(r, 226, 240, 235);
        centered(r, box.y + 16, 1.5f, label);
    }
    color(r, 112, 129, 139);
    centered(r, 530, 1, "ESC: TILBAKE");
}

static void game_view(SDL_Renderer *r, const GameState *game, bool paused,
                      const Score scores[MAX_SCORES], int score_count)
{
    /* Cellebredden velges fra både høyde og bredde for å bevare proporsjoner. */
    const BoardPreset *preset = &PRESETS[game->preset];
    float cell = SDL_min(840.0f / preset->width, 504.0f / preset->height);
    float board_w = cell * preset->width;
    float board_h = cell * preset->height;
    float left = (WINDOW_WIDTH - board_w) / 2;
    float top = 76 + (510 - board_h) / 2;
    SDL_FRect board = {left, top, board_w, board_h};
    SDL_FRect score_box = {320, 10, 320, 60};
    char title[80];
    int x;
    int y;
    int index;

    background(r);
    color(r, 17, 27, 36);
    SDL_RenderFillRect(r, &board);
    color(r, 28, 43, 54);
    for (x = 0; x <= preset->width; ++x)
        SDL_RenderLine(r, left + x * cell, top, left + x * cell, top + board_h);
    for (y = 0; y <= preset->height; ++y)
        SDL_RenderLine(r, left, top + y * cell, left + board_w, top + y * cell);
    color(r, 88, 108, 120);
    for (x = 0; x < preset->width; ++x) {
        SDL_FRect wall_top = {left + x * cell + 1, top + 1, cell - 2, cell - 2};
        SDL_FRect wall_bottom = {left + x * cell + 1,
                                 top + (preset->height - 1) * cell + 1,
                                 cell - 2, cell - 2};
        SDL_RenderFillRect(r, &wall_top); SDL_RenderFillRect(r, &wall_bottom);
    }
    for (y = 1; y < preset->height - 1; ++y) {
        SDL_FRect wall_left = {left + 1, top + y * cell + 1, cell - 2, cell - 2};
        SDL_FRect wall_right = {left + (preset->width - 1) * cell + 1,
                                top + y * cell + 1, cell - 2, cell - 2};
        SDL_RenderFillRect(r, &wall_left); SDL_RenderFillRect(r, &wall_right);
    }
    for (index = game->length - 1; index >= 0; --index) {
        float inset = SDL_max(1.0f, cell * 0.12f);
        SDL_FRect body = {left + game->snake[index].x * cell + inset,
                          top + game->snake[index].y * cell + inset,
                          cell - inset * 2, cell - inset * 2};
        color(r, index == 0 ? 137 : 68, index == 0 ? 255 : 220,
              index == 0 ? 183 : 132);
        SDL_RenderFillRect(r, &body);
    }
    color(r, 255, 91, 105);
    float inset = SDL_max(2.0f, cell * 0.2f);
    SDL_FRect food = {left + game->food.x * cell + inset,
                      top + game->food.y * cell + inset,
                      cell - inset * 2, cell - inset * 2};
    SDL_RenderFillRect(r, &food);
    snprintf(title, sizeof(title), "%s  %d x %d", preset->name,
             preset->width, preset->height);
    color(r, 219, 231, 238);
    text(r, 60, 34, 1.25f, title);
    color(r, 25, 42, 53);
    SDL_RenderFillRect(r, &score_box);
    color(r, 255, 204, 80);
    SDL_RenderRect(r, &score_box);
    snprintf(title, sizeof(title), "POENG: %d", game->length);
    text(r, score_box.x + 16.0f, score_box.y + 14.0f, 2.0f, title);
    {
        int rank = score_rank(scores, score_count, game->length);
        if (rank <= MAX_SCORES) snprintf(title, sizeof(title), "RANK: #%d", rank);
        else snprintf(title, sizeof(title), "RANK: >%d", MAX_SCORES);
        color(r, 137, 255, 183);
        text(r, score_box.x + 185.0f, score_box.y + 18.0f, 1.5f, title);
    }
    color(r, 112, 129, 139);
    snprintf(title, sizeof(title),
             "NIVAA: %d  |  PILTASTER / WASD  |  ESC: AVBRYT",
             game->eaten + 1);
    centered(r, 608, 1, title);
    if (paused) {
        SDL_FRect shade = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 2, 5, 9, 210);
        SDL_RenderFillRect(r, &shade);
        color(r, 255, 204, 80);
        centered(r, 265, 3.0f, "PAUSE");
        color(r, 226, 240, 235);
        centered(r, 330, 1.25f, "TRYKK P ELLER MELLOMROM FOR A FORTSETTE");
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

static void game_over_view(SDL_Renderer *r, const GameState *game,
                           int final_rank, const char *name, bool score_saved)
{
    char result[160];
    background(r);
    panel(r, (SDL_FRect){205, 120, 550, 390});
    color(r, game->reason == END_BOARD_FULL ? 255 : 255,
          game->reason == END_BOARD_FULL ? 204 : 91,
          game->reason == END_BOARD_FULL ? 80 : 105);
    centered(r, 165, 2.5f,
             game->reason == END_BOARD_FULL ? "BRETTET ER FYLT!" : "GAME OVER");
    snprintf(result, sizeof(result), "%s FIKK LENGDE %d", name, game->length);
    color(r, 226, 240, 235);
    centered(r, 275, 1.5f, result);
    if (final_rank <= MAX_SCORES)
        snprintf(result, sizeof(result), "HIGH SCORE: PLASS #%d",
                 final_rank);
    else
        snprintf(result, sizeof(result), "HIGH SCORE: UTENFOR TOPP %d",
                 MAX_SCORES);
    color(r, 255, 204, 80);
    centered(r, 330, 1.5f, result);
    if (final_rank > MAX_SCORES) {
        color(r, 137, 151, 160);
        centered(r, 375, 1.0f, "RESULTATET KOM IKKE PAA TOPPLISTEN");
    } else {
        color(r, score_saved ? 68 : 255, score_saved ? 220 : 91,
              score_saved ? 132 : 105);
        centered(r, 375, 1.0f, score_saved ? "RESULTATET ER LAGRET"
                                          : "RESULTATET KUNNE IKKE LAGRES");
    }
    color(r, 112, 129, 139);
    centered(r, 420, 1, "R: SPILL IGJEN  |  ENTER: HOVEDMENY");
}

static int board_click(SDL_Renderer *r, SDL_Event *event)
{
    /* Vinduskoordinater konverteres til den logiske 960x640-flaten. */
    float x;
    float y;
    int index;
    if (!SDL_RenderCoordinatesFromWindow(r, event->button.x, event->button.y,
                                         &x, &y)) return -1;
    for (index = 0; index < (int)SDL_arraysize(PRESETS); ++index) {
        SDL_FRect box = {280, 180 + index * 72, 400, 50};
        if (x >= box.x && x <= box.x + box.w &&
            y >= box.y && y <= box.y + box.h) return index;
    }
    return -1;
}

static int menu_click(SDL_Renderer *r, SDL_Event *event)
{
    float x;
    float y;
    int index;
    if (!SDL_RenderCoordinatesFromWindow(r, event->button.x, event->button.y,
                                         &x, &y)) return -1;
    for (index = 0; index < 3; ++index) {
        SDL_FRect box = button_box(index);
        if (x >= box.x && x <= box.x + box.w &&
            y >= box.y && y <= box.y + box.h) return index;
    }
    return -1;
}

static void center_window_on_pointer_display(SDL_Window *window)
{
    float mouse_x;
    float mouse_y;
    SDL_Point mouse_position;
    SDL_DisplayID display;
    int centered;

    /* Velg fysisk skjerm fra pekeren; primærskjermen er sikker reserve. */
    SDL_GetGlobalMouseState(&mouse_x, &mouse_y);
    mouse_position.x = (int)mouse_x;
    mouse_position.y = (int)mouse_y;
    display = SDL_GetDisplayForPoint(&mouse_position);
    if (display == 0) {
        display = SDL_GetPrimaryDisplay();
    }
    centered = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
    if (!SDL_SetWindowPosition(window, centered, centered)) {
        fprintf(stderr, "Kunne ikke sentrere vinduet: %s\n", SDL_GetError());
    }
}

#ifndef SNAKE_GUI_NO_MAIN
int main(int argc, char **argv)
{
    /* View er en enkel tilstandsmaskin; bare én skjerm tegnes per bilde. */
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Event event;
    Score scores[MAX_SCORES] = {0};
    GameState game = {0};
    Uint64 next_step = 0;
    int final_rank = 0;
    bool paused = false;
    bool score_saved = false;
    char player_name[MAX_NAME_BYTES + 1] = "";
    char window_title[64];
    int score_count;
    View view = VIEW_MENU;
    bool running = true;
    bool needs_redraw = true;
    bool smoke = argc > 1 && strcmp(argv[1], "--smoke-test") == 0;

    if (sodium_init() < 0) return 1;
    srand((unsigned int)(time(NULL) ^ (time_t)getpid()));
    score_count = load_scores(scores);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Kunne ikke starte SDL3: %s\n", SDL_GetError());
        return 1;
    }
    if (!TTF_Init()) {
        fprintf(stderr, "Kunne ikke starte SDL3_ttf: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    ui_font = open_ui_font();
    if (ui_font == NULL)
        fprintf(stderr, "Fant ikke Noto Sans; bruker SDL reservefont.\n");
    snprintf(window_title, sizeof(window_title), "Snake %s - SDL3", GAME_VERSION);
    if (!SDL_CreateWindowAndRenderer(window_title, WINDOW_WIDTH,
                                     WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE,
                                     &window, &renderer)) {
        fprintf(stderr, "Kunne ikke lage vinduet: %s\n", SDL_GetError());
        if (ui_font != NULL) TTF_CloseFont(ui_font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    center_window_on_pointer_display(window);
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    while (running) {
        while (SDL_PollEvent(&event)) {
            /* Vindus- og inputhendelser kan endre synlig tilstand. */
            needs_redraw = true;
            SDL_Scancode key = event.type == SDL_EVENT_KEY_DOWN
                                   ? event.key.scancode : SDL_SCANCODE_UNKNOWN;
            int click = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                            ? menu_click(renderer, &event) : -1;
            if (event.type == SDL_EVENT_QUIT) running = false;
            else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST &&
                     view == VIEW_GAME) paused = true;
            else if (view == VIEW_MENU) {
                if (key == SDL_SCANCODE_1 || click == 0) {
                    player_name[0] = '\0';
                    view = VIEW_NAME;
                    SDL_StartTextInput(window);
                }
                else if (key == SDL_SCANCODE_2 || click == 1) {
                    refresh_expired_scores(scores, &score_count);
                    view = VIEW_SCORES;
                } else if (key == SDL_SCANCODE_3 || click == 2 ||
                           key == SDL_SCANCODE_ESCAPE) running = false;
            } else if (view == VIEW_SCORES) {
                if (key == SDL_SCANCODE_ESCAPE || key == SDL_SCANCODE_RETURN ||
                    key == SDL_SCANCODE_BACKSPACE) view = VIEW_MENU;
            } else if (view == VIEW_NAME) {
                if (event.type == SDL_EVENT_TEXT_INPUT) {
                    size_t used = strlen(player_name);
                    size_t added = strlen(event.text.text);
                    int current_characters = utf8_character_count(player_name);
                    int added_characters = utf8_character_count(event.text.text);
                    if (used + added <= MAX_NAME_BYTES &&
                        current_characters >= 0 && added_characters >= 0 &&
                        current_characters + added_characters <= MAX_NAME_CHARS) {
                        memcpy(player_name + used, event.text.text, added + 1);
                    }
                } else if (key == SDL_SCANCODE_BACKSPACE &&
                           player_name[0] != '\0') {
                    size_t used = strlen(player_name);
                    do { --used; } while (used > 0 &&
                        ((unsigned char)player_name[used] & 0xc0) == 0x80);
                    player_name[used] = '\0';
                } else if (key == SDL_SCANCODE_RETURN &&
                           player_name[0] != '\0') {
                    SDL_StopTextInput(window);
                    view = VIEW_BOARD;
                } else if (key == SDL_SCANCODE_ESCAPE) {
                    SDL_StopTextInput(window);
                    view = VIEW_MENU;
                }
            } else if (view == VIEW_BOARD) {
                int selected = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                                   ? board_click(renderer, &event) : -1;
                if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_4)
                    selected = (int)(key - SDL_SCANCODE_1);
                if (selected >= 0) {
                    if (game_init(&game, selected)) {
                        paused = false;
                        final_rank = 0;
                        score_saved = false;
                        next_step = SDL_GetTicks() + START_DELAY_MS;
                        view = VIEW_GAME;
                    }
                } else if (key == SDL_SCANCODE_ESCAPE) view = VIEW_MENU;
            } else if (view == VIEW_GAME) {
                Direction candidate = game.requested;
                bool direction_key = true;
                if (key == SDL_SCANCODE_UP || key == SDL_SCANCODE_W)
                    candidate = DIR_UP;
                else if (key == SDL_SCANCODE_DOWN || key == SDL_SCANCODE_S)
                    candidate = DIR_DOWN;
                else if (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_A)
                    candidate = DIR_LEFT;
                else if (key == SDL_SCANCODE_RIGHT || key == SDL_SCANCODE_D)
                    candidate = DIR_RIGHT;
                else direction_key = false;
                if (direction_key && !paused)
                    (void)game_request_direction(&game, candidate);
                if (key == SDL_SCANCODE_P || key == SDL_SCANCODE_SPACE) {
                    paused = !paused;
                    if (!paused) next_step = SDL_GetTicks() + START_DELAY_MS;
                }
                if (key == SDL_SCANCODE_ESCAPE) {
                    game_destroy(&game);
                    view = VIEW_MENU;
                }
            } else if (view == VIEW_GAME_OVER) {
                if (key == SDL_SCANCODE_R) {
                    int selected = game.preset;
                    game_destroy(&game);
                    if (game_init(&game, selected)) {
                        paused = false;
                        final_rank = 0;
                        next_step = SDL_GetTicks() + START_DELAY_MS;
                        view = VIEW_GAME;
                    }
                } else if (key == SDL_SCANCODE_RETURN ||
                           key == SDL_SCANCODE_ESCAPE) {
                    game_destroy(&game);
                    refresh_expired_scores(scores, &score_count);
                    view = VIEW_MENU;
                }
            }
        }
        if (view == VIEW_GAME && !paused) {
            Uint64 now = SDL_GetTicks();
            /* Ta igjen forsinkede steg uten å knytte hastigheten til FPS. */
            while (view == VIEW_GAME && now >= next_step) {
                if (!game_step(&game)) {
                    final_rank = score_rank(scores, score_count, game.length);
                    score_saved = save_score(scores, &score_count, player_name,
                                             game.length);
                    view = VIEW_GAME_OVER;
                } else {
                    next_step += (Uint64)game_delay_ms(&game);
                }
                needs_redraw = true;
            }
        }
        if (needs_redraw) {
            if (view == VIEW_MENU) menu(renderer, scores, score_count);
            else if (view == VIEW_SCORES)
                scores_view(renderer, scores, score_count);
            else if (view == VIEW_NAME) name_view(renderer, player_name);
            else if (view == VIEW_BOARD) board_view(renderer);
            else if (view == VIEW_GAME)
                game_view(renderer, &game, paused, scores, score_count);
            else game_over_view(renderer, &game, final_rank, player_name,
                                score_saved);
            SDL_RenderPresent(renderer);
            needs_redraw = false;
        }
        if (smoke) running = false;
        else SDL_Delay(16);
    }
    game_destroy(&game);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (ui_font != NULL) TTF_CloseFont(ui_font);
    ui_font = NULL;
    TTF_Quit();
    SDL_Quit();
    return 0;
}
#endif
