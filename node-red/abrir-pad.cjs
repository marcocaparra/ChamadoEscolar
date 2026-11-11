const { spawn } = require('node:child_process');
const { constants, copyFileSync, existsSync, mkdirSync, readFileSync } = require('node:fs');
const { networkInterfaces } = require('node:os');
const path = require('node:path');
const { setTimeout: aguardar } = require('node:timers/promises');

const projeto = path.resolve(__dirname, '..');
const runtime = path.join(projeto, '.node-red');
const entrada = path.join(runtime, 'node_modules', 'node-red', 'red.js');
const fluxo = path.join(runtime, 'flows.json');

async function responderStatus(base) {
    try {
        const resposta = await fetch(base + '/status', {
            headers: { 'X-PAD-Protocol': '1' },
            signal: AbortSignal.timeout(800)
        });
        return resposta.ok && (await resposta.text()).trim() === 'PAD_OK';
    } catch {
        return false;
    }
}

function lerTelegram() {
    const token = process.env.PAD_TELEGRAM_TOKEN?.trim();
    const grupo = process.env.PAD_TELEGRAM_CHAT_ID?.trim();
    if (token && /^-?\d+$/.test(grupo || '')) {
        return Promise.resolve({ PAD_TELEGRAM_TOKEN: token, PAD_TELEGRAM_CHAT_ID: grupo });
    }
    const formulario = String.raw`
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$form = [System.Windows.Forms.Form]::new()
$form.Text = 'PAD - Telegram'
$form.ClientSize = [System.Drawing.Size]::new(460, 192)
$form.StartPosition = 'CenterScreen'
$form.TopMost = $true
$form.Add_Shown({ $form.Activate(); $token.Focus() })
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.MinimizeBox = $false
$form.AutoScaleMode = 'Dpi'

function Add-Label($text, $top) {
    $label = [System.Windows.Forms.Label]::new()
    $label.Text = $text
    $label.AutoSize = $true
    $label.Location = [System.Drawing.Point]::new(16, $top)
    $form.Controls.Add($label)
}
function Add-Field($top, $text) {
    $field = [System.Windows.Forms.TextBox]::new()
    $field.Location = [System.Drawing.Point]::new(16, $top)
    $field.Width = 320
    $field.Text = $text
    $form.Controls.Add($field)
    return $field
}
Add-Label 'Token do bot (Ctrl+V para colar)' 14
$token = Add-Field 34 $env:PAD_TELEGRAM_TOKEN
$token.UseSystemPasswordChar = $true
Add-Label 'ID do grupo (inclua o sinal de menos, se houver)' 68
$grupo = Add-Field 88 $env:PAD_TELEGRAM_CHAT_ID
function Add-PasteButton($field, $top) {
    $button = [System.Windows.Forms.Button]::new()
    $button.Text = 'Colar'
    $button.Location = [System.Drawing.Point]::new(348, $top)
    $button.Size = [System.Drawing.Size]::new(94, 24)
    $button.Tag = $field
    $button.Add_Click({
        param($sender, $eventArgs)
        try {
            $texto = [System.Windows.Forms.Clipboard]::GetText().Trim()
            if (-not $texto) { throw 'Copie o valor no Telegram antes de clicar em Colar.' }
            $sender.Tag.Text = $texto
            $sender.Tag.Focus()
        } catch {
            [void][System.Windows.Forms.MessageBox]::Show($sender.FindForm(), 'Nao foi possivel ler o texto copiado. Copie o valor novamente e clique em Colar.', 'PAD')
        }
    })
    $form.Controls.Add($button)
}
Add-PasteButton $token 33
Add-PasteButton $grupo 87
Add-Label 'Deixe o token vazio para iniciar sem Telegram.' 121

$iniciar = [System.Windows.Forms.Button]::new()
$iniciar.Text = 'Iniciar PAD'
$iniciar.Location = [System.Drawing.Point]::new(330, 152)
$iniciar.Size = [System.Drawing.Size]::new(112, 28)
$iniciar.Add_Click({
    if ($token.Text.Trim() -and $grupo.Text.Trim() -notmatch '^-?\d+$') {
        [void][System.Windows.Forms.MessageBox]::Show($form, 'Cole o ID inteiro do grupo, incluindo o sinal de menos.', 'ID do grupo')
        $grupo.Focus()
        return
    }
    $form.DialogResult = 'OK'
})
$form.Controls.Add($iniciar)
$form.AcceptButton = $iniciar
try {
    if ($form.ShowDialog() -ne 'OK') { exit 2 }
    @{token=$token.Text.Trim(); grupo=$grupo.Text.Trim()} | ConvertTo-Json -Compress
} finally {
    $form.Dispose()
}
`;
    return new Promise((resolve, reject) => {
        console.log('Abrindo a janela do Telegram...');
        const janela = spawn('powershell.exe', ['-NoProfile', '-STA', '-Command', formulario], {
            stdio: ['ignore', 'pipe', 'pipe']
        });
        let dados = '';
        janela.stdout.setEncoding('utf8');
        janela.stdout.on('data', trecho => { dados += trecho; });
        janela.stderr.resume();
        janela.on('error', () => reject(Error('Não foi possível abrir a janela do Telegram.')));
        janela.on('close', codigo => {
            if (codigo === 2) return resolve(null);
            if (codigo !== 0) return reject(Error('A janela do Telegram não conseguiu iniciar.'));
            try {
                const { token, grupo } = JSON.parse(dados);
                resolve({ PAD_TELEGRAM_TOKEN: token, PAD_TELEGRAM_CHAT_ID: token ? grupo : '' });
            } catch {
                reject(Error('Não foi possível ler os dados da janela do Telegram.'));
            }
        });
    });
}

