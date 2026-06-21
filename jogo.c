#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"

#include "inc/ssd1306.h"
#include "inc/font.h"

#include <inttypes.h>
#include <time.h>
#include <stdlib.h>

#include "jogo.h"

volatile Pipe pipes[5] = {0};
volatile Bird bird = {0};
volatile Status status = START;
volatile bool last_btn = true;

int main()
{
    init_config();
    play_init_sound();

    while (1)
    {
        switch (status)
        {
        case START:
        case GAME_OVER:
            if (!gpio_get(BOTAO_A))
            {
                game_start();
            }
            sleep_ms(10);
            break;

        case PLAYING:
        {
            bool btn = gpio_get(BOTAO_A);
            if (!btn && last_btn)
            {
                bird.vel_y = JUMP_VELOCITY;
            }
            last_btn = btn;

            bird.vel_y += GRAVITY;
            bird.pos_y += bird.vel_y;

            for (uint8_t i = 0; i < 5; i++)
            {
                pipes[i].pos_x += PIPE_VELOCITY;
                if (pipes[i].pos_x <= -5)
                {
                    pipes[i].pos_x = 175;
                    pipes[i].gap_y = (rand() % (58 - GAP_SIZE)) + 3;
                    continue;
                }

                // largura do cano = 5px (x .. x+4), pássaro ocupa x = 12..18
                int16_t px = pipes[i].pos_x;
                bool overlap_x = (px <= 18) && (px + 4 >= 12);
                if (!overlap_x)
                {
                    continue;
                }

                float bird_top = bird.pos_y;
                float bird_bottom = bird.pos_y + BIRD_SIZE;
                float gap_top = pipes[i].gap_y;
                float gap_bottom = pipes[i].gap_y + GAP_SIZE;

                if (bird_top <= gap_top || bird_bottom >= gap_bottom)
                {
                    game_over();
                    break;
                }
            }

            if (bird.pos_y >= HEIGHT - BIRD_SIZE)
            {
                game_over();
            }
            if (bird.pos_y < 0)
            {
                bird.pos_y = 0;
                bird.vel_y = 0;
            }
            sleep_ms(50);
            break;
        }
        case WAITING:
            sleep_ms(10);
            break;
        }
    }

    return 0;
}

void render_task()
{
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_t ssd;

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, END, I2C_PORT);
    ssd1306_config(&ssd);

    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    while (1)
    {
        switch (status)
        {
        case START:
            ssd1306_hline(&ssd, 22, 101, 19, true);
            ssd1306_hline(&ssd, 22, 101, 41, true);
            ssd1306_vline(&ssd, 22, 19, 41, true);
            ssd1306_vline(&ssd, 101, 19, 41, true);
            ssd1306_draw_string(&ssd, "FLAPBIRD", 31, 23);
            ssd1306_draw_string(&ssd, "press btn", 27, 31);
            ssd1306_send_data(&ssd);

            // espera em vez de ficar redesenhando a tela inicial
            while (status == START)
                ;

            // limpar tela
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            break;

        case PLAYING:
            draw_objects(&ssd);
            break;

        case GAME_OVER:
            ssd1306_hline(&ssd, 22, 101, 19, true);
            ssd1306_hline(&ssd, 22, 101, 41, true);
            ssd1306_vline(&ssd, 22, 19, 41, true);
            ssd1306_vline(&ssd, 101, 19, 41, true);
            ssd1306_draw_string(&ssd, "YOU LOST", 31, 23);
            ssd1306_draw_string(&ssd, "try again", 27, 31);
            ssd1306_send_data(&ssd);

            while (status == GAME_OVER)
                ;

            // limpar tela
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            break;

        case WAITING:
            sleep_ms(10);
            break;
        }
    }
}

