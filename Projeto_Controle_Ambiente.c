// ---------------- Bibliotecas - Início ----------------

// Biblioteca padrão de entrada e saída do C (Foi usada para debugging)
#include <stdio.h>

// Bibliotecas do pico SDK de mais alto nível
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// Bibliotecas do pico SDK de hardware
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"

#include "inc/ssd1306.h" // Header para controle do display OLED
#include "inc/font.h"    // Header com as fontes para o display

#include "ws2812.pio.h"  // Header para controle dos LEDs WS2812

// ---------------- Bibliotecas - Fim ----------------



// ---------------- Definições - Início ----------------

// Configurações do I2C para comunicação com o display OLED
#define I2C_PORT i2c1 // Porta I2C
#define I2C_SDA 14    // Pino de dados
#define I2C_SCL 15    // Pino de clock
#define ADDRESS 0x3C  // Endereço do display

// Definições da matriz de LEDs
#define LED_COUNT 25  // Número total de LEDs na matriz
#define MATRIX_PIN 7  // Pino da matriz de LEDs
struct pixel_t {   // Estrutura para armazenar as cores de um LED WS2812
  uint8_t G, R, B; // Componentes de cor (verde, vermelho e azul)
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;
npLED_t leds[LED_COUNT];
PIO np_pio; // Instância do PIO
uint sm;    // State machine para controle dos LEDs

//  Configurações do PWM para os LEDs
#define WRAP_VALUE 4095 // Valor do WRAP
#define DIV_VALUE 1.0   // Valor do divisor de clock
#define RED_LED 13      // Pino do LED vermelho
#define BLUE_LED 12     // Pino do LED azul

// Configuração do joystick
#define JSK_SEL 22 // Pino do botão do joystick
#define JSK_Y 26   // Pino do eixo Y do joystick
#define JSK_X 27   // Pino do eixo X do joystick

// Configuração dos botões
#define BUTTON_A 5 // Pino do botão A
#define BUTTON_B 6 // Pino do botão B

// Configuração dos buzzers
#define BUZZER_A 21 // Pino do buzzer A
#define BUZZER_B 10 // Pino do buzzer B

// ---------------- Definições - Fim ----------------



// ---------------- Variáveis - Início ----------------

// Variáveis para controle de interrupções
static volatile uint32_t last_time = 0; // Armazena o último tempo registrado nas interrupções dos botões
static volatile bool flag_b = false;    // Flag de controle para o botão B
static volatile bool switch_b = true;   // Estado do botão B (Representa o sinal que está sendo recebido do sensor de nível do umidificador)

// Variáveis para o display
static volatile uint8_t screen_state = 0;            // Estado atual da tela
static volatile char string1[] = "T:000C*\0";;       // String da temperatura
static volatile char string2[] = "U:000%\0";         // String da umidade
static volatile char string3[] = "fan:medium\0";     // String do ventilador
static volatile char string4[] = "humidifier:off\0"; // String do umidificador

// Variáveis de controle do display
static volatile int fan_low = 26;             // Limite para ativar a velocidade mínima do ventilador
static volatile int fan_medium = 30;          // Limite para ativar a velocidade média do ventilador
static volatile int fan_high = 34;            // Limite para ativar a velocidade máxima do ventilador
static volatile int humidifier_on = 60;       // Limite para ativação do umidificador
static volatile bool face_humidifier = false; // Flag do umidificador para controlar qual rosto deve aparecer no display
static volatile bool face_fan = false;        // Flag do ventilador para controlar qual rosto deve aparecer no display
static volatile bool change_screen = false;   // Flag para indicar se o desenho na matriz de LEDs deve ser alterada

// Variáveis para o joystick
static volatile uint16_t y_high=4095, y_low=0, y_middle_high=2047, y_middle_low=2047; // Limites do eixo Y (Calibração)
static volatile uint16_t x_high=4095, x_low=0, x_middle_high=2047, x_middle_low=2047; // Limites do eixo X (Calibração)
static volatile uint16_t x_value=2047, y_value=2047; // Valores capturados pelo joystick
static volatile int x_scaled = 0, y_scaled = 0;      // Valores do joystick convertidos para valores de temperatura e umidade

// Variáveis de contrle para as telas
static volatile uint32_t t_last_time = 0; // Último tempo registrado para debouncing
static volatile uint8_t contador = 0;     // Contador para alterar os limites de velocidade do ventilador

// ---------------- Variáveis - Fim ----------------



// ---------------- Inicializações - Início ----------------

// Inicializa o display OLED via I2C
void init_display(ssd1306_t *ssd) {
    i2c_init(I2C_PORT, 400 * 1000); // Inicializa o I2C com frequência de 400 kHz

    // Configura os pinos SDA e SCL como I2C e habilita pull-ups
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa e configura o display
    ssd1306_init(ssd, WIDTH, HEIGHT, false, ADDRESS, I2C_PORT);
    ssd1306_config(ssd);
    ssd1306_send_data(ssd);
    
    // Limpa o display
    ssd1306_fill(ssd, false);
    ssd1306_send_data(ssd);
}

// Inicializa o LED RGB com PWM
void init_rgb() {
    uint slice;

    // Configura o pino do LED vermelho como saída PWM
    gpio_set_function(RED_LED, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(RED_LED);
    pwm_set_clkdiv(slice, DIV_VALUE);
    pwm_set_wrap(slice, WRAP_VALUE);
    pwm_set_gpio_level(RED_LED, 0);
    pwm_set_enabled(slice, true);
    // Configura o pino do LED azul como saída PWM
    gpio_set_function(BLUE_LED, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BLUE_LED);
    pwm_set_clkdiv(slice, DIV_VALUE);
    pwm_set_wrap(slice, WRAP_VALUE);
    pwm_set_gpio_level(BLUE_LED, 0);
    pwm_set_enabled(slice, true);
}

// Inicializa os buzzers com PWM
void init_buzzers() {
    uint slice;

    // Configuração do Buzzer A
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_A);
    pwm_set_clkdiv(BUZZER_A, 125);
    pwm_set_wrap(BUZZER_A, 3822);
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_enabled(slice, true);

    // Configuração do Buzzer B
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_B);
    pwm_set_clkdiv(BUZZER_B, 125);
    pwm_set_wrap(BUZZER_B, 2024);
    pwm_set_gpio_level(BUZZER_B, 0);
    pwm_set_enabled(slice, true);
}

