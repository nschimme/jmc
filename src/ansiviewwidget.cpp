#include "ansiviewwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication> // For clipboard
#include <QClipboard>
#include <QScrollBar>
#include <QDebug>

// Temporary default colors - these should ideally come from ProfileManager or a theme
const QColor AnsiViewWidget::defaultColors[16] = {
    QColor(0, 0, 0),        // Black
    QColor(128, 0, 0),      // Dark Red
    QColor(0, 128, 0),      // Dark Green
    QColor(128, 128, 0),    // Dark Yellow
    QColor(0, 0, 128),      // Dark Blue
    QColor(128, 0, 128),    // Dark Magenta
    QColor(0, 128, 128),    // Dark Cyan
    QColor(192, 192, 192),  // Light Gray (Silver)
    QColor(128, 128, 128),  // Dark Gray
    QColor(255, 0, 0),      // Red
    QColor(0, 255, 0),      // Green
    QColor(255, 255, 0),    // Yellow
    QColor(0, 0, 255),      // Blue
    QColor(255, 0, 255),    // Magenta
    QColor(0, 255, 255),    // Cyan
    QColor(255, 255, 255)   // White
};


AnsiViewWidget::AnsiViewWidget(QWidget *parent)
    : QWidget(parent),
      m_wndCode(0),
      m_maxScrollbackLines(1000), // Default, should be configurable
      m_currentFgColor(defaultColors[7]), // Default to light gray
      m_currentBgColor(defaultColors[0]), // Default to black
      m_isBold(false),
      m_isSelecting(false),
      m_charWidth(0),
      m_lineHeight(0),
      m_linesInView(0),
      m_charsInLine(0)
      // m_scrollPosition(0)
{
    // setFont(QFont("Monospace", 10)); // Set a default monospace font
    m_font = QFont("Monospace", 10); // TODO: Load from ProfileManager
    QFontMetrics fm(m_font);
    m_charWidth = fm.horizontalAdvance(QLatin1Char('M')); // Average width for monospace
    m_lineHeight = fm.height();

    // setAutoFillBackground(true); // QPainter will handle background
    setFocusPolicy(Qt::StrongFocus); // To receive mouse events properly

    // Example: Populate with some test lines
    // for(int i = 0; i < 50; ++i) {
    //     appendLine(QString("Test line %1 with \x1b[1;31mred text\x1b[0m and \x1b[32mgreen text\x1b[0m.").arg(i));
    // }
    // appendLine("This is a very long line that should wrap around multiple times to test the wrapping functionality of the AnsiViewWidget. It needs to be significantly longer than the typical width of the widget to ensure that wrapping occurs as expected. More text to make it longer still. And even more.");
    m_scrollBar = new QScrollBar(Qt::Vertical, this);
    connect(m_scrollBar, &QScrollBar::actionTriggered, this, &AnsiViewWidget::handleScrollbarAction);

    updateMetrics(); // Initial calculation
    rebuildScreenLines(); // Initial build
    updateScrollbarRange();

    qDebug() << "AnsiViewWidget created for wndCode:" << m_wndCode;
}

AnsiViewWidget::~AnsiViewWidget()
{
    qDebug() << "AnsiViewWidget destroyed for wndCode:" << m_wndCode;
}

void AnsiViewWidget::setWindowCode(int code)
{
    m_wndCode = code;
}

int AnsiViewWidget::windowCode() const
{
    return m_wndCode;
}

void AnsiViewWidget::appendLine(const QString& line)
{
    m_scrollbackBuffer.append(line);
    if (m_scrollbackBuffer.size() > m_maxScrollbackLines) {
        m_scrollbackBuffer.removeFirst();
    }
    // TODO: Add sophisticated scrollbar handling
    // For now, just repaint
    // update(); // Request a repaint - update() is now called by rebuildScreenLines/updateScrollbarRange
    rebuildScreenLines(); // This will also call update() and updateScrollbarRange()
}

void AnsiViewWidget::clearDisplay()
{
    m_scrollbackBuffer.clear();
    m_screenLines.clear(); // Clear the display cache
    m_currentFgColor = defaultColors[7]; // Reset default colors
    m_currentBgColor = defaultColors[0];
    m_isBold = false;
    m_topScreenLineIndex = 0;
    updateScrollbarRange();
    update(); // Request repaint
}


