# Monitor de Temperatura RP2040 com ADC, DMA e OLED SSD1306

## Objetivo

Executar práticas experimentais de manipulação e controle de transferências de dados em sistemas embarcados, utilizando o controlador DMA (Direct Memory Access) para otimização de desempenho em operações entre memória e periféricos. Desenvolvendo um sistema embarcado que utilize o controlador DMA do RP2040 para capturar automaticamente as amostras do sensor de temperatura interno (canal ADC4) e exibir os valores em um display OLED SSD1306, utilizando comunicação I2C.

## Funcionalidades Principais

1.  Utiliza o canal 4 do ADC do microcontrolador RP2040, que fornece acesso ao sensor de temperatura interno.
2.  Configura o ADC para amostragem contínua (free-running), com o sistema adquirindo blocos de amostras de forma periódica.
3.  Emprega o controlador DMA para transferir automaticamente os dados do registrador FIFO do ADC para um buffer na RAM.
4.  Interpreta os dados recebidos do ADC (formato digital de 12 bits) e aplica a fórmula de conversão para temperatura em graus Celsius.
5.  Armazena o valor da temperatura convertido e o formata como uma string para exibição no display.
6.  Estabelece comunicação com o display OLED SSD1306 usando o barramento I2C (`i2c1` nos pinos GPIO14 e GPIO15).
7.  Atualiza o display com os valores de temperatura lidos periodicamente (a cada 1000ms no código atual).
8.  Garante que o DMA e a rotina de exibição não entrem em conflito direto pelo acesso aos mesmos dados de forma não sincronizada, utilizando uma abordagem sequencial e bloqueante para as operações críticas (espera pelo DMA antes de processar/exibir). A arbitragem de barramento é gerenciada pelo hardware do RP2040.
9.  Define um intervalo de leitura e atualização (1000ms) para garantir um equilíbrio entre responsividade e estabilidade da leitura da temperatura.
10. Avalia o uso de DMA como forma de reduzir a carga de processamento da CPU na aquisição de dados, possibilitando, em projetos futuros, a implementação de estados de baixo consumo de energia de forma mais eficiente.
11. Evita polling ativo da CPU para a leitura de cada amostra ADC e o uso de delays excessivos que prejudiquem o desempenho geral, utilizando o mecanismo DREQ do DMA e a função `sleep_ms` para temporização.

## Materiais e Conceitos Envolvidos

1.  **ADC (Conversor Analógico-Digital)**
    * Leitura do canal interno ADC4.
    * Conversão do valor digital (12 bits) em temperatura (Celsius) usando a equação baseada na tensão de referência e nas características do sensor, conforme o datasheet do RP2040.
2.  **Sensor de Temperatura Interno**
    * Recurso embutido no microcontrolador RP2040.
    * Acessado via ADC canal 4.
3.  **DMA (Direct Memory Access)**
    * Configuração de um canal DMA para operação Periférico (ADC) → Memória (RAM).
    * Transferência automática de dados do FIFO do ADC para um buffer na RAM, controlada por DREQ (DMA Request) do ADC.
    * Redução significativa da carga de trabalho da CPU durante a aquisição das amostras.
4.  **Comunicação I2C**
    * Protocolo serial de dois fios (SDA e SCL) usado para comunicar com o display OLED.
    * No projeto, são utilizados os pinos GPIO14 (SDA) e GPIO15 (SCL) na interface `i2c1`.
5.  **Display OLED SSD1306**
    * Display gráfico que opera com o controlador SSD1306, recebendo comandos de inicialização e dados de imagem via I2C.
    * Utiliza uma biblioteca C compatível (ex: `inc/ssd1306.h` e sua implementação, que neste projeto gerencia um buffer de display externo).
6.  **Formatação e Exibição de Texto**
    * Conversão do valor de temperatura (tipo `float`) para uma string de caracteres (`char[]`).
    * Envio da string para o buffer do display e, subsequentemente, para o display físico, com posicionamento correto.
