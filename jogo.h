#ifndef JOGO_H
#define JOGO_H

#include <stdint.h>
#include <stdbool.h>
#include "inc/ssd1306.h"

#define BOTAO_A 5
#define BOTAO_B 6
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define END 0x3C
#define BUZZER_A 10
#define BUZZER_B 21

#define GRAVITY 0.3f
#define JUMP_VELOCITY -2.2f
#define PIPE_VELOCITY -1

#define GAP_SIZE 26
#define PIPE_DISTANCE 37
#define BIRD_WIDTH 7
#define PIPE_WIDTH 5

typedef struct {
    int16_t pos_x;
    uint8_t gap_y;
    bool passed;
} Pipe;

typedef struct {
    float pos_y;
    float vel_y;
} Bird;

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER,
    GAME_STATE_PAUSE    
} State;

void play_init_sound();
void play_game_start_sound();
void play_game_over_sound();
void init_config();
void render_task();
void game_start();
void game_over();
void draw_objects(ssd1306_t *ssd);

#endif