void AnsiViewWidget::updateMetrics() {
    if (m_font.pointSize() <= 0 && m_font.pixelSize() <= 0) {
        // Fallback if font is not properly set, to prevent division by zero
        m_font.setPointSize(10);
    }
    QFontMetrics fm(m_font);
    m_lineHeight = fm.height();
    m_charAvgWidth = fm.horizontalAdvance(QLatin1Char('M')); // Monospace assumption
    if (m_lineHeight <= 0) m_lineHeight = 12; // Minimum line height
    if (m_charAvgWidth <= 0) m_charAvgWidth = 8; // Minimum char width

    m_linesInView = qMax(1, height() / m_lineHeight);
    m_charsPerLine = qMax(1, (width() - m_scrollBar->width()) / m_charAvgWidth);

    m_scrollBar->setPageStep(m_linesInView);
    m_scrollBar->setSingleStep(1);
    // qDebug() << "Metrics Updated: LineHeight:" << m_lineHeight << "CharWidth:" << m_charAvgWidth
    //          << "LinesInView:" << m_linesInView << "CharsPerLine:" << m_charsPerLine;
}

QString AnsiViewWidget::stripAnsiCodes(const QString& textWithAnsi) const {
    QString plainText;
    plainText.reserve(textWithAnsi.length());
    for (int i = 0; i < textWithAnsi.length(); ++i) {
        if (textWithAnsi[i] == QChar(0x1B)) { // ESC
            // Skip until 'm' or end of string
            int endAnsi = textWithAnsi.indexOf(QLatin1Char('m'), i);
            if (endAnsi != -1) {
                i = endAnsi; // Continue after 'm'
            } else {
                break; // Incomplete ANSI sequence, stop processing
            }
        } else {
            plainText.append(textWithAnsi[i]);
        }
    }
    return plainText;
}

void AnsiViewWidget::rebuildScreenLines() {
    m_screenLines.clear();
    m_lineWrap = true; // TODO: Get this from ProfileManager
    if (m_profileManager) {
        m_lineWrap = m_profileManager->getLineWrap();
    }


    for (int bufferIdx = 0; bufferIdx < m_scrollbackBuffer.size(); ++bufferIdx) {
        const QString& logicalLine = m_scrollbackBuffer.at(bufferIdx);
        if (!m_lineWrap || m_charsPerLine <= 0) {
            m_screenLines.push_back({bufferIdx, 0, logicalLine.length(), logicalLine});
        } else {
            int currentPosInLogicalLine = 0;
            int segmentIndex = 0;
            QString remainingPartOfLogicalLine = logicalLine;

            while (currentPosInLogicalLine < logicalLine.length() || (segmentIndex == 0 && logicalLine.isEmpty())) {
                QString textForThisScreenLine;
                int visibleLengthOnScreen = 0;
                int charsConsumedFromLogical = 0;

                // Iterate through the remaining part of the logical line to build one screen line
                for (int charIdx = 0; charIdx < remainingPartOfLogicalLine.length(); ++charIdx) {
                    QChar currentChar = remainingPartOfLogicalLine[charIdx];
                    if (currentChar == QChar(0x1B)) { // ANSI Escape
                        int endAnsi = remainingPartOfLogicalLine.indexOf(QLatin1Char('m'), charIdx);
                        if (endAnsi != -1) {
                            textForThisScreenLine.append(remainingPartOfLogicalLine.mid(charIdx, endAnsi - charIdx + 1));
                            charsConsumedFromLogical += (endAnsi - charIdx + 1);
                            charIdx = endAnsi; // Advance loop counter
                        } else { // Incomplete ANSI, append rest and break
                            textForThisScreenLine.append(remainingPartOfLogicalLine.mid(charIdx));
                            charsConsumedFromLogical += remainingPartOfLogicalLine.length() - charIdx;
                            break;
                        }
                    } else { // Regular character
                        if (visibleLengthOnScreen < m_charsPerLine) {
                            textForThisScreenLine.append(currentChar);
                            charsConsumedFromLogical++;
                            visibleLengthOnScreen++;
                        } else {
                            break; // This screen line is full
                        }
                    }
                }

                m_screenLines.push_back({bufferIdx, currentPosInLogicalLine, charsConsumedFromLogical, textForThisScreenLine});
                currentPosInLogicalLine += charsConsumedFromLogical;

                if (charsConsumedFromLogical > 0 && charsConsumedFromLogical <= remainingPartOfLogicalLine.length()) {
                     remainingPartOfLogicalLine.remove(0, charsConsumedFromLogical);
                } else {
                    // Should not happen if logic is correct, or means empty line processed
                     if (logicalLine.isEmpty() && segmentIndex == 0) {
                        // Handled the empty line case
                     }
                    break;
                }
                segmentIndex++;
                if (textForThisScreenLine.isEmpty() && charsConsumedFromLogical == 0 && !remainingPartOfLogicalLine.isEmpty()){
                    // This can happen if an ANSI sequence is at the very end.
                    // Or if m_charsPerLine is extremely small. Break to avoid infinite loop.
                    qWarning() << "Empty screen line segment generated with remaining text, breaking wrap for line:" << bufferIdx;
                    break;
                }
                 if (logicalLine.isEmpty() && segmentIndex >0) break; // Ensure empty line only gets one ScreenLine
            }
             if (logicalLine.isEmpty() && segmentIndex == 0) { // Handle case where loop was not entered for empty line
                m_screenLines.push_back({bufferIdx, 0, 0, QString()});
            }
        }
    }

    // Adjust m_topScreenLineIndex if it's now out of bounds
    if (m_topScreenLineIndex >= (int)m_screenLines.size() && !m_screenLines.empty()) {
        m_topScreenLineIndex = m_screenLines.size() - 1;
    }
    if (m_screenLines.empty()) {
        m_topScreenLineIndex = 0;
    }


    updateScrollbarRange();
    update(); // Trigger repaint
}

