#include "inputbarwidget.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

InputBarWidget::InputBarWidget(QWidget *parent)
    : QWidget(parent),
      m_historyIndex(-1), // -1 means current line is not from history initially
      m_maxHistorySize(100), // Default, should be configurable
      m_isCompleting(false),
      m_completionIndex(0),
      m_clearInputOnSend(true), // Default behavior
      m_tokenInput(false),
      m_killOneToken(false),
      m_scrollEndOnSend(true),
      m_minStrLenForHistory(1),
      m_cursorAtEndForHistoryRecall(true)

{
    m_inputEdit = new QLineEdit(this);
    m_inputEdit->setObjectName("inputLineEdit"); // For styling or testing

    // Setup completer
    m_keywordModel = new QStringListModel(this);
    m_completer = new QCompleter(m_keywordModel, this);
    m_completer->setWidget(m_inputEdit);
    m_completer->setCompletionMode(QCompleter::PopupCompletion); // Or UnfilteredPopupCompletion
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    // m_inputEdit->setCompleter(m_completer); // Enable if standard completer is used

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_inputEdit);
    layout->setContentsMargins(0,0,0,0); // No extra margins
    setLayout(layout);

    // Event filter for QLineEdit to catch Enter, Tab, Up, Down
    m_inputEdit->installEventFilter(this);

    // Load settings from ProfileManager if available (placeholder)
    // e.g., m_maxHistorySize = profileManager->getHistorySize();
    // m_clearInputOnSend = profileManager->getClearInputOption();
    // etc.

    qDebug() << "InputBarWidget created";
}

InputBarWidget::~InputBarWidget()
{
    qDebug() << "InputBarWidget destroyed";
}

QString InputBarWidget::currentText() const
{
    return m_inputEdit->text();
}

void InputBarWidget::setText(const QString& text)
{
    m_inputEdit->setText(text);
}

void InputBarWidget::clearInput()
{
    m_inputEdit->clear();
}

void InputBarWidget::setFocusToInput()
{
    m_inputEdit->setFocus();
}


void InputBarWidget::updateKeywords(const QStringList& keywords)
{
    m_keywordModel->setStringList(keywords);
    qDebug() << "InputBarWidget keywords updated, count:" << keywords.size();
}

bool InputBarWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_inputEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        int key = keyEvent->key();
        Qt::KeyboardModifiers modifiers = keyEvent->modifiers();

        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            onReturnPressed();
            return true; // Event handled
        } else if (key == Qt::Key_Up) {
            navigateHistory(true);
            return true;
        } else if (key == Qt::Key_Down) {
            navigateHistory(false);
            return true;
        } else if (key == Qt::Key_Tab) {
            // Custom tab completion if completer is not fully handling it
            // For now, let QCompleter try, or implement JMC's specific logic
            handleTabCompletion(); // Call custom handler
            return true; // Prevent default tab behavior (focus change)
        } else if ((key == Qt::Key_V && modifiers & Qt::ControlModifier) ||
                   (key == Qt::Key_Insert && modifiers & Qt::ShiftModifier) ) {
            handlePaste();
            return true; // We handled paste
        }
        // Reset completion state if other keys are pressed
        if (key != Qt::Key_Tab && key != Qt::Key_Shift && key != Qt::Key_Control && key != Qt::Key_Alt) {
            m_isCompleting = false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void InputBarWidget::onReturnPressed()
{
    QString text = m_inputEdit->text();
    int tokenSetupVal = 0;
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) tokenSetupVal = 2;
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) tokenSetupVal = 1;


    if (text.length() >= m_minStrLenForHistory) {
        if (m_history.contains(text)) {
            m_history.removeOne(text);
        }
        m_history.append(text);
        while (m_history.size() > m_maxHistorySize) {
            m_history.removeFirst();
        }
    }
    m_historyIndex = -1; // Reset history navigation index
    m_isCompleting = false;

    emit lineEntered(text, tokenSetupVal);

    // Handle post-send text based on options (mirroring CEditBar::GetLine)
    if (tokenSetupVal == 0) { // Normal send
        if (m_clearInputOnSend) {
            m_inputEdit->clear();
        } else if (m_tokenInput) {
            int spacePos = text.indexOf(' ');
            QString remainingText;
            if (spacePos != -1 && !m_killOneToken) {
                remainingText = text.left(spacePos + 1);
            } else if (spacePos == -1 && !m_killOneToken) {
                remainingText = text + " ";
            }
            // if m_killOneToken is true, and spacePos != -1, remainingText is empty
            m_inputEdit->setText(remainingText);
            if (m_scrollEndOnSend) {
                m_inputEdit->setCursorPosition(remainingText.length());
            } else {
                m_inputEdit->setCursorPosition(0);
            }
        } else { // Not clearing, not tokenizing
             m_inputEdit->selectAll(); // Default MFC behavior often selects all
        }
    } else if (tokenSetupVal & 2) { // Shift pressed
        m_inputEdit->selectAll();
    } else { // Control pressed (tokenSetupVal == 1)
        m_inputEdit->clear();
    }

}

