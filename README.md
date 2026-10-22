# OPT101 — Arduino

Trabalho desenvolvido para a disciplina **Tópicos Especiais em Interfaces Computacionais**, utilizando **Arduino**.

## 📚 Disciplina

**Tópicos Especiais em Interfaces Computacionais**

## 💻 Tecnologias

- Arduino
- C/C++ (Arduino)

## 🎯 Objetivo

O objetivo do trabalho é aprender a usar o meio sem fio, com arduínos.

## 📁 Estrutura

```text
.
├── README.md
├── backend/
│   ├── requirements.txt
│   └── server.py
├── frontend/
│   ├── app.js
│   ├── index.html
│   └── styles.css
└── arduino/
    └── arduino.ino
```

## 🚀 Executar a aplicação

O backend usa um ambiente virtual Python próprio e se comunica com o Arduino pela
porta serial USB, em **115200 baud**. Os pré-requisitos, detalhes do frontend e
da API estão em [DOCUMENTATION.md](./DOCUMENTATION.md).

### Backend

Na raiz do projeto, crie e ative o ambiente virtual e instale as dependências:

```bash
python3 -m venv backend/.venv
source backend/.venv/bin/activate
python -m pip install -r backend/requirements.txt
python backend/server.py
```

O servidor ficará disponível em `http://127.0.0.1:8000`. Para sair do ambiente
virtual, use `deactivate`.

### Frontend

Em outro terminal, na raiz do projeto, sirva os arquivos estáticos:

```bash
python3 -m http.server 5500 --directory frontend
```

Abra `http://127.0.0.1:5500`, atualize a lista de portas e conecte à porta do
Arduino. Feche o Monitor Serial da IDE do Arduino antes de conectar, pois uma
porta serial só pode ser usada por um programa de cada vez.

## 👨‍🎓 Trabalho acadêmico

Este projeto foi desenvolvido como atividade acadêmica para a disciplina **Tópicos Especiais em Interfaces Computacionais**.

**Universidade:** Universidade Tecnológica Federal do Paraná
