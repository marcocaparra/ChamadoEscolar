#include <Arduino.h>
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// CONFIGURACAO COMUM AS 47 PLACAS; salas cadastradas no Node-RED.


// IP DO COMPUTADOR/SERVIDOR NODE-RED. Nao e o IP publico da internet.


// O ESP32 recebe seu proprio IP, gateway e DNS automaticamente por DHCP.

const int BOTOES[3] = {25, 26, 27};
const int LEDS_BOTOES[3] = {32, 33, 23};
const char* TIPOS[3] = {"pedagogico", "limpeza", "infraestrutura"};
const char* CHAVES[3] = {"rev0", "rev1", "rev2"};
const int LED_SISTEMA = 2;
const unsigned long DEBOUNCE_MS = 50;
const unsigned long SINCRONIZACAO_MS = 2000;
const unsigned long INTERVALO_STATUS_MS = 5000;
const unsigned long VALIDADE_STATUS_MS = 15000;

Preferences preferencias;
uint32_t revisoes[3] = {0, 0, 0};
portMUX_TYPE travaEstado = portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> servidorDisponivel{false};
std::atomic<unsigned long> ultimaRespostaStatus{0};
int ultimaLeitura[3] = {HIGH, HIGH, HIGH};
int estadoEstavel[3] = {HIGH, HIGH, HIGH};
unsigned long ultimaMudanca[3] = {0, 0, 0};
bool estavaConectado = false;
String urlChamado;
String urlStatus;

// Revisao impar = pedido ativo; par = encerrado. Zero = nunca acionado.
// Gravar um unico contador por botao evita salvar LED e estado separadamente.
void copiarEstado(uint32_t destino[3]) {
  portENTER_CRITICAL(&travaEstado);
  for (int i = 0; i < 3; i++) destino[i] = revisoes[i];
  portEXIT_CRITICAL(&travaEstado);
}

void falhaInicial(const char* mensagem) {
  Serial.println(mensagem);
  while (true) delay(1000);
}

// Consulta somente Node-RED. Telegram nao interfere no LED do sistema.
void tarefaStatus(void* parametro) {
  bool anterior = false;
  bool primeira = true;
  for (;;) {
    bool ok = false;
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient cliente;
      HTTPClient http;
      http.setConnectTimeout(2000);
      http.setTimeout(2000);
      if (http.begin(cliente, urlStatus)) {
        int codigo = http.GET();
        if (codigo == 200) {
          String corpo = http.getString();
          corpo.trim();
          ok = corpo == "BOTOES_MAGICOS_OK";
        }
        http.end();
      }
    }
    if (ok) ultimaRespostaStatus.store(millis());
    servidorDisponivel.store(ok);
    if (primeira || ok != anterior) {
      Serial.println(ok ? "NODE-RED DISPONIVEL." : "NODE-RED SEM CONFIRMACAO. Confira /status.");
      anterior = ok;
      primeira = false;
    }
    vTaskDelay(pdMS_TO_TICKS(INTERVALO_STATUS_MS));
  }
}

