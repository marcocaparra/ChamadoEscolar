# PAD (Ponto de Apoio Docente)

Sistema de chamados escolares com ESP32, Node-RED e painel de gestão. O primeiro toque abre o pedido e mantém o LED aceso; um novo toque após pelo menos cinco segundos cancela o pedido e apaga o LED. No painel, a coordenação pode confirmar, cancelar e excluir chamados. As abas são Recebidos, Concluídos, Cancelados e Excluídos. Confirmar, cancelar ou excluir pelo painel também apaga o LED na próxima sincronização. Com o Telegram configurado, o cancelamento e a exclusão pelo painel enviam um aviso ao grupo.

## Baixar no notebook de casa

Instale o Git e, com acesso à conta `marcocaparra`, execute:

```powershell
git clone https://github.com/marcocaparra/PontoDeApoioDocente.git
cd PontoDeApoioDocente
```

Para receber atualizações depois, execute `git pull` dentro da pasta clonada.

## Arquivos

- [Firmware PAD](firmware/PAD/PAD.ino) e [diagnóstico de botões e LEDs](firmware/DiagnosticoPAD/DiagnosticoPAD.ino).
- [Fluxo do Node-RED](node-red/flows.json) e [configuração do servidor e do histórico](node-red/settings.js).
- [Código do painel](web/painel.html). A mesma página está incorporada ao fluxo para permitir a importação no Node-RED.
- [Guia de montagem e operação](docs/guia-completo.txt) e [validação histórica](docs/validacao-historica.txt).

## Configurar o ESP32

1. Instale a Arduino IDE e o suporte de placas ESP32 da Espressif.
2. Copie `firmware/PAD/config.example.h` para `config.h`, na mesma pasta.
3. Preencha nome e senha do Wi-Fi, endereço do computador que executa o Node-RED e porta.
4. Abra `PAD.ino`, selecione placa/porta e carregue o programa.

O arquivo `config.h` é ignorado pelo Git: a senha não é publicada. No notebook de casa, crie esse arquivo novamente com as configurações da rede que utilizará.

Use o IPv4 local do computador que roda o Node-RED. O ESP32 clássico usa Wi-Fi de 2,4 GHz e recebe seu próprio endereço por DHCP.

## Rodar nesta pasta no Windows

Com Node.js 22.9 ou superior, configure o `config.h` e dê dois cliques em **Abrir PAD.cmd**. O atalho instala o Node-RED na primeira execução, prepara o fluxo e abre o painel no endereço definido em `NODE_RED_HOST`. Na janela do Telegram, cole o token e o ID do grupo. Para usar o painel sem Telegram, deixe o token vazio.

Os Deploys, o cadastro local e o histórico ficam na pasta `.node-red`. Para partir de uma exportação local, salve-a como `node-red/flows.local.json` antes da primeira execução. A aba **Dashboard** usa os registros do PAD e oferece filtros por data, sala, serviço e situação.

## Cadastrar caixas pelo painel

Abra **Cadastrar caixas** no painel. Para uma placa, preencha **MAC da placa** e **Sala**. Para um lote, copie as células do Excel ou do Google Planilhas e cole em **Colar lista de caixas**. Com os cabeçalhos **Sala** e **MAC**, as colunas podem estar em qualquer ordem e podem vir junto com outras, como **Kit**. Sem cabeçalho, copie só as duas colunas ou use uma linha `MAC;Sala` por placa. A prévia abre automaticamente após colar. Confira e clique em **Salvar cadastro**. Use o MAC Wi-Fi do ESP32. A placa pode estar desligada durante o cadastro e aparecerá em **Conexão das caixas**.

## Configurar o Node-RED manualmente

1. Instale Node.js LTS e Node-RED. Consulte https://nodered.org/docs/getting-started/windows.
2. Inicie o Node-RED com `node-red/settings.js`. Se você já usa um arquivo de configuração, copie dele a seção `contextStorage` para o seu arquivo e reinicie.
3. Abra `http://localhost:1880`, escolha Import e selecione `node-red/flows.json`.
4. No bloco **Identificar sala**, abra a aba **On Start** (Na inicialização) e cadastre os MACs reais e as salas. O cadastro de exemplo começa vazio; utilize o identificador exibido no Monitor Serial a 115200 baud.
5. Para habilitar o Telegram, defina `PAD_TELEGRAM_TOKEN` e `PAD_TELEGRAM_CHAT_ID` no ambiente em que o Node-RED é iniciado.
6. Clique em Deploy. Abra `http://localhost:1880/status`: deve retornar o sinal de disponibilidade (`PAD_OK` quando solicitado com `X-PAD-Protocol: 1`).
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
