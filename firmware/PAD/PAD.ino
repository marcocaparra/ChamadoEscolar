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
const unsigned long ESPERA_CANCELAMENTO_MS = 5000;
const unsigned long INTERVALO_PISCA_MS = 500;
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
uint32_t revisaoAoTocar[3] = {};
unsigned long inicioEsperaCancelamento[3] = {};
bool aguardandoCancelamento[3] = {};
unsigned long ultimaMudanca[3] = {0, 0, 0};
std::atomic<uint32_t> encerramentos[3]{};
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

// So o loop grava na memoria e atualiza o LED de cada servico.
void registrarRevisao(int servico, uint32_t nova, const char* mensagem) {
  if (preferencias.putUInt(CHAVES[servico], nova) != sizeof(uint32_t)) {
    Serial.println("ERRO AO SALVAR: estado e LED mantidos.");
    return;
  }
  portENTER_CRITICAL(&travaEstado);
  revisoes[servico] = nova;
  portEXIT_CRITICAL(&travaEstado);
  const bool ativo = nova % 2;
  aguardandoCancelamento[servico] = ativo;
  if (ativo) inicioEsperaCancelamento[servico] = millis();
  digitalWrite(LEDS_BOTOES[servico], ativo ? HIGH : LOW);
  Serial.printf("%s: %s. Revisao %lu.\n", TIPOS[servico], mensagem, (unsigned long)nova);
}

bool receberEncerramentos(const String& resposta, const uint32_t enviados[3]) {
  const char prefixo[] = "PAD_SYNC_OK\n";
  if (!resposta.startsWith(prefixo)) return false;
  const char* cursor = resposta.c_str() + sizeof(prefixo) - 1;
  uint32_t confirmados[3] = {};
  for (int i = 0; i < 3; i++) {
    uint64_t valor = 0;
    const char* inicio = cursor;
    while (*cursor >= '0' && *cursor <= '9') {
      valor = valor * 10 + (*cursor++ - '0');
      if (valor > UINT32_MAX) return false;
    }
    if (cursor == inicio || *cursor != (i == 2 ? '\0' : ',')
        || (valor != 0 && (valor != enviados[i] || valor % 2 == 0))) return false;
    confirmados[i] = (uint32_t)valor;
    if (i < 2) cursor++;
  }
  for (int i = 0; i < 3; i++) {
    if (confirmados[i]) encerramentos[i].store(confirmados[i]);
  }
  return true;
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
        http.addHeader("X-PAD-Protocol", "1");
        int codigo = http.GET();
        if (codigo == 200) {
          String corpo = http.getString();
          corpo.trim();
          ok = corpo == "PAD_OK" || corpo == "BOTOES_MAGICOS_OK";
        }
        http.end();
      }
    }
    ok = ok && WiFi.status() == WL_CONNECTED;
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
        http.addHeader("X-PAD-Protocol", "3");
        codigo = http.POST(corpo);
        if (codigo == 200) {
          String resposta = http.getString();
          resposta.trim();
          ok = receberEncerramentos(resposta, copia);
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
        if (codigo == 200) Serial.println("Confira se o fluxo do Node-RED suporta o protocolo PAD 3.");
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
  digitalWrite(LED_SISTEMA, LOW);
  // Mantem as revisoes ja gravadas nas placas antes da mudanca de nome.
  if (!preferencias.begin("botoesV3", false)) falhaInicial("ERRO: nao foi possivel abrir a memoria de estados.");
  for (int i = 0; i < 3; i++) {
    pinMode(BOTOES[i], INPUT_PULLUP);
    pinMode(LEDS_BOTOES[i], OUTPUT);
    revisoes[i] = preferencias.getUInt(CHAVES[i], 0);
    aguardandoCancelamento[i] = revisoes[i] % 2;
    inicioEsperaCancelamento[i] = millis();
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
      servidorDisponivel.store(false);
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
  Serial.println("PAD: primeiro toque abre. Outro toque apos 5 segundos cancela.");
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
      Serial.println("Wi-Fi caiu. Estados dos botoes mantidos; sincronizacao volta automaticamente.");
    }
  }
  for (int i = 0; i < 3; i++) {
    if (aguardandoCancelamento[i] && agora - inicioEsperaCancelamento[i] >= ESPERA_CANCELAMENTO_MS) {
      aguardandoCancelamento[i] = false;
    }
    const uint32_t encerrado = encerramentos[i].exchange(0);
    if (encerrado && encerrado == revisoes[i] && encerrado < UINT32_MAX) {
      registrarRevisao(i, encerrado + 1, "ENCERRADO PELO PAINEL / LED APAGADO");
    }
    int leitura = digitalRead(BOTOES[i]);
    if (leitura != ultimaLeitura[i]) {
      ultimaMudanca[i] = agora;
      if (leitura == LOW) revisaoAoTocar[i] = revisoes[i];
    }
    if (agora - ultimaMudanca[i] >= DEBOUNCE_MS && leitura != estadoEstavel[i]) {
      estadoEstavel[i] = leitura;
      if (leitura == LOW && revisaoAoTocar[i] == revisoes[i]) {
        const bool ativo = revisoes[i] % 2;
        if (aguardandoCancelamento[i]) {
          Serial.printf("%s: aguarde 5 segundos antes de cancelar.\n", TIPOS[i]);
        } else if (revisoes[i] == UINT32_MAX || (!ativo && revisoes[i] == UINT32_MAX - 1)) {
          Serial.println("Limite de revisoes atingido. Solicite manutencao do cadastro.");
        } else {
          registrarRevisao(i, revisoes[i] + 1,
                           ativo ? "CANCELADO PELA BOTOEIRA / LED APAGADO" : "ABERTO / LED ACESO");
        }
      }
    }
    ultimaLeitura[i] = leitura;
  }
  // O LED geral exige Wi-Fi e uma resposta recente e valida de /status.
  const bool statusConfirmado = servidorDisponivel.load();
  const unsigned long ultimaResposta = ultimaRespostaStatus.load();
  const bool nodeRedAcessivel = conectado && statusConfirmado &&
    millis() - ultimaResposta < VALIDADE_STATUS_MS;
  // O loop so comeca depois de WiFi.begin(); pisca enquanto aguarda confirmacao.
  const bool faseAcesa = (agora / INTERVALO_PISCA_MS) % 2 == 0;
  digitalWrite(LED_SISTEMA, nodeRedAcessivel || faseAcesa ? HIGH : LOW);
  delay(1);
}
