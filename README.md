# Jogo da Cobrinha (Snake Game) - RP2040 / bitDogLab

## Objetivo do Projeto
Este projeto implementa o clássico "Jogo da Cobrinha" (Snake Game) num microcontrolador RP2040 (Raspberry Pi Pico / bitDogLab). O objetivo principal é demonstrar o uso avançado de periféricos de hardware e arquitetura de software em sistemas embarcados utilizando a linguagem C e o Pico SDK.

O projeto destaca-se pela implementação de um **Game Loop Não-Bloqueante** (usando temporizadores de hardware para multitarefa cooperativa) e uma **Máquina de Estados** para gerir a lógica de movimentação, prevenindo colisões inválidas.

## Principais Funcionalidades
* **Controle Analógico:** Leitura de um joystick via ADC (Conversor Analógico-Digital) com tratamento de zona morta (*deadzone*) e calibração de eixos.
* **Renderização Gráfica:** Comunicação I2C com um ecrã OLED SSD1306 (128x64 pixels), utilizando um *Frame Buffer* em memória RAM para atualizações de tela fluidas (30+ FPS).
* **Física de Jogo:** Detecção de colisão com as bordas da tela e com o próprio corpo da cobra.
* **Sistema de Pontuação:** Renderização de uma fonte Bitmap personalizada (5x8 pixels) para exibir a pontuação em tempo real no ecrã.
* **Reinício Automático:** Detecção de inputs após o *Game Over* para reiniciar a partida sem necessidade de reset físico do hardware.

## Dependências e Hardware Necessário

### Hardware
* Placa de desenvolvimento com chip RP2040 (ex: **Raspberry Pi Pico** ou **bitDogLab**).
* Display OLED SSD1306 (128x64) com interface I2C.
* Joystick Analógico (2 eixos: X e Y).
* Cabos jumper e Protoboard (caso não utilize uma placa integrada como a bitDogLab).

### Software / Ferramentas
* **Pico SDK** devidamente instalado e configurado no seu ambiente.
* **CMake** (para geração do build).
* **Toolchain ARM GCC** (`arm-none-eabi-gcc`).
* Extensão Raspberry Pi Pico para VS Code (opcional, mas recomendado).

## Mapeamento de Pinos (Pinagem)

As configurações de hardware estão centralizadas no início do código-fonte. O mapeamento padrão para a placa bitDogLab é:

| Componente | Função | Pino GPIO | Observação |
| :--- | :--- | :--- | :--- |
| **OLED (I2C)** | SDA | `GPIO 14` | I2C1 |
| **OLED (I2C)** | SCL | `GPIO 15` | I2C1 |
| **Joystick** | Eixo X | `GPIO 26` | Canal ADC 0 (Horizontal) |
| **Joystick** | Eixo Y | `GPIO 27` | Canal ADC 1 (Vertical) |


## Instruções de Instalação e Execução

### 1. Clonar o Repositório e Preparar o Ambiente
Certifique-se de que a variável de ambiente `PICO_SDK_PATH` está configurada apontando para a sua instalação do Pico SDK.

```bash
git clone [https://github.com/seu-usuario/seu-repositorio-snake.git](https://github.com/seu-usuario/seu-repositorio-snake.git)
cd seu-repositorio-snake
mkdir build
cd build