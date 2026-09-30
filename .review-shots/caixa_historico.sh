#!/bin/bash
set -eu
mkdir -p /tmp/qehome /tmp/runtime /src/.review-shots
chmod 700 /tmp/runtime
export HOME=/tmp/qehome XDG_DATA_HOME=/tmp/qehome XDG_RUNTIME_DIR=/tmp/runtime DISPLAY=:99
Xvfb :99 -screen 0 1440x900x24 >/tmp/xvfb.log 2>&1 &
sleep 0.3
openbox >/tmp/openbox.log 2>&1 &
sleep 0.3
/build/QEstoqueLoja >/tmp/app-init.log 2>&1 &
sleep 4
pkill -f QEstoqueLoja || true
sleep 0.5
DB=$(find /tmp/qehome -name estoque.db | head -1)
TERM=$(hostname | tr '[:lower:]' '[:upper:]')
sqlite3 "$DB" "INSERT INTO operadores (nome, pin_hash, pin_salt, ativo, tentativas_falhas, bloqueado) VALUES ('Maria', 'x', 'y', 1, 0, 0);"
sqlite3 "$DB" "INSERT INTO caixas (id_operador, terminal, status, aberto_em, troco_inicial, troco_sugerido) VALUES (1, '$TERM', 'ABERTO', datetime('now'), 150.00, 150.00);"
/build/QEstoqueLoja --preview-caixa historico >/tmp/app-hist.log 2>&1 &
sleep 4
import -window root /src/.review-shots/caixa_historico.png
pkill -f QEstoqueLoja || true
ls -l /src/.review-shots/caixa_historico.png
echo HIST_OK
