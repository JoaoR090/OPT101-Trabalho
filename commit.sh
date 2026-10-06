#!/usr/bin/env bash

# Para o script se algum comando falhar
set -e

# Obtém a data e hora atual
DATA=$(date '+%Y-%m-%d %H:%M:%S')

# Adiciona todas as alterações
git add .

# Verifica se existem alterações para commit
if git diff --cached --quiet; then
    echo "Nenhuma alteração para commitar."
    exit 0
fi

# Cria o commit usando a data como mensagem
git commit -m "$DATA"

echo "Commit criado: $DATA"