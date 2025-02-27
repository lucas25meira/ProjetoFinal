/* Programa de capacitação EMBARCATECH
 * Unidade 7 / Capítulo 1 - Projetos Práticos
 * Tarefa 1 - Projeto Final
 *
 * Programa desenvolvido por:
 *      - Lucas Meira de Souza Leite
 * 
 * Objetivos: 
 *      - Utilização de recusos gerais da BitDogLab e conceitos abordados na capacitação
 */

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2818b.pio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"
#include "hardware/clocks.h"

//Definições Display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define BUZZER 21
#define LED_VERMELHO 13
#define LED_AZUL 12
#define LED_VERDE 11
#define NUM_LEDS 25
#define PB_JOYSTICK 22
#define WS2812_PIN 7
#define BOTAO_A 5
#define BOTAO_B 6
#define TEMPO_DEBOUNCE 200
#define TEMPO to_ms_since_boot(get_absolute_time())
volatile unsigned long TEMPODEBOUNCE; 

// Definição de pixel GRB
struct pixel_t {
    uint8_t G, R, B;};  // Três valores de 8-bits compõem um pixel.
  
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.
  
// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[NUM_LEDS];
  
// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;
volatile bool Iniciar = false;
volatile bool Tampado = false;
volatile bool EnvaseFinalizado = false;
volatile bool Rotulagem = false;

//Arrays contendo os LEDS que irão compor a visualização de cada linha
const uint8_t Linha1[5] = {2, 3, 1, 4, 0};
const uint8_t Linha2[5] = {7, 6, 8, 5, 9};
const uint8_t Linha3[5] = {12, 13, 11, 14, 10};
const uint8_t Linha4[3] = {17, 16, 18};
const uint8_t Linha5[3] = {21, 22, 23};
const uint8_t Rotulo[3] = {6, 7, 8};


/*
* Inicializa a máquina PIO para controle da matriz de LEDs.
*/
void npInit(uint pin) {

    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
  
    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
      np_pio = pio1;
      sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }
  
    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
  
    // Limpa buffer de pixels.
    for (uint i = 0; i < NUM_LEDS; ++i) {
      leds[i].R = 0;
      leds[i].G = 0;
      leds[i].B = 0;
    }
}
  
/*
* Atribui uma cor RGB a um LED.
*/
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}
  
/*
* Limpa o buffer de pixels.
*/
void npClear() {
    for (uint i = 0; i < NUM_LEDS; ++i)
      npSetLED(i, 0, 0, 0);
}
  
/*
* Escreve os dados do buffer nos LEDs.
*/
void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < NUM_LEDS; ++i) {
      pio_sm_put_blocking(np_pio, sm, leds[i].G);
      pio_sm_put_blocking(np_pio, sm, leds[i].R);
      pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
}
//
void AcionaBuzzer(uint8_t buzzer, uint16_t notefreq, uint16_t duration_ms){
    int halfc = 1000000 / (2 * notefreq); // Calcula o tempo de espera para cada meio ciclo de onda
    int cycles = (notefreq * duration_ms) / 1000; // Número total de ciclos necessários para a duração

    for (uint8_t i = 0; i < cycles; i++) {
        gpio_put(BUZZER, 1);  // Muda o estado para HIGH iniciando 1º parte do ciclo
        sleep_us(halfc); // Aguarda metade do tempo de um ciclo (meio período)
        gpio_put(BUZZER, 0);  // Muda o estado para LOW  iniciando 2º parte do ciclo
        sleep_us(halfc); // Aguarda a outra metade do tempo (meio período)
    }
}
  
// Prototipo da função de interrupção
static void gpio_irq(uint gpio, uint32_t events);
  
