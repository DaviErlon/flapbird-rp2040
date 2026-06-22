#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"

#include "inc/ssd1306.h"
#include "inc/font.h"

#include <inttypes.h>
#include <string.h>
#include <stdio.h>

// definições das constantes
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
// x % GAP_RANDOM = [0,31];             intervalo de geracao
// [0,31] + 3 = [3, 34];                centralização do intervalo de geracao
// [3,34] + [0, GAP_SiZE] = [3,60]      gap visivel tem ao menos 3px dos extremos, display_y = [0, 63] px
#define GAP_RANDOM 58 - GAP_SIZE
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

static inline void play_init_sound();
static inline void play_game_start_sound();
static inline void play_game_over_sound();
static inline void init_config();
static void render_task();
static inline void game_start();
static inline void game_over();
static inline void draw_objects(ssd1306_t *ssd);
static inline void draw_pipes(ssd1306_t *ssd, Pipe pps[5], bool value);
static inline void draw_bird(ssd1306_t *ssd, int8_t p, bool value);
static inline void game_over_screen(ssd1306_t *ssd);
static inline void paused_screen(ssd1306_t *ssd);
static inline void menu_screen(ssd1306_t *ssd);
static inline void clear_screen(ssd1306_t *ssd);

// objetos compartilhados entre os núcleos do rp2040
volatile Pipe pipes[5] = {0};
volatile Bird bird = {0};
volatile State state = GAME_STATE_MENU;
volatile uint8_t points = 0;
volatile char pts[4];

// programa do core0
int main()
{   
    // inicialização de todos os recursos utilizados
    // GPIOs, Multicore, Display, etc 
    init_config();
    
    // loop de processamento da fisica do flapbird  
    while(true) {
        static bool btn;
        static bool last_btn_a = true;
        static bool last_btn_b = true;
        switch(state) {
        // os estado menu e game over apenas aguardam o jogo começar
        case GAME_STATE_MENU:
        case GAME_STATE_GAME_OVER:
            btn = gpio_get(BOTAO_B);
            if (!btn && last_btn_b){
                game_start();
                last_btn_a = gpio_get(BOTAO_A);    
                last_btn_b = gpio_get(BOTAO_B);    
            }
            last_btn_b = btn;
            sleep_ms(10);
            break;

        // estado de playing processa a fisica
        case GAME_STATE_PLAYING:
            btn = gpio_get(BOTAO_B);
            if (!btn && last_btn_b){
                state = GAME_STATE_PAUSE;
                last_btn_b = btn;
                break;
            }
            last_btn_b = btn;

            btn = gpio_get(BOTAO_A);
            
            // detecta pulo
            if (!btn && last_btn_a)
                bird.vel_y = JUMP_VELOCITY;
            
            last_btn_a = btn;

            // atualiza passaro
            bird.vel_y += GRAVITY;
            bird.pos_y += bird.vel_y;

            // atualiza os canos e detecta colisão 
            for(uint8_t i = 0; i < 5; i++) {
                pipes[i].pos_x += PIPE_VELOCITY;
                // se desaparece da tela é 'respawnado' ao final
                if (pipes[i].pos_x <= -5) {
                    pipes[i].pos_x = 175;
                    pipes[i].gap_y = (rand() % (GAP_RANDOM)) + 3;
                    pipes[i].passed = false;
                    continue;
                }

                // largura do cano = 5px, 
                // largura do pássaro = 7px, fixo em x no intervalo [12, 18] 
                // se nao estiver no intervalo critico de colisao apenas continuar
                int16_t px = pipes[i].pos_x;
                if (!((px <= 18) && (px + 4 >= 12)))
                    continue;

                // limites de colisao em y
                float bird_top = bird.pos_y;
                float bird_bottom = bird.pos_y + BIRD_WIDTH;
                float gap_top = pipes[i].gap_y;
                float gap_bottom = pipes[i].gap_y + GAP_SIZE;

                if (bird_top <= gap_top || bird_bottom >= gap_bottom) {
                    game_over();
                    break;
                }
                // acresimo no placar
                if (px < 10 && !pipes[i].passed) {
                    pipes[i].passed = true;
                    points++;
                }
            }
            // colisao com o chao
            if (bird.pos_y >= HEIGHT - BIRD_WIDTH)
                game_over();

            // não ultrapassar os limites da tela
            if (bird.pos_y < 0) {
                bird.pos_y = 0;
                bird.vel_y = 0;
            }
            sleep_ms(50);
            break;
        // aguarda mas não reinicializa as variaveis
        case GAME_STATE_PAUSE:
            btn = gpio_get(BOTAO_B);
            if (!btn && last_btn_b){
                state = GAME_STATE_PLAYING;
                last_btn_a = gpio_get(BOTAO_A);    
                last_btn_b = gpio_get(BOTAO_B);
            }
            last_btn_b = btn;
            sleep_ms(10);
            break;
        }
    }
    return 0;
}