// Inicializa o joystick
void init_joystick() {

    // Inicializa o botão do joystick
    gpio_init(JSK_SEL);
    gpio_set_dir(JSK_SEL, GPIO_IN);
    gpio_pull_up(JSK_SEL);

    // Inicializa o ADC do eixo Y e X e joystick
    adc_init();
    adc_gpio_init(JSK_Y);
    adc_gpio_init(JSK_X);
}

// Inicializa os botões A e B
void init_buttons() {
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
}

// -------- Matriz - Início --------

// Inicializa a máquina PIO para controle da matriz de LEDs
void npInit(uint pin) {

    // Carrega o programa PIO para controle dos LEDs
    uint offset = pio_add_program(pio0, &ws2812_program);
    np_pio = pio0;

    // Obtém uma máquina de estado PIO disponível
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }

    // Inicializa a máquina de estado com o WS2812.pio
    ws2812_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa o buffer de pixels
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

// Atribui uma cor RGB a um LED específico na matriz
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

// Limpa todos os LEDs na matriz
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        npSetLED(i, 0, 0, 0);
    }
}

// Escreve os dados do buffer para os LEDs
void npWrite() {
    // Escreve cada dado de 8 bits dos pixels em sequência no buffer da máquina PIO
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us para o reset
}

// Função para facilitar o desenho na matriz utilizando 3 matrizes cos os valores RGB
void npDraw(uint8_t vetorR[5][5], uint8_t vetorG[5][5], uint8_t vetorB[5][5]) {
  int i, j,idx,col;
    for (i = 0; i < 5; i++) {
        idx = (4 - i) * 5; // Calcula o índice base para a linha
        for (j = 0; j < 5; j++) {
            col = (i % 2 == 0) ? (4 - j) : j; // Inverte a ordem das colunas nas linhas pares
            npSetLED(idx + col, vetorR[i][j], vetorG[i][j], vetorB[i][j]); // Preenche o buffer com os valores das matrizes
        }
    }
}

