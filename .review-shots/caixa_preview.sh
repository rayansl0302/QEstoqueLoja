#!/bin/bash
set -u
mkdir -p /tmp/qehome /tmp/runtime /src/.review-shots
chmod 700 /tmp/runtime
export HOME=/tmp/qehome XDG_DATA_HOME=/tmp/qehome XDG_RUNTIME_DIR=/tmp/runtime DISPLAY=:99
Xvfb :99 -screen 0 1440x900x24 >/tmp/xvfb.log 2>&1 &
sleep 0.3
openbox >/tmp/openbox.log 2>&1 &
sleep 0.3
/build/QEstoqueLoja >/tmp/app-caixa.log 2>&1 &
sleep 4
import -window root /src/.review-shots/caixa_main.png
echo CAPTURA_MAIN
# deixa o app rodando para o passo seguinte se necessário
# mata no final do script completo
pkill -f QEstoqueLoja || true
echo CAPTURA_OK
