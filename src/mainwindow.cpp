#include "mainwindow.h"
#include "profilemanager.h"
#include "ansiviewwidget.h"
#include "inputbarwidget.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog> // For future use
#include <QDockWidget>
#include <QSplitter>   // For future use
#include <QLabel>      // For status bar example
#include <QCloseEvent> // For closeEvent

// Constructor
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_profileManager = new ProfileManager(this);

    // Create UI elements
    createActions();
    createMenus();
    createToolBars();
    createStatusBarCustom();
    setupCentralWidget();
    createDockWindows();

    setWindowTitle(tr("JMCQt MUD Client"));
    resize(1024, 768);

    // loadAppSettings(); // TODO: Implement later

    // Connect signals and slots (initial connections)
    connect(m_profileManager, &ProfileManager::profileLoaded, this, &MainWindow::onProfileLoaded);
    connect(m_inputBarWidget, &InputBarWidget::lineEntered, this, &MainWindow::onLineEnteredFromInputBar);
    connect(m_profileManager, &ProfileManager::keywordsChanged, m_inputBarWidget, &InputBarWidget::updateKeywords);

    // Connect textAddedToBuffer to all AnsiViewWidgets (main and aux)
    connect(m_profileManager, &ProfileManager::textAddedToBuffer, this, [this](int wndCode, const QString& text){
        if (wndCode == 0 && m_mainAnsiView) { // Main view
            m_mainAnsiView->appendLine(text);
        } else if (wndCode > 0 && wndCode <= MAX_AUX_OUTPUTS && m_auxAnsiViews[wndCode - 1]) { // Aux views
            m_auxAnsiViews[wndCode - 1]->appendLine(text);
        }
    });
    // Connect bufferCleared to all AnsiViewWidgets
    connect(m_profileManager, &ProfileManager::bufferCleared, this, [this](int wndCode){
        if (wndCode == 0 && m_mainAnsiView) {
            m_mainAnsiView->clearDisplay();
        } else if (wndCode > 0 && wndCode <= MAX_AUX_OUTPUTS && m_auxAnsiViews[wndCode - 1]) {
            m_auxAnsiViews[wndCode - 1]->clearDisplay();
        }
    });

    // Connect ProfileManager's display/font/color update signals to MainWindow slots
    connect(m_profileManager, &ProfileManager::displayOptionsChanged, this, &MainWindow::onDisplayOptionsChanged);
    connect(m_profileManager, &ProfileManager::fontChanged, this, &MainWindow::onFontChanged);
    connect(m_profileManager, &ProfileManager::ansiColorsChanged, this, &MainWindow::onAnsiColorsChanged);


    // ProfileManager constructor already attempts to load the last/default profile.
    // onProfileLoaded will be called if successful.
    // If no profile is loaded, UI might need to reflect this (e.g., disable certain actions).
}

MainWindow::~MainWindow()
{
    // saveAppSettings(); // TODO: Implement later
    // m_profileManager is a child QObject, Qt handles its deletion.
    // m_workerThread in ProfileManager is also parented or set to deleteLater.
}