function abrirPainel(base) {
    const url = base + '/gestao';
    console.log('Painel disponível em ' + url);
    const navegador = spawn('explorer.exe', [url], { stdio: 'ignore' });
    navegador.on('error', () => console.log('Abra o endereço acima no navegador.'));
}

async function prepararRuntime() {
    mkdirSync(runtime, { recursive: true });
    if (!existsSync(entrada)) {
        console.log('Instalando o Node-RED para o PAD. Esta etapa só acontece na primeira execução.');
        await new Promise((resolve, reject) => {
            const instalacao = spawn('cmd.exe', ['/d', '/c', 'npm install --save-exact node-red@5.0.9 --no-audit --no-fund'], {
                cwd: runtime, stdio: 'inherit'
            });
            instalacao.on('error', reject);
            instalacao.on('exit', codigo => {
                if (codigo === 0 && existsSync(entrada)) resolve();
                else reject(Error('Não foi possível instalar o Node-RED.'));
            });
        });
    }
    if (!existsSync(fluxo)) {
        const local = path.join(__dirname, 'flows.local.json');
        const origem = existsSync(local) ? local : path.join(__dirname, 'flows.json');
        try {
            copyFileSync(origem, fluxo, constants.COPYFILE_EXCL);
        } catch (erro) {
            if (erro.code !== 'EEXIST') throw erro;
        }
    }
}

async function iniciar() {
    const arquivoConfig = path.join(projeto, 'firmware', 'PAD', 'config.h');
    if (!existsSync(arquivoConfig)) {
        throw Error('Copie firmware/PAD/config.example.h para config.h e informe o IP deste computador.');
    }
    const config = readFileSync(arquivoConfig, 'utf8');
    const host = config.match(/\bNODE_RED_HOST\s*=\s*"([^"]+)"\s*;/)?.[1];
    const porta = Number(config.match(/\bNODE_RED_PORTA\s*=\s*(\d+)\s*;/)?.[1]);
    if (!Number.isInteger(porta) || porta < 1 || porta > 65535) {
        throw Error('Informe uma porta TCP válida em NODE_RED_PORTA no config.h.');
    }
    const ips = Object.values(networkInterfaces()).flat()
        .filter(rede => rede.family === 'IPv4' && !rede.internal)
        .map(rede => rede.address);
    if (!ips.includes(host)) {
        throw Error('O endereço do config.h não pertence a este computador. IPs disponíveis: ' + (ips.join(', ') || 'nenhum'));
    }
    const base = 'http://' + host + ':' + porta;
    if (await responderStatus(base)) {
        abrirPainel(base);
        return;
    }
    if (await responderStatus('http://127.0.0.1:' + porta)) {
        throw Error('O PAD já está rodando apenas neste computador. Encerre a janela dele e abra o atalho novamente.');
    }
    await prepararRuntime();
    const telegram = await lerTelegram();
    if (!telegram) return;
    const servidor = spawn(process.execPath, [
        entrada, '--userDir', runtime, '--settings', path.join(__dirname, 'settings.js'),
        '--port', String(porta), fluxo
    ], {
        cwd: projeto, stdio: 'inherit',
        env: { ...process.env, ...telegram, CHAMADO_ESCOLAR_HOST: host }
    });
    let terminou = false;
    servidor.on('error', erro => {
        terminou = true;
        console.error(erro.message);
        process.exitCode = 1;
    });
    servidor.on('exit', codigo => {
        terminou = true;
        process.exitCode = codigo ?? 0;
    });
    for (let i = 0; i < 30 && !terminou; i++) {
        if (await responderStatus(base)) {
            abrirPainel(base);
            console.log('Deixe esta janela aberta durante os testes.');
            return;
        }
        await aguardar(500);
    }
    if (!terminou) console.log('O painel ainda não respondeu. Confira as mensagens do Node-RED acima.');
}

iniciar().catch(erro => {
    console.error(erro.message);
    process.exitCode = 1;
});
