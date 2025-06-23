#ifndef INPUTBARWIDGET_H
#ifndef INPUTBARWIDGET_H
#define INPUTBARWIDGET_H

#include <QWidget>
#include <QStringList>

class QLineEdit;
class QCompleter;
class QStringListModel;

/**
 * @brief The InputBarWidget class provides the user input field with history and tab-completion.
 *
 * This widget replaces the CEditBar from the MFC version. It contains a QLineEdit
 * for text input and implements features like command history (accessed via Up/Down arrows),
 * tab-completion based on keywords from ProfileManager, and custom multi-line paste handling.
 *
 * Porting Notes:
 * - Wraps a QLineEdit.
 * - Command history (m_History in CEditBar) maps to m_history (QStringList).
 *   Navigation logic (PrevLine, NextLine) will be reimplemented.
 * - Tab-completion (CEditBar::PreTranslateMessage for VK_TAB):
 *   - JMC's tab-completion is a multi-press cycle. This can be implemented by:
 *     1. On first Tab, identify word prefix, get all matches from ProfileManager.
 *     2. If matches found, store them and insert the first. Set a flag (m_isExtendingTabCompletion).
 *     3. Subsequent Tabs cycle through the stored matches.
 *     4. Other key presses reset the flag.
 *   - Alternatively, a QCompleter can be used, but custom logic might be needed in an
 *     event filter to achieve the exact multi-press cycling behavior if QCompleter's default
 *     isn't sufficient. For now, planning custom logic in eventFilter.
 * - Paste handling (CEditBar::DoPaste for multi-line): The eventFilter will intercept
 *   Ctrl+V/Shift+Insert, get clipboard text, split by newlines, and emit lineEntered
 *   for each line, placing the last partial line (if any) back in the QLineEdit.
 * - Configuration options (m_bClearInput, m_bTokenInput, etc.) will be public methods
 *   allowing MainWindow or ProfileManager to set them.
 * - Emits lineEntered(text, tokenSetup) when user submits input, where tokenSetup indicates
 *   Shift/Ctrl state, similar to the original.
 */
class InputBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InputBarWidget(QWidget *parent = nullptr);
    ~InputBarWidget();

    QString currentText() const;
    void setText(const QString& text);
    void clearInput();
    void setFocusToInput();

    // Configuration setters, typically called after ProfileManager loads a profile
    void setHistorySize(int size);
    void setMinStrLenForHistory(int length);
    void setClearInputOnSend(bool clear);
    void setTokenInputOptions(bool enabled, bool killOneToken);
    void setCursorAtEndForHistoryRecall(bool atEnd);
    void setScrollEndOnSend(bool scrollEnd);
    // Method to load history from ProfileManager (e.g. on profile switch)
    void loadHistory(const QStringList& history);
    QStringList getHistory() const; // To save history via ProfileManager

public slots:
    void updateKeywords(const QStringList& keywords); // For tab-completion model

signals:
    void lineEntered(const QString& text, int tokenSetup); // tokenSetup for Shift/Ctrl state

protected:
    // Event filter on QLineEdit to catch Enter, Tab, Up, Down, Paste
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onReturnPressed(); // Triggered by Enter in eventFilter
    // Slot to reset tab completion if text changes by means other than tab completion itself
    void onInputTextChanged(const QString& text);

private:
    QLineEdit* m_inputEdit;
    // QCompleter* m_completer; // Standard Qt completer, might use if custom logic is too complex
    QStringListModel* m_keywordModelForCompleter; // Data model for keywords if using QCompleter

    QStringList m_history;      // Command history buffer
    int m_historyIndex;         // Current position when navigating history (-1 or m_history.size() if on new line)
    QString m_currentTypedText; // Stores text user was typing before starting history navigation (for masked history scroll)

    // --- Tab completion state (for JMC's specific multi-press tab behavior) ---
    bool m_isExtendingTabCompletion;      // True if tab cycling through matches
    QStringList m_currentTabMatches;      // List of words matching current prefix
    int m_currentTabMatchIndex;           // Index in m_currentTabMatches
    QString m_tabCompletionOriginalPrefix;// The part of the word user typed, e.g., "he" for "help"
    QString m_textBeforeTabPrefix;        // Text in input before the prefix being completed, e.g., "cmd "
    QString m_textAfterTabOriginalCursor; // Text in input after original cursor pos at tab start (not strictly needed if replacing whole word)

    // --- Configurable options (mirrored from CEditBar) ---
    int m_maxHistorySize;
    int m_minStrLenForHistory;
    bool m_bClearInputOnSend;
    bool m_bTokenInput;      // If true, process input as tokens after sending
    bool m_bKillOneToken;    // Works with m_bTokenInput
    bool m_bScrollEndOnSend; // If not clearing, move cursor to end
    bool m_bCursorAtEndForHistoryRecall;

    // --- Private Methods ---
    void navigateHistory(bool goUp); // Handles Up/Down arrow keys for history
    void startTabCompletion();       // Initiates tab completion sequence
    void cycleTabCompletion();       // Cycles through matches on subsequent Tab presses
    void resetTabCompletionState(bool textChangedDueToTab = false);  // Resets tab completion variables
    void handlePaste();              // Custom paste logic for multi-line text
};

#endif // INPUTBARWIDGET_H
