# Documentação da Aplicação

## Visão geral

A aplicação tem duas partes:

- **Backend** (`backend/`): um servidor Python que encontra portas seriais,
  conecta ao Arduino a **115200 baud**, lê as mensagens enviadas pela placa e
  disponibiliza os dados por uma API HTTP.
- **Frontend** (`frontend/`): uma página web simples para selecionar a porta,
  conectar ou desconectar, ver o estado da conexão e acompanhar as mensagens
  recebidas.

O Arduino envia os dados por USB serial. O backend classifica como pacote as
linhas que seguem o formato `Rcvd ... Destino: ... Origem: ... Controle: ...
Dados: ...`; outras linhas, como mensagens de confirmação e diagnóstico, também
são mostradas, mas como mensagens comuns.

O código do Arduino não foi alterado. A aplicação apenas lê a saída serial que
o programa atual já produz; não envia comandos para a placa.

## O que é necessário

- Python 3 instalado.
- Arduino conectado ao computador por USB e executando o programa do projeto.
- Permissão do sistema para acessar a porta serial.
- Uma porta USB serial disponível. No Linux, pode ser necessário adicionar o
  usuário ao grupo que tem acesso à porta (frequentemente `dialout`) e entrar
  novamente na sessão.
- O Monitor Serial da IDE do Arduino fechado enquanto a aplicação estiver
  conectada. Em geral, uma porta serial não pode ser aberta por dois programas
  ao mesmo tempo.

Não é necessário instalar dependências JavaScript: o frontend usa HTML, CSS e
JavaScript nativos do navegador. O backend instala `pyserial` no ambiente
virtual local.

## Como executar

Execute os comandos a partir da pasta raiz do projeto.

### 1. Preparar e iniciar o backend

Crie o ambiente virtual e instale as dependências:

```bash
python3 -m venv backend/.venv
```

Ative-o:

```bash
# Linux ou macOS
source backend/.venv/bin/activate
```

```powershell
# Windows PowerShell
backend\.venv\Scripts\Activate.ps1
```

Instale as dependências e inicie o servidor:

```bash
python -m pip install -r backend/requirements.txt
python backend/server.py
```

O backend fica disponível em `http://127.0.0.1:8000`. Mantenha esse terminal
aberto enquanto usar a aplicação. Para sair do ambiente virtual depois, execute
`deactivate`.

### 2. Iniciar o frontend

Abra outro terminal na raiz do projeto e inicie um servidor de arquivos estáticos:

```bash
python3 -m http.server 5500 --directory frontend
```

No Windows, se `python3` não estiver disponível, use `python` no lugar. Abra
`http://127.0.0.1:5500` no navegador. Com o backend em execução, atualize a
lista de portas, selecione a porta do Arduino e pressione **Conectar**.

## Uso da tela

- **Atualizar portas** consulta novamente as portas seriais disponíveis.
- **Conectar** abre a porta selecionada em 115200 baud.
- **Desconectar** encerra a conexão serial.
- **Atividade serial** mostra tanto os pacotes identificados quanto as demais
  mensagens recebidas.
- **Limpar tela** limpa as mensagens exibidas no navegador; não altera os dados
  da placa.

## API do backend

O frontend consulta o backend local pelos seguintes endpoints:

| Método | Endpoint | Função |
| --- | --- | --- |
| `GET` | `/api/ports` | Lista as portas seriais disponíveis. |
| `GET` | `/api/status` | Retorna conexão, porta, velocidade, quantidade de pacotes e horário da última atividade. |
| `GET` | `/api/events?after=ID` | Retorna os eventos posteriores ao identificador informado. |
| `POST` | `/api/connect` | Conecta à porta indicada em JSON, por exemplo `{"port": "/dev/ttyACM0"}`. |
| `POST` | `/api/disconnect` | Desconecta da porta serial. |

## Testes

Com o ambiente virtual ativo, execute na raiz do projeto:

```bash
python -m unittest discover -s backend -v
```
