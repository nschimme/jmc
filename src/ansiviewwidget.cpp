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
    update(); // Request a repaint
}

void AnsiViewWidget::clearDisplay()
{
    m_scrollbackBuffer.clear();
    m_currentFgColor = defaultColors[7];
    m_currentBgColor = defaultColors[0];
    m_isBold = false;
    update();
}

void AnsiViewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setFont(m_font);

    // Fill background completely with default BG color initially
    painter.fillRect(rect(), m_currentBgColor);


    // Determine visible lines based on scroll position (to be implemented)
    // For now, draw last N lines that fit
    int linesToDraw = height() / m_lineHeight;
    m_linesInView = linesToDraw;
    m_charsInLine = width() / m_charWidth;


    int firstLineIdx = qMax(0, m_scrollbackBuffer.size() - linesToDraw);
    m_lineWrapCounts.clear();

    int yPos = 0; // Start drawing from the top

    for (int i = 0; i < m_scrollbackBuffer.size(); ++i) { // Draw all lines for now, until scrolling is perfect
        if (yPos >= height()) break; // Stop if we run out of vertical space

        const QString& line = m_scrollbackBuffer.at(i);
        // Reset ANSI state for each new logical line from buffer
        // This matches typical terminal behavior where attributes don't carry over lines unless explicitly resent.
        // However, the original JMC code seems to carry pStrLastEsc. For now, reset.
        // If pStrLastEsc behavior is desired, m_currentFgColor etc. should NOT be reset here.
        // Let's try to match JMC's pStrLastEsc behavior by NOT resetting here initially.
        // The `parseAndDrawLine` will update these as it goes.

        parseAndDrawLine(painter, line, yPos, i /*lineIndexInView*/);
        // yPos is updated by parseAndDrawLine based on actual lines drawn (due to wrapping)
    }


    // If selection is active, draw selection rectangle (or invert colors in parseAndDrawLine)
    if (m_isSelecting) {
        // This is a simplified selection rectangle.
        // True terminal selection inverts colors of selected text.
        // QRect selectionRect = QRect(m_selectionStartPoint, m_selectionEndPoint).normalized();
        // painter.setCompositionMode(QPainter::CompositionMode_Difference); // Example effect
        // painter.fillRect(selectionRect, Qt::white);
        // For now, parseAndDrawLine should handle selection highlighting.
        // We just trigger a repaint if selection changes.
    }
}

