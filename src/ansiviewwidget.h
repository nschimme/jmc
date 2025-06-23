#ifndef ANSIVIEWWIDGET_H
#ifndef ANSIVIEWWIDGET_H
#define ANSIVIEWWIDGET_H

#include <QWidget>
#include <QStringList>
#include <QColor>
#include <vector>
#include <QFont>

class ProfileManager;
class QScrollBar;

/**
 * @brief The AnsiViewWidget class is responsible for rendering text with ANSI escape codes.
 *
 * This widget serves as the display area for MUD output, both for the main view
 * and for any auxiliary output windows. It handles ANSI color parsing, text wrapping,
 * scrolling, and text selection. It aims to replicate the functionality of CSmcView
 * and CAnsiWnd from the MFC version.
 *
 * Porting Notes:
 * - Replaces CSmcView and CAnsiWnd. A single, configurable widget is preferred.
 * - Uses QPainter for all drawing in its paintEvent.
 * - ANSI parsing logic (SetCurrentANSI, DrawWithANSI from MFC) needs to be ported.
 * - Scrollback buffer (m_strList in MFC) maps to m_scrollbackBuffer (QStringList).
 * - Line wrapping logic (NumOfLines, m_LineCountsList in MFC) needs careful porting.
 *   A more robust approach might involve pre-calculating display lines.
 * - Text selection (OnLButtonDown, OnMouseMove, OnLButtonUp in MFC) will be handled
 *   using Qt mouse events. Mapping screen coordinates to text coordinates is key.
 * - Scrolling (OnVScroll in MFC) will be managed using a QScrollBar and by updating
 *   the visible portion of the scrollback buffer.
 * - Interacts with ProfileManager to get font, colors, and display options.
 * - Receives new lines to display via slots connected to ProfileManager signals.
 * - m_wndCode identifies if it's the main view (0) or an auxiliary view (1-N)
 *   to fetch appropriate data/settings if they differ.
 */
class AnsiViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AnsiViewWidget(QWidget *parent = nullptr);
    ~AnsiViewWidget();

    void setWindowCode(int code);
    int windowCode() const;

    void setProfileManager(ProfileManager* manager);

public slots:
    void appendLine(const QString& line); // Appends a raw line from ProfileManager
    void clearDisplay();
    void updateDisplayOptions(); // Triggered by ProfileManager::displayOptionsChanged
    void updateFont();           // Triggered by ProfileManager::fontChanged
    void updateAnsiColors();     // Triggered by ProfileManager::ansiColorsChanged
    void setFrozen(bool frozen); // To pause/resume updates

private slots:
    void handleScrollbarAction(int action); // Connected to m_scrollBar

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    int m_wndCode; // 0 for main view, 1+ for auxiliary views
    ProfileManager* m_profileManager;

    QStringList m_scrollbackBuffer; // Holds all logical lines (with ANSI codes)
    int m_maxScrollbackLines;       // Max number of logical lines to keep

    // --- ANSI parsing state for the current drawing operation ---
    QColor m_currentFgColor;
    QColor m_currentBgColor;
    bool m_isBold;
    // Palettes are fetched from ProfileManager when updated
    QList<QColor> m_ansiForegroundPalette;
    QList<QColor> m_ansiBackgroundPalette;

    // --- Selection State ---
    bool m_isSelecting;
    QPoint m_selectionStartPoint; // Mouse physical start point
    QPoint m_selectionEndPoint;   // Mouse physical current/end point
    // For actual text selection, we'll need to map these to buffer coordinates

    // --- Font and Line Metrics (derived from m_profileManager->getCurrentFont()) ---
    QFont m_font;
    int m_charAvgWidth; // Average width of a character (for monospace)
    int m_lineHeight;   // Height of a single screen line

    // --- Display & Wrapping Cache ---
    // Structure to hold information about how logical lines are wrapped into screen lines
    struct ScreenLine {
        int sourceBufferIndex; // Index into m_scrollbackBuffer
        int startCharIndexInSource; // Start character index in the source line
        int length;             // Number of characters on this screen line from the source
        QString textSegment;    // The actual text segment for this screen line (could be plain or with ANSI)
                                // This might be redundant if we parse on-the-fly during paint
    };
    std::vector<ScreenLine> m_screenLines; // All renderable screen lines after wrapping
    int m_topScreenLineIndex; // Index in m_screenLines that is currently at the top of the view

    QScrollBar* m_scrollBar;

    // --- Cached Options from ProfileManager ---
    bool m_lineWrap;
    bool m_showTimestamps; // This might be handled by ProfileManager before text is sent
    bool m_rectangleSelection;
    bool m_removeEscFromSelection;
    bool m_showHiddenText; // If FG==BG, how to render
    bool m_isFrozen;       // If true, view updates are paused

    // --- Private Methods ---
    void updateMetrics();        // Recalculate m_charAvgWidth, m_lineHeight based on font
    void rebuildScreenLines();   // Re-calculates m_screenLines based on m_scrollbackBuffer and wrapping
    void updateScrollbarRange();
    QString stripAnsiCodes(const QString& textWithAnsi) const; // Helper for length calculation and plain text copy

    // Core drawing routine for a single logical line from buffer, handles wrapping
    // void drawLogicalLine(QPainter& painter, int bufferIndex, int& currentY); // To be replaced/rethought
    // Helper to draw a segment of text with specific attributes
    // int drawTextSegment(QPainter& painter, int x, int y, const QString& text,
    //                      const QColor& fg, const QColor& bg, bool bold, bool isSelectedSegment); // This will be part of paintEvent's loop
    void applyAnsiCode(const QString& ansiSequence); // Updates m_currentFgColor, etc.

    QPoint mapPointToBufferCoordinates(const QPoint& widgetPoint, int& bufferLineIndex, int& charIndexInLine) const;
    QString getSelectedTextContent() const;
    void copySelectionToClipboard();

    void scrollLines(int numLines); // Positive for down, negative for up
};

#endif // ANSIVIEWWIDGET_H
