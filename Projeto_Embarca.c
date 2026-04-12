#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#define I2C_PORT i2c1
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15
#define I2C_BAUDRATE 400000
#define SSD1306_ADDR 0x3C

#define JOY_X_ADC_CH 0
#define JOY_Y_ADC_CH 1
#define JOY_X_PIN 26
#define JOY_Y_PIN 27

#define OLED_W 128
#define OLED_H 64
#define CELL_SIZE 4

#define GRID_W (OLED_W / CELL_SIZE)
#define GRID_H (OLED_H / CELL_SIZE)
#define SNAKE_MAX (GRID_W * GRID_H)

#define JOY_CENTER 2048
#define JOY_DEADZONE 700
#define JOY_SAMPLE_MS 25
#define GAME_TICK_MS 140

typedef struct {
    uint8_t x;
    uint8_t y;
} Point;

// Enum das possiveis direções da cobra.
typedef enum {
    DIR_UP = 0,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} Direction;
//Enum do input do joystick
typedef enum {
    EVT_NONE = 0,
    EVT_UP,
    EVT_RIGHT,
    EVT_DOWN,
    EVT_LEFT
} InputEvent;

//estado global do jogo
typedef struct {
    Point body[SNAKE_MAX];
    uint16_t length;
    Direction dir_current;
    Direction dir_next;
    Point food;
    bool game_over;
    uint32_t score;
} GameState;

 GameState game;
 uint8_t oled_buffer[OLED_W * OLED_H / 8];

//Maquina de estado pra impedir a cobra de se churrascar a linha é a direção atual da cobra e a coluna é o input do joystick
 const Direction dir_fsm[4][5] = {
    {DIR_UP,    DIR_UP,    DIR_RIGHT,    DIR_UP,     DIR_LEFT},
    {DIR_RIGHT, DIR_UP,    DIR_RIGHT,    DIR_DOWN,   DIR_RIGHT},
    {DIR_DOWN,  DIR_DOWN,  DIR_RIGHT,    DIR_DOWN,   DIR_LEFT},
    {DIR_LEFT,  DIR_UP,    DIR_LEFT,     DIR_DOWN,   DIR_LEFT},
};

 void ssd1306_cmd(uint8_t cmd) {
    uint8_t packet[2] = {0x00, cmd};
    i2c_write_blocking(I2C_PORT, SSD1306_ADDR, packet, 2, false);
}

 void ssd1306_init(void) {
    sleep_ms(100);
    ssd1306_cmd(0xAE);
    ssd1306_cmd(0x20);
    ssd1306_cmd(0x00);
    ssd1306_cmd(0xB0);
    ssd1306_cmd(0xC8);
    ssd1306_cmd(0x00);
    ssd1306_cmd(0x10);
    ssd1306_cmd(0x40);
    ssd1306_cmd(0x81);
    ssd1306_cmd(0x7F);
    ssd1306_cmd(0xA1);
    ssd1306_cmd(0xA6);
    ssd1306_cmd(0xA8);
    ssd1306_cmd(0x3F);
    ssd1306_cmd(0xA4);
    ssd1306_cmd(0xD3);
    ssd1306_cmd(0x00);
    ssd1306_cmd(0xD5);
    ssd1306_cmd(0x80);
    ssd1306_cmd(0xD9);
    ssd1306_cmd(0xF1);
    ssd1306_cmd(0xDA);
    ssd1306_cmd(0x12);
    ssd1306_cmd(0xDB);
    ssd1306_cmd(0x40);
    ssd1306_cmd(0x8D);
    ssd1306_cmd(0x14);
    ssd1306_cmd(0xAF);
}

//Todo o buffer daa ram para a tela por meio do i2c, envia blocos de 16 bytes pra não estourar o pobre do 12c
 void ssd1306_show(void) {
    uint8_t packet[17];
    packet[0] = 0x40;

    for (uint8_t page = 0; page < 8; page++) {
        ssd1306_cmd(0xB0 + page);
        ssd1306_cmd(0x00);
        ssd1306_cmd(0x10);

        uint16_t base = page * OLED_W;
        for (uint8_t col = 0; col < OLED_W; col += 16) {
            memcpy(&packet[1], &oled_buffer[base + col], 16);
            i2c_write_blocking(I2C_PORT, SSD1306_ADDR, packet, sizeof(packet), false);
        }
    }
}

//zera a tela
 void oled_clear(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

//acende ou apaga um pixel manipulando bits individuais do buffer da tela
 void oled_set_pixel(uint8_t x, uint8_t y, bool on) {
    if (x >= OLED_W || y >= OLED_H) {
        return;
    }

    uint16_t index = x + (y / 8) * OLED_W;
    uint8_t mask = 1u << (y % 8);
    if (on) {
        oled_buffer[index] |= mask;
    } else {
        oled_buffer[index] &= (uint8_t)(~mask);
    }
}

//retangulo pra desenhar a cobra e a comida
 void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on) {
    for (uint8_t yy = 0; yy < h; yy++) {
        for (uint8_t xx = 0; xx < w; xx++) {
            oled_set_pixel((uint8_t)(x + xx), (uint8_t)(y + yy), on);
        }
    }
}
//verifica se dois pontos são iguais
 bool point_equals(Point a, Point b) {
    return (a.x == b.x) && (a.y == b.y);
}

