#ifndef TEST_ENTRADANFE_SERVICE_H
#define TEST_ENTRADANFE_SERVICE_H

#include <QObject>

class TestEntradaNfeService : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();

    void importa_xml_ok();
    void importa_nota_duplicada_nao_duplica();
    void destinatario_diferente_bloqueia_e_permite_com_confirmacao();
    void nota_cancelada_e_recusada();
    void xml_sem_protocolo_e_recusado();
    void nfce_e_recusada();
    void xml_invalido_e_recusado();
    void nota_propria_e_recusada();
    void completa_nota_que_so_tinha_resumo();
    void importa_nota_com_pis_aliquota_sem_travar();
};

#endif // TEST_ENTRADANFE_SERVICE_H
