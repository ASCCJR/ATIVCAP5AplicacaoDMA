#include <stdio.h>
#include <string.h> // Para memset, sprintf
#include <stdlib.h> // Para abs, se necessário, mas não usado aqui

// SDK do Pico
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

// Biblioteca padrão para o display SSD1306 de acordo com o código original
#include "inc/ssd1306.h"

// --- Configurações do Display e I2C ---
const uint I2C_SDA_PIN = 14; // Pino SDA do I2C (GPIO14)
const uint I2C_SCL_PIN = 15; // Pino SCL do I2C (GPIO15)
#define I2C_PORT i2c1        // Usando a porta I2C1

// Estas variáveis serão inicializadas na função setup_i2c_oled
struct render_area frame_area;
uint8_t ssd_display_buffer[ssd1306_buffer_length]; // Buffer para o display

// --- Configurações do ADC e DMA ---
#define ADC_TEMP_CHANNEL 4                  // Canal do ADC para o sensor de temperatura interno
#define NUM_ADC_SAMPLES_PER_READ 10         // Número de amostras ADC para fazer média
uint16_t adc_sample_buffer[NUM_ADC_SAMPLES_PER_READ]; // Buffer para as amostras do ADC
int dma_adc_channel;                        // Canal DMA a ser usado

/**
 * @brief Converte o valor bruto do ADC para graus Celsius.
 *
 * @param raw_adc O valor bruto de 12 bits do ADC.
 * @return A temperatura em graus Celsius.
 */
float convert_adc_to_celsius(uint16_t raw_adc) {
    // Fator de conversão: ADC de 12 bits (0-4095) para tensão (0-3.3V)
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw_adc * conversion_factor;

    // Fórmula do datasheet do RP2040 para o sensor de temperatura
    // T = 27 - (ADC_voltage - 0.706) / 0.001721
    float temperature_c = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature_c;
}

/**
 * @brief Configura o ADC para ler o sensor de temperatura e o DMA para capturar as amostras.
 */
void setup_adc_dma() {
    adc_init(); // Inicializa o hardware ADC
    adc_set_temp_sensor_enabled(true); // Habilita o sensor de temperatura interno
    adc_select_input(ADC_TEMP_CHANNEL); // Seleciona o canal ADC4 (sensor de temperatura)

    // Configura o FIFO do ADC
    adc_fifo_setup(
        true,    // Habilitar escrita no FIFO
        true,    // Habilitar DREQ (DMA Request) quando o FIFO atingir o limiar
        1,       // Limiar DREQ (1 amostra)
        false,   // Não incluir bit de erro no FIFO
        false    // Leituras de 16 bits (sem deslocamento de byte para 8 bits)
    );

    // Define o divisor de clock do ADC. 0 significa clock do ADC = 48MHz / 96 = 500kHz.
    // Para o sensor de temperatura, a amostragem pode ser mais lenta, mas o DMA lida com a taxa.
    // Fazer a média de várias amostras ajuda na precisão.
    adc_set_clkdiv(0);

    // Requisita um canal DMA vago
    dma_adc_channel = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_adc_channel);

    // Configura o canal DMA
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16); // Transferência de dados de 16 bits
    channel_config_set_read_increment(&cfg, false);           // Endereço de leitura fixo (FIFO do ADC)
    channel_config_set_write_increment(&cfg, true);           // Endereço de escrita incremental (nosso buffer)
    channel_config_set_dreq(&cfg, DREQ_ADC);                  // Controlado pelo DREQ do ADC

    // Configura o DMA com os parâmetros, mas não inicia ainda
    dma_channel_configure(
        dma_adc_channel,
        &cfg,
        adc_sample_buffer,      // Endereço de escrita inicial (buffer de amostras)
        &adc_hw->fifo,          // Endereço de leitura (FIFO do ADC)
        NUM_ADC_SAMPLES_PER_READ, // Número de transferências por bloco
        false                   // Não iniciar a transferência agora
    );

    // Inicia o ADC em modo de execução livre (free-running)
    adc_run(true);
}