void init_config()
{
    multicore_launch_core1(render_task);
    srand(time(NULL));
    
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

void play_buzzer(uint freq_a, uint freq_b, uint tempo_ms)
{
    if (freq_a == 0 || freq_b == 0)
    {
        sleep_ms(tempo_ms);
        return;
    }

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

void play_init_sound()
{
    play_buzzer(900, 1200, 160);
    play_buzzer(950, 1200, 120);
    play_buzzer(1000, 1200, 80);
}

void play_game_start_sound()
{
    play_buzzer(523, 659, 80);
    play_buzzer(659, 784, 80);
    play_buzzer(784, 1046, 150);
    play_buzzer(865, 1046, 200);
}

void play_game_over_sound()
{
    play_buzzer(800, 600, 140);
    play_buzzer(700, 500, 110);
    play_buzzer(600, 500, 110);
    play_buzzer(500, 500, 180);
}

void game_start()
{
    for (uint8_t i = 0; i < 5; i++)
    {
        pipes[i] = (Pipe){(int16_t)(127 + i * PIPE_DISTANCE), (uint8_t)((rand() % (58 - GAP_SIZE)) + 3)};
    }
    last_btn = gpio_get(BOTAO_A);
    bird = (Bird){32.0f, 0.0f};
    status = PLAYING;
    play_game_start_sound();
}

void game_over()
{
    status = GAME_OVER;
    play_game_over_sound();
}

void draw_objects(ssd1306_t *ssd)
{
    Pipe pps[5];
    for (uint8_t i = 0; i < 5; i++)
    {
        pps[i].pos_x = pipes[i].pos_x;
        pps[i].gap_y = pipes[i].gap_y;
    }
    int p = (int)(bird.pos_y);

    for (uint8_t i = 0; i < 5; i++)
    {
        int16_t x16 = pps[i].pos_x;
        uint8_t y1 = pps[i].gap_y;
        uint8_t y2 = pps[i].gap_y + GAP_SIZE;
        uint8_t w = 5;

        if (x16 >= 128 || x16 <= -5)
        {
            continue;
        }

        int8_t x;
        if (x16 >= 124)
        {
            w = 128 - x16;
            x = (int8_t)x16;
        }
        else if (x16 < 0)
        {
            w += x16;
            x = 0;
        }
        else
        {
            x = (int8_t)x16;
        }

        ssd1306_rect(ssd, 0, x, w, y1, true, true);
        ssd1306_rect(ssd, y2, x, w, 64 - y2, true, true);
    }

    ssd1306_vline(ssd, 12, p + 2, p + 5, true);
    ssd1306_vline(ssd, 13, p + 1, p + 6, true);
    ssd1306_vline(ssd, 14, p, p + 7, true);
    ssd1306_vline(ssd, 15, p, p + 7, true);
    ssd1306_vline(ssd, 16, p, p + 7, true);
    ssd1306_vline(ssd, 17, p + 1, p + 6, true);
    ssd1306_vline(ssd, 18, p + 2, p + 5, true);

    ssd1306_send_data(ssd);

    ssd1306_vline(ssd, 12, p + 2, p + 5, false);
    ssd1306_vline(ssd, 13, p + 1, p + 6, false);
    ssd1306_vline(ssd, 14, p, p + 7, false);
    ssd1306_vline(ssd, 15, p, p + 7, false);
    ssd1306_vline(ssd, 16, p, p + 7, false);
    ssd1306_vline(ssd, 17, p + 1, p + 6, false);
    ssd1306_vline(ssd, 18, p + 2, p + 5, false);

    for (uint8_t i = 0; i < 5; i++)
    {
        int16_t x16 = pps[i].pos_x;
        uint8_t y1 = pps[i].gap_y;
        uint8_t y2 = pps[i].gap_y + GAP_SIZE;
        uint8_t w = 5;

        if (x16 >= 128 || x16 <= -5)
        {
            continue;
        }

        int8_t x;
        if (x16 >= 124)
        {
            w = 128 - x16;
            x = (int8_t)x16;
        }
        else if (x16 < 0)
        {
            w += x16;
            x = 0;
        }
        else
        {
            x = (int8_t)x16;
        }

        ssd1306_rect(ssd, 0, x, w, y1, false, true);
        ssd1306_rect(ssd, y2, x, w, 64 - y2, false, true);
    }
}