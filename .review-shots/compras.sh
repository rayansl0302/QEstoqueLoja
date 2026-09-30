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
sleep 3
pkill -f QEstoqueLoja || true
sleep 0.3
DB=$(find /tmp/qehome -name estoque.db | head -1)
sqlite3 "$DB" <<'SQL'
INSERT INTO clientes (id, nome, cpf, eh_pf, uf) VALUES (50, 'DISTRIBUIDORA EXEMPLO LTDA', '12345678000199', 0, 'SP');
INSERT INTO notas_fiscais (id, cstat, nnf, serie, modelo, tp_amb, xml_path, valor_total, cnpjemit, chnfe, cuf, finalidade, saida, dhemi, id_emissorcliente)
VALUES (1, '100', 4521, '1', '55', 1, 'xmlNf/entradas/35240912345678000199550010000045211000045210-nfe.xml', 1834.50, '12345678000199', '35240912345678000199550010000045211000045210', '35', 'ENTRADA EXTERNA', 0, '2026-09-28T10:15:00-03:00', 50);
INSERT INTO produtos_nota (quantidade, descricao, preco, codigo_barras, un_comercial, ncm, cfop, csosn, nitem, id_nf, status, adicionado)
VALUES (24, 'CAFE TORRADO E MOIDO 500G', 14.90, '7891234567890', 'UN', '09012100', '5102', '102', 1, 1, 'NORMAL', 0),
       (12, 'ACUCAR REFINADO 1KG', 4.25, '7890000011112', 'UN', '17019900', '5102', '102', 2, 1, 'NORMAL', 0),
       (6, 'OLEO DE SOJA 900ML', 7.80, 'SEM GTIN', 'UN', '15079011', '5102', '102', 3, 1, 'NORMAL', 0),
       (30, 'BISCOITO RECHEADO 130G', 2.35, '7899876543210', 'UN', '19053100', '5102', '102', 4, 1, 'NORMAL', 1),
       (10, 'LEITE INTEGRAL 1L', 5.10, '7896543210987', 'UN', '04012010', '5102', '102', 5, 1, 'NORMAL', 0);
INSERT INTO produtos (quantidade, descricao, preco, codigo_barras, nf, un_comercial) VALUES (3, 'BISCOITO RECHEADO 130G', 3.50, '7899876543210', 0, 'UN');
SQL
/build/QEstoqueLoja >/tmp/app-compras.log 2>&1 &
sleep 4
xdotool mousemove 1252 194 click 1
sleep 2.5
import -window root /src/.review-shots/compras.png
xdotool mousemove 606 592 click 1
sleep 1.2
import -window root /src/.review-shots/compras_confirm.png
xdotool key Return
sleep 2.5
import -window root /src/.review-shots/compras_result.png
xdotool key Return
sleep 1
import -window root /src/.review-shots/compras_after.png
DB=$(find /tmp/qehome -name estoque.db | head -1)
sqlite3 -header "$DB" "SELECT id, quantidade, descricao, preco, codigo_barras, nf, preco_fornecedor, porcent_lucro, ncm FROM produtos;"
sqlite3 -header "$DB" "SELECT nitem, adicionado FROM produtos_nota;"
pkill -f QEstoqueLoja || true
echo CAPTURA_OK
