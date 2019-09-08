//
// Created by simone on 05/08/19.
//

#include "editor.h"
#include "ui_editor.h"
#include <QMenuBar>
#include <QMenu>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtPrintSupport/qtprintsupportglobal.h>
#include <QPrintDialog>
#include <QPrinter>

Editor::Editor(QString siteId, QWidget *parent, Controller *controller) : textEdit(new QTextEdit(this)), textDocument(textEdit->document()),
                                                                          siteId(siteId), QMainWindow(parent), ui(new Ui::Editor), controller(controller) {
    ui->setupUi(this);
    setWindowTitle(QCoreApplication::applicationName());
    setCentralWidget(textEdit);

    ui->dockWidget->setTitleBarWidget(new QLabel("Online users"));

    ui->userListWidget->resize(this->geometry().width(), this->geometry().height());

    QPixmap pix;
    pix.load("/Users/andrea/Documents/sfondi/preview.jpeg");
    // TODO: from QByteArray to QPixMap

    ui->actionAvatar->setIcon(QIcon(pix));
    ui->actionAvatar->setIconVisibleInMenu(true);

    this->textCursor = textEdit->textCursor();

    setupTextActions();

    // Controller
    connect(textDocument, &QTextDocument::contentsChange,
            this, &Editor::onTextChanged);

    //connect(ui->actionNew_File, &QAction::triggered, this, &Editor::on_actionNew_file_triggered);
    /*connect(ui->actionOpen, &QAction::triggered, this, &Editor::on_actionOpen_triggered);
    connect(ui->actionShare_file, &QAction::triggered, this, &Editor::on_actionShare_file_triggered);
    connect(ui->actionSave_as_PDF, &QAction::triggered, this, &Editor::on_actionSave_as_PDF_triggered);
    connect(ui->actionLogout, &QAction::triggered, this, &Editor::on_actionLogout_triggered);*/
}

void Editor::setupTextActions() {
    QToolBar *tb = addToolBar(tr("Format Actions"));
    QMenu *menu = menuBar()->addMenu(tr("F&ormat"));

    // bold
    const QIcon boldIcon = QIcon::fromTheme("format-text-bold", QIcon(":/images/win/textbold.png"));
    actionTextBold = menu->addAction(boldIcon, tr("&Bold"), this, &Editor::textBold);
    actionTextBold->setShortcut(Qt::CTRL + Qt::Key_B);
    actionTextBold->setPriority(QAction::LowPriority);
    QFont bold;
    bold.setBold(true);
    actionTextBold->setFont(bold);
    tb->addAction(actionTextBold);
    actionTextBold->setCheckable(true);
}

