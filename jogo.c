#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"

#include "inc/ssd1306.h"
#include "inc/font.h"

#include <inttypes.h>
#include "jogo.h"

volatile Pipe pipes[5] = {0};
volatile uint8_t next_pipe = 0; 
volatile Bird bird = {0};
volatile Status status = START_SCREEN;

int main() {
    init_config();

    play_start_sound();

    while (1) {
        switch (status) {
            case START_SCREEN:
                if (!gpio_get(BOTAO_A)) {
                    game_start();
                    sleep_ms(100);
                }
                break;

            case PLAYING:
                // atualizar física
                // atualizar canos
                // detectar colisões
                break;

            case GAME_OVER:
                if (!gpio_get(BOTAO_A)) {
                    game_start();
                    sleep_ms(100);
                }
                break;
        }

        sleep_ms(20);
    }

    return 0;
}

void render_task() {

    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_t ssd;

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, END, I2C_PORT);
    ssd1306_config(&ssd);

    while (1) {

        switch (status) {

            case START_SCREEN:
                // desenhar tela inicial

                multicore_fifo_pop_blocking();
                // limpar tela !
                break;
                
                case PLAYING:
                
                // desenhar bird
                // desenhar pipes
                break;
                
                case GAME_OVER:
                // desenhar tela de derrota
                
                multicore_fifo_pop_blocking();
                // limpar tela !
                break;
        }
    }
}

void init_config(){
    multicore_launch_core1(render_task);
        
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);
    
    uint slice_a = pwm_gpio_to_slice_num(BUZZER_A);
    uint slice_b = pwm_gpio_to_slice_num(BUZZER_B);
    
    pwm_set_clkdiv(slice_a, 4.0f);
    pwm_set_clkdiv(slice_b, 4.0f);
    
    pwm_set_enabled(slice_a, true);
    pwm_set_enabled(slice_b, true);
}

void play_buzzer(uint freq_a, uint freq_b, uint tempo_ms) {

    uint slice_a = pwm_gpio_to_slice_num(BUZZER_A);
    uint slice_b = pwm_gpio_to_slice_num(BUZZER_B);

    uint ch_a = pwm_gpio_to_channel(BUZZER_A);
    uint ch_b = pwm_gpio_to_channel(BUZZER_B);

    uint32_t wrap_a = 125000000 / (4 * freq_a) - 1;
    uint32_t wrap_b = 125000000 / (4 * freq_b) - 1;

    pwm_set_wrap(slice_a, wrap_a);
    pwm_set_wrap(slice_b, wrap_b);

    pwm_set_chan_level(slice_a, ch_a, wrap_a / 2);
    pwm_set_chan_level(slice_b, ch_b, wrap_b / 2);

    sleep_ms(tempo_ms);

    pwm_set_chan_level(slice_a, ch_a, 0);
    pwm_set_chan_level(slice_b, ch_b, 0);

    sleep_ms(30);
}

void play_start_sound() {
    play_buzzer(900, 1200, 160);
    play_buzzer(950, 1200, 120);
    play_buzzer(1000, 1200, 80);
}

void play_game_sound() {
    play_buzzer(523, 659, 80);
    play_buzzer(659, 784, 80);
    play_buzzer(784, 1046, 150);
    play_buzzer(865, 1046, 200);
}

void play_game_over_sound() {
    play_buzzer(800, 600, 140);
    play_buzzer(700, 500, 110);
    play_buzzer(600, 500, 110);
    play_buzzer(500, 500, 180);
}

void game_start(){
    play_game_sound();
    status = PLAYING;
    multicore_fifo_push_blocking(1);
}