void AnsiViewWidget::updateScrollbarRange() {
    int maxVal = qMax(0, (int)m_screenLines.size() - m_linesInView);
    m_scrollBar->setRange(0, maxVal);
    m_scrollBar->setValue(m_topScreenLineIndex);
    m_scrollBar->setVisible(maxVal > 0);
}

void AnsiViewWidget::handleScrollbarAction(int action) {
    Q_UNUSED(action); // Action type (e.g. QAbstractSlider::SliderSingleStepAdd) not directly used here
    int newValue = m_scrollBar->value();
    if (newValue != m_topScreenLineIndex) {
        m_topScreenLineIndex = newValue;
        update(); // Repaint
    }
}

void AnsiViewWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_scrollBar->setGeometry(width() - m_scrollBar->width(), 0, m_scrollBar->width(), height());
    updateMetrics();
    rebuildScreenLines(); // Rebuild display cache as wrapping might change
}

void AnsiViewWidget::wheelEvent(QWheelEvent* event) {
    if(m_scrollBar->isVisible()){
        int numDegrees = event->angleDelta().y() / 8;
        int numSteps = -numDegrees / 15; // Standard scroll delta is 120 for one step
        m_scrollBar->setValue(m_scrollBar->value() + numSteps * m_scrollBar->singleStep());
    }
    event->accept();
}


void AnsiViewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setFont(m_font);

    painter.fillRect(rect(), m_currentBgColor); // Default background

    if (m_lineHeight <= 0) return;

    // Reset ANSI state if this is the very first segment of a new logical line being drawn
    // This is a simplification. True "pStrLastEsc" would require knowing if the *previous screen line*
    // was a continuation of the same logical line. For now, this is too complex.
    // Let's assume state carries over unless explicitly reset by \e[0m.
    // The m_currentFgColor, m_currentBgColor, m_isBold are member variables and will persist.

    int yPx = 0; // Pixel Y position for drawing current screen line
    for (int i = 0; i < m_linesInView; ++i) {
        int screenLineIdx = m_topScreenLineIndex + i;
        if (screenLineIdx >= (int)m_screenLines.size()) break;

        const ScreenLine& sLine = m_screenLines[screenLineIdx];
        const QString& textSegmentToDraw = sLine.textSegment;

        int currentXPx = 0;
        int segmentPartStartIndex = 0;

        for (int charIdx = 0; charIdx < textSegmentToDraw.length(); ++charIdx) {
            if (textSegmentToDraw[charIdx] == QChar(0x1B) &&
                charIdx + 1 < textSegmentToDraw.length() &&
                textSegmentToDraw[charIdx+1] == QChar('[')) { // ANSI Escape

                // Draw preceding plain text part of the current segment
                if (charIdx > segmentPartStartIndex) {
                    QString plainPart = textSegmentToDraw.mid(segmentPartStartIndex, charIdx - segmentPartStartIndex);
                    painter.setPen(m_currentFgColor);
                    painter.fillRect(currentXPx, yPx, plainPart.length() * m_charAvgWidth, m_lineHeight, m_currentBgColor);
                    painter.drawText(currentXPx, yPx + painter.fontMetrics().ascent(), plainPart);
                    currentXPx += plainPart.length() * m_charAvgWidth;
                }

                // Parse ANSI code
                int ansiEndIndex = textSegmentToDraw.indexOf(QLatin1Char('m'), charIdx);
                if (ansiEndIndex != -1) {
                    applyAnsiCode(textSegmentToDraw.mid(charIdx, ansiEndIndex - charIdx + 1));
                    charIdx = ansiEndIndex; // Continue after 'm'
                    segmentPartStartIndex = charIdx + 1;
                } else { // Incomplete ANSI, treat rest as literal
                    segmentPartStartIndex = charIdx; // Next segment starts here
                    break;
                }
            }
        }

        // Draw any remaining plain text part of the current segment
        if (segmentPartStartIndex < textSegmentToDraw.length()) {
            QString plainPart = textSegmentToDraw.mid(segmentPartStartIndex);
            painter.setPen(m_currentFgColor);
            painter.fillRect(currentXPx, yPx, plainPart.length() * m_charAvgWidth, m_lineHeight, m_currentBgColor);
            painter.drawText(currentXPx, yPx + painter.fontMetrics().ascent(), plainPart);
            currentXPx += plainPart.length() * m_charAvgWidth;
        }

        // Fill rest of the screen line with current background color
        if (currentXPx < (width() - m_scrollBar->width())) {
            painter.fillRect(currentXPx, yPx, (width() - m_scrollBar->width()) - currentXPx, m_lineHeight, m_currentBgColor);
        }

        yPx += m_lineHeight; // Move to next screen line position
    }
}

