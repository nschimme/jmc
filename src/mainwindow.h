#ifndef MAINWINDOW_H
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "profilemanager.h" // For MAX_AUX_OUTPUTS (ProfileManager::MAX_OUTPUT)


// Forward declarations
class QAction;
class QMenu;
class QToolBar;
class QStatusBar;
class QDockWidget;
class QLabel;

class AnsiViewWidget;
class InputBarWidget;
// class ProfileManager; // Already included for static const

/**
 * @brief The MainWindow class is the main application window.
 *
 * It replaces CMainFrame from the MFC version. It sets up the main UI structure,
 * including menus, toolbars, status bar, the central ANSI display area,
 * an input bar, and multiple dockable auxiliary output windows.
 * It owns the ProfileManager instance and connects UI actions to its slots.
 *
 * Porting Notes:
 * - Derives from QMainWindow to get menu, toolbar, status bar, and docking support.
 * - Toolbar (m_wndToolBar in CMainFrame) maps to QToolBar.
 * - Status bar (m_wndStatusBar/CJMCStatus in CMainFrame) maps to QStatusBar.
 *   - Indicators (ID_INDICATOR_CONNECTED, etc.) will be QLabels or custom widgets
 *     on the QStatusBar. Owner-drawn indicators will need custom QLabel painting or QPixmap.
 * - Edit bar (m_editBar in CMainFrame) maps to InputBarWidget, placed in a QDockWidget.
 * - Output windows (m_coolBar array in CMainFrame) map to an array of QDockWidgets,
 *   each containing an AnsiViewWidget. QMainWindow::saveState/restoreState will handle
 *   their positions/sizes, augmented by QSettings for titles or specific states not
 *   covered by Qt's built-in persistence.
 * - Splitter window (m_wndSplitter in CMainFrame) is replaced by QMainWindow's central
 *   widget area and dock widget system. If a splitter is explicitly needed between
 *   the main view and something else, a QSplitter can be used.
 * - System tray icon (sysTray in CMainFrame) will be managed by QSystemTrayIcon,
 *   likely owned by MainWindow or Application.
 * - Options dialogs (OnOptionsOptions, OnEditJmcobjects) will be invoked from menu actions
 *   and will interact with ProfileManager.
 * - Message handling (WM_USER messages in CMainFrame) will be replaced by Qt's
 *   signals and slots mechanism.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override; // To save settings, profile on exit

private slots:
    // --- File Menu Actions ---
    void newProfile();          // CSmcApp::OnFileNewProfile
    void openProfile();         // CSmcApp::OnFileLoadprofile
    void saveProfile();         // CSmcApp::OnFileSaveprofile
    void appExit();             // CSmcApp::ExitInstance (partially), CFrameWnd::OnClose

    // --- Edit Menu Actions (placeholders, connect to focus widget's slots) ---
    // void copyAction();
    // void pasteAction();
    // void cutAction();
    // void selectAllAction();

    // --- Options Menu Actions ---
    void openMasterOptionsDialog();   // CMainFrame::OnOptionsOptions (CSmcPropertyDlg with multiple pages)
    void openJmcObjectsDialog();    // CMainFrame::OnEditJmcobjects (CJmcObjectsDlg with multiple pages)
    void openFontDialog();          // CSmcDoc::OnOptionsFont
    void openAnsiColorsDialog();    // CSmcDoc::OnOptionsColors
    void openKeywordsDialog();      // CSmcDoc::OnOptionsKeywords
    void openScrollOptionsDialog(); // CSmcDoc::OnOptionsScrollbuffer

    // --- Scripting Menu Actions ---
    void reloadScripts();           // CSmcDoc::OnScriptingReload
    void openScriptParseDialog();   // CSmcDoc::OnParseScript -> CScriptParseDlg
    void breakScript();             // CSmcDoc::OnBreakScript
    void launchDebugger();          // CSmcDoc::OnScriptingLunchdebuger
    void updateDebuggerActionUI();  // CSmcDoc::OnUpdateScriptingLunchdebuger

    // --- View Menu Actions ---
    // QDockWidget::toggleViewAction() will be used for most dockable windows.
    // For CMudEmuDlg, which was a separate dialog:
    void toggleMudEmulatorView();   // CSmcDoc::OnViewMudemulator -> CMudEmuDlg (may become a dockable AnsiViewWidget)
    void updateMudEmulatorViewActionUI(); // CSmcDoc::OnUpdateViewMudemulator

    // --- Help Menu Actions ---
    void about();                   // CSmcApp::OnAppAbout
    void aboutQt();                 // Standard Qt dialog

    // --- Slots for ProfileManager signals ---
    void onProfileLoaded(const QString& profileName); // Update window title, UI state
    void onKeywordsChangedForInputBar(const QStringList& keywords); // Forward to InputBarWidget
    void onDisplayOptionsChanged();   // For AnsiViewWidgets to refresh (e.g. line wrap)
    void onFontChanged();             // For AnsiViewWidgets and InputBar to refresh
    void onAnsiColorsChanged();       // For AnsiViewWidgets to refresh
    void onCommandCharChanged(wchar_t newChar); // May update UI or behavior

    // --- Slots for InputBarWidget signals ---
    void onLineEnteredFromInputBar(const QString& text, int tokenSetup); // Forward to ProfileManager

    // --- Slots for internal UI updates (e.g. status bar) ---
    void updateStatusBarIndicators(); // Placeholder for status updates from ProfileManager/MudEngineWorker

private:
    void createActions();       // Create QAction objects
    void createMenus();         // Create QMenu objects and add actions
    void createToolBars();      // Create QToolBar objects and add actions
    void createStatusBarCustom(); // Setup QStatusBar with custom widgets
    void createDockWindows();   // Setup QDockWidgets for input bar and output views
    void setupCentralWidget();  // Setup the main AnsiViewWidget

    void loadAppSettings();     // For window geometry, last profile, dock states from QSettings
    void saveAppSettings();     // Save these settings

    ProfileManager* m_profileManager; // Owns the profile data and MUD engine logic

    // --- Main UI Elements ---
    AnsiViewWidget* m_mainAnsiView;   // Central display area
    InputBarWidget* m_inputBarWidget; // User input field
    QDockWidget*    m_inputBarDock;   // Dock for the input bar

    // Auxiliary output windows (array of dock widgets each containing an AnsiViewWidget)
    // Using ProfileManager::MAX_OUTPUT to ensure consistency
    AnsiViewWidget* m_auxAnsiViews[ProfileManager::MAX_OUTPUT];
    QDockWidget*    m_auxDockWidgets[ProfileManager::MAX_OUTPUT];

    // Placeholder for the MUD Emulator dialog/widget (originally CMudEmuDlg)
    // QDialog* m_mudEmulatorDialog; // Or it could be another AnsiViewWidget in a dock

    // --- Menus ---
    QMenu *m_fileMenu;
    QMenu *m_editMenu;
    QMenu *m_optionsMenu;
    QMenu *m_scriptingMenu;
    QMenu *m_viewMenu;          // For toggling dock windows and other views
    QMenu *m_helpMenu;

    // --- Toolbars (examples) ---
    QToolBar *m_fileToolBar;
    // QToolBar *m_editToolBar; // If needed

    // --- QActions (grouped by menu for clarity) ---
    // File Menu
    QAction *m_newProfileAction;
    QAction *m_openProfileAction;
    QAction *m_saveProfileAction;
    QAction *m_exitAction;

    // Edit Menu (placeholders, connect to focus widget's slots e.g. m_mainAnsiView->copy())
    // QAction *m_copyAction;
    // QAction *m_pasteAction;
    // QAction *m_cutAction;
    // QAction *m_selectAllAction;

    // Options Menu
    QAction *m_masterOptionsAction; // For the main options dialog (CSmcPropertyDlg)
    QAction *m_jmcObjectsAction;    // For CJmcObjectsDlg
    QAction *m_fontAction;
    QAction *m_ansiColorsAction;
    QAction *m_keywordsAction;
    QAction *m_scrollOptionsAction;

    // Scripting Menu
    QAction *m_reloadScriptsAction;
    QAction *m_scriptParseDialogAction; // For CScriptParseDlg
    QAction *m_breakScriptAction;
    QAction *m_launchDebuggerAction;
    QAction *m_debuggerActionUpdateUI; // To enable/disable based on bAllowDebug

    // View Menu
    QAction *m_toggleMudEmulatorViewAction; // For CMudEmuDlg equivalent

    // Help Menu
    QAction *m_aboutAction;
    QAction *m_aboutQtAction;

    // --- Status Bar Custom Widgets (from CMainFrame::indicators and CJMCStatus) ---
    // These will be QLabels or custom QWidgets added to the QStatusBar.
    // Custom drawing will require QLabel subclasses that can render simple ANSI or use QPixmap.
    QLabel* m_statusTickerLabel;        // Original: ID_TICKER
    QLabel* m_statusInfo1Label;         // Original: ID_INDICATOR_INFO1 (owner-draw)
    QLabel* m_statusInfo2Label;         // Original: ID_INDICATOR_INFO2 (owner-draw)
    QLabel* m_statusInfo3Label;         // Original: ID_INDICATOR_INFO3 (owner-draw)
    QLabel* m_statusInfo4Label;         // Original: ID_INDICATOR_INFO4 (owner-draw)
    QLabel* m_statusInfo5Label;         // Original: ID_INDICATOR_INFO5 (owner-draw)
    QLabel* m_statusConnectedLabel;     // Original: ID_INDICATOR_CONNECTED (owner-draw with bitmap)
    QLabel* m_statusLoggedLabel;        // Original: ID_INDICATOR_LOGGED (owner-draw with bitmap)
    QLabel* m_statusPathWritingLabel;   // Original: ID_PATH_WRITING (owner-draw with bitmap)
    // Note: The first pane (ID_SEPARATOR) in MFC was for messages; QStatusBar::showMessage handles this.
};

#endif // MAINWINDOW_H
