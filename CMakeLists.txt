# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# VS Code SDK setup
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Importa o SDK do RP2040
include(pico_sdk_import.cmake)

project(temp_dma_oled C CXX ASM)

# Inicializa o SDK
pico_sdk_init()

# Adiciona o executável e os arquivos fonte
add_executable(temp_dma_oled 
    main.c 
    inc/ssd1306_i2c.c
)

# Configura nome e versão
pico_set_program_name(temp_dma_oled "temp_dma_oled")
pico_set_program_version(temp_dma_oled "0.1")

# Ativa I/O via USB (desativa UART)
pico_enable_stdio_uart(temp_dma_oled 0)
pico_enable_stdio_usb(temp_dma_oled 1)

# Adiciona diretórios de include
target_include_directories(temp_dma_oled PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/inc
)

# Linka bibliotecas necessárias
target_link_libraries(temp_dma_oled
    pico_stdlib
    hardware_i2c
    hardware_adc
    hardware_dma
)

# Gera arquivos bin/uf2/etc
pico_add_extra_outputs(temp_dma_oled)