void AnsiViewWidget::parseAndDrawLine(QPainter& painter, const QString& line, int& yPos, int lineIndexInView)
{
    if (m_lineHeight <= 0 || m_charWidth <= 0) return;

    int currentX = 0;
    int segmentStartIndex = 0;
    bool lineWrapped = false;

    // Store initial colors for this line before ANSI codes modify them
    QColor initialLineFg = m_currentFgColor;
    QColor initialLineBg = m_currentBgColor;
    bool initialLineBold = m_isBold;

    for (int j = 0; j < line.length(); ++j) {
        if (line[j] == QChar(0x1B) && j + 1 < line.length() && line[j+1] == QChar('[')) { // ANSI Escape
            // Draw preceding text segment
            if (j > segmentStartIndex) {
                QString segment = line.mid(segmentStartIndex, j - segmentStartIndex);
                // Handle wrapping for this segment
                int charsInSegment = segment.length();
                while(charsInSegment > 0) {
                    if (currentX >= width() && m_charWidth > 0) { // Wrap
                        currentX = 0;
                        yPos += m_lineHeight;
                        lineWrapped = true;
                        if (yPos >= height()) return; // Stop if out of bounds
                        // Fill new wrapped line background
                        painter.fillRect(0, yPos, width(), m_lineHeight, m_currentBgColor);
                    }
                    int charsToDraw = qMin(charsInSegment, (width() - currentX) / m_charWidth);
                    if (charsToDraw <= 0 && currentX < width()) charsToDraw = 1; // Draw at least one if space
                    if (charsToDraw <= 0 && currentX >=width()) break; // No space left

                    QString partToDraw = segment.left(charsToDraw);
                    segment.remove(0, charsToDraw);
                    charsInSegment -= charsToDraw;

                    painter.setPen(m_currentFgColor); // Apply current FG color
                    painter.fillRect(currentX, yPos, partToDraw.length() * m_charWidth, m_lineHeight, m_currentBgColor);
                    painter.drawText(currentX, yPos + painter.fontMetrics().ascent(), partToDraw);
                    currentX += partToDraw.length() * m_charWidth;
                }
            }

            // Parse ANSI code
            int ansiEndIndex = line.indexOf(QLatin1Char('m'), j);
            if (ansiEndIndex != -1) {
                setCurrentAnsiState(line.mid(j, ansiEndIndex - j + 1));
                j = ansiEndIndex; // Continue after 'm'
                segmentStartIndex = j + 1;
            } else { // Incomplete ANSI, treat rest as literal
                segmentStartIndex = j;
                break;
            }
        }
    }

    // Draw remaining segment
    if (segmentStartIndex < line.length()) {
        QString segment = line.mid(segmentStartIndex);
        int charsInSegment = segment.length();
        while(charsInSegment > 0) {
            if (currentX >= width() && m_charWidth > 0) { // Wrap
                currentX = 0;
                yPos += m_lineHeight;
                lineWrapped = true;
                if (yPos >= height()) return;
                 painter.fillRect(0, yPos, width(), m_lineHeight, m_currentBgColor);
            }
            int charsToDraw = qMin(charsInSegment, (width() - currentX) / m_charWidth);
            if (charsToDraw <= 0 && currentX < width()) charsToDraw = 1;
            if (charsToDraw <= 0 && currentX >= width()) break;


            QString partToDraw = segment.left(charsToDraw);
            segment.remove(0, charsToDraw);
            charsInSegment -= charsToDraw;

            painter.setPen(m_currentFgColor);
            painter.fillRect(currentX, yPos, partToDraw.length() * m_charWidth, m_lineHeight, m_currentBgColor);
            painter.drawText(currentX, yPos + painter.fontMetrics().ascent(), partToDraw);
            currentX += partToDraw.length() * m_charWidth;
        }
    }

    // Fill rest of the logical line (even if wrapped) with current BG
    if (currentX < width()) {
         painter.fillRect(currentX, yPos, width() - currentX, m_lineHeight, m_currentBgColor);
    }
    yPos += m_lineHeight; // Move to next line position for subsequent calls

    // Restore colors to how they were at the start of this logical line if we are not carrying over (pStrLastEsc behavior)
    // For pStrLastEsc behavior, these are already updated by setCurrentAnsiState and will carry over.
    // m_currentFgColor = initialLineFg;
    // m_currentBgColor = initialLineBg;
    // m_isBold = initialLineBold;
}


void AnsiViewWidget::setCurrentAnsiState(const QString& ansiCode)
{
    // Example: "\x1b[1;31;44m" -> bold, red fg, blue bg
    QStringList parts = ansiCode.mid(2, ansiCode.length() - 3).split(';'); // Remove \x1b[ and m

    for (const QString& partStr : parts) {
        bool ok;
        int code = partStr.toInt(&ok);
        if (!ok) continue;

        if (code == 0) { // Reset
            m_currentFgColor = defaultColors[7];
            m_currentBgColor = defaultColors[0];
            m_isBold = false;
        } else if (code == 1) { // Bold
            m_isBold = true;
        } else if (code >= 30 && code <= 37) { // Foreground color
            m_currentFgColor = defaultColors[code - 30 + (m_isBold ? 8 : 0)];
        } else if (code >= 40 && code <= 47) { // Background color
            m_currentBgColor = defaultColors[code - 40]; // Bold doesn't affect BG in standard ANSI
        } else if (code >= 90 && code <= 97) { // Bright foreground (aixterm)
             m_currentFgColor = defaultColors[code - 90 + 8];
        } else if (code >= 100 && code <= 107) { // Bright background (aixterm)
             m_currentBgColor = defaultColors[code - 100 + 8];
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