void main()
{
    stdio_init_all();
    TEMPODEBOUNCE = TEMPO;
    i2c_init(I2C_PORT, 400 * 1000);
      
    npInit(WS2812_PIN);

    // ------------------------- DISPLAY ------------------------    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA); // Pull up the data line
    gpio_pull_up(I2C_SCL); // Pull up the clock line
    ssd1306_t ssd; // Inicializa a estrutura do display  
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
  
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);    
    
    // ------------------------- BOTOES  E LEDS ------------------------ 
    gpio_init(LED_VERMELHO); //Inicializa o led vermelho
    gpio_set_dir(LED_VERMELHO, GPIO_OUT); //Define o led como saida
    
    gpio_init(LED_VERDE); //Inicializa o led verde
    gpio_set_dir(LED_VERDE, GPIO_OUT); //Define o led como saida    
  
    gpio_init(LED_AZUL); //Inicializa o led verde
    gpio_set_dir(LED_AZUL, GPIO_OUT); //Define o led como saida    
  
    gpio_init(BOTAO_A);                    // Inicializa o botão
    gpio_set_dir(BOTAO_A, GPIO_IN);        // Configura o pino como entrada
    gpio_pull_up(BOTAO_A);                 // Habilita o pull-up interno
  
    gpio_init(BOTAO_B);                    // Inicializa o botão
    gpio_set_dir(BOTAO_B, GPIO_IN);        // Configura o pino como entrada
    gpio_pull_up(BOTAO_B);                 // Habilita o pull-up interno

    gpio_init (PB_JOYSTICK);
    gpio_set_dir(PB_JOYSTICK, GPIO_IN);
    gpio_pull_up(PB_JOYSTICK);

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_put(BUZZER, false);

    npClear(); // Inicia os leds
    npWrite();

    //Escreve mensagem display
    ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
    ssd1306_draw_string(&ssd, "PARA INICIAR", 17, 18); // Desenha no displaY
    ssd1306_draw_string(&ssd, "PRESSIONE", 28, 30); // Desenha no display
    ssd1306_draw_string(&ssd, "BOTAO A", 38, 42); // Desenha no display                             
    ssd1306_send_data(&ssd); // Atualiza o display    

    gpio_put(LED_VERMELHO, true);
    gpio_put(LED_VERDE, true);
    gpio_put(LED_AZUL, true); 

    // Configuração da interrupção com callback
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_RISE, true, &gpio_irq);    
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_RISE, true, &gpio_irq);
    gpio_set_irq_enabled_with_callback(PB_JOYSTICK, GPIO_IRQ_EDGE_RISE, true, &gpio_irq);
    
    while (true) {
        if ((Iniciar) && (!EnvaseFinalizado)){
            AcionaBuzzer(BUZZER, 880, 200);
            npClear(); // Inicia os leds
            npWrite();

            //Exibe no display o status do 
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);    

            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "ENVASE", 40, 20); // Desenha no display
            ssd1306_draw_string(&ssd, "INICIADO", 30, 32); // Desenha no display
            ssd1306_send_data(&ssd);

            gpio_put(LED_VERMELHO, false);
            gpio_put(LED_VERDE, false);
            gpio_put(LED_AZUL, true);
        
            sleep_ms(1000);
            for (int i = 0; i < 5; i++){
                //Primeira linha
                npSetLED(Linha1[i], 0, 0, 100);
                npWrite();
                sleep_ms(1000);            
            }        
            for (int i = 0; i < 5; i++){
                //Segunda linha
                npSetLED(Linha2[i], 0, 0, 100);
                npWrite();
                sleep_ms(1000);            
            }                
            for (int i = 0; i < 5; i++){
                //Terceira linha
                npSetLED(Linha3[i], 0, 0, 100);
                npWrite();
                sleep_ms(1000);            
            }                
            for (int i = 0; i < 3; i++){
                //Quarta linha
                npSetLED(Linha4[i], 0, 0, 100);
                npWrite();
                sleep_ms(1000);            
            }
            EnvaseFinalizado = true;

            //Exibe no display o status de conclusão
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);    
            
            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "ENVASE", 40, 20); // Desenha no display
            ssd1306_draw_string(&ssd, "CONCLUIDO", 28, 32); // Desenha no display
            ssd1306_send_data(&ssd);                            
        }
        if ((Tampado) && (EnvaseFinalizado)){
            gpio_put(LED_VERMELHO, true);
            gpio_put(LED_VERDE, false);
            gpio_put(LED_AZUL, false); 
            
            //Exibe no display o status de conclusão
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);    

            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "VEDACAO", 33, 20); // Desenha no display
            ssd1306_draw_string(&ssd, "INICIADA", 30, 32); // Desenha no display
            ssd1306_send_data(&ssd); 
            
            sleep_ms(1000);
            for (int i = 0; i < 3; i++){
                //Quarta linha
                npSetLED(Linha5[i], 100, 0, 0);
                npWrite();
                sleep_ms(1000);            
            }       
            
            //Exibe no display o status de conclusão
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);    

            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "VEDACAO", 33, 20); // Desenha no display
            ssd1306_draw_string(&ssd, "CONCLUIDA", 28, 32); // Desenha no display
            ssd1306_send_data(&ssd);           
            
            //Tampado = false;
            EnvaseFinalizado = false;
            Iniciar = false;
        }
        if ((Tampado) && (Rotulagem)){

            gpio_put(LED_VERMELHO, false);
            gpio_put(LED_VERDE, true);
            gpio_put(LED_AZUL, false); 
            
            //Exibe no display o status de conclusão
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);    

            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "ROTULAGEM", 28, 20); // Desenha no display
            ssd1306_draw_string(&ssd, "INICIADA", 30, 32); // Desenha no display
            ssd1306_send_data(&ssd); 
            
            sleep_ms(1000);
            for (int i = 0; i < 3; i++){
                //Quarta linha
                npSetLED(Rotulo[i], 0, 100, 0);
                npWrite();
                sleep_ms(1000);            
            }       
            
            //Exibe no display o status de conclusão
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);    

            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "ROTULAGEM", 28, 20); // Desenha no display
            ssd1306_draw_string(&ssd, "CONCLUIDA", 28, 32); // Desenha no display
            ssd1306_send_data(&ssd); 
            
            gpio_put(LED_VERMELHO, true);
            gpio_put(LED_VERDE, true);
            gpio_put(LED_AZUL, true); 

            Rotulagem = false;
            EnvaseFinalizado = false;
            Tampado = false;

            AcionaBuzzer(BUZZER, 880, 200);
            AcionaBuzzer(BUZZER, 300, 200);                        
            sleep_ms(2000);            

            //Exibe no display o status de conclusão
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);    

            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "PROCESSO", 31, 20); // Desenha no display
            ssd1306_draw_string(&ssd, "CONCLUIDO", 28, 32); // Desenha no display
            ssd1306_send_data(&ssd); 

            gpio_put(LED_VERMELHO, false);
            gpio_put(LED_VERDE, false);
            gpio_put(LED_AZUL, false);     
        }
    }
}

void gpio_irq(uint gpio, uint32_t events)
{
    int tamanho_array;    
    switch (gpio)
    {
        case BOTAO_A:
        if (TEMPO - TEMPODEBOUNCE > TEMPO_DEBOUNCE){
            Iniciar = true;            
        }
        break;
        case BOTAO_B: 
        if (TEMPO - TEMPODEBOUNCE > TEMPO_DEBOUNCE){
            Tampado = true;
        }        
        break;
        case PB_JOYSTICK: 
        if (TEMPO - TEMPODEBOUNCE > TEMPO_DEBOUNCE){
            Rotulagem = true;
        }        
        break;
    }
    TEMPODEBOUNCE = TEMPO;    
}