void InputBarWidget::navigateHistory(bool goUp)
{
    if (m_history.isEmpty()) return;

    if (m_historyIndex == -1) { // Was not navigating, store current text
        // m_currentTypedText = m_inputEdit->text(); // Store only if using masked scroll
        m_historyIndex = m_history.size(); // Start from end for 'up'
    }

    if (goUp) {
        if (m_historyIndex > 0) {
            m_historyIndex--;
            m_inputEdit->setText(m_history.at(m_historyIndex));
        }
    } else { // Go down
        if (m_historyIndex < m_history.size() - 1) {
            m_historyIndex++;
            m_inputEdit->setText(m_history.at(m_historyIndex));
        } else if (m_historyIndex == m_history.size() - 1) {
            // Reached end of history, restore originally typed text or clear
            m_historyIndex = -1; // Or m_history.size() to allow going up again
            m_inputEdit->clear(); // Or restore m_currentTypedText
        }
    }
    if (m_cursorAtEndForHistoryRecall) {
        m_inputEdit->setCursorPosition(m_inputEdit->text().length());
    } else {
         m_inputEdit->setCursorPosition(0);
    }
    m_isCompleting = false;
}

void InputBarWidget::handleTabCompletion()
{
    // This is a simplified version of JMC's tab completion.
    // JMC's logic:
    // 1. Get word before cursor.
    // 2. On first Tab, find all matches from pDoc->m_lstTabWords. Add original word.
    // 3. If matches > 1, enter m_bExtending mode. Display next match.
    // 4. Subsequent Tabs cycle through matches.
    // 5. Other key presses reset m_bExtending.

    QString currentLine = m_inputEdit->text();
    int cursorPos = m_inputEdit->cursorPosition();

    if (!m_isCompleting) {
        int wordStartPos = currentLine.lastIndexOf(' ', cursorPos - 1) + 1;
        m_completionPrefix = currentLine.mid(wordStartPos, cursorPos - wordStartPos);

        if (m_completionPrefix.isEmpty()) return; // No prefix to complete

        QStringList potentialMatches;
        // In a real scenario, m_keywordModel->stringList() would come from ProfileManager
        for (const QString& keyword : m_keywordModel->stringList()) {
            if (keyword.startsWith(m_completionPrefix, Qt::CaseInsensitive)) {
                potentialMatches.append(keyword);
            }
        }
        if (potentialMatches.isEmpty() || (potentialMatches.size() == 1 && potentialMatches.first().compare(m_completionPrefix, Qt::CaseInsensitive) == 0) ) {
             m_isCompleting = false; return;
        }

        m_isCompleting = true;
        m_completionIndex = 0; // Start with the first match

        // Insert the first match
        QString firstMatch = potentialMatches.at(m_completionIndex);
        QString textBefore = currentLine.left(wordStartPos);
        QString textAfter = currentLine.mid(cursorPos);
        m_inputEdit->setText(textBefore + firstMatch + textAfter);
        m_inputEdit->setCursorPosition(textBefore.length() + firstMatch.length());

    } else { // Already completing, cycle to next
        // This part needs the list of current matches to cycle through.
        // For simplicity, this is not fully implemented here without storing current matches.
        // A more robust way would be to use QCompleter's signals or iterate its model.
        // Or, re-filter based on m_completionPrefix and advance m_completionIndex.

        // Basic idea if we had currentMatches list:
        // m_completionIndex = (m_completionIndex + 1) % currentMatches.size();
        // QString nextMatch = currentMatches.at(m_completionIndex);
        // ... (reconstruct text and set cursor position) ...

        // For now, just beep or do nothing on subsequent tabs in this simplified version
        QApplication::beep();
    }
}

void InputBarWidget::handlePaste()
{
    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    if (mimeData->hasText()) {
        QString pastedText = mimeData->text();
        QStringList lines = pastedText.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);

        QString currentInput = m_inputEdit->text();
        int selStart = m_inputEdit->selectionStart();
        int selEnd = m_inputEdit->cursorPosition();
        if (selStart == -1) selStart = selEnd; // No selection, just cursor pos

        QString textBeforeSelection = currentInput.left(selStart);
        QString textAfterSelection = currentInput.mid(selEnd);

        for (int i = 0; i < lines.size(); ++i) {
            QString lineToProcess;
            if (i == 0) { // First line of paste
                lineToProcess = textBeforeSelection + lines.at(i);
                if (lines.size() == 1) { // Single line paste
                    lineToProcess += textAfterSelection;
                    m_inputEdit->setText(lineToProcess);
                    m_inputEdit->setCursorPosition(textBeforeSelection.length() + lines.at(i).length());
                } else { // Multi-line paste, first line
                    emit lineEntered(lineToProcess, 0); // Send immediately
                }
            } else if (i < lines.size() -1 ) { // Middle lines of multi-line paste
                 emit lineEntered(lines.at(i), 0); // Send immediately
            } else { // Last line of multi-line paste
                lineToProcess = lines.at(i) + textAfterSelection;
                m_inputEdit->setText(lineToProcess);
                m_inputEdit->setCursorPosition(lines.at(i).length());
            }
        }
    }
}