// This function is no longer needed as its logic is integrated into paintEvent's loop.
// void AnsiViewWidget::drawLogicalLine(QPainter& painter, int bufferIndex, int& currentYPx) { }


void AnsiViewWidget::applyAnsiCode(const QString& ansiCode)
{
    // Example: "\x1b[1;31;44m" -> bold, red fg, blue bg
    QStringList parts = ansiCode.mid(2, ansiCode.length() - 3).split(';'); // Remove \x1b[ and m

    for (const QString& partStr : parts) {
        bool ok;
        int code = partStr.toInt(&ok);
        if (!ok) continue;

        if (code == 0) { // Reset
            if (m_ansiForegroundPalette.size() > 7) m_currentFgColor = m_ansiForegroundPalette.at(7);
            else m_currentFgColor = defaultColors[7];
            if (m_ansiBackgroundPalette.size() > 0) m_currentBgColor = m_ansiBackgroundPalette.at(0);
            else m_currentBgColor = defaultColors[0];
            m_isBold = false;
        } else if (code == 1) { // Bold
            m_isBold = true;
            // Re-apply current FG color if bold changes it
            int currentFgBase = -1;
            for(int i=0; i<8; ++i) { // Check against first 8 (normal) colors
                if (!m_ansiForegroundPalette.isEmpty() && i < m_ansiForegroundPalette.size() && m_currentFgColor == m_ansiForegroundPalette.at(i)) currentFgBase = i;
                else if (m_currentFgColor == defaultColors[i]) currentFgBase = i;
                if (!m_ansiForegroundPalette.isEmpty() && (i+8) < m_ansiForegroundPalette.size() && m_currentFgColor == m_ansiForegroundPalette.at(i+8)) currentFgBase = i; // Check bright
                else if (m_currentFgColor == defaultColors[i+8]) currentFgBase = i;

            }
            if (currentFgBase != -1) { // if currentFg was a known base color
                 if (m_ansiForegroundPalette.size() > currentFgBase + 8) m_currentFgColor = m_ansiForegroundPalette.at(currentFgBase + 8);
                 else m_currentFgColor = defaultColors[currentFgBase + 8];
            }

        } else if (code >= 30 && code <= 37) { // Foreground color
            int colorIndex = code - 30;
            if (m_isBold) colorIndex += 8;
            if (m_ansiForegroundPalette.size() > colorIndex) m_currentFgColor = m_ansiForegroundPalette.at(colorIndex);
            else if (colorIndex < 16) m_currentFgColor = defaultColors[colorIndex];
            // else, color unchanged if palette too small and index too high
        } else if (code >= 40 && code <= 47) { // Background color
            int colorIndex = code - 40;
            // Original JMC seemed to allow bold to make background colors bright too,
            // but standard ANSI usually doesn't. For now, stick to standard.
            // If m_isBold && !m_profileManager->getDarkOnlyColors() colorIndex +=8;
            if (m_ansiBackgroundPalette.size() > colorIndex) m_currentBgColor = m_ansiBackgroundPalette.at(colorIndex);
            else if (colorIndex < 16) m_currentBgColor = defaultColors[colorIndex];
            // else, color unchanged
        } else if (code >= 90 && code <= 97) { // Bright foreground (aixterm-style)
            int colorIndex = code - 90 + 8; // Directly use bright index
            if (m_ansiForegroundPalette.size() > colorIndex) m_currentFgColor = m_ansiForegroundPalette.at(colorIndex);
            else if (colorIndex < 16) m_currentFgColor = defaultColors[colorIndex];
        } else if (code >= 100 && code <= 107) { // Bright background (aixterm-style)
            int colorIndex = code - 100 + 8;
            if (m_ansiBackgroundPalette.size() > colorIndex) m_currentBgColor = m_ansiBackgroundPalette.at(colorIndex);
            else if (colorIndex < 16) m_currentBgColor = defaultColors[colorIndex];
        }
        // TODO: Add more codes as needed (underline, blink, etc. if porting those)
    }
    // If bold is active and current FG is a dark color, switch to bright version
    // This needs to be done carefully after all codes in a sequence are processed.
    // For simplicity, the above logic for 30-37 already adds 8 if bold.
}


void AnsiViewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isSelecting = true;
        m_selectionStartPoint = event->pos();
        m_selectionEndPoint = event->pos();
        update(); // Repaint to show selection start (if visual feedback is immediate)
    }
}

void AnsiViewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelecting) {
        m_selectionEndPoint = event->pos();
        update(); // Repaint to update selection
    }
}

void AnsiViewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        m_selectionEndPoint = event->pos(); // Final endpoint
        // Process selection (e.g., copy to clipboard)
        copySelectionToClipboard();
        update(); // Repaint to clear selection visual (if any)
    }
}

void AnsiViewWidget::copySelectionToClipboard() {
    if (m_selectionStartPoint == m_selectionEndPoint || m_lineHeight <=0 || m_charWidth <=0) {
        return;
    }

    QRect selectionRect = QRect(m_selectionStartPoint, m_selectionEndPoint).normalized();
    QString selectedText;

    // Determine start and end text coordinates
    // This is a simplified version. A more accurate one would map pixel coords to char cells.
    int startLine = qMax(0, (selectionRect.top() / m_lineHeight) + (m_scrollbackBuffer.size() - m_linesInView) );
    int endLine = qMin(m_scrollbackBuffer.size() - 1, (selectionRect.bottom() / m_lineHeight)  + (m_scrollbackBuffer.size() - m_linesInView));

    int startCol = qMax(0, selectionRect.left() / m_charWidth);
    int endCol = qMin(m_charsInLine -1 , selectionRect.right() / m_charWidth);


    for (int i = startLine; i <= endLine; ++i) {
        if (i >= m_scrollbackBuffer.size() || i < 0) continue;

        QString line = m_scrollbackBuffer.at(i);
        // TODO: Strip ANSI codes from 'line' before taking substring, or handle them.
        // For now, simple substring without ANSI stripping.

        int currentLineStartCol = 0;
        int currentLineEndCol = line.length() -1;

        if (i == startLine) currentLineStartCol = startCol;
        if (i == endLine) currentLineEndCol = endCol;

        if (pDoc && !pDoc->m_bRemoveESCSelection) { // pDoc is not available here, need alternative
             // TODO: Get this option from ProfileManager
        }


        // Simplified: just take the characters, ignoring ANSI for copy for now
        QString textToCopy;
        QString plainLine;
        int visibleCharIndex = 0;
        for(int k=0; k < line.length(); ++k) {
            if (line[k] == QChar(0x1B)) {
                int ansiEnd = line.indexOf('m', k);
                if (ansiEnd != -1) {
                    // if (!stripAnsiForCopy) selectedText.append(line.mid(k, ansiEnd - k + 1));
                    k = ansiEnd;
                    continue;
                }
            }
            if (visibleCharIndex >= currentLineStartCol && visibleCharIndex <= currentLineEndCol) {
                 textToCopy.append(line[k]);
            }
            visibleCharIndex++;
        }
        selectedText.append(textToCopy);
        if (i < endLine) {
            selectedText.append(QLatin1Char('\n'));
        }
    }

    if (!selectedText.isEmpty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(selectedText);
        qDebug() << "Copied to clipboard:" << selectedText;
    }
}