void Editor::textBold() {
    QTextCharFormat fmt;
    fmt.setFontWeight(actionTextBold->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnWordOrSelection(fmt);
}

void Editor::mergeFormatOnWordOrSelection(const QTextCharFormat &format) {
    QTextCursor cursor = textEdit->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
    textEdit->mergeCurrentCharFormat(format);
}

void Editor::setController(Controller *controller) {
    Editor::controller = controller;
}

void Editor::onTextChanged(int position, int charsRemoved, int charsAdded) {
    qDebug() << "editor.cpp - onTextChanged()     position: " << position << " chars added: " << charsAdded << " chars removed: " << charsRemoved;

    saveCursor();

    if(validSignal(position, charsAdded, charsRemoved)) {
        //qDebug() << "VALID SIGNAL";
        //std::cout << "VALID SIGNAL" << std::endl;

        if(position == 0 && charsAdded > 0 && charsRemoved > 0) {
            // correction when paste something in first position.
            charsAdded--;
            charsRemoved--;
        }

        if(charsRemoved) {
            // get startPos
            int line, ch;
            textCursor.setPosition(position);
            line = textCursor.blockNumber();
            ch = textCursor.positionInBlock();
            Pos startPos{ch, line}; // Pos(int ch, int line);

            // get endPos
            undo();
            textCursor.setPosition(position + charsRemoved);
            line = textCursor.blockNumber();
            ch = textCursor.positionInBlock();
            Pos endPos{ch, line}; // Pos(int ch, int line);
            redo();

            //qDebug() << "DELETING: startPos: (" << startPos.getLine() << ", " << startPos.getCh() << ") - endPos: ("  << endPos.getLine() << ", " << endPos.getCh() << ")";
            //qDebug() << "startPos:" << startPos.getLine() << startPos.getCh();
            //qDebug() << "endPos:" << endPos.getLine() << endPos.getCh();
            //qDebug() << "DEBUG - this->controller->localDelete(" << startPos.getLine() << startPos.getCh() << ", " << endPos.getLine() << endPos.getCh() << ")";
            this->controller->localDelete(startPos, endPos);
        }

        if(charsAdded) {
            QTextCursor cursor = textEdit->textCursor();
            QString chars = textEdit->toPlainText().mid(position, charsAdded);

            for(int i=0; i<charsAdded; i++) {
                // for each char added
                cursor.setPosition(position + i);
                int line = cursor.blockNumber();
                int ch = cursor.positionInBlock();
                Pos startPos{ch, line}; // Pos(int ch, int line, const std::string);
                // select char
                cursor.setPosition(position + i + 1, QTextCursor::KeepAnchor);

                CharFormat charFormat = getSelectedCharFormat(cursor);

                this->controller->localInsert(chars.at(i), charFormat, startPos);
            }
        }

        restoreCursor();
    } else {
        //qDebug() << "INVALID SIGNAL";
        //std::cout << "INVALID SIGNAL" << std::endl;
        if(this->startSelection != this->endSelection) {
            // text was selected... restore the selction
            restoreCursorSelection();
        } else {
            restoreCursor();
        }
    }
}

CharFormat Editor::getSelectedCharFormat(QTextCursor cursor) {
    bool    bold = cursor.charFormat().fontWeight() == QFont::Bold,
            italic = cursor.charFormat().fontItalic(),
            underline = cursor.charFormat().fontUnderline();
    QColor color{ cursor.charFormat().foreground().color() };

    //qDebug() << "italic:" << italic;
    //qDebug() << "bold:" << bold;
    //qDebug() << "underline:" << underline;
    //qDebug() << "color:" << color.name();

    CharFormat charFormat{
            bold,
            italic,
            underline,
            color
    };

    return charFormat;
}

void Editor::insertChar(char character, CharFormat charFormat, Pos pos) {
    int oldCursorPos = textCursor.position();

    textCursor.movePosition(QTextCursor::Start);
    textCursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, pos.getLine());
    textCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos.getCh());

    // setting char format
    QTextCharFormat format;
    format.setFontWeight((charFormat.isBold() ? QFont::Bold : QFont::Normal));
    format.setFontItalic(charFormat.isItalic());
    format.setFontItalic(charFormat.isItalic());
    format.setForeground(charFormat.getColor());

    QTextDocument *doc = textEdit->document();
    disconnect(doc, &QTextDocument::contentsChange,
               this, &Editor::onTextChanged);

    textCursor.insertText(QString{character});
    textCursor.select(QTextCursor::BlockUnderCursor);
    //textCursor.mergeCharFormat(format); // TODO BUG HERE! if this is done, from next time a wrong signal is emitted when editor get focus.
    textEdit->mergeCurrentCharFormat(format);

    connect(doc, &QTextDocument::contentsChange,
            this, &Editor::onTextChanged);

    textCursor.setPosition(oldCursorPos);
}

void Editor::deleteChar(Pos pos) {
    int oldCursorPos = textCursor.position();

    textCursor.movePosition(QTextCursor::Start);
    textCursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, pos.getLine());
    textCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos.getCh());

    QTextDocument *doc = textEdit->document();
    disconnect(doc, &QTextDocument::contentsChange,
               this, &Editor::onTextChanged);

    textCursor.deleteChar();

    connect(doc, &QTextDocument::contentsChange,
            this, &Editor::onTextChanged);

    textCursor.setPosition(oldCursorPos);
}

void Editor::on_actionNew_File_triggered() {
    QMessageBox::information(this, "File", "File!");
}

void Editor::on_actionShare_file_triggered() {
    QMessageBox::information(this, "Share", "Share!");
}

void Editor::on_actionOpen_triggered() {
    QMessageBox::information(this, "Open", "Open!");
    emit showFinder();
}

void Editor::on_actionSave_as_PDF_triggered() {
    QFileDialog fileDialog(this, tr("Export PDF"));
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.setMimeTypeFilters(QStringList("application/pdf"));
    fileDialog.setDefaultSuffix("pdf");
    if (fileDialog.exec() != QDialog::Accepted)
        return;
    QString fileName = fileDialog.selectedFiles().first();
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    textEdit->document()->print(&printer);
//	statusBar()->showMessage(tr("Exported \"%1\"").arg(QDir::toNativeSeparators(fileName)));

    QMessageBox::information(this, "PDF", "File Esportato");
}

