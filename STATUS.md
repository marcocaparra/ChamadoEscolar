# PAD — status do projeto

Atualizado em **11/11/2026**, após a apresentação ao professor.

O **Ponto de Apoio Docente (PAD)** é um kit do ENIAC para solicitar Orientação Pedagógica, Limpeza ou Infraestrutura da própria sala. O ESP32 envia o chamado ao Node-RED, que registra o pedido, atualiza o painel e pode notificar um grupo do Telegram.

## O que já funciona no projeto

- **Caixa e painel:** as três botoeiras abrem chamados. Um segundo toque, após cinco segundos, cancela. As ações de confirmar, cancelar e excluir no painel também liberam a botoeira na próxima sincronização.
- **Gestão:** o painel mostra chamados recebidos, concluídos, cancelados e excluídos, além de tempo de espera, filtro por serviço e conexão das caixas por sala.
- **Cadastro:** a engenharia pode associar MAC Wi-Fi e sala pelo painel, individualmente ou colando células de uma planilha. A prévia exige confirmação antes de salvar.
- **Dashboard:** já mostra totais por situação, serviço e data, com filtros por período, serviço, situação e sala. A atualização automática atual usa os registros do Node-RED.
- **Telegram:** com o bot e o grupo configurados, avisa sobre aberturas, cancelamentos pela caixa e cancelamentos ou exclusões no painel.

## Sugestões do professor

| Sugestão | Situação | Próximo passo |
| --- | --- | --- |
| **Relatório de chamados com Google Planilhas** | O Dashboard e os filtros já existem. A integração automática com o Google Planilhas está pendente. | Definir qual planilha usar e confirmar a direção da sincronização. A proposta inicial é o PAD continuar registrando os chamados e atualizar a planilha pelo ID de cada chamado, incluindo mudanças de situação. |
| **Enfermaria** | Sugestão registrada. O escopo ainda está aberto. | Definir com a escola como a solicitação deve ser feita, quem recebe e se envolve uma quarta botoeira ou outra forma de acionamento. |
| **Plano de negócio no Canva** | Material a preparar. | Montar uma apresentação com problema, público, proposta de valor, funcionamento do PAD, piloto, implantação e custos. |
| **Orçamento** | Planilha de custos a preparar. | Levantar quantidades e preços de peças, montagem, instalação e manutenção. Separar custo por kit, custo do piloto e estimativa para as **30 salas planejadas**. |

## Como retomar em outro computador

1. Clone ou atualize o [repositório do PAD](https://github.com/marcocaparra/PontoDeApoioDocente) e siga o [README](README.md).
2. Crie `firmware/PAD/config.h` a partir de `config.example.h` e preencha os dados da rede onde o PAD vai rodar. O token do Telegram e o ID do grupo são informados localmente pelo atalho **Abrir PAD.cmd**.
3. Para migrar uma instalação existente com as mesmas salas e chamados, transfira por um meio autorizado o fluxo em uso e os dados de contexto da pasta local `.node-red/`. Uma cópia nova do GitHub começa com o fluxo de referência e sem os dados da instalação anterior.
4. Para editar o site, use [web/painel.html](web/painel.html) e sincronize o HTML com o bloco `v3_html` de [node-red/flows.json](node-red/flows.json). A instalação em uso tem seu próprio fluxo em `.node-red/flows.json`.

Antes de atualizar este arquivo, confira o código ou o teste correspondente. Registre sugestões como pendentes até que a implementação ou a decisão esteja confirmada.