QPoint AnsiViewWidget::mapPointToTextCoordinates(const QPoint& widgetPoint) const
{
    if (m_lineHeight <= 0 || m_charWidth <= 0) return QPoint(0,0);
    // Needs to account for scroll position
    int line = (widgetPoint.y() / m_lineHeight) /* + m_scrollPosition */;
    int col = widgetPoint.x() / m_charWidth;
    return QPoint(col, line);
}

void AnsiViewWidget::updateScrollbar()
{
    // TODO: Implement when QScrollBar is added or QAbstractScrollArea is used
}


// --- Slots for ProfileManager signals ---
void AnsiViewWidget::setProfileManager(ProfileManager* manager) {
    m_profileManager = manager;
    if (m_profileManager) {
        // Initial fetch of settings when manager is set
        updateFont(); // Calls updateMetrics & rebuildScreenLines
        updateAnsiColors(); // Calls rebuildScreenLines
        updateDisplayOptions(); // Calls updateMetrics & rebuildScreenLines
    } else {
        // Reset to defaults if manager is removed
        m_font = QFont("Monospace", 10);
        initializeDefaultAnsiColorsFromPalette(); // Use a helper to set m_currentFg/Bg
        m_lineWrap = true;
        // ... reset other cached options ...
        updateMetrics();
        rebuildScreenLines();
    }
}

void AnsiViewWidget::updateDisplayOptions() {
    if (!m_profileManager) return;

    bool oldLineWrap = m_lineWrap;
    m_lineWrap = m_profileManager->getLineWrap();
    m_rectangleSelection = m_profileManager->getRectangleSelection();
    m_removeEscFromSelection = m_profileManager->getRemoveEscFromSelection();
    m_showHiddenText = m_profileManager->getShowHiddenText();
    // m_maxScrollbackLines = m_profileManager->getScrollbackSize(); // Add to PM if needed

    if (oldLineWrap != m_lineWrap) {
        updateMetrics();      // CharsPerLine might change if scrollbar appears/disappears due to wrapping change
        rebuildScreenLines(); // Line wrapping fundamentally changes screen line layout
    } else {
        update(); // Other options might only require a repaint
    }
    qDebug() << "AnsiViewWidget::updateDisplayOptions for wndCode:" << m_wndCode << "LineWrap:" << m_lineWrap;
}

void AnsiViewWidget::updateFont() {
    if (!m_profileManager) return;
    m_font = m_profileManager->getCurrentFont();
    // Ensure a minimum sensible font size if loaded font is invalid
    if (m_font.pointSize() <= 0 && m_font.pixelSize() <= 0) {
        m_font.setPointSize(10);
    }
    updateMetrics();      // Font change affects metrics
    rebuildScreenLines(); // Font change affects all rendering
    qDebug() << "AnsiViewWidget::updateFont for wndCode:" << m_wndCode << "Font:" << m_font.family();
}

void AnsiViewWidget::updateAnsiColors() {
    if (!m_profileManager) return;
    m_ansiForegroundPalette = m_profileManager->getForegroundColors();
    m_ansiBackgroundPalette = m_profileManager->getBackgroundColors();

    initializeDefaultAnsiColorsFromPalette(); // Sets m_currentFgColor/BgColor from palettes

    rebuildScreenLines(); // Repaint with new colors (indirectly via update())
    qDebug() << "AnsiViewWidget::updateAnsiColors for wndCode:" << m_wndCode;
}

void AnsiViewWidget::initializeDefaultAnsiColorsFromPalette() {
    // Set current drawing colors to default ANSI colors from the loaded palette
    // Default FG is typically color 7 (white/light gray), BG is color 0 (black)
    if (m_ansiForegroundPalette.size() > 7) m_currentFgColor = m_ansiForegroundPalette.at(7);
    else m_currentFgColor = defaultColors[7];

    if (m_ansiBackgroundPalette.size() > 0) m_currentBgColor = m_ansiBackgroundPalette.at(0);
    else m_currentBgColor = defaultColors[0];

    m_isBold = false; // Reset bold state too
}


void AnsiViewWidget::setFrozen(bool frozen) {
    m_isFrozen = frozen;
    if (!m_isFrozen) {
        update(); // Repaint if unfrozen to show any pending updates
    }
    qDebug() << "AnsiViewWidget::setFrozen called with" << frozen << "for wndCode:" << m_wndCode;
}