// Envia ESTADOS ABSOLUTOS e revisoes, nunca uma ordem de alternar.
// Repetir a mesma revisao nao abre/encerra pedidos novamente no servidor.
void tarefaSincronizacao(void* parametro) {
  uint32_t ultimaConfirmada[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
  int ultimoErro = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      uint32_t copia[3];
      copiarEstado(copia);
      String corpo = String("{\"dispositivo\":\"") + WiFi.macAddress() + "\",\"estados\":[";
      for (int i = 0; i < 3; i++) {
        if (i) corpo += ',';
        corpo += String("{\"tipo\":\"") + TIPOS[i] + "\",\"ativo\":";
        corpo += (copia[i] % 2) ? "true" : "false";
        corpo += String(",\"revisao\":") + String((unsigned long)copia[i]) + "}";
      }
      corpo += "]}";
      WiFiClient cliente;
      HTTPClient http;
      http.setConnectTimeout(2000);
      http.setTimeout(4000);
      int codigo = -1;
      bool ok = false;
      if (http.begin(cliente, urlChamado)) {
        http.addHeader("Content-Type", "application/json");
        codigo = http.POST(corpo);
        if (codigo == 200) {
          String resposta = http.getString();
          resposta.trim();
          ok = resposta == "BOTOES_MAGICOS_SYNC_OK";
        }
        http.end();
      }
      if (ok) {
        bool mudou = ultimoErro != 0;
        for (int i = 0; i < 3; i++) {
          if (copia[i] != ultimaConfirmada[i]) mudou = true;
          ultimaConfirmada[i] = copia[i];
        }
        if (mudou) Serial.println("ESTADOS SINCRONIZADOS COM O PAINEL.");
        ultimoErro = 0;
      } else if (ultimoErro != (codigo == 200 ? -2 : codigo)) {
        ultimoErro = codigo == 200 ? -2 : codigo;
        Serial.printf("SINCRONIZACAO PENDENTE (HTTP %d). Tentarei novamente.\n", codigo);
        if (codigo == 404) Serial.println("Cadastre o MAC da placa e confira POST /chamado.");
        if (codigo == 409) Serial.println("Revisao antiga: confira reset de memoria/troca de placa antes de continuar.");
        if (codigo == 503) Serial.println("Configure contextStorage arquivo no Node-RED. Veja o guia.");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(SINCRONIZACAO_MS));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_SISTEMA, OUTPUT);
  digitalWrite(LED_SISTEMA, HIGH);  // Aceso desde o inicio, independente da rede.
  if (!preferencias.begin("botoesV3", false)) falhaInicial("ERRO: nao foi possivel abrir a memoria de estados.");
  for (int i = 0; i < 3; i++) {
    pinMode(BOTOES[i], INPUT_PULLUP);
    pinMode(LEDS_BOTOES[i], OUTPUT);
    revisoes[i] = preferencias.getUInt(CHAVES[i], 0);
    digitalWrite(LEDS_BOTOES[i], (revisoes[i] % 2) ? HIGH : LOW);
    ultimaLeitura[i] = digitalRead(BOTOES[i]);
    estadoEstavel[i] = ultimaLeitura[i];
  }
  String base = String("http://") + NODE_RED_HOST + ":" + String(NODE_RED_PORTA);
  urlChamado = base + "/chamado";
  urlStatus = base + "/status";
  // A reconexao fica por conta do driver; nao chamar reconnect enquanto conecta.
  WiFi.onEvent([](WiFiEvent_t evento, WiFiEventInfo_t info) {
    if (evento == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      Serial.printf("WI-FI DESCONECTADO: motivo %u. Confira rede/senha/frequencia.\n",
                    (unsigned int)info.wifi_sta_disconnected.reason);
    }
  });
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
  if (xTaskCreate(tarefaSincronizacao, "sincronizar", 8192, nullptr, 1, nullptr) != pdPASS) {
    falhaInicial("ERRO: nao foi possivel iniciar sincronizacao.");
  }
  if (xTaskCreate(tarefaStatus, "statusNodeRed", 6144, nullptr, 1, nullptr) != pdPASS) {
    falhaInicial("ERRO: nao foi possivel iniciar consulta de status.");
  }
  Serial.println("Sistema V3: primeiro toque abre; segundo toque encerra. Estado salvo na placa.");
}

void loop() {
  unsigned long agora = millis();
  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado != estavaConectado) {
    estavaConectado = conectado;
    if (conectado) {
      Serial.print("IP do ESP32 (DHCP): "); Serial.println(WiFi.localIP());
      Serial.print("Gateway/roteador: "); Serial.println(WiFi.gatewayIP());
      Serial.print("ID PARA CADASTRAR NO NODE-RED: "); Serial.println(WiFi.macAddress());
    } else {
      servidorDisponivel.store(false);
      Serial.println("Wi-Fi caiu. LEDs mantidos; sincronizacao volta automaticamente.");
    }
  }
  for (int i = 0; i < 3; i++) {
    int leitura = digitalRead(BOTOES[i]);
    if (leitura != ultimaLeitura[i]) ultimaMudanca[i] = agora;
    if (agora - ultimaMudanca[i] >= DEBOUNCE_MS && leitura != estadoEstavel[i]) {
      estadoEstavel[i] = leitura;
      if (leitura == LOW) {
        uint32_t copia[3];
        copiarEstado(copia);
        if (copia[i] == UINT32_MAX) {
          Serial.println("Limite de revisoes atingido. Solicite manutencao do cadastro.");
        } else {
          uint32_t nova = copia[i] + 1;
          // So altera o LED se o novo estado foi salvo com sucesso.
          if (preferencias.putUInt(CHAVES[i], nova) != sizeof(uint32_t)) {
            Serial.println("ERRO AO SALVAR: estado e LED mantidos. Tente novamente.");
          } else {
            portENTER_CRITICAL(&travaEstado);
            revisoes[i] = nova;
            portEXIT_CRITICAL(&travaEstado);
            bool ativo = nova % 2;
            digitalWrite(LEDS_BOTOES[i], ativo ? HIGH : LOW);
            Serial.printf("%s: %s. Revisao %lu.\n", TIPOS[i], ativo ? "ABERTO / LED ACESO" : "ENCERRADO / LED APAGADO", (unsigned long)nova);
          }
        }
      }
    }
    ultimaLeitura[i] = leitura;
  }
  // LED de funcionamento permanece aceso desde setup().
  delay(1);
}
