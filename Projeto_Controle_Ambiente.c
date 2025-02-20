#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"

#include "inc/ssd1306.h"
#include "inc/font.h"
#include "ws2812.pio.h"

// Configuração do display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ADDRESS 0x3C

// Configuração do PWM
#define WRAP_VALUE 4095
#define DIV_VALUE 1.0
#define RED_LED 13

// Configuração do joystick
#define JSK_SEL 22
#define JSK_Y 26
#define JSK_X 27

// Configuração dos botões
#define BUTTON_A 5
#define BUTTON_B 6

// Configuração dos buzzers
#define BUZZER_A 21
#define BUZZER_B 10

// Variáveis para as interrupções
static volatile uint32_t last_time = 0;
static volatile bool flag_b = false;

// Variáveis para o display
static volatile uint8_t screen_state = 0;
static volatile char string1[] = "T:000C*\0";;
static volatile char string2[] = "U:000%\0";
static volatile char string3[] = "fan:medium\0";
static volatile char string4[] = "humidifier:off\0";

// Variáveis de controle do display
static volatile int fan_low = 26, fan_medium = 30,fan_high = 32;
static volatile int humidifier_on = 60;
static volatile bool face_humidifier = false;
static volatile bool face_fan = false;

// Variáveis para o joystick
static volatile uint16_t x_high=4095, x_low=0, x_middle_high=2047, x_middle_low=2047;
static volatile uint16_t y_high=4095, y_low=0, y_middle_high=2047, y_middle_low=2047;
static volatile uint16_t x_value=2047, y_value=2047;
static volatile int x_scaled = 0, y_scaled = 0;

// ---------------- Inicializações - Início ----------------

void init_display(ssd1306_t *ssd) {
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(ssd, WIDTH, HEIGHT, false, ADDRESS, I2C_PORT);
    ssd1306_config(ssd);
    ssd1306_send_data(ssd);
    
    ssd1306_fill(ssd, false);
    ssd1306_send_data(ssd);
}

void init_joystick() {
    gpio_init(JSK_SEL);
    gpio_set_dir(JSK_SEL, GPIO_IN);
    gpio_pull_up(JSK_SEL);

    adc_init();
    adc_gpio_init(JSK_X);
    adc_gpio_init(JSK_Y);
}

void init_rgb() {
    uint slice;

    gpio_set_function(RED_LED, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(RED_LED);
    pwm_set_clkdiv(slice, DIV_VALUE);
    pwm_set_wrap(slice, WRAP_VALUE);
    pwm_set_gpio_level(RED_LED, 0);
    pwm_set_enabled(slice, true);
}

void init_buttons() {
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
}

void init_buzzers() {
    uint slice;
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_A);
    pwm_set_clkdiv(BUZZER_A, 125);
    pwm_set_wrap(BUZZER_A, 3822);
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_enabled(slice, true);
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_B);
    pwm_set_clkdiv(BUZZER_B, 125);
    pwm_set_wrap(BUZZER_B, 2024);
    pwm_set_gpio_level(BUZZER_B, 0);
    pwm_set_enabled(slice, true);
}


// ---------------- Inicializações - Fim ----------------

