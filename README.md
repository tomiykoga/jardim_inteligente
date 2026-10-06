# 🌱 Estação de Monitoramento de Jardim Inteligente

Sistema embarcado com **ESP32** que monitora o microclima e o solo de um jardim, exibe os dados em um **LCD 16x2 I2C**, envia telemetria pela **Serial** e, como melhorias, faz **irrigação automática** e dispara **alarmes visuais e sonoros** em condições críticas.

Projeto desenvolvido e simulado em **VS Code + PlatformIO + Wokwi Simulator**.

> Trabalho complementar da **Avaliação NP1** — Sistemas Microcontrolados — Engenharia da Computação (UNIP).
> Autor: **Wellington Koga**

---

## ✨ Funcionalidades

**Projeto base**
- Leitura de **temperatura (°C)** e **umidade do ar (%)** com o sensor DHT22
- Leitura da **umidade do solo (0–100%)** simulada por um potenciômetro
- Exibição contínua no **LCD 16x2 I2C**
- Telemetria na **Serial a 115200 baud**
- Leituras a cada **2 segundos** usando `millis()` (sem travar o programa com `delay`)

**Melhoria 1 — Irrigação automática**
- **Módulo relé** (bomba d'água) + **LED azul** indicador
- Liga quando o solo fica **abaixo de 30%** e desliga **acima de 40%** (histerese)
- Estado da irrigação mostrado no LCD (`Irr:ON` / `Irr:OFF`) e na Serial

**Melhoria 2 — Alarme visual e sonoro**
- **LED vermelho** pisca quando a temperatura passa de **35 °C**
- **LED amarelo** acende com **solo seco (< 30%)** ou **ar seco (< 20%)**
- **Buzzer** bipa de forma intermitente com **temperatura > 35 °C** ou **umidade do ar < 20%**
- **Botão "Silenciar"** tratado por **interrupção externa** (desliga só o som; os LEDs continuam avisando)
- Mensagem de alerta alternando na 2ª linha do LCD

---

## 🔌 Mapa de ligações

| Componente | Pino do componente | Pino do ESP32 |
|---|---|---|
| LCD 16x2 I2C | SDA / SCL | GPIO 21 / GPIO 22 |
| LCD 16x2 I2C | VCC / GND | 5V / GND |
| DHT22 | SDA (dado) | GPIO 4 |
| Potenciômetro (sonda de solo) | SIG | GPIO 34 (ADC1) |
| Módulo relé (bomba) | IN | GPIO 26 |
| LED azul (irrigação) + 220 Ω | Ânodo | GPIO 27 |
| LED vermelho (calor) + 220 Ω | Ânodo | GPIO 32 |
| LED amarelo (seco) + 220 Ω | Ânodo | GPIO 33 |
| Buzzer | Pino 2 (+) | GPIO 18 |
| Botão Silenciar | 1.l / 2.l | GPIO 14 / GND |

---

## 📁 Estrutura do projeto

```
jardim_inteligente/
├── platformio.ini   # Placa (esp32dev), framework Arduino e bibliotecas
├── wokwi.toml       # Caminho dos binários para o simulador Wokwi
├── diagram.json     # Circuito virtual (componentes e ligações)
├── src/
│   └── main.cpp     # Firmware comentado
└── README.md
```

---

## ▶️ Como abrir e executar a simulação

### Pré-requisitos
1. [Visual Studio Code](https://code.visualstudio.com/)
2. Extensão **PlatformIO IDE** (`platformio.platformio-ide`)
3. Extensão **Wokwi Simulator** (`wokwi.wokwi-vscode`)
4. Licença gratuita do Wokwi ativada (`F1` → **Wokwi: Request a New License Key**)

### Passo a passo
1. Baixe o repositório (**Code → Download ZIP**) ou clone:
   ```bash
   git clone https://github.com/tomiykoga/jardim_inteligente
   ```
2. Coloque a pasta em um caminho **sem espaços e sem acentos**, por exemplo `C:\Projetos\jardim_inteligente`.
3. No VS Code: **File → Open Folder** e selecione a pasta `jardim_inteligente`.
4. Aguarde o PlatformIO baixar a plataforma ESP32 e as bibliotecas (só na primeira vez).
5. Clique no **✓ (Build)** na barra inferior do VS Code e aguarde `[SUCCESS]` no terminal.
6. Abra o arquivo `diagram.json` e clique em **▶ Play** (ou `F1` → **Wokwi: Start Simulator**).

### Como testar
| Ação na simulação | Resultado esperado |
|---|---|
| Girar o potenciômetro para baixo de 30% | Relé e LED azul ligam, LED amarelo acende, LCD mostra `Irr:ON` |
| Girar o potenciômetro para acima de 40% | Relé e LED azul desligam, LCD mostra `Irr:OFF` |
| Clicar no DHT22 e colocar temperatura > 35 °C | LED vermelho pisca, buzzer bipa, LCD alterna com `!! CALOR >35C !!` |
| Colocar umidade do ar < 20% | LED amarelo acende, buzzer bipa, LCD alterna com `!AR SECO (<20%)!` |
| Apertar o botão **Silenciar** durante um alarme | Buzzer para; LEDs continuam; Serial mostra `[EVENTO] Alarme sonoro SILENCIADO` |
| Voltar as condições ao normal | Alarme é rearmado automaticamente |

### Exemplo de saída na Serial
```
=== Estacao de Monitoramento de Jardim Inteligente ===
Sistema pronto. Leituras a cada 2 segundos.
[    1s] Temp:  25.0 C | Ar:  55 % | Solo:  50 % | Irrigacao: DESLIGADA | Alarme: NORMAL
[EVENTO] Irrigacao LIGADA  (solo em 22%)
[    5s] Temp:  25.0 C | Ar:  55 % | Solo:  22 % | Irrigacao: LIGADA    | Alarme: NORMAL
```

---

## 🛠️ Problemas comuns

| Problema | Solução |
|---|---|
| `Firmware not found` no Wokwi | Rode o **Build** antes. Confira se `esp32dev` no `wokwi.toml` é igual ao `[env:esp32dev]` do `platformio.ini`. |
| Erro de compilação com caminho estranho | A pasta do projeto não pode ter **espaços nem acentos**. |
| Wokwi pede licença | `F1` → **Wokwi: Request a New License Key** e faça login no navegador. |
| LCD sem texto | Confira o endereço I2C `0x27` e as ligações SDA (21) / SCL (22). |

---

## 🧰 Tecnologias
ESP32 · Arduino Framework · PlatformIO · Wokwi · C++ · LiquidCrystal_I2C · Adafruit DHT
