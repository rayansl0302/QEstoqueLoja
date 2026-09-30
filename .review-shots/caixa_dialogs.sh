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
import -window root /src/.review-shots/caixa_main.png
pkill -f QEstoqueLoja || true
sleep 0.4

DB=$(find /tmp/qehome -name estoque.db | head -1)
echo "DB=$DB"
TERM=$(hostname | tr '[:lower:]' '[:upper:]')
echo "TERM=$TERM"
sqlite3 "$DB" <<SQL
INSERT INTO operadores (nome, pin_hash, pin_salt, ativo, tentativas_falhas, bloqueado)
VALUES ('Maria', 'x', 'y', 1, 0, 0);
SQL

capturar() {
  local tela=$1
  local out=$2
  pkill -f QEstoqueLoja || true
  sleep 0.3
  /build/QEstoqueLoja --preview-caixa "$tela" >/tmp/app-"$tela".log 2>&1 &
  sleep 3.2
  import -window root "$out"
  pkill -f QEstoqueLoja || true
  sleep 0.3
}

capturar operadores /src/.review-shots/caixa_operadores.png
capturar abrir /src/.review-shots/caixa_abrir.png
capturar historico /src/.review-shots/caixa_historico.png
capturar fechar /src/.review-shots/caixa_fechar_aviso.png

sqlite3 "$DB" <<SQL
INSERT INTO caixas (id_operador, terminal, status, aberto_em, troco_inicial, troco_sugerido)
VALUES (1, '$TERM', 'ABERTO', datetime('now'), 150.00, 150.00);
SQL

capturar fechar /src/.review-shots/caixa_fechar.png
capturar sangria /src/.review-shots/caixa_sangria.png

/build/QEstoqueLoja >/tmp/app-aberto.log 2>&1 &
sleep 3
import -window root /src/.review-shots/caixa_main_aberto.png
pkill -f QEstoqueLoja || true

ls -l /src/.review-shots/caixa_*.png
echo CAPTURA_OK