// Desenhos das caras no display
void draw_happy(ssd1306_t *ssd,uint8_t x0,uint8_t y0) {
    uint8_t max_y = y0+22;
    uint8_t max_x = x0+22;
    uint8_t face[22][22] = {

        {0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0},
        {0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
        {0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0},
        {0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0},
        {1,1,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,1,1},
        {0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0},
        {0,1,1,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,1,1,0},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
        {0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0},
        {0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0},
        {0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0}
    };

    for (uint8_t y = y0; y < max_y; y++) {
        for (uint8_t x = x0; x < max_x; x++) {
            ssd1306_pixel(ssd, x, y, face[y-y0][x-x0]);
        }
    }
}
void draw_neutral(ssd1306_t *ssd,uint8_t x0,uint8_t y0) {
    uint8_t max_y = y0+22;
    uint8_t max_x = x0+22;
    uint8_t face[22][22] = {

        {0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0},
        {0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
        {0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0},
        {0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0},
        {1,1,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {0,1,1,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,1,1,0},
        {0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
        {0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0},
        {0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0},
        {0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0}
    };

    for (uint8_t y = y0; y < max_y; y++) {
        for (uint8_t x = x0; x < max_x; x++) {
            ssd1306_pixel(ssd, x, y, face[y-y0][x-x0]);
        }
    }
}
void draw_sad(ssd1306_t *ssd,uint8_t x0,uint8_t y0) {
    uint8_t max_y = y0+22;
    uint8_t max_x = x0+22;
    uint8_t face[22][22] = {

        {0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0},
        {0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
        {0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0},
        {0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0},
        {1,1,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
        {1,1,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,1,1},
        {0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0},
        {0,1,1,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,1,1,0},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0},
        {0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0},
        {0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0},
        {0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0}
    };

    for (uint8_t y = y0; y < max_y; y++) {
        for (uint8_t x = x0; x < max_x; x++) {
            ssd1306_pixel(ssd, x, y, face[y-y0][x-x0]);
        }
    }
}

// Leitura do joystick
uint16_t read_y() {
    adc_select_input(0);
    return adc_read();
}
uint16_t read_x() {
    adc_select_input(1);
    return adc_read();
}
int scale(int min1, int max1, int min2, int max2,int x1) {
    return ( (((x1-min1)*(max2-min2))/(max1-min1))+min2 );
}

void beep(uint tempo) {
    pwm_set_gpio_level(BUZZER_A, 1911);
    sleep_ms(tempo/4);
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_gpio_level(BUZZER_B, 1012);
    sleep_ms(tempo/4);
    pwm_set_gpio_level(BUZZER_B, 0);
    pwm_set_gpio_level(BUZZER_A, 1911);
    sleep_ms(tempo/4);
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_gpio_level(BUZZER_B, 1012);
    sleep_ms(tempo/4);
    pwm_set_gpio_level(BUZZER_B, 0);
}

void gpio_irq_callback(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if( (current_time - last_time) > 200 ) {
        last_time = current_time;
        if(gpio == BUTTON_B) {
            flag_b = true;
        }
    }
}

void tela_inicial(ssd1306_t *ssd) {
    y_value = read_y();
    x_value = read_x();

    y_scaled = scale(y_low,y_high,-15,50,y_value);
    x_scaled = scale(x_low,x_high,0,100,x_value);
    if(y_scaled > 50) {
        y_scaled = 50;
    }else
    if(y_scaled < -15) {
        y_scaled = -15;
    }
    if(x_scaled > 100) {
        x_scaled = 100;
    }else
    if(x_scaled < 0) {
        x_scaled = 0;
    }

    sprintf((char *)&string1[2],"%3dC*\0",y_scaled);
    sprintf((char *)&string2[2],"%3d%%\0",x_scaled);

    if(y_scaled < fan_low) {
        sprintf((char *)&string3[4],"off   \0");
        pwm_set_gpio_level(RED_LED,0);
        face_fan = true;
    }else
    if(y_scaled < fan_medium) {
        sprintf((char *)&string3[4],"low   \0");
        pwm_set_gpio_level(RED_LED,1365);
        face_fan = true;
    }else
    if(y_scaled < fan_high) {
        sprintf((char *)&string3[4],"medium\0");
        pwm_set_gpio_level(RED_LED,2730);
        face_fan = true;
    }else {
        sprintf((char *)&string3[4],"high  \0");
        pwm_set_gpio_level(RED_LED,4095);
        face_fan = false;
    }

    if(x_scaled > humidifier_on) {
        sprintf((char *)&string4[12],"ff\0");
        face_humidifier = true;
    }else {
        sprintf((char *)&string4[12],"n \0");
        face_humidifier = false;
    }

    if(face_fan && face_humidifier) {
        draw_happy(ssd,84,6);
    }else
    if(!face_fan && !face_humidifier) {
        draw_sad(ssd,84,6);
    }else {
        draw_neutral(ssd,84,6);
    }

    ssd1306_rect(ssd, 0, 0, 128, 64, true, false);
    ssd1306_rect(ssd, 2, 2, 124, 60, true, false);
    ssd1306_vline(ssd,63,3,30,true);
    ssd1306_vline(ssd,64,3,30,true);
    ssd1306_hline(ssd,3,124,31,true);
    ssd1306_hline(ssd,3,124,32,true);
    ssd1306_draw_string(ssd,(char *)string1,6,7);
    ssd1306_draw_string(ssd,(char *)string2,6,20);
    ssd1306_draw_string(ssd,(char *)string3,6,37);
    ssd1306_draw_string(ssd,(char *)string4,6,50);

    ssd1306_send_data(ssd);
}

void selecionar_temperatura() {
    
}

void selecionar_umidade() {
    
}

void calibrar_joystick() {
    
}

int main() {
    ssd1306_t ssd;

    stdio_init_all();
    init_display(&ssd);
    init_joystick();
    init_rgb();
    init_buttons();
    init_buzzers();

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);

    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_callback);

    while (true) {
        sleep_ms(10);
        switch(screen_state) {
            case 0:
                tela_inicial(&ssd);
                break;
            case 1:
                selecionar_temperatura();
                break;
            case 2:
                selecionar_umidade();
                break;
            case 3:
                calibrar_joystick();
                break;
            default:
                screen_state = 0;
        }

        if(flag_b) {
            beep(400);
            flag_b = false;
        }
    }
}