// -------- Matriz - Fim --------

// ---------------- Inicializações - Fim ----------------



// ---------------- Desenhos - Início ----------------

// -------- Display - Início --------

// Função para desenhar uma cara feliz no display
void draw_happy(ssd1306_t *ssd,uint8_t x0,uint8_t y0) {
    uint8_t max_y = y0+22;
    uint8_t max_x = x0+22;
    uint8_t face[22][22] = { // Matriz que representa a cara feliz (1 = pixel aceso, 0 = pixel apagado)
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
    for (uint8_t y = y0; y < max_y; y++) { // Percorre a matriz e desenha os pixels correspondentes no display
        for (uint8_t x = x0; x < max_x; x++) {
            ssd1306_pixel(ssd, x, y, face[y-y0][x-x0]);
        }
    }
}

// Função para desenhar uma cara neutra no display
void draw_neutral(ssd1306_t *ssd,uint8_t x0,uint8_t y0) {
    uint8_t max_y = y0+22;
    uint8_t max_x = x0+22;
    uint8_t face[22][22] = { // Matriz que representa a cara neutra (1 = pixel aceso, 0 = pixel apagado)
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
    for (uint8_t y = y0; y < max_y; y++) { // Percorre a matriz e desenha os pixels correspondentes no display
        for (uint8_t x = x0; x < max_x; x++) {
            ssd1306_pixel(ssd, x, y, face[y-y0][x-x0]);
        }
    }
}

// Função para desenhar uma cara triste no display
void draw_sad(ssd1306_t *ssd,uint8_t x0,uint8_t y0) {
    uint8_t max_y = y0+22;
    uint8_t max_x = x0+22;
    uint8_t face[22][22] = { // Matriz que representa a face triste (1 = pixel aceso, 0 = pixel apagado)
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
    for (uint8_t y = y0; y < max_y; y++) { // Percorre a matriz e desenha os pixels correspondentes no display
        for (uint8_t x = x0; x < max_x; x++) {
            ssd1306_pixel(ssd, x, y, face[y-y0][x-x0]);
        }
    }
}

// -------- Display - Fim --------

// -------- Matriz - Início --------

// Função para exibir na matriz de LEDs o símbolo indicando que a água do umidificador está acabando
void humidifier_matrix() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorR[5][5] = {
        {  1  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  1  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  1  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  1  }
    };
      uint8_t vetorG[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
    uint8_t vetorB[5][5] = {
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  0  ,  1  ,  1  ,  0  },
        {  1  ,  1  ,  0  ,  1  ,  1  },
        {  1  ,  1  ,  1  ,  0  ,  1  },
        {  0  ,  1  ,  1  ,  1  ,  0  }
    };
    npDraw(vetorR,vetorG,vetorB); // Carrega os buffers
    npWrite();                    // Escreve na matriz de LEDs
    npClear();                    // Limpa os buffers (não necessário, mas por garantia)
}

// Função para exibir o símbolo indicando a tela de mudança de temperaturas do ventilador na matriz de LEDs
void temperature_screen() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorR[5][5] = {
        {  1  ,  1  ,  1  ,  1  ,  1  },
        {  1  ,  0  ,  1  ,  0  ,  1  },
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  1  ,  1  ,  1  ,  0  }
    };
      uint8_t vetorGB[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
    npDraw(vetorR,vetorGB,vetorGB); // Carrega os buffers
    npWrite();                      // Escreve na matriz de LEDs
    npClear();                      // Limpa os buffers (não necessário, mas por garantia)
}

// Função para exibir o símbolo indicando a tela de mudança de umidade mínima do umidificador na matriz de LEDs
void humidifier_screen() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorRG[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
      uint8_t vetorB[5][5] = {
        {  1  ,  0  ,  0  ,  0  ,  1  },
        {  1  ,  0  ,  0  ,  0  ,  1  },
        {  1  ,  0  ,  0  ,  0  ,  1  },
        {  1  ,  0  ,  0  ,  0  ,  1  },
        {  1  ,  1  ,  1  ,  1  ,  1  }
    };
    npDraw(vetorRG,vetorRG,vetorB); // Carrega os buffers
    npWrite();                      // Escreve na matriz de LEDs
    npClear();                      // Limpa os buffers (não necessário, mas por garantia)
}

// Função para exibir o símbolo indicando a tela de calibração do joystick ("sensores") na matriz de LEDs
void calibration_screen() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorB[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
      uint8_t vetorRG[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  1  ,  1  ,  1  ,  0  },
        {  0  ,  1  ,  0  ,  1  ,  0  },
        {  0  ,  1  ,  1  ,  1  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
    npDraw(vetorRG,vetorRG,vetorB); // Carrega os buffers
    npWrite();                      // Escreve na matriz de LEDs
    npClear();                      // Limpa os buffers (não necessário, mas por garantia)
}

// Função para exibir o símbolo de seta para cima na matriz de LEDs para a calibração
void seta_cima() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorG[5][5] = {
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  1  ,  1  ,  1  ,  0  },
        {  1  ,  0  ,  1  ,  0  ,  1  },
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  0  ,  1  ,  0  ,  0  }
    };
      uint8_t vetorRB[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
    npDraw(vetorRB,vetorG,vetorRB); // Carrega os buffers
    npWrite();                      // Escreve na matriz de LEDs
    npClear();                      // Limpa os buffers (não necessário, mas por garantia)
}