// programa do core1
static void render_task()
{
    // inicialização do display em paralelo com as inicializações do core0
    i2c_init(I2C_PORT, 400 * 1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // objeto display ssd1306 128x64
    ssd1306_t ssd;
    
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, END, I2C_PORT);
    ssd1306_config(&ssd);
    
    // limpar display
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    
    while(true) {
        switch (state) {
        case GAME_STATE_PLAYING:
            // renderiza todos os objetos
            draw_objects(&ssd);
            break;
        case GAME_STATE_MENU:
            menu_screen(&ssd);

            // dedicar o programa a esperar resposta do core0
            while (state == GAME_STATE_MENU)
                sleep_ms(20);
            
            // limpar tela
            clear_screen(&ssd);
            break;    
        case GAME_STATE_GAME_OVER:
            game_over_screen(&ssd);
            while (state == GAME_STATE_GAME_OVER)
                sleep_ms(20);
            clear_screen(&ssd);
            break;
            
        case GAME_STATE_PAUSE:
            paused_screen(&ssd);
            while (state == GAME_STATE_PAUSE)
                sleep_ms(20);
            clear_screen(&ssd);
        break;
        }
    }
}

static inline void init_config()
{
    // ativa o core1 para inicializar o display já em paralelo
    multicore_launch_core1(render_task);
    
    /* 
    * a função rand será utilizada para a criação
    * pseudo aleatoria das passagens pelos canos
    */
   srand(time_us_32());
   
   // botao
   gpio_init(BOTAO_A);
   gpio_set_dir(BOTAO_A, GPIO_IN);
   gpio_pull_up(BOTAO_A);
   
   gpio_init(BOTAO_B);
   gpio_set_dir(BOTAO_B, GPIO_IN);
   gpio_pull_up(BOTAO_B);
   
   // PWM dos buzzers
   gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
   gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);
   
   uint slice_a = pwm_gpio_to_slice_num(BUZZER_A);
   uint slice_b = pwm_gpio_to_slice_num(BUZZER_B);

    pwm_set_clkdiv(slice_a, 4.0f);
    pwm_set_clkdiv(slice_b, 4.0f);
    
    pwm_set_enabled(slice_a, true);
    pwm_set_enabled(slice_b, true);
    
    // tocar som de inicialização
    play_init_sound();
}

static inline void draw_objects(ssd1306_t *ssd)
{
    // placar
    snprintf((char *)pts, sizeof(pts), "%03u", points);
    
    /*
    * fazer uma cópia é importante para manter a consistencia
    * entre os frames, pois enquanto o core1 desenha o core0
    * pode alterar o valor desses objetos
    */ 
   Pipe pps[5];
   for (uint8_t i = 0; i < 5; i++){
       pps[i].pos_x = pipes[i].pos_x;
       pps[i].gap_y = pipes[i].gap_y;
    }
    int8_t p = (int)(bird.pos_y);
    
    //desenhar canos, placar e o passaro
    draw_pipes(ssd, pps, true);
    ssd1306_draw_string(ssd, (const char*)pts, 99, 3);
    draw_bird(ssd, p, true);
    
    // enviar os dados desenhados no buffer para o display
    ssd1306_send_data(ssd);
    
    // apagar tudo o que foi desenhado do buffer para o proximo frame
    ssd1306_rect(ssd, 2, 98, 26, 10, false, true);
    draw_bird(ssd, p, false);
    draw_pipes(ssd, pps, false);
}

//funcoes de auxilio para desenho
static inline void menu_screen(ssd1306_t *ssd){
    ssd1306_hline(ssd, 22, 101, 19, true);
    ssd1306_hline(ssd, 22, 101, 41, true);
    ssd1306_vline(ssd, 22, 19, 41, true);
    ssd1306_vline(ssd, 101, 19, 41, true);
    ssd1306_draw_string(ssd, "FLAPBIRD", 31, 23);
    ssd1306_draw_string(ssd, "press btn", 27, 31);
    ssd1306_send_data(ssd);
}

static inline void paused_screen(ssd1306_t *ssd){
    ssd1306_hline(ssd, 22, 101, 19, true);
    ssd1306_hline(ssd, 22, 101, 41, true);
    ssd1306_vline(ssd, 22, 19, 41, true);
    ssd1306_vline(ssd, 101, 19, 41, true);
    ssd1306_draw_string(ssd, "PAUSED", 39, 23);
    ssd1306_draw_string(ssd, "pres btn", 27, 31);
    ssd1306_send_data(ssd);
}

static inline void game_over_screen(ssd1306_t *ssd) {
    char str[11];
    
    snprintf(str, sizeof(str), "score %s", (const char*)pts);
    
    ssd1306_hline(ssd, 22, 101, 19, true);
    ssd1306_hline(ssd, 22, 101, 50, true);
    ssd1306_vline(ssd, 22, 19, 50, true);
    ssd1306_vline(ssd, 101, 19, 50, true);
    
    ssd1306_draw_string(ssd, "YOU LOST", 31, 23);
    ssd1306_draw_string(ssd, "try again", 27, 31);
    ssd1306_draw_string(ssd, (const char*)str, 27, 40);
    ssd1306_send_data(ssd);
}

