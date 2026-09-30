#!/bin/bash
set -u
mkdir -p /tmp/qehome /tmp/runtime /src/.review-shots
chmod 700 /tmp/runtime
export HOME=/tmp/qehome XDG_DATA_HOME=/tmp/qehome XDG_RUNTIME_DIR=/tmp/runtime DISPLAY=:99
Xvfb :99 -screen 0 1440x900x24 >/tmp/xvfb.log 2>&1 &
sleep 0.3
openbox >/tmp/openbox.log 2>&1 &
sleep 0.3
/build/QEstoqueLoja --pdv >/tmp/app-init.log 2>&1 &
sleep 3
pkill -f QEstoqueLoja || true
sleep 0.3
DB=$(find /tmp/qehome -name estoque.db | head -1)
sqlite3 "$DB" "INSERT INTO produtos (quantidade, descricao, preco, codigo_barras, nf, un_comercial) VALUES (5, 'Cafe torrado 500g', 1.40, '7891234567890', 0, 'UN');"
/build/QEstoqueLoja --pdv >/tmp/app-pdv.log 2>&1 &
sleep 4
xdotool key F4
sleep 0.2
xdotool key Return
sleep 0.3
xdotool key F10
sleep 0.8
import -window root /src/.review-shots/pagamento.png
pkill -f QEstoqueLoja || true
echo CAPTURA_OK