7.  **Controle de Tempo e Sincronização**
    * Uso de `sleep_ms()` para definir o intervalo de atualização periódica do display.
    * Uso de `dma_channel_wait_for_finish_blocking()` para sincronizar a CPU com a conclusão das transferências DMA.

## Requisitos Técnicos e Implementação

O firmware utiliza as seguintes bibliotecas do SDK do Raspberry Pi Pico:
* `pico/stdlib.h`
* `hardware/adc.h`
* `hardware/dma.h`
* `hardware/i2c.h`

**Funções e Lógica Chave:**

* **Captura de Temperatura (ADC):**
    * `adc_init()`: Inicializa o módulo ADC do RP2040.
    * `adc_set_temp_sensor_enabled(true)`: Ativa o sensor de temperatura interno conectado ao canal 4 do ADC.
    * `adc_select_input(4)`: Seleciona o canal ADC4 para realizar a leitura.
    * `adc_fifo_setup(...)`: Configura o FIFO do ADC, permitindo que os dados sejam armazenados em uma fila e utilizados pelo DMA através de DREQs.
    * `adc_run(true)`: Coloca o ADC em modo de amostragem contínua (free-running).

* **Transferência Automatizada (DMA):**
    * `dma_claim_unused_channel(true)`: Reserva um canal DMA disponível.
    * `dma_channel_get_default_config()`: Obtém uma configuração padrão para o canal DMA escolhido.
    * A configuração do canal (`dma_channel_config`) é customizada usando funções `channel_config_set_*()` para definir o tamanho da transferência (16 bits), modo de incremento dos endereços de leitura (não incrementa, FIFO) e escrita (incrementa, buffer RAM), e o sinal DREQ do ADC como gatilho.
    * `dma_channel_configure(...)`: Define os parâmetros da transferência (origem - FIFO do ADC, destino - buffer na RAM, tamanho do bloco, e se inicia imediatamente - configurado para não iniciar aqui).
    * `dma_channel_set_write_addr(..., ..., true)`: No loop principal, esta função é usada para definir o endereço de destino e, crucialmente, com o argumento `trigger` como `true`, **inicia/dispara** a transferência DMA configurada.
    * `dma_channel_wait_for_finish_blocking()`: Aguarda, de forma bloqueante, a conclusão da transferência DMA do bloco de amostras.

* **Apresentação Visual (Display OLED SSD1306 via I2C):**
    * `i2c_init()`: Inicializa o periférico I2C (`i2c1`) com a velocidade de clock definida.
    * `gpio_set_function()` e `gpio_pull_up()`: Configuram os pinos GPIO14 (SDA) e GPIO15 (SCL) para a função I2C com pull-ups internos.
    * `ssd1306_init()`: (Função da biblioteca do display) Inicializa o display SSD1306 enviando comandos de configuração via I2C.
    * `memset(ssd_display_buffer, 0, ...)`: (No código principal, antes de desenhar) Limpa o buffer de frame externo (`ssd_display_buffer`) preenchendo-o com zeros. Esta etapa serve ao propósito de "limpar a tela" antes de desenhar novas informações.
    * `ssd1306_draw_string(ssd_display_buffer, x, y, str)`: (Função da biblioteca do display) Escreve uma string no buffer de frame externo (`ssd_display_buffer`) nas coordenadas desejadas.
    * `render_on_display(ssd_display_buffer, &frame_area)`: (Função da biblioteca do display) Envia o conteúdo do buffer de frame externo (`ssd_display_buffer`) para o display OLED, efetivamente atualizando a tela com as novas informações.

## ✅ Como os Requisitos Técnicos Foram Atendidos

O código implementa as seguintes funcionalidades para cumprir os requisitos técnicos detalhados:

1.  **Uso das Bibliotecas do SDK do Pico:**
    * **Como foi atendido:** O código inicia com as diretivas `#include` para `hardware/adc.h`, `hardware/dma.h` e `hardware/i2c.h`, além de `pico/stdlib.h`, garantindo acesso às funções necessárias para interagir com o hardware do RP2040.

