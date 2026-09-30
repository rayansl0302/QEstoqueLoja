#ifndef LOGACESSODIALOG_H
#define LOGACESSODIALOG_H

#include <QDialog>

class QTableWidget;

// Log de acesso para o gerente: sessões (entrada/saída e motivo) e ações sensíveis
// (uso do PIN do gerente, bloqueio/desbloqueio de sessão, sessão invalidada...).
// Só leitura. O acesso é conferido por quem abre (MainWindow::exigirGerente).
class LogAcessoDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LogAcessoDialog(QWidget *parent = nullptr);

private:
    QTableWidget *tabelaSessoes;
    QTableWidget *tabelaAcoes;
    void carregar();
};

#endif // LOGACESSODIALOG_H