// se a cobra andar a cauda vai sair do lugar, então tem que ignorar a cauda pra não colidir com ela propria
 bool snake_contains(Point p, bool ignore_tail) {
    uint16_t limit = game.length;
    if (ignore_tail && limit > 0) {
        limit -= 1;
    }

    for (uint16_t i = 0; i < limit; i++) {
        if (point_equals(game.body[i], p)) {
            return true;
        }
    }
    return false;
}
//Gera a comida que não esteja dentro da cobbra
 void spawn_food(void) {
    Point p;
    do {
        p.x = (uint8_t)(rand() % GRID_W);
        p.y = (uint8_t)(rand() % GRID_H);
    } while (snake_contains(p, false));

    game.food = p;
}

//Prepara um novo jogo posiciona a cobra no meio da tela e gera a comida
 void reset_game(void) {
    memset(&game, 0, sizeof(game));
    game.length = 3;

    uint8_t cx = GRID_W / 2;
    uint8_t cy = GRID_H / 2;

    game.body[0] = (Point){cx, cy};
    game.body[1] = (Point){(uint8_t)(cx - 1), cy};
    game.body[2] = (Point){(uint8_t)(cx - 2), cy};

    game.dir_current = DIR_RIGHT;
    game.dir_next = DIR_RIGHT;
    game.game_over = false;
    game.score = 0;

    spawn_food();
}

// Le os valores e transforma nos inputs do joystick
 InputEvent read_joystick_event(void) {
    adc_select_input(JOY_X_ADC_CH);
    uint16_t x = adc_read();

    adc_select_input(JOY_Y_ADC_CH);
    uint16_t y = adc_read();

    // bitdoglab com eixo trocados pq o meu joystick veio meio bugado então tive que inverter os eixos. Direita subia e esquerdda descia 
    if (x > (JOY_CENTER + JOY_DEADZONE)) {
        return EVT_UP;
    }
    if (x < (JOY_CENTER - JOY_DEADZONE)) {
        return EVT_DOWN;
    }
    if (y > (JOY_CENTER + JOY_DEADZONE)) {
        return EVT_RIGHT;
    }
    if (y < (JOY_CENTER - JOY_DEADZONE)) {
        return EVT_LEFT;
    }
    return EVT_NONE;
}

// calcula onde a cabeça vai aumentar com base na direção atual
 Point next_head(Point head, Direction dir) {
    Point next = head;
    switch (dir) {
        case DIR_UP:
            if (next.y > 0) { //subir diminui o y
                next.y--;
            } else {
                next.y = (uint8_t)255;
            }
            break;
        case DIR_RIGHT:
            next.x++; //direitaa aumenta o x
            break;
        case DIR_DOWN:
            next.y++; //baixo aumenta o y
            break;
        case DIR_LEFT: // Esquerda diminui o x
            if (next.x > 0) {
                next.x--;
            } else {
                next.x = (uint8_t)255;
            }
            break;
        default:
            break;
    }
    return next;
}

 void game_tick(void) {
    if (game.game_over) {
        return;
    }

    game.dir_current = game.dir_next;

    Point new_head = next_head(game.body[0], game.dir_current);
// Colisão com as paredes
    if (new_head.x >= GRID_W || new_head.y >= GRID_H) {
        game.game_over = true;
        return;
    }

    bool will_grow = point_equals(new_head, game.food);

    // Colisão com o proprio corpo
    if (snake_contains(new_head, !will_grow)) {
        game.game_over = true;
        return;
    }
    // Atualiza o tamanho e pontuação da cobra

    if (will_grow && game.length < SNAKE_MAX) {
        game.length++;
        game.score++;
    }
    // Move o corpo copia a posição de tras pra frente

    for (int i = (int)game.length - 1; i > 0; i--) {
        game.body[i] = game.body[i - 1];
    }
    game.body[0] = new_head;

    if (will_grow) {
        spawn_food();
    }
}