void Editor::on_actionLogout_triggered() {
    QMessageBox::information(this, "Logout", "Logout!");
    emit logout();
}

Editor::~Editor() {
    delete ui;
}

void Editor::undo() {
    disconnect(textDocument, &QTextDocument::contentsChange,
               this, &Editor::onTextChanged);
    textEdit->undo();
    connect(textDocument, &QTextDocument::contentsChange,
            this, &Editor::onTextChanged);
}

void Editor::redo() {
    disconnect(textDocument, &QTextDocument::contentsChange,
               this, &Editor::onTextChanged);
    textEdit->redo();
    connect(textDocument, &QTextDocument::contentsChange,
            this, &Editor::onTextChanged);
}

bool Editor::validSignal(int position, int charsAdded, int charsRemoved) {
    // check if signal is valid
    bool validSignal = true;

    int currentDocumentSize = textDocument->characterCount()-1;
    undo();
    int undoDocumentSize = textDocument->characterCount()-1;
    redo();
    if(charsAdded == charsRemoved && currentDocumentSize != (undoDocumentSize + charsAdded - charsRemoved)) {
        // wrong signal when editor gets focus and something happen.
        //qDebug() << "WRONG SIGNAL 1";
        validSignal = false;
    }

    if(validSignal && charsAdded == charsRemoved && (position+charsRemoved) > (textDocument->characterCount()-1)) {
        //qDebug() << "WRONG SIGNAL 2";
        // wrong signal when client add new line after it takes focus or when it move the cursor in the editor after the focus acquired
        validSignal = false;
    }

    QString test = textEdit->toPlainText().mid(position, charsAdded);
    if(validSignal && charsAdded == charsRemoved && test.isEmpty()) {
        // wrong signal when editor opens.
        //qDebug() << "WRONG SIGNAL 3";
        validSignal = false;
    }

    // check if text selected
    bool textSelected = false;
    int beginPos = textEdit->textCursor().selectionStart();
    int endPos = textEdit->textCursor().selectionEnd();
    int currentSize = textEdit->toPlainText().size();
    if(beginPos != endPos) {
        textSelected = true;
    }
    if(validSignal && textSelected && charsAdded == charsRemoved && currentSize != 0) {
        // this solve the bug when we select text (in multilines), when the textedit has not the fucus, and we delete them.
        //qDebug() << "WRONG SIGNAL 4";
        validSignal = false;
    }

    return validSignal;
}

void Editor::resizeEvent(QResizeEvent *event) {
    ui->userListWidget->resize(textEdit->geometry().width(), textEdit->geometry().height() - 18);
}

void Editor::removeUser(QString user) {
    users.erase(std::remove_if(users.begin(), users.end(), [user](const QString &s) {
        return s == user;
    }));

    ui->userListWidget->clear();
    ui->userListWidget->addItems(users);
}

void Editor::setUsers(QStringList users) {
    this->users = users;
    ui->userListWidget->addItems(users);
}

void Editor::saveCursor() {
    QTextCursor cursor = this->textEdit->textCursor();
    this->cursorPos = cursor.position();
    this->startSelection = cursor.selectionStart();
    this->endSelection = cursor.selectionEnd();
}

void Editor::restoreCursor() {
    QTextCursor cursor = this->textCursor;
    cursor.setPosition(this->cursorPos, QTextCursor::MoveAnchor);
    textEdit->setTextCursor(cursor);
}

void Editor::restoreCursorSelection() {
    QTextCursor cursor = this->textCursor;
    cursor.setPosition(this->startSelection);
    cursor.setPosition(this->endSelection, QTextCursor::KeepAnchor);
    textEdit->setTextCursor(cursor);
}

void Editor::replaceText(const QString initialText) {
    QTextDocument *doc = textEdit->document();

    disconnect(doc, &QTextDocument::contentsChange,
               this, &Editor::onTextChanged);

    textEdit->setText(initialText);

    connect(doc, &QTextDocument::contentsChange,
            this, &Editor::onTextChanged);

    QTextCursor newCursor = textEdit->textCursor();
    newCursor.movePosition(QTextCursor::End);
    textEdit->setTextCursor(newCursor);
}

void Editor::reset() {
    ui->userListWidget->clear();
}