/**
 * @brief Configura a comunicação I2C e inicializa o display OLED SSD1306.
 */
void setup_i2c_oled() {
    // Inicialização do I2C
    i2c_init(I2C_PORT, ssd1306_i2c_clock * 1000); // Usa a velocidade definida na lib do SSD1306
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Inicialização completa do OLED SSD1306
    ssd1306_init();

    // Prepara a área de renderização para o display
    // (ssd1306_width pixels por ssd1306_n_pages páginas)
    // Essas dimensões devem vir da sua biblioteca ssd1306.h
    frame_area.start_column = 0;
    frame_area.end_column = ssd1306_width - 1;
    frame_area.start_page = 0;
    frame_area.end_page = ssd1306_n_pages - 1;

    // Calcula o tamanho do buffer necessário para a área de renderização
    // Esta função também deve vir da sua biblioteca
    calculate_render_area_buffer_length(&frame_area);

    // Zera o buffer do display inteiro inicialmente
    memset(ssd_display_buffer, 0, ssd1306_buffer_length);
    render_on_display(ssd_display_buffer, &frame_area); // Mostra a tela limpa
}

/**
 * @brief Função principal.
 */
int main() {
    // Inicializa todas as E/S padrão (necessário para printf via USB serial, se usado)
    stdio_init_all();

    // Aguarda um pouco para o caso de querer abrir o terminal serial
    // sleep_ms(2000);
    // printf("Iniciando Leitor de Temperatura Interna com OLED SSD1306...\n");

    // Configura I2C e o display OLED
    setup_i2c_oled();

    // Configura ADC e DMA para o sensor de temperatura
    setup_adc_dma();

    char temp_str[25]; // String para armazenar a temperatura formatada

    while (true) {
        // Inicia uma nova captura DMA para o buffer de amostras
        // O último argumento 'true' faz com que a transferência comece imediatamente
        dma_channel_set_write_addr(dma_adc_channel, adc_sample_buffer, true);

        // Aguarda a DMA completar a transferência do bloco de amostras
        dma_channel_wait_for_finish_blocking(dma_adc_channel);

        // Calcula a média das amostras capturadas para estabilizar a leitura
        uint32_t sum_adc_raw = 0;
        for (int i = 0; i < NUM_ADC_SAMPLES_PER_READ; i++) {
            sum_adc_raw += adc_sample_buffer[i];
        }
        uint16_t avg_adc_raw = sum_adc_raw / NUM_ADC_SAMPLES_PER_READ;

        // Converte o valor médio do ADC para graus Celsius
        float temperatura_atual = convert_adc_to_celsius(avg_adc_raw);

        // (Opcional) Imprime no console serial para depuração
        // printf("Temperatura: %.2f C (Raw ADC: %d)\n", temperatura_atual, avg_adc_raw);

        // Formata a string da temperatura para exibição no OLED
        sprintf(temp_str, "Temp: %.2f C", temperatura_atual);

        // Limpa o buffer do display (preenche com 0s)
        memset(ssd_display_buffer, 0, ssd1306_buffer_length);

        // Desenha a string da temperatura no buffer do display
        // Ajuste as coordenadas X, Y (5, 5 aqui) conforme necessário para o seu display
        ssd1306_draw_string(ssd_display_buffer, 5, 5, temp_str);
        
        // (Opcional) Você pode adicionar mais informações ao display aqui
        // Ex: ssd1306_draw_string(ssd_display_buffer, 5, 15, "RP2040 DMA");

        // Envia o buffer atualizado para ser renderizado no display OLED
        render_on_display(ssd_display_buffer, &frame_area);

        // Intervalo entre as atualizações do display
        sleep_ms(1000); // Atualiza a cada 1 segundo
    }

    return 0; // Este ponto normalmente não é alcançado em um sistema embarcado
}
