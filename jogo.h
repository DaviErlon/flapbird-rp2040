#ifndef J0GO_H
#define JOGO_H

#define BOTAO_A 5

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define END 0x3C

#define BUZZER_A 10
#define BUZZER_B 21

void play_buzzer(uint freq_a, uint freq_b, uint tempo_ms);
void play_start_sound();
void play_game_sound();
void play_game_over_sound();
void init_config();
void render_task();
void game_start();

#define GAP_SIZE 15
#define PIPE_DISTANCE 35

typedef struct {
    uint8_t pos_x;
    uint8_t gap_y;
} Pipe;

#define GRAVITY 0.5f
#define BIRD_SIZE 7
#define JUMP_VELOCITY -4.0f

typedef struct {
    uint8_t pos_y;
    float vel_y;
} Bird;

typedef enum {
    START_SCREEN,
    GAME_OVER,
    PLAYING
} Status;

#endif