2.  **Captura da Tensão do Sensor de Temperatura Interno (ADC):**
    * **Como foi atendido:**
        * A função `setup_adc_dma()` chama `adc_init()` para inicializar o periférico ADC.
        * `adc_set_temp_sensor_enabled(true)` é utilizada para ativar o sensor de temperatura interno do RP2040.
        * `adc_select_input(ADC_TEMP_CHANNEL)` (com `ADC_TEMP_CHANNEL` definido como 4) seleciona o canal correto para a leitura do sensor de temperatura.
        * `adc_fifo_setup(...)` configura o FIFO do ADC, habilitando-o, ativando as requisições DMA (DREQ) e definindo o limiar para DREQ, o que é crucial para a integração com o DMA.
        * Adicionalmente, `adc_run(true)` inicia o ADC em modo de conversão contínua (free-running).

3.  **Transferência Automatizada de Amostras ADC para RAM (DMA):**
    * **Como foi atendido:**
        * Na função `setup_adc_dma()`, `dma_claim_unused_channel()` aloca um canal DMA, e `dma_channel_get_default_config()` obtém a configuração padrão para este canal.
        * `dma_channel_configure(...)` aplica as configurações detalhadas da transferência: a origem é o FIFO do ADC (`&adc_hw->fifo`), o destino é o buffer na RAM (`adc_sample_buffer`), o tamanho de cada transferência de dados é de 16 bits, o número de transferências por bloco é `NUM_ADC_SAMPLES_PER_READ`, e o DREQ do ADC controla o fluxo de dados.
        * A transferência é iniciada no loop principal através da chamada `dma_channel_set_write_addr(dma_adc_channel, adc_sample_buffer, true)`. O argumento `true` (trigger) nesta função dispara a operação DMA.
        * A função `dma_channel_wait_for_finish_blocking()` é utilizada para pausar a execução da CPU até que o DMA complete a transferência do bloco de amostras.

4.  **Apresentação Visual da Temperatura no Display OLED (SSD1306):**
    * **Como foi atendido:**
        * Na função `setup_i2c_oled()`, `i2c_init()` inicializa a interface I2C1, e os pinos GPIO14 e GPIO15 são configurados para I2C. Em seguida, `ssd1306_init()` (da biblioteca de display fornecida pelo usuário) envia os comandos de inicialização para o display SSD1306 via I2C.
        * Para limpar a tela antes de cada atualização, o código utiliza `memset(ssd_display_buffer, 0, ssd1306_buffer_length)`, que preenche o buffer de frame externo com zeros.
        * A função `ssd1306_draw_string(ssd_display_buffer, ...)` é chamada para desenhar a string com a temperatura formatada no buffer de frame externo.
        * Finalmente, `render_on_display(ssd_display_buffer, &frame_area)` envia o conteúdo do buffer de frame externo para o display OLED, atualizando a informação visualizada. Estas funções de manipulação de buffer externo (`memset`, `render_on_display`) cumprem os papéis de limpar a tela e exibir os dados, adaptando-se ao modelo da biblioteca gráfica utilizada.

## Base do Código

Para a interface com o display OLED SSD1306, este projeto tomou como referência e inspiração a estrutura e as funções para manipulação de um buffer de display externo encontradas no repositório [BitDogLab-C - display_oled](https://github.com/BitDogLab/BitDogLab-C/tree/main/display_oled), adaptando-as conforme necessário para este sistema.

## Pinagem Utilizada

* **I2C1 (Display OLED SSD1306):**
    * GPIO14: I2C1 SDA
    * GPIO15: I2C1 SCL
* **ADC (Sensor de Temperatura Interno):**
    * ADC4: Conectado internamente ao sensor de temperatura.

## Propósito Educacional

Este projeto foi desenvolvido com fins estritamente educacionais e aprendizado durante a residência em sistemas embarcados pelo EmbarcaTech.