// Função para exibir o símbolo de seta para baixo na matriz de LEDs para a calibração
void seta_baixo() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorG[5][5] = {
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  1  ,  0  ,  1  ,  0  ,  1  },
        {  0  ,  1  ,  1  ,  1  ,  0  },
        {  0  ,  0  ,  1  ,  0  ,  0  }
    };
      uint8_t vetorRB[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
    npDraw(vetorRB,vetorG,vetorRB); // Carrega os buffers
    npWrite();                      // Escreve na matriz de LEDs
    npClear();                      // Limpa os buffers (não necessário, mas por garantia)
}

// Função para exibir o símbolo de seta para a esquerda na matriz de LEDs para a calibração
void seta_esquerda() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorG[5][5] = {
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  1  ,  0  ,  0  ,  0  },
        {  1  ,  1  ,  1  ,  1  ,  1  },
        {  0  ,  1  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  1  ,  0  ,  0  }
    };
      uint8_t vetorRB[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
    npDraw(vetorRB,vetorG,vetorRB); // Carrega os buffers
    npWrite();                      // Escreve na matriz de LEDs
    npClear();                      // Limpa os buffers (não necessário, mas por garantia)
}

// Função para exibir o símbolo de seta para a direita na matriz de LEDs para a calibração
void seta_direita() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorG[5][5] = {
        {  0  ,  0  ,  1  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  1  ,  0  },
        {  1  ,  1  ,  1  ,  1  ,  1  },
        {  0  ,  0  ,  0  ,  1  ,  0  },
        {  0  ,  0  ,  1  ,  0  ,  0  }
    };
      uint8_t vetorRB[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
    npDraw(vetorRB,vetorG,vetorRB); // Carrega os buffers
    npWrite();                      // Escreve na matriz de LEDs
    npClear();                      // Limpa os buffers (não necessário, mas por garantia)
}

// Função para exibir o símbolo que representa o meio na matriz de LEDs para a calibração
void meio() {
    // Matrizes que representam os LEDs vermelhos, verdes e azuis
    uint8_t vetorG[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  1  ,  1  ,  1  ,  0  },
        {  0  ,  1  ,  0  ,  1  ,  0  },
        {  0  ,  1  ,  1  ,  1  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
      uint8_t vetorRB[5][5] = {
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  },
        {  0  ,  0  ,  0  ,  0  ,  0  }
    };
    npDraw(vetorRB,vetorG,vetorRB); // Carrega os buffers
    npWrite();                      // Escreve na matriz de LEDs
    npClear();                      // Limpa os buffers (não necessário, mas por garantia)
}

// -------- Matriz - Fim --------

// ---------------- Desenhos - Fim ----------------



// ---------------- Funções - Início ----------------

// -------- Joystick - Início --------

// Leitura do eixo y do joystick
uint16_t read_y() {
    adc_select_input(0);
    return adc_read();
}

// Leitura do eixo x do joystick
uint16_t read_x() {
    adc_select_input(1);
    return adc_read();
}

