# ChamadoEscolar

Sistema de chamados escolares com ESP32, Node-RED e painel de gestão. O primeiro toque abre o pedido e mantém o LED aceso; o segundo encerra o pedido e apaga o LED. O painel separa os pedidos recebidos dos encerrados pelo botão.

## Baixar no notebook de casa

Instale o Git e, com acesso à conta `marcocaparra`, execute:

```powershell
git clone https://github.com/marcocaparra/ChamadoEscolar.git
cd ChamadoEscolar
```

O repositório é privado. O Git pode solicitar autenticação pelo navegador. Para receber atualizações depois, execute `git pull` dentro desta pasta.

## Arquivos

- `firmware/botoes_magicos_v3/`: programa principal do ESP32.
- `diagnostico_botoes_leds/`: teste dos botões e LEDs.
- `node-red/flows.json`: fluxo para importar no Node-RED, sem credenciais.
- `node-red/configuracao_settings.txt`: configuração para salvar o histórico.
- `web/painel_gestao.html`: referência do painel já incorporado ao fluxo.
- `docs/GUIA_COMPLETO.txt`: guia completo de montagem e funcionamento.
- `docs/VALIDACAO.txt`: verificações realizadas anteriormente.

## Configurar o ESP32

1. Instale a Arduino IDE e o suporte de placas ESP32 da Espressif.
2. Copie `firmware/botoes_magicos_v3/config.example.h` para `config.h`, na mesma pasta.
3. Preencha nome e senha do Wi-Fi, endereço do computador que executa o Node-RED e porta.
4. Abra `botoes_magicos_v3.ino`, selecione placa/porta e carregue o programa.

O arquivo `config.h` é ignorado pelo Git: a senha não é publicada. No notebook de casa, crie esse arquivo novamente com as configurações da rede que utilizará.

O endereço `192.168.2.128` usado na escola pode ser diferente em casa. Use o IP local do computador do Node-RED; `localhost` no ESP32 aponta para a própria placa. O ESP32 clássico utiliza Wi-Fi de 2,4 GHz. As 47 placas recebem seus próprios IPs por DHCP.

## Configurar o Node-RED

1. Instale Node.js LTS e Node-RED. Consulte https://nodered.org/docs/getting-started/windows.
2. No `settings.js` do seu Node-RED, configure os stores `memoria` e `arquivo` conforme `node-red/configuracao_settings.txt`. Reinicie o Node-RED.
3. Abra `http://localhost:1880`, escolha Import e selecione `node-red/flows.json`.
4. No bloco **Identificar sala**, cadastre os MACs reais e as salas. O cadastro de exemplo começa vazio; utilize o identificador exibido no Monitor Serial a 115200 baud.
5. Opcionalmente, preencha token e chat ID no bloco **Telegram opcional**.
6. Clique em Deploy. Abra `http://localhost:1880/status`: deve responder `BOTOES_MAGICOS_OK`.
7. Abra `http://localhost:1880/gestao` para testar o painel.

Não mantenha duas versões das mesmas rotas ativas. O arquivo `flows.local.json`, quando presente neste computador, conserva a configuração local para uso particular e é ignorado pelo Git. Dados de pedidos e histórico do servidor não fazem parte do repositório.

## Pinos

| Serviço | Botão | LED |
|---|---:|---:|
| Orientação Pedagógica | 25 | 32 |
| Limpeza | 26 | 33 |
| Infraestrutura | 27 | 23 |
| Sistema/Node-RED | — | 2 |

Botões entre GPIO e GND com `INPUT_PULLUP`. Cada LED comum utiliza seu próprio resistor de 330 Ω em série, com o cátodo no GND. Iluminação integrada de botões pode exigir outro circuito.

## Trabalhar e enviar mudanças

```powershell
git pull
git status
git add .
git commit -m "Descreva a alteração"
git push
```

Se editar os blocos do Node-RED, exporte o fluxo atualizado para `node-red/flows.json` antes do commit e substitua senhas/tokens por exemplos. O Git não exporta mudanças do editor automaticamente. Não versione configurações reais nem cópias de segurança com credenciais.

O guia detalha o comportamento da versão V3. Nesta organização para Git, as configurações do firmware passaram do início do `.ino` para `config.h`; prevalecem as instruções deste README para esse passo.
