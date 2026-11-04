#include <Arduino.h>

// Teste temporario: nao usa Wi-Fi nem altera os pedidos salvos na memoria.
// LEDs comuns com resistor: GPIO -> resistor -> anodo; catodo -> GND.
const int leds[4] = {2, 32, 33, 23};
const int botoes[3] = {25, 26, 27};
const char* nomes[4] = {"SISTEMA", "PEDAGOGICO", "LIMPEZA", "INFRAESTRUTURA"};
unsigned long inicioEtapa = 0;
int etapa = 0;
int ultimaLeitura[3] = {HIGH, HIGH, HIGH};

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }
  for (int i = 0; i < 3; i++) pinMode(botoes[i], INPUT_PULLUP);
  Serial.println("TESTE: cada LED acende por 2 segundos, independentemente do botao.");
  Serial.println("Aperte os botoes para conferir LOW=pressionado / HIGH=solto no Serial.");
  digitalWrite(leds[etapa], HIGH);
  Serial.printf("LED %s: GPIO %d ACESO\n", nomes[etapa], leds[etapa]);
  inicioEtapa = millis();
}

void loop() {
  if (millis() - inicioEtapa >= 2000) {
    digitalWrite(leds[etapa], LOW);
    etapa = (etapa + 1) % 4;
    digitalWrite(leds[etapa], HIGH);
    Serial.printf("LED %s: GPIO %d ACESO\n", nomes[etapa], leds[etapa]);
    inicioEtapa = millis();
  }
  for (int i = 0; i < 3; i++) {
    int leitura = digitalRead(botoes[i]);
    if (leitura != ultimaLeitura[i]) {
      Serial.printf("BOTAO GPIO %d: %s\n", botoes[i], leitura == LOW ? "PRESSIONADO (LOW)" : "SOLTO (HIGH)");
      ultimaLeitura[i] = leitura;
    }
  }
  delay(5);
}
