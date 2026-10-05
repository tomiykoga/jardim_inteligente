/*
 * ============================================================================
 *  ESTAÇÃO DE MONITORAMENTO DE JARDIM INTELIGENTE
 *  Sistemas Microcontrolados - Avaliação NP1 (Trabalho Complementar)
 *  Plataforma: ESP32 DevKit + VS Code + PlatformIO + Wokwi
 * ============================================================================
 *
 *  Projeto base:
 *    - DHT22  -> temperatura (°C) e umidade do ar (%)
 *    - Potenciômetro -> simula a sonda de umidade do solo (0% a 100%)
 *    - LCD 16x2 I2C -> exibição local dos dados
 *    - Serial 115200 baud -> telemetria / log
 *    - Leitura periódica a cada 2 segundos (sem usar delay)
 *
 *  Melhoria 1 - Irrigação automática:
 *    - Módulo relé (bomba d'água) + LED azul indicador
 *    - Liga quando o solo fica abaixo de 30% e desliga acima de 40%
 *      (histerese, para o relé não ficar ligando/desligando sem parar)
 *
 *  Melhoria 2 - Alarme visual e sonoro para condições críticas:
 *    - LED vermelho pisca se a temperatura passar de 35 °C
 *    - LED amarelo acende se o solo estiver seco (<30%) ou o ar seco (<20%)
 *    - Buzzer bipa de forma intermitente se temperatura > 35 °C
 *      ou umidade do ar < 20%
 *    - Botão "silenciar" ligado a uma INTERRUPÇÃO externa: desliga o som
 *      do buzzer até a condição crítica acabar (os LEDs continuam avisando)
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>               // comunicação I2C
#include <LiquidCrystal_I2C.h>  // display LCD com módulo I2C
#include <DHT.h>                // sensor de temperatura e umidade

// ----------------------------------------------------------------------------
//  MAPA DE PINOS (ESP32)
// ----------------------------------------------------------------------------
#define PINO_DHT          4    // Dado do DHT22
#define PINO_SOLO         34   // Potenciômetro (entrada analógica ADC1)
#define PINO_RELE         26   // Entrada IN do módulo relé (bomba)
#define PINO_LED_IRRIG    27   // LED azul: irrigação ligada
#define PINO_LED_CALOR    32   // LED vermelho: calor excessivo
#define PINO_LED_SECO     33   // LED amarelo: solo seco / ar seco
#define PINO_BUZZER       18   // Buzzer
#define PINO_BOTAO        14   // Botão para silenciar o alarme
// LCD I2C: SDA = GPIO 21 e SCL = GPIO 22 (pinos I2C padrão do ESP32)

// ----------------------------------------------------------------------------
//  PARÂMETROS DO SISTEMA
// ----------------------------------------------------------------------------
#define TIPO_DHT              DHT22
#define ENDERECO_LCD          0x27     // Endereço I2C do LCD no Wokwi
#define INTERVALO_LEITURA_MS  2000UL   // Leitura dos sensores a cada 2 s

#define SOLO_LIGA_IRRIGACAO      30    // Abaixo disso: liga a bomba (%)
#define SOLO_DESLIGA_IRRIGACAO   40    // Acima disso: desliga a bomba (%)
#define LIMITE_TEMP_CRITICA      35.0  // Calor excessivo (°C)
#define LIMITE_UMIDADE_AR_BAIXA  20.0  // Ar muito seco (%)

#define FREQ_BUZZER_HZ        2000     // Tom do alarme
#define PERIODO_PISCA_MS      500UL    // Pisca LED / bipa buzzer a cada 0,5 s
#define DEBOUNCE_BOTAO_MS     200UL    // Filtro contra trepidação do botão

// ----------------------------------------------------------------------------
//  OBJETOS DOS PERIFÉRICOS
// ----------------------------------------------------------------------------
LiquidCrystal_I2C lcd(ENDERECO_LCD, 16, 2);
DHT dht(PINO_DHT, TIPO_DHT);

// ----------------------------------------------------------------------------
//  VARIÁVEIS GLOBAIS DE ESTADO
// ----------------------------------------------------------------------------
float temperatura = 0;        // °C
float umidadeAr = 0;          // %
int   umidadeSolo = 0;        // %
bool  leituraDhtOk = false;   // false se o DHT22 falhar

bool irrigacaoLigada = false;
bool alarmeCalor = false;     // temperatura > 35 °C
bool alarmeArSeco = false;    // umidade do ar < 20%
bool alertaSoloSeco = false;  // solo < 30%
bool alarmeSilenciado = false;

bool mostrarTelaAlerta = false;  // alterna a linha 2 do LCD quando há alarme

unsigned long ultimaLeitura = 0;
unsigned long ultimoPisca = 0;
bool faseAlarme = false;         // liga/desliga do pisca e do bipe
bool buzzerTocando = false;      // estado atual do buzzer

// Variáveis usadas dentro da interrupção precisam ser "volatile"
volatile bool botaoFoiPressionado = false;
unsigned long ultimoBotao = 0;

// ----------------------------------------------------------------------------
//  ROTINA DE INTERRUPÇÃO (ISR) DO BOTÃO
//  Deve ser curta: apenas sinaliza o evento; o tratamento fica no loop().
//  IRAM_ATTR coloca a função na RAM interna, exigência do ESP32 para ISRs.
// ----------------------------------------------------------------------------
void IRAM_ATTR isrBotaoSilenciar() {
  botaoFoiPressionado = true;
}

// ----------------------------------------------------------------------------
//  LEITURA DOS SENSORES
// ----------------------------------------------------------------------------
void lerSensores() {
  // DHT22: temperatura e umidade do ar
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    leituraDhtOk = false;            // falha na comunicação com o sensor
  } else {
    leituraDhtOk = true;
    temperatura = t;
    umidadeAr = h;
  }

  // Solo: o ADC do ESP32 tem 12 bits (0 a 4095).
  // Convertemos para porcentagem de 0% (seco) a 100% (encharcado).
  int leituraBruta = analogRead(PINO_SOLO);
  umidadeSolo = map(leituraBruta, 0, 4095, 0, 100);
  umidadeSolo = constrain(umidadeSolo, 0, 100);
}

// ----------------------------------------------------------------------------
//  MELHORIA 1: CONTROLE DA IRRIGAÇÃO COM HISTERESE
// ----------------------------------------------------------------------------
void controlarIrrigacao() {
  if (!irrigacaoLigada && umidadeSolo < SOLO_LIGA_IRRIGACAO) {
    irrigacaoLigada = true;
    Serial.printf("[EVENTO] Irrigacao LIGADA  (solo em %d%%)\n", umidadeSolo);
  } else if (irrigacaoLigada && umidadeSolo > SOLO_DESLIGA_IRRIGACAO) {
    irrigacaoLigada = false;
    Serial.printf("[EVENTO] Irrigacao DESLIGADA (solo em %d%%)\n", umidadeSolo);
  }

  digitalWrite(PINO_RELE, irrigacaoLigada ? HIGH : LOW);
  digitalWrite(PINO_LED_IRRIG, irrigacaoLigada ? HIGH : LOW);
}

// ----------------------------------------------------------------------------
//  MELHORIA 2: AVALIAÇÃO DAS CONDIÇÕES CRÍTICAS
// ----------------------------------------------------------------------------
void avaliarAlarmes() {
  bool calorAntes = alarmeCalor;
  bool arSecoAntes = alarmeArSeco;

  // Só avalia temperatura/umidade do ar se o DHT22 respondeu
  alarmeCalor  = leituraDhtOk && (temperatura > LIMITE_TEMP_CRITICA);
  alarmeArSeco = leituraDhtOk && (umidadeAr < LIMITE_UMIDADE_AR_BAIXA);
  alertaSoloSeco = (umidadeSolo < SOLO_LIGA_IRRIGACAO);

  // Registra no Serial quando um alarme começa
  if (alarmeCalor && !calorAntes) {
    Serial.printf("[ALERTA] Temperatura critica: %.1f C\n", temperatura);
  }
  if (alarmeArSeco && !arSecoAntes) {
    Serial.printf("[ALERTA] Ar muito seco: %.0f%%\n", umidadeAr);
  }

  // Quando não há mais condição crítica, o alarme é "rearmado":
  // na próxima ocorrência o buzzer volta a tocar.
  if (!alarmeCalor && !alarmeArSeco && alarmeSilenciado) {
    alarmeSilenciado = false;
    Serial.println("[EVENTO] Condicoes normalizadas - alarme rearmado");
  }

  // LED amarelo: aceso fixo enquanto o solo ou o ar estiverem secos
  digitalWrite(PINO_LED_SECO, (alertaSoloSeco || alarmeArSeco) ? HIGH : LOW);
}

// ----------------------------------------------------------------------------
//  Pisca o LED vermelho e faz o buzzer bipar SEM usar delay(),
//  assim o resto do programa continua rodando normalmente.
// ----------------------------------------------------------------------------
void atualizarSinaisDeAlarme() {
  unsigned long agora = millis();
  if (agora - ultimoPisca >= PERIODO_PISCA_MS) {
    ultimoPisca = agora;
    faseAlarme = !faseAlarme;
  }

  // LED vermelho pisca enquanto houver calor excessivo
  digitalWrite(PINO_LED_CALOR, (alarmeCalor && faseAlarme) ? HIGH : LOW);

  // Buzzer bipa se houver condição crítica e o alarme não foi silenciado
  // (só chama tone/noTone quando o estado muda, para não sobrecarregar)
  bool deveTocar = (alarmeCalor || alarmeArSeco) && !alarmeSilenciado && faseAlarme;
  if (deveTocar && !buzzerTocando) {
    tone(PINO_BUZZER, FREQ_BUZZER_HZ);
    buzzerTocando = true;
  } else if (!deveTocar && buzzerTocando) {
    noTone(PINO_BUZZER);
    buzzerTocando = false;
  }
}

// ----------------------------------------------------------------------------
//  Trata o botão (sinalizado pela interrupção) com debounce por software
// ----------------------------------------------------------------------------
void tratarBotao() {
  if (!botaoFoiPressionado) return;
  botaoFoiPressionado = false;

  unsigned long agora = millis();
  if (agora - ultimoBotao < DEBOUNCE_BOTAO_MS) return;  // trepidação: ignora
  ultimoBotao = agora;

  if (alarmeCalor || alarmeArSeco) {
    alarmeSilenciado = true;
    noTone(PINO_BUZZER);
    buzzerTocando = false;
    Serial.println("[EVENTO] Alarme sonoro SILENCIADO pelo botao");
  }
}

// ----------------------------------------------------------------------------
//  Escreve uma linha completa no LCD (completa com espaços até 16 colunas,
//  evitando "sobras" de textos anteriores sem precisar de lcd.clear()).
// ----------------------------------------------------------------------------
void escreverLinhaLCD(uint8_t linha, const char *texto) {
  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%-16s", texto);
  lcd.setCursor(0, linha);
  lcd.print(buffer);
}

// ----------------------------------------------------------------------------
//  ATUALIZAÇÃO DO DISPLAY LCD 16x2
//  Linha 1: temperatura e umidade do ar
//  Linha 2: umidade do solo e estado da irrigação
//           (se houver alarme, alterna com a mensagem de alerta)
// ----------------------------------------------------------------------------
void atualizarLCD() {
  char linha[17];

  // Linha 1
  if (leituraDhtOk) {
    snprintf(linha, sizeof(linha), "T:%.1fC Ar:%.0f%%", temperatura, umidadeAr);
  } else {
    snprintf(linha, sizeof(linha), "Erro no DHT22!");
  }
  escreverLinhaLCD(0, linha);

  // Linha 2
  bool haAlarme = alarmeCalor || alarmeArSeco;
  if (haAlarme && mostrarTelaAlerta) {
    if (alarmeCalor && alarmeArSeco) {
      snprintf(linha, sizeof(linha), "!CALOR + AR SECO");
    } else if (alarmeCalor) {
      snprintf(linha, sizeof(linha), "!! CALOR >35C !!");
    } else {
      snprintf(linha, sizeof(linha), "!AR SECO (<20%%)!");
    }
  } else {
    snprintf(linha, sizeof(linha), "Solo%3d%% Irr:%s",
             umidadeSolo, irrigacaoLigada ? "ON" : "OFF");
  }
  escreverLinhaLCD(1, linha);

  // Na próxima atualização mostra a outra tela
  mostrarTelaAlerta = haAlarme ? !mostrarTelaAlerta : false;
}

// ----------------------------------------------------------------------------
//  TELEMETRIA VIA SERIAL (115200 baud)
// ----------------------------------------------------------------------------
void enviarTelemetria() {
  unsigned long segundos = millis() / 1000;

  if (leituraDhtOk) {
    Serial.printf("[%5lus] Temp: %5.1f C | Ar: %3.0f %% | ",
                  segundos, temperatura, umidadeAr);
  } else {
    Serial.printf("[%5lus] Temp: ERRO    | Ar: ERRO  | ", segundos);
  }

  const char *estado = "NORMAL";
  if (alarmeCalor && alarmeArSeco) estado = "CALOR + AR SECO";
  else if (alarmeCalor)            estado = "CALOR";
  else if (alarmeArSeco)           estado = "AR SECO";

  Serial.printf("Solo: %3d %% | Irrigacao: %-9s | Alarme: %s%s\n",
                umidadeSolo,
                irrigacaoLigada ? "LIGADA" : "DESLIGADA",
                estado,
                alarmeSilenciado ? " (silenciado)" : "");
}

// ----------------------------------------------------------------------------
//  SETUP: executa uma vez ao ligar a placa
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== Estacao de Monitoramento de Jardim Inteligente ===");

  // Saídas digitais
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_LED_IRRIG, OUTPUT);
  pinMode(PINO_LED_CALOR, OUTPUT);
  pinMode(PINO_LED_SECO, OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);

  // Botão com resistor de pull-up interno: solto = HIGH, pressionado = LOW
  pinMode(PINO_BOTAO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PINO_BOTAO), isrBotaoSilenciar, FALLING);

  // Entrada analógica do solo (resolução de 12 bits)
  analogReadResolution(12);

  // Sensores e display
  dht.begin();
  Wire.begin(21, 22);  // SDA, SCL
  lcd.init();
  lcd.backlight();

  // Tela de abertura + teste rápido dos LEDs
  escreverLinhaLCD(0, "Jardim Intelig.");
  escreverLinhaLCD(1, "Iniciando...");
  digitalWrite(PINO_LED_IRRIG, HIGH);
  digitalWrite(PINO_LED_CALOR, HIGH);
  digitalWrite(PINO_LED_SECO, HIGH);
  delay(1500);  // único delay do programa: só na inicialização
  digitalWrite(PINO_LED_IRRIG, LOW);
  digitalWrite(PINO_LED_CALOR, LOW);
  digitalWrite(PINO_LED_SECO, LOW);

  Serial.println("Sistema pronto. Leituras a cada 2 segundos.");

  // Força a primeira leitura logo no início do loop
  ultimaLeitura = millis() - INTERVALO_LEITURA_MS;
}

// ----------------------------------------------------------------------------
//  LOOP: executa continuamente
//  Usa millis() para temporização não bloqueante: o botão, o pisca do LED
//  e o bipe do buzzer continuam respondendo entre uma leitura e outra.
// ----------------------------------------------------------------------------
void loop() {
  unsigned long agora = millis();

  if (agora - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = agora;

    lerSensores();          // 1. Lê DHT22 e solo
    controlarIrrigacao();   // 2. Melhoria 1: relé + LED azul
    avaliarAlarmes();       // 3. Melhoria 2: verifica condições críticas
    atualizarLCD();         // 4. Mostra no display
    enviarTelemetria();     // 5. Envia log pela Serial
  }

  tratarBotao();              // Botão de silenciar (via interrupção)
  atualizarSinaisDeAlarme();  // Pisca LED vermelho e bipa o buzzer
}