// Leitura para converter escalas (Transformar os valores do joystick em valores de temperatura e umidade)
int scale(int min1, int max1, int min2, int max2,int x1) {
    return ( (((x1-min1)*(max2-min2))/(max1-min1))+min2 );
}

// Função para calibrar o eixo y do joystick
void calibrate_jsk_y_values() {
    uint16_t value, temp;
    int i;
    adc_select_input(0);

    // Indica ao usuário através da matriz de LEDs para colocar o joystick para cima e espera 2 segundos
    seta_cima();
    sleep_ms(2000);

    // Lê repetidamente o valor do eixo y e guarda o menor valor registrado no topo
    value = 4095;
    for(i=0;i<300;i++) {
        temp = adc_read();
        if(temp < value) {
            value = temp;
        }
        sleep_ms(10);
    }
    y_high = value;

    // Indica ao usuário através da matriz de LEDs para colocar o joystick para o meio e espera 2 segundos
    meio();
    sleep_ms(2000);

    // Lê repetidamente o valor do eixo y e guarda o maior valor registrado no meio
    value = 0;
    for(i=0;i<300;i++) {
        temp = adc_read();
        if(temp > value) {
            value = temp;
        }
        sleep_ms(10);
    }
    y_middle_high = value;

    // Indica ao usuário através da matriz de LEDs para colocar o joystick para baixo e espera 2 segundos
    seta_baixo();
    sleep_ms(2000);

    // Lê repetidamente o valor do eixo y e guarda o maior valor registrado na base
    value = 0;
    for(i=0;i<300;i++) {
        temp = adc_read();
        if(temp > value) {
            value = temp;
        }
        sleep_ms(10);
    }
    y_low = value;

    // Indica ao usuário através da matriz de LEDs para colocar o joystick para o meio e espera 2 segundos
    meio();
    sleep_ms(2000);

    // Lê repetidamente o valor do eixo y e guarda o menor valor registrado no meio
    value = 4095;
    for(i=0;i<300;i++) {
        temp = adc_read();
        if(temp < value) {
            value = temp;
        }
        sleep_ms(10);
    }
    y_middle_low = value;
}

// Função para calibrar o eixo x do joystick
void calibrate_jsk_x_values() {
    uint16_t value, temp;
    int i;
    adc_select_input(1);

    // Indica ao usuário através da matriz de LEDs para colocar o joystick para a direita e espera 2 segundos
    seta_direita();
    sleep_ms(2000);

    // Lê repetidamente o valor do eixo x e guarda o menor valor registrado na direita
    value = 4095;
    for(i=0;i<300;i++) {
        temp = adc_read();
        if(temp < value) {
            value = temp;
        }
        sleep_ms(10);
    }
    x_high = value;

    // Indica ao usuário através da matriz de LEDs para colocar o joystick para o meio e espera 2 segundos
    meio();
    sleep_ms(2000);

    // Lê repetidamente o valor do eixo x e guarda o maior valor registrado no meio
    value = 0;
    for(i=0;i<300;i++) {
        temp = adc_read();
        if(temp > value) {
            value = temp;
        }
        sleep_ms(10);
    }
    x_middle_high = value;

    // Indica ao usuário através da matriz de LEDs para colocar o joystick para a esquerda e espera 2 segundos
    seta_esquerda();
    sleep_ms(2000);

    // Lê repetidamente o valor do eixo x e guarda o maior valor registrado na esquerda
    value = 0;
    for(i=0;i<300;i++) {
        temp = adc_read();
        if(temp > value) {
            value = temp;
        }
        sleep_ms(10);
    }
    x_low = value;

    // Indica ao usuário através da matriz de LEDs para colocar o joystick para o meio e espera 2 segundos
    meio();
    sleep_ms(2000);

    // Lê repetidamente o valor do eixo x e guarda o menor valor registrado no meio
    value = 4095;
    for(i=0;i<300;i++) {
        temp = adc_read();
        if(temp < value) {
            value = temp;
        }
        sleep_ms(10);
    }
    x_middle_low = value;
}

// -------- Joystick - Fim --------

// -------- Buzzers - Início --------