// 3x5 para mensagem curta no game over
 void draw_glyph_3x5(uint8_t x, uint8_t y, const uint8_t rows[5]) {
    for (uint8_t ry = 0; ry < 5; ry++) {
        for (uint8_t rx = 0; rx < 3; rx++) {
            bool on = ((rows[ry] >> (2 - rx)) & 0x1u) != 0;
            if (on) {
                oled_set_pixel((uint8_t)(x + rx), (uint8_t)(y + ry), true);
            }
        }
    }
}

 void draw_char_3x5(uint8_t x, uint8_t y, char c) {
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    static const uint8_t g_s[5] = {0x7, 0x4, 0x7, 0x1, 0x7};
    static const uint8_t g_c[5] = {0x7, 0x4, 0x4, 0x4, 0x7};
    static const uint8_t g_o[5] = {0x7, 0x5, 0x5, 0x5, 0x7};
    static const uint8_t g_r[5] = {0x6, 0x5, 0x6, 0x5, 0x5};
    static const uint8_t g_e[5] = {0x7, 0x4, 0x7, 0x4, 0x7};
    static const uint8_t g_0[5] = {0x7, 0x5, 0x5, 0x5, 0x7};
    static const uint8_t g_1[5] = {0x2, 0x6, 0x2, 0x2, 0x7};
    static const uint8_t g_2[5] = {0x7, 0x1, 0x7, 0x4, 0x7};
    static const uint8_t g_3[5] = {0x7, 0x1, 0x7, 0x1, 0x7};
    static const uint8_t g_4[5] = {0x5, 0x5, 0x7, 0x1, 0x1};
    static const uint8_t g_5[5] = {0x7, 0x4, 0x7, 0x1, 0x7};
    static const uint8_t g_6[5] = {0x7, 0x4, 0x7, 0x5, 0x7};
    static const uint8_t g_7[5] = {0x7, 0x1, 0x1, 0x1, 0x1};
    static const uint8_t g_8[5] = {0x7, 0x5, 0x7, 0x5, 0x7};
    static const uint8_t g_9[5] = {0x7, 0x5, 0x7, 0x1, 0x7};
    static const uint8_t colon[5] = {0x0, 0x2, 0x0, 0x2, 0x0};

    const uint8_t *glyph = blank;
    switch (c) {
        case 'S': glyph = g_s; break;
        case 'C': glyph = g_c; break;
        case 'O': glyph = g_o; break;
        case 'R': glyph = g_r; break;
        case 'E': glyph = g_e; break;
        case '0': glyph = g_0; break;
        case '1': glyph = g_1; break;
        case '2': glyph = g_2; break;
        case '3': glyph = g_3; break;
        case '4': glyph = g_4; break;
        case '5': glyph = g_5; break;
        case '6': glyph = g_6; break;
        case '7': glyph = g_7; break;
        case '8': glyph = g_8; break;
        case '9': glyph = g_9; break;
        case ':': glyph = colon; break;
        default: break;
    }
    draw_glyph_3x5(x, y, glyph);
}

 void draw_text_3x5(uint8_t x, uint8_t y, const char *text) {
    while (*text) {
        draw_char_3x5(x, y, *text++);
        x = (uint8_t)(x + 4);
    }
}

 void draw_score_value(uint8_t x, uint8_t y, uint32_t value) {
    char buf[11];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
    draw_text_3x5(x, y, buf);
}

// Desenha a comida e a cobra na tela e desenha a borda de game over se o jogo tiver acabado
 void draw_cell(Point p) {
    oled_fill_rect((uint8_t)(p.x * CELL_SIZE), (uint8_t)(p.y * CELL_SIZE), CELL_SIZE, CELL_SIZE, true);
}

 void render_game(void) {
    oled_clear();

    draw_cell(game.food);
    for (uint16_t i = 0; i < game.length; i++) {
        draw_cell(game.body[i]);
    }

    if (game.game_over) {
        oled_fill_rect(0, 0, OLED_W, 1, true);
        oled_fill_rect(0, OLED_H - 1, OLED_W, 1, true);
        oled_fill_rect(0, 0, 1, OLED_H, true);
        oled_fill_rect(OLED_W - 1, 0, 1, OLED_H, true);

        oled_fill_rect(30, 25, 68, 14, false);
        draw_text_3x5(34, 28, "SCORE:");
        draw_score_value(62, 28, game.score);
    }

    ssd1306_show();
}

 void setup_peripherals(void) {
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    adc_init();
    adc_gpio_init(JOY_X_PIN);
    adc_gpio_init(JOY_Y_PIN);

    ssd1306_init();
}

int main(void) {
    stdio_init_all();
    setup_peripherals();
    //Pinos i2c
    srand((unsigned int)to_us_since_boot(get_absolute_time()));
    reset_game();

    // Pinos analogicos
    absolute_time_t next_tick = delayed_by_ms(get_absolute_time(), GAME_TICK_MS);
    absolute_time_t next_input = delayed_by_ms(get_absolute_time(), JOY_SAMPLE_MS);

    while (true) {
        // Le o joystick a cada 25ms
        if (absolute_time_diff_us(get_absolute_time(), next_input) <= 0) {
            InputEvent evt = read_joystick_event();
            game.dir_next = dir_fsm[game.dir_current][evt];
            next_input = delayed_by_ms(next_input, JOY_SAMPLE_MS);
        }

        //Atualiza o jogo e tela
        if (absolute_time_diff_us(get_absolute_time(), next_tick) <= 0) {
            game_tick();
            render_game();
            next_tick = delayed_by_ms(next_tick, GAME_TICK_MS);
        }

        sleep_ms(2);
    }
}
