/*
 * ============================================================================
 *  ESTAÇÃO DE MONITORAMENTO DE JARDIM INTELIGENTE
 *  ETAPA 2 - Projeto base
 *    - DHT22  -> temperatura (°C) e umidade do ar (%)
 *    - Potenciômetro -> simula a sonda de umidade do solo (0% a 100%)
 *    - LCD 16x2 I2C -> exibição local dos dados
 *    - Serial 115200 baud -> telemetria / log
 *    - Leitura periódica a cada 2 segundos (sem usar delay)
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
// LCD I2C: SDA = GPIO 21 e SCL = GPIO 22 (pinos I2C padrão do ESP32)

// ----------------------------------------------------------------------------
//  PARÂMETROS DO SISTEMA
// ----------------------------------------------------------------------------
#define TIPO_DHT              DHT22
#define ENDERECO_LCD          0x27     // Endereço I2C do LCD no Wokwi
#define INTERVALO_LEITURA_MS  2000UL   // Leitura dos sensores a cada 2 s

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

unsigned long ultimaLeitura = 0;

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
//  Linha 2: umidade do solo
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
  snprintf(linha, sizeof(linha), "Solo:%3d%%", umidadeSolo);
  escreverLinhaLCD(1, linha);
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

  Serial.printf("Solo: %3d %%\n", umidadeSolo);
}

// ----------------------------------------------------------------------------
//  SETUP: executa uma vez ao ligar a placa
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== Estacao de Monitoramento de Jardim Inteligente ===");

  // Entrada analógica do solo (resolução de 12 bits)
  analogReadResolution(12);

  // Sensores e display
  dht.begin();
  Wire.begin(21, 22);  // SDA, SCL
  lcd.init();
  lcd.backlight();

  // Tela de abertura
  escreverLinhaLCD(0, "Jardim Intelig.");
  escreverLinhaLCD(1, "Iniciando...");
  delay(1500);  // único delay do programa: só na inicialização

  Serial.println("Sistema pronto. Leituras a cada 2 segundos.");

  // Força a primeira leitura logo no início do loop
  ultimaLeitura = millis() - INTERVALO_LEITURA_MS;
}

// ----------------------------------------------------------------------------
//  LOOP: executa continuamente
//  Usa millis() para temporização não bloqueante.
// ----------------------------------------------------------------------------
void loop() {
  unsigned long agora = millis();

  if (agora - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = agora;

    lerSensores();          // 1. Lê DHT22 e solo
    atualizarLCD();         // 2. Mostra no display
    enviarTelemetria();     // 3. Envia log pela Serial
  }
}