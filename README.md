# Flappy Bird para Raspberry Pi Pico

Uma implementação do clássico **Flappy Bird** desenvolvida em **C** para o **Raspberry Pi Pico (RP2040)** utilizando um display OLED SSD1306, botões para controle do jogo e buzzers para efeitos sonoros.

## Demonstração

<p align="center">
  <img src="assets/demo.gif" alt="Demonstração do jogo" width="400">
</p>

## Funcionalidades

- Menu inicial.
- Sistema de pausa.
- Tela de Game Over.
- Contador de pontuação.
- Obstáculos gerados dinamicamente.
- Física simples com gravidade e impulso de salto.
- Efeitos sonoros utilizando buzzers.
- Renderização em display OLED SSD1306 via I2C.
- Execução utilizando os dois núcleos do RP2040 para separar lógica e renderização.

## Hardware Utilizado

- Raspberry Pi Pico (RP2040)
- Display OLED SSD1306 (128x64)
- 2 Botões
- 2 Buzzers

## Conexões

| Componente | GPIO |
|------------|------|
| Botão A | GP5 |
| Botão B | GP6 |
| Buzzer A | GP10 |
| Buzzer B | GP21 |
| SDA (OLED) | GP14 |
| SCL (OLED) | GP15 |

## Controles

| Ação | Botão |
|-------|--------|
| Saltar | A |
| Iniciar jogo | B |
| Pausar/Continuar | B |
| Reiniciar após derrota | B |

## Compilação

O projeto utiliza o **Pico SDK** e pode ser compilado com CMake.

```bash
mkdir build
cd build
cmake ..
make
```

Após a compilação, grave o arquivo `.uf2` gerado na memória do Raspberry Pi Pico.

## Estrutura do Projeto

```text
.
├── assets/
│   └── demo.gif
├── inc/
│   ├── ssd1306.h
│   └── font.h
├── jogo.h
├── main.c
└── CMakeLists.txt
```

## Sobre o Projeto

Este projeto foi desenvolvido com o objetivo de praticar programação embarcada utilizando o RP2040, incluindo:

- Manipulação de GPIO.
- Comunicação I2C.
- Geração de áudio com PWM.
- Programação concorrente com multicore.
- Estruturas de dados e gerenciamento de estado do jogo.
- Renderização gráfica em displays OLED.

## Licença

Este projeto é disponibilizado para fins educacionais.