// Emite um som alternando entre dois buzzers por um tempo determinado
void beep(uint tempo) {
    // Ativa o BUZZER A
    pwm_set_gpio_level(BUZZER_A, 1911);
    sleep_ms(tempo/4);

    // Desativa o BUZZER A e ativa o BUZZER B
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_gpio_level(BUZZER_B, 1012);
    sleep_ms(tempo/4);

    // Ativa o BUZZER A e desativa o BUZZER B
    pwm_set_gpio_level(BUZZER_B, 0);
    pwm_set_gpio_level(BUZZER_A, 1911);
    sleep_ms(tempo/4);

    // Desativa o BUZZER A e ativa o BUZZER B
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_gpio_level(BUZZER_B, 1012);
    sleep_ms(tempo/4);

    // Desativa o BUZZER B
    pwm_set_gpio_level(BUZZER_B, 0);
}

// -------- Buzzers - Fim --------

// -------- Callback - Início --------

// Callback para tratar as interrupções do botão do joystick e do botão B
void gpio_irq_callback(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time()); // Obtém o tempo atual em ms

    // Debounce de 200 ms
    if( (current_time - last_time) > 200 ) {
        last_time = current_time;

        if(gpio == JSK_SEL) { // Verifica se foi o botão do joystick
            if(screen_state < 4) {
                screen_state++;   // Avança para a próxima tela
            }else {
                screen_state = 0; // Retorna à primeira tela
            }
            change_screen = true;
        }else
        if(gpio == BUTTON_B) { // Verifica se foi o botão B
            flag_b = true; // Simula o "sinal" do sensor de nível do umidificador, indicando que mudou
        }

    }
}

// -------- Callback - Fim --------

