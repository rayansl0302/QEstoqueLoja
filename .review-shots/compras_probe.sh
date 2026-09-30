#!/bin/bash
set -u
mkdir -p /tmp/qehome /tmp/runtime /src/.review-shots
chmod 700 /tmp/runtime
export HOME=/tmp/qehome XDG_DATA_HOME=/tmp/qehome XDG_RUNTIME_DIR=/tmp/runtime DISPLAY=:99
Xvfb :99 -screen 0 1440x900x24 >/tmp/xvfb.log 2>&1 &
sleep 0.3
openbox >/tmp/openbox.log 2>&1 &
sleep 0.3
/build/QEstoqueLoja >/tmp/app-init.log 2>&1 &
sleep 4
import -window root /src/.review-shots/main.png
pkill -f QEstoqueLoja || true
sleep 0.3
DB=$(find /tmp/qehome -name estoque.db | head -1)
echo "DB=$DB"
sqlite3 "$DB" ".schema notas_fiscais"
sqlite3 "$DB" ".schema produtos_nota"
sqlite3 "$DB" ".schema clientes"
echo PROBE_OK