static inline void clear_screen(ssd1306_t *ssd){
    ssd1306_fill(ssd, false);
    ssd1306_send_data(ssd);
}

static inline void draw_bird(ssd1306_t *ssd, int8_t p, bool value){
    ssd1306_vline(ssd, 12, p + 2, p + 5, value);
    ssd1306_vline(ssd, 13, p + 1, p + 6, value);
    ssd1306_vline(ssd, 14, p, p + 7, value);
    ssd1306_vline(ssd, 15, p, p + 7, value);
    ssd1306_vline(ssd, 16, p, p + 7, value);
    ssd1306_vline(ssd, 17, p + 1, p + 6, value);
    ssd1306_vline(ssd, 18, p + 2, p + 5, value);
}

static inline void draw_pipes(ssd1306_t *ssd, Pipe pps[5], bool value) {
    // loop que desenha os canos
    for (uint8_t i = 0; i < 5; i++) {
        
        /*
        * com base em px iremos tratar os casos:
        * cano fora da tela
        * cano nas bordas da tela
        * cano dentro da tela
        */ 
        int16_t px = pps[i].pos_x;
        
        // se estiver fora da tela não tenta desenha
        if (px >= 128 || px <= -5)
        continue;
    
        // parte de cima: (top1 = 0)
        uint8_t h1 = pps[i].gap_y;
       
        // parte de baixo:
        uint8_t top2 = pps[i].gap_y + GAP_SIZE;
        uint8_t h2 = 64 - top2;
        
        // ambas as partes possuem mesma largura
        uint8_t w = PIPE_WIDTH;
        
        
        // tratamento de redimensionamento no caso 
        // dos canos estarem nas bordas do display
        int8_t left;
        if (px >= 124) {
            w = 128 - px;
            left = (int8_t)px;
        } else if (px < 0){
            w += px;
            left = 0;
        } else {
            // o cano esta dentro da tela
            left = (int8_t)px;
        }
        
        ssd1306_rect(ssd, 0, left, w, h1, value, true);
        ssd1306_rect(ssd, top2, left, w, h2, value, true);
    }
}

// funcoes de inicio e fim de jogo
static inline void game_start()
{
    for (uint8_t i = 0; i < 5; i++) {
        pipes[i] = (Pipe){
            (int16_t)(127 + i * PIPE_DISTANCE), // começam fora da tela 
            (uint8_t)((rand() % (GAP_RANDOM)) + 3), // gap pseudo aleatorio
            false
        };
    }
    bird = (Bird){32.0f, 0.0f};
    state = GAME_STATE_PLAYING;
    points = 0;
    play_game_start_sound();
}

static inline void game_over()
{
    state = GAME_STATE_GAME_OVER;
    play_game_over_sound();
}

// função generica para emitir uma frequencia aos buzzers
static void play_buzzer(uint freq_a, uint freq_b, uint tempo_ms)
{
    static uint slice_a = UINT32_MAX;
    static uint slice_b = 0;
    static uint ch_a = 0;
    static uint ch_b = 0;

    if (freq_a == 0 || freq_b == 0){
        return;
    }

    if (slice_a == UINT32_MAX) {
        slice_a = pwm_gpio_to_slice_num(BUZZER_A);
        slice_b = pwm_gpio_to_slice_num(BUZZER_B);
        
        ch_a = pwm_gpio_to_channel(BUZZER_A);
        ch_b = pwm_gpio_to_channel(BUZZER_B);
    }
    
    // frequencia pwm
    uint32_t wrap_a = 125000000 / (4 * freq_a) - 1;
    uint32_t wrap_b = 125000000 / (4 * freq_b) - 1;

    // level / wrap = duty cycle
    pwm_set_wrap(slice_a, wrap_a);
    pwm_set_wrap(slice_b, wrap_b);
    
    pwm_set_chan_level(slice_a, ch_a, wrap_a / 2); // 50% duty cycle
    pwm_set_chan_level(slice_b, ch_b, wrap_b / 2);

    // problema de atenção!
    sleep_ms(tempo_ms);

    // desativar som
    pwm_set_chan_level(slice_a, ch_a, 0);
    pwm_set_chan_level(slice_b, ch_b, 0);

    sleep_ms(20);
}

// funcoes de som
static inline void play_init_sound()
{
    play_buzzer(900, 1200, 160);
    play_buzzer(950, 1200, 120);
    play_buzzer(1000, 1200, 80);
}

static inline void play_game_start_sound()
{
    play_buzzer(523, 659, 80);
    play_buzzer(659, 784, 80);
    play_buzzer(784, 1046, 150);
    play_buzzer(865, 1046, 200);
}

static inline void play_game_over_sound()
{
    play_buzzer(800, 600, 140);
    play_buzzer(700, 500, 110);
    play_buzzer(600, 500, 110);
    play_buzzer(500, 500, 180);
}