// -------- Seleção de telas - Início --------

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
        pwm_set_gpio_level(RED_LED, 0);
        face_fan = true;
    }else
    if(y_scaled < fan_medium) {
        sprintf((char *)&string3[4],"low   \0");
        pwm_set_gpio_level(RED_LED, 1365);
        face_fan = true;
    }else
    if(y_scaled < fan_high) {
        sprintf((char *)&string3[4],"medium\0");
        pwm_set_gpio_level(RED_LED, 2730);
        face_fan = true;
    }else {
        sprintf((char *)&string3[4],"high  \0");
        pwm_set_gpio_level(RED_LED, 4095);
        face_fan = false;
    }

    if(x_scaled > humidifier_on) {
        sprintf((char *)&string4[11],"off\0");
        face_humidifier = true;
        pwm_set_gpio_level(BLUE_LED, 0);
    }else {
        sprintf((char *)&string4[11],"on \0");
        face_humidifier = false;
        pwm_set_gpio_level(BLUE_LED, 1365);
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

void selecionar_temperatura(ssd1306_t *ssd) {
    uint32_t t_current_time = to_ms_since_boot(get_absolute_time());

    y_value = read_y();

    y_scaled = scale(y_low,y_high,-15,50,y_value);
    if(y_scaled > 50) {
        y_scaled = 50;
    }else
    if(y_scaled < -15) {
        y_scaled = -15;
    }

    sprintf((char *)&string2[2],"    \0");
    sprintf((char *)&string4[11],"   \0");
    ssd1306_draw_string(ssd,(char *)string2,6,20);
    ssd1306_draw_string(ssd,(char *)string4,6,50);
    pwm_set_gpio_level(BLUE_LED, 0);

    sprintf((char *)&string1[2],"%3dC*\0",y_scaled);

    ssd1306_draw_string(ssd,(char *)string1,6,7);

    switch(contador) {
        case 0:
            pwm_set_gpio_level(RED_LED, 1365);
            sprintf((char *)&string3[4],"low   \0");
            draw_happy(ssd,84,6);
            break;
        case 1:
            pwm_set_gpio_level(RED_LED, 2730);
            sprintf((char *)&string3[4],"medium\0");
            draw_happy(ssd,84,6);
            break;
        case 2:
            pwm_set_gpio_level(RED_LED, 4095);
            sprintf((char *)&string3[4],"high  \0");
            draw_sad(ssd,84,6);
            break;
        default:
            contador = 0;
    }
    ssd1306_draw_string(ssd,(char *)string3,6,37);
    
    ssd1306_send_data(ssd);

    if( (t_current_time - t_last_time) > 200 ) {
        if(!gpio_get(BUTTON_A)) {
            t_last_time = t_current_time;
            switch(contador) {
                case 0:
                    fan_low = y_scaled;
                    contador++;
                    beep(120);
                    break;
                case 1:
                    fan_medium = y_scaled;
                    contador++;
                    beep(120);
                    break;
                case 2:
                    fan_high = y_scaled;
                    contador++;
                    beep(120);
                    break;
                default:
                    contador = 0;
            }
        }
    }
}

void selecionar_umidade(ssd1306_t *ssd) {
    uint32_t t_current_time = to_ms_since_boot(get_absolute_time());

    x_value = read_x();

    x_scaled = scale(x_low,x_high,0,100,x_value);
    if(x_scaled > 100) {
        x_scaled = 100;
    }else
    if(x_scaled < 0) {
        x_scaled = 0;
    }
    
    sprintf((char *)&string1[2],"     \0");
    sprintf((char *)&string3[4],"      \0");
    ssd1306_draw_string(ssd,(char *)string1,6,7);
    ssd1306_draw_string(ssd,(char *)string3,6,37);
    pwm_set_gpio_level(RED_LED, 0);
    
    sprintf((char *)&string2[2],"%3d%%\0",x_scaled);
    ssd1306_draw_string(ssd,(char *)string2,6,20);

    pwm_set_gpio_level(BLUE_LED, 1365);
    sprintf((char *)&string4[11],"on \0");
    draw_sad(ssd,84,6);
    ssd1306_draw_string(ssd,(char *)string4,6,50);
    
    ssd1306_send_data(ssd);

    if( (t_current_time - t_last_time) > 200 ) {
        if(!gpio_get(BUTTON_A)) {
            t_last_time = t_current_time;
            humidifier_on = x_scaled;
            beep(120);
        }
    }
}

void calibrar_joystick() {
    uint32_t t_current_time = to_ms_since_boot(get_absolute_time());
    if( (t_current_time - t_last_time) > 200 ) {
        if(!gpio_get(BUTTON_A)) {
            t_last_time = t_current_time;
            gpio_set_irq_enabled(JSK_SEL, GPIO_IRQ_EDGE_FALL, false);
            beep(120);
            calibrate_jsk_y_values();
            calibrate_jsk_x_values();
            beep(120);
            calibration_screen();
            gpio_set_irq_enabled(JSK_SEL, GPIO_IRQ_EDGE_FALL, true);
        }
    }
}

// -------- Seleção de telas - Fim --------

// ---------------- Funções - Fim ----------------



// ---------------- Main - Início ----------------

int main() {
    ssd1306_t ssd;

    stdio_init_all();
    init_display(&ssd);
    npInit(MATRIX_PIN);
    npClear();
    npWrite();

    init_joystick();
    init_rgb();
    init_buttons();
    init_buzzers();

    gpio_set_irq_enabled_with_callback(JSK_SEL, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_callback);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_callback);

    while (true) {
        sleep_ms(10);
        switch(screen_state) {
            case 0:
                tela_inicial(&ssd);
                break;
            case 1:
                selecionar_temperatura(&ssd);
                break;
            case 2:
                selecionar_umidade(&ssd);
                break;
            case 3:
                calibrar_joystick();
                break;
            default:
                screen_state = 0;
        }

        if(flag_b) {
            flag_b = false;
            if(switch_b) {
                humidifier_matrix();
                beep(400);
                switch_b = false;
            }else {
                npClear();
                npWrite();
                switch_b = true;
            }
        }

        if(change_screen) {
            change_screen = false;
            switch(screen_state) {
                case 0:
                    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);
                    npClear();
                    npWrite();
                    break;
                case 1:
                    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, false);
                    temperature_screen();
                    break;
                case 2:
                    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, false);
                    humidifier_screen();
                    break;
                case 3:
                    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, false);
                    pwm_set_gpio_level(BLUE_LED, 0);
                    pwm_set_gpio_level(RED_LED, 0);
                    tela_inicial(&ssd);
                    calibration_screen();
                    break;
            }
        }
    }
}

// ---------------- Main - Fim ----------------