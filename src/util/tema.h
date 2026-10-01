#ifndef TEMA_H
#define TEMA_H

class QApplication;

// Tema visual do programa: fonte, paleta e folha de estilo global.
// Telas que já têm estilo próprio (.ui) continuam mandando nas propriedades que definem;
// o tema cobre o que elas não definem (campos, tabelas, menus, barras de rolagem, diálogos...).
namespace Tema {
void aplicar(QApplication &app);
}

#endif // TEMA_H