void MainWindow::createActions()
{
    // File Menu
    m_newProfileAction = new QAction(tr("&New Profile..."), this);
    connect(m_newProfileAction, &QAction::triggered, this, &MainWindow::newProfile);

    m_openProfileAction = new QAction(tr("&Open Profile..."), this);
    connect(m_openProfileAction, &QAction::triggered, this, &MainWindow::openProfile);

    m_saveProfileAction = new QAction(tr("&Save Profile"), this);
    connect(m_saveProfileAction, &QAction::triggered, this, &MainWindow::saveProfile);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcuts(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::appExit);

    // Options Menu
    m_masterOptionsAction = new QAction(tr("&Options..."), this);
    connect(m_masterOptionsAction, &QAction::triggered, this, &MainWindow::openMasterOptionsDialog);

    m_jmcObjectsAction = new QAction(tr("&JMC Objects..."), this);
    connect(m_jmcObjectsAction, &QAction::triggered, this, &MainWindow::openJmcObjectsDialog);

    m_fontAction = new QAction(tr("F&ont..."), this);
    connect(m_fontAction, &QAction::triggered, this, &MainWindow::openFontDialog);

    m_ansiColorsAction = new QAction(tr("ANSI &Colors..."), this);
    connect(m_ansiColorsAction, &QAction::triggered, this, &MainWindow::openAnsiColorsDialog);

    m_keywordsAction = new QAction(tr("&Keywords..."), this);
    connect(m_keywordsAction, &QAction::triggered, this, &MainWindow::openKeywordsDialog);

    m_scrollOptionsAction = new QAction(tr("&Scroll Options..."), this);
    connect(m_scrollOptionsAction, &QAction::triggered, this, &MainWindow::openScrollOptionsDialog);

    // Scripting Menu
    m_reloadScriptsAction = new QAction(tr("&Reload Scripts"), this);
    connect(m_reloadScriptsAction, &QAction::triggered, this, &MainWindow::reloadScripts);

    m_scriptParseDialogAction = new QAction(tr("Script &Parse Dialog..."), this);
    connect(m_scriptParseDialogAction, &QAction::triggered, this, &MainWindow::openScriptParseDialog);

    m_breakScriptAction = new QAction(tr("&Break Script Execution"), this);
    connect(m_breakScriptAction, &QAction::triggered, this, &MainWindow::breakScript);

    m_launchDebuggerAction = new QAction(tr("&Launch Debugger"), this);
    connect(m_launchDebuggerAction, &QAction::triggered, this, &MainWindow::launchDebugger);
    // Connect the action's enabled state to ProfileManager::m_bAllowScriptDebug later
    // m_launchDebuggerAction->setEnabled(m_profileManager->getAllowScriptDebug());
    // connect(m_profileManager, &ProfileManager::allowScriptDebugChanged, m_launchDebuggerAction, &QAction::setEnabled);


    // View Menu
    m_toggleMudEmulatorViewAction = new QAction(tr("&MUD Emulator"), this);
    m_toggleMudEmulatorViewAction->setCheckable(true);
    connect(m_toggleMudEmulatorViewAction, &QAction::triggered, this, &MainWindow::toggleMudEmulatorView);
    // Update check state based on CMudEmuDlg visibility (later)
    // connect(this, &MainWindow::mudEmulatorVisibilityChanged, m_toggleMudEmulatorViewAction, &QAction::setChecked);


    // Help Menu
    m_aboutAction = new QAction(tr("&About JMCQt..."), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::about);

    m_aboutQtAction = new QAction(tr("About &Qt..."), this);
    connect(m_aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::createMenus()
{
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_newProfileAction);
    m_fileMenu->addAction(m_openProfileAction);
    m_fileMenu->addAction(m_saveProfileAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    // TODO: Add edit actions (Copy, Paste, etc.) later

    m_optionsMenu = menuBar()->addMenu(tr("&Options"));
    m_optionsMenu->addAction(m_masterOptionsAction);
    m_optionsMenu->addAction(m_jmcObjectsAction);
    m_optionsMenu->addSeparator();
    m_optionsMenu->addAction(m_fontAction);
    m_optionsMenu->addAction(m_ansiColorsAction);
    m_optionsMenu->addAction(m_keywordsAction);
    m_optionsMenu->addAction(m_scrollOptionsAction);

    m_scriptingMenu = menuBar()->addMenu(tr("&Scripting"));
    m_scriptingMenu->addAction(m_reloadScriptsAction);
    m_scriptingMenu->addAction(m_scriptParseDialogAction);
    m_scriptingMenu->addAction(m_breakScriptAction);
    m_scriptingMenu->addSeparator();
    m_scriptingMenu->addAction(m_launchDebuggerAction);

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->addAction(m_toggleMudEmulatorViewAction);
    // Dock widget toggle actions will be added in createDockWindows()

    menuBar()->addSeparator();

    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_helpMenu->addAction(m_aboutAction);
    m_helpMenu->addAction(m_aboutQtAction);
}

void MainWindow::createToolBars()
{
    m_fileToolBar = addToolBar(tr("File"));
    m_fileToolBar->addAction(m_newProfileAction);
    m_fileToolBar->addAction(m_openProfileAction);
    m_fileToolBar->addAction(m_saveProfileAction);
    // TODO: Add icons to actions
}

void MainWindow::createStatusBarCustom()
{
    statusBar()->showMessage(tr("Ready"));
    // TODO: Implement custom status bar widgets for indicators from CMainFrame
    // m_statusConnectedLabel = new QLabel("CONN"); statusBar()->addPermanentWidget(m_statusConnectedLabel);
    // m_statusLoggedLabel = new QLabel("LOG"); statusBar()->addPermanentWidget(m_statusLoggedLabel);
    // ... and others for ticker, info panes. These will need custom painting or rich text.
}

void MainWindow::setupCentralWidget()
{
    m_mainAnsiView = new AnsiViewWidget(this);
    m_mainAnsiView->setWindowCode(0);
    if (m_profileManager) m_mainAnsiView->setProfileManager(m_profileManager); // Pass manager
    setCentralWidget(m_mainAnsiView);
}

void MainWindow::createDockWindows()
{
    // Input Bar Dock Widget
    m_inputBarWidget = new InputBarWidget(this);
    if(m_profileManager) {
        m_inputBarWidget->setProfileManager(m_profileManager); // Set PM for settings
        m_inputBarWidget->updateKeywords(m_profileManager->keywords()); // Initial keywords
        m_inputBarWidget->loadHistory(m_profileManager->getCommandHistory()); // Initial history
    }

    m_inputBarDock = new QDockWidget(tr("Input"), this);
    m_inputBarDock->setWidget(m_inputBarWidget);
    m_inputBarDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_inputBarDock);
    if(m_viewMenu) m_viewMenu->addAction(m_inputBarDock->toggleViewAction());

    // Auxiliary Output Dock Widgets
    for (int i = 0; i < MAX_AUX_OUTPUTS; ++i)
    {
        m_auxAnsiViews[i] = new AnsiViewWidget(this);
        m_auxAnsiViews[i]->setWindowCode(i + 1);
        if (m_profileManager) m_auxAnsiViews[i]->setProfileManager(m_profileManager);

        m_auxDockWidgets[i] = new QDockWidget(tr("Output %1").arg(i + 1), this);
        m_auxDockWidgets[i]->setWidget(m_auxAnsiViews[i]);
        addDockWidget(Qt::RightDockWidgetArea, m_auxDockWidgets[i]);
        if(m_viewMenu) m_viewMenu->addAction(m_auxDockWidgets[i]->toggleViewAction());
        if (i > 0) {
            m_auxDockWidgets[i]->setVisible(false);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // saveAppSettings(); // TODO
    if (m_profileManager) m_profileManager->saveProfile(); // Save current profile data
    if (m_profileManager) m_profileManager->saveGlobalSettings(); // Save global settings like last profile
    if (m_inputBarWidget && m_profileManager) {
        m_profileManager->setCommandHistory(m_inputBarWidget->getHistory());
    }
    if (m_profileManager) {
        // saveProfile() saves .opt and tabwords.txt
        // saveGlobalSettings() saves jmc.ini (which includes history now)
        // stopMudEngineThread should be called before ProfileManager destructor if not parented properly for deleteLater.
        // For now, ProfileManager destructor calls saveGlobalSettings (which saves history).
        // And ProfileManager constructor calls startMudEngineThread.
        // So, just ensuring ProfileManager is saved is enough.
        // It's better if ProfileManager::saveProfile also calls saveGlobalSettings, or if PM destructor explicitly calls stopMudEngineThread then save.
        // For now, this is okay as PM's destructor saves global settings.
    }
    QMainWindow::closeEvent(event);
}

// --- Slots Implementation ---
void MainWindow::newProfile()
{
    // QMessageBox::information(this, tr("New Profile"), tr("Not yet fully implemented."));
    if (m_profileManager) {
       // TODO: Use a dialog to get new profile name and copy option
       // For now, simple example:
       QString newName = "TestNewProfile"; // Replace with QInputDialog
       bool copyCurrent = true; // Replace with QCheckBox or similar
       m_profileManager->createNewProfile(newName, copyCurrent, m_profileManager->currentProfileName());
    }
}

void MainWindow::openProfile()
{
    // QMessageBox::information(this, tr("Open Profile"), tr("Not yet fully implemented."));
    if (m_profileManager) {
       // TODO: Use QFileDialog or a custom dialog listing .opt files in settings dir
       // For now, simple example:
       // QString profileToLoad = "Default"; // Replace with selection
       // m_profileManager->loadProfile(profileToLoad);
        QMessageBox::information(this, "Open Profile", "Placeholder: Select a profile.");
    }
}

void MainWindow::saveProfile()
{
    if (m_profileManager) {
        m_profileManager->saveProfile();
        statusBar()->showMessage(tr("Profile saved."), 3000);
    }
}

void MainWindow::appExit()
{
    close(); // Triggers closeEvent where settings and profile are saved
}

void MainWindow::openMasterOptionsDialog() { QMessageBox::information(this, "Options", "Master Options dialog not implemented."); }
void MainWindow::openJmcObjectsDialog() { QMessageBox::information(this, "JMC Objects", "JMC Objects dialog not implemented."); }
void MainWindow::openFontDialog() { QMessageBox::information(this, "Font", "Font dialog not implemented."); }
void MainWindow::openAnsiColorsDialog() { QMessageBox::information(this, "ANSI Colors", "ANSI Colors dialog not implemented."); }
void MainWindow::openKeywordsDialog() { QMessageBox::information(this, "Keywords", "Keywords dialog not implemented."); }
void MainWindow::openScrollOptionsDialog() { QMessageBox::information(this, "Scroll Options", "Scroll Options dialog not implemented."); }

void MainWindow::reloadScripts() {
    if(m_profileManager) m_profileManager->reloadScripts();
    statusBar()->showMessage(tr("Reload scripts requested."), 2000);
}
void MainWindow::openScriptParseDialog() { QMessageBox::information(this, "Script Parse", "Script Parse dialog not implemented."); }
void MainWindow::breakScript() {
    // if(m_profileManager) m_profileManager->sendBreakToMudEngine(); // Example
    QMessageBox::information(this, "Break Script", "Break Script not implemented.");
}
void MainWindow::launchDebugger() {
    // if(m_profileManager) m_profileManager->sendLaunchDebuggerToMudEngine(); // Example
    QMessageBox::information(this, "Launch Debugger", "Launch Debugger not implemented.");
}
void MainWindow::updateDebuggerActionUI() { /* TODO: Update m_launchDebuggerAction enabled state based on ProfileManager */ }
void MainWindow::toggleMudEmulatorView() { QMessageBox::information(this, "MUD Emulator", "MUD Emulator view not implemented."); }


void MainWindow::about()
{
    QMessageBox::about(this, tr("About JMCQt"),
        tr("This is <b>JMCQt</b>, a MUD client ported from an original Win32 MFC application."
           "<p>This version is under development."));
}

void MainWindow::onProfileLoaded(const QString& profileName)
{
    setWindowTitle(tr("JMCQt - %1").arg(profileName));
    if (m_inputBarWidget && m_profileManager) {
         m_inputBarWidget->updateKeywords(m_profileManager->keywords());
         // TODO: Load history into InputBarWidget
         // m_inputBarWidget->loadHistory(m_profileManager->getProfileHistory());
         // TODO: Configure InputBarWidget with options from ProfileManager
         // m_inputBarWidget->setHistorySize(m_profileManager->getInputHistorySize());
         // ... etc.
         m_inputBarWidget->loadHistory(m_profileManager->getCommandHistory()); // Load history
    }
    if (m_mainAnsiView) {
        m_mainAnsiView->clearDisplay();
        m_mainAnsiView->setProfileManager(m_profileManager); // Ensure it has the latest manager
    }
    for (int i = 0; i < MAX_AUX_OUTPUTS; ++i) {
        if(m_auxAnsiViews[i]) {
            m_auxAnsiViews[i]->clearDisplay();
            m_auxAnsiViews[i]->setProfileManager(m_profileManager);
        }
    }
    // Trigger updates for views based on newly loaded profile settings
    onFontChanged();
    onAnsiColorsChanged();
    onDisplayOptionsChanged();

    statusBar()->showMessage(tr("Profile '%1' loaded.").arg(profileName), 3000);
    m_inputBarWidget->setFocusToInput(); // Focus input after profile load
}

void MainWindow::onLineEnteredFromInputBar(const QString& text, int tokenSetup)
{
    if (m_profileManager) {
        // tokenSetup from InputBarWidget could be passed if MudEngineWorker needs it
        m_profileManager->sendCommandToMud(text);
    }
    // InputBarWidget itself handles clearing/tokenizing based on its internal settings,
    // which should be configured by ProfileManager data after profile load.
}

void MainWindow::updateStatusBarIndicators() {
    // TODO: Get status from ProfileManager (e.g. connection, logging) and update QLabels
    // For example:
    // if (m_profileManager->isConnected()) m_statusConnectedLabel->setPixmap(connectedIcon);
    // else m_statusConnectedLabel->setPixmap(disconnectedIcon);
    // m_statusInfo1Label->setText(m_profileManager->getInfoPaneText(1)); // Needs ANSI rendering for label
}

void MainWindow::onDisplayOptionsChanged() {
    if(m_mainAnsiView) m_mainAnsiView->updateDisplayOptions();
    for(int i=0; i<MAX_AUX_OUTPUTS; ++i) {
        if(m_auxAnsiViews[i]) m_auxAnsiViews[i]->updateDisplayOptions();
    }
}
void MainWindow::onFontChanged() {
    if(m_mainAnsiView) m_mainAnsiView->updateFont();
    for(int i=0; i<MAX_AUX_OUTPUTS; ++i) {
        if(m_auxAnsiViews[i]) m_auxAnsiViews[i]->updateFont();
    }
    // if(m_inputBarWidget && m_profileManager) {
    //    m_inputBarWidget->setFont(m_profileManager->getCurrentFont());
    // }
}
void MainWindow::onAnsiColorsChanged() {
    if(m_mainAnsiView) m_mainAnsiView->updateAnsiColors();
    for(int i=0; i<MAX_AUX_OUTPUTS; ++i) {
        if(m_auxAnsiViews[i]) m_auxAnsiViews[i]->updateAnsiColors();
    }
}

// TODO: Implement loadAppSettings and saveAppSettings using QSettings for window geometry and state
void MainWindow::loadAppSettings() {
    // QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    // restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    // restoreState(settings.value("mainWindow/windowState").toByteArray());
    // QString lastProfile = settings.value("Main/LastProfile", "Default").toString();
    // if(m_profileManager && !m_profileManager->currentProfileName().isEmpty() && m_profileManager->currentProfileName() != lastProfile) {
    //    m_profileManager->loadProfile(lastProfile);
    // } else if (m_profileManager && m_profileManager->currentProfileName().isEmpty()){
    //    m_profileManager->loadProfile(lastProfile);
    // }
}

void MainWindow::saveAppSettings() {
    // QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    // settings.setValue("mainWindow/geometry", saveGeometry());
    // settings.setValue("mainWindow/windowState", saveState());
    // if(m_profileManager) settings.setValue("Main/LastProfile", m_profileManager->currentProfileName());
}
