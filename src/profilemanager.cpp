#include "profilemanager.h"
#include <QSettings>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QThread>
#include "mudengineworker.h" // Now include the full definition


// Default ANSI Colors (similar to CSmcDoc::DefColors)
// These will be used if no colors are found in settings.
static const QColor defaultAnsiColors[16] = {
    QColor(0, 0, 0),        // Black
    QColor(128, 0, 0),      // Dark Red
    QColor(0, 128, 0),      // Dark Green
    QColor(128, 128, 0),    // Dark Yellow (Olive)
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

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent),
      m_globalSettings(nullptr),
      m_profileSettings(nullptr),
      m_cCommandChar(L'#'), // Default from CSmcDoc
      m_bRectangleSelection(false),
      m_bRemoveESCSelection(true),
      m_bLineWrap(true),
      m_bShowTimestamps(false),
      m_bShowHiddenText(true),
      m_bDarkOnly(false),
      m_commandDelimiter(L';'), // Default from CSmcDoc
      m_mudEngineWorker(nullptr),
      m_workerThread(nullptr)
{
    initPaths();
    initializeDefaultAnsiColors(); // Initialize with defaults first
    m_font = QFont("Fixedsys", 10); // Default from CSmcDoc, adjust size as needed

    loadGlobalSettings(); // Load global settings like last profile, default font/colors
    // Attempt to load the last used profile
    QString lastProfile = m_globalSettings->value("Main/LastProfile", "Default").toString();
    if (!loadProfile(lastProfile)) {
        // If last profile fails, try loading "Default" profile explicitly
        if (lastProfile != "Default" && !loadProfile("Default")) {
             qWarning() << "Failed to load 'Default' profile. Using fallback settings.";
             // Apply some very basic fallback settings or leave as initialized
             m_currentProfileName = "Default"; // Still set a name
             // No specific .opt loaded, so it will use defaults or empty values for profile items
             emit profileLoaded(m_currentProfileName);
        }
    }
    qDebug() << "ProfileManager created. Initial profile attempt:" << m_currentProfileName;

    loadCommandHistory(); // Load global command history
    startMudEngineThread(); // Start the worker thread
}

ProfileManager::~ProfileManager()
{
    // stopMudEngineThread(); // Call this first to ensure worker is stopped
    saveCommandHistory();   // Save history before settings objects are deleted
    saveGlobalSettings(); // Save last profile name etc.

    if (m_profileSettings) {
        delete m_profileSettings;
        m_profileSettings = nullptr;
    }
    if (m_globalSettings) {
        delete m_globalSettings;
        m_globalSettings = nullptr;
    }
    qDebug() << "ProfileManager destroyed";
}

void ProfileManager::initPaths()
{
    // In original JMC, szBASE_DIR was GetCurrentDirectory.
    // For a modern Qt app, it's better to use QStandardPaths or QCoreApplication::applicationDirPath().
    // Let's use applicationDirPath for simplicity, assuming settings are relative to exe.
    // A more robust solution for user data would be QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).
    m_baseDir = QCoreApplication::applicationDirPath();
    m_settingsDir = QDir(m_baseDir).filePath("settings");
    QDir dir(m_settingsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        qInfo() << "Created settings directory:" << m_settingsDir;
    }

    QString globalIniPath = QDir(m_baseDir).filePath("jmc.ini");
    m_globalSettings = new QSettings(globalIniPath, QSettings::IniFormat, this);
    qDebug() << "Global settings (jmc.ini) path:" << globalIniPath;
}

void ProfileManager::initializeDefaultAnsiColors()
{
    m_foregroundColors.clear();
    m_backgroundColors.clear();
    for(int i=0; i<16; ++i) {
        m_foregroundColors.append(defaultAnsiColors[i]);
        m_backgroundColors.append(defaultAnsiColors[i]); // In JMC, BackColors were initially same as ForeColors
    }
}

void ProfileManager::loadGlobalSettings()
{
    if (!m_globalSettings) return;

    // Font (LOGFONT in MFC)
    // QSettings can't store QFont directly in INI in a compatible way with LOGFONT.
    // We'll store FaceName, Height, Weight, CharSet, PitchAndFamily.
    // For simplicity, load common attributes.
    m_font.setFamily(m_globalSettings->value("Font/FaceName", "Fixedsys").toString());
    m_font.setPointSize(m_globalSettings->value("Font/PointSize", 10).toInt()); // MFC lfHeight is negative for point size
    m_font.setWeight(static_cast<QFont::Weight>(m_globalSettings->value("Font/Weight", QFont::Normal).toInt()));
    m_font.setFixedPitch(m_globalSettings->value("Font/FixedPitch", true).toBool());
    // Note: LOGFONT charset and pitch&family are more complex to map directly if specific values were critical.
    // For now, a good monospace default like "Fixedsys" or "Courier New" is fine.

    // ANSI Colors (Foreground, Background, DarkOnly)
    QByteArray fgColorData = m_globalSettings->value("Colors/Foreground").toByteArray();
    if (fgColorData.size() == sizeof(COLORREF) * 16) {
        const COLORREF* mfcColors = reinterpret_cast<const COLORREF*>(fgColorData.constData());
        m_foregroundColors.clear();
        for (int i = 0; i < 16; ++i) {
            m_foregroundColors.append(QColor(GetRValue(mfcColors[i]), GetGValue(mfcColors[i]), GetBValue(mfcColors[i])));
        }
    } // else, defaults from initializeDefaultAnsiColors() are kept.

    QByteArray bgColorData = m_globalSettings->value("Colors/Background").toByteArray();
    if (bgColorData.size() == sizeof(COLORREF) * 16) {
        const COLORREF* mfcColors = reinterpret_cast<const COLORREF*>(bgColorData.constData());
        m_backgroundColors.clear();
        for (int i = 0; i < 16; ++i) {
            m_backgroundColors.append(QColor(GetRValue(mfcColors[i]), GetGValue(mfcColors[i]), GetBValue(mfcColors[i])));
        }
    }
    m_bDarkOnly = m_globalSettings->value("Colors/DarkOnly", false).toBool();

    // Scripting language GUID (m_guidScriptLang in CSmcApp)
    // QByteArray scriptLangGuid = m_globalSettings->value("Script/LANGGUID").toByteArray();
    // If porting to QJSEngine, this might just be a preference for JS version or not needed.

    // Other global options from CSmcDoc constructor / destructor that were saved to szGLOBAL_PROFILE
    nScrollSize = m_globalSettings->value("Options/Scroll", 300).toInt(); // Used by AnsiViewWidget
    m_cCommandChar = QChar(m_globalSettings->value("Options/CommandChar", L'#').toUInt()).toLatin1(); // Assuming wchar_t was ASCII range
    // bConnectBeep = m_globalSettings->value("Options/ConnectBeep", false).toBool(); // Example
    // bAutoReconnect = m_globalSettings->value("Options/AutoReconnect", false).toBool(); // Example
    m_bSplitOnBackscroll = m_globalSettings->value("Options/SplitOnBackscroll", true).toBool(); // UI concern
    // bAllowDebug (scripting) = m_globalSettings->value("Script/AllowDebug", false).toBool();
    // nScripterrorOutput (scripting) = m_globalSettings->value("Script/ErrOutput", 0).toInt();

    // Options from CEditBar saved globally in [Main] section
    m_inputHistorySize = m_globalSettings->value("Main/History", 20).toInt();
    m_clearInputAfterSend = m_globalSettings->value("Main/ClearInput", true).toBool();
    m_inputBarTokenInput = m_globalSettings->value("Main/TokenInput", false).toBool();
    m_inputBarKillOneToken = m_globalSettings->value("Main/KillOneToken", false).toBool();
    m_inputBarScrollEnd = m_globalSettings->value("Main/ScrollEnd", true).toBool();
    // These were loaded by CMainFrame for CEditBar in the original
    m_inputBarCursorPosWhileListing = m_globalSettings->value("Main/CursorWileList", 1).toInt(); // CSmcDoc used AfxGetApp()->GetProfileInt for these
    m_inputBarMinStrLenForHistory = m_globalSettings->value("Main/MinStrLen", 2).toInt();


    // Scripting global options from [Script] section
    // m_scriptLanguageGuid = m_globalSettings->value("Script/LANGGUID").toByteArray(); // For QJSEngine if needed
    m_bAllowScriptDebug = m_globalSettings->value("Script/AllowDebug", false).toBool();
    m_nScriptErrorOutputWnd = m_globalSettings->value("Script/ErrOutput", 0).toInt();


    qDebug() << "Global settings loaded.";
    emit fontChanged();
    emit ansiColorsChanged();
}

void ProfileManager::saveGlobalSettings()
{
    if (!m_globalSettings) return;
    m_globalSettings->setValue("Main/LastProfile", m_currentProfileName);

    // Font
    m_globalSettings->setValue("Font/FaceName", m_font.family());
    m_globalSettings->setValue("Font/PointSize", m_font.pointSize());
    m_globalSettings->setValue("Font/Weight", static_cast<int>(m_font.weight()));
    m_globalSettings->setValue("Font/FixedPitch", m_font.fixedPitch());

    // ANSI Colors
    QByteArray fgColorData; fgColorData.resize(sizeof(COLORREF) * 16);
    COLORREF* fgColors = reinterpret_cast<COLORREF*>(fgColorData.data());
    for(int i=0; i<16; ++i) fgColors[i] = RGB(m_foregroundColors[i].red(), m_foregroundColors[i].green(), m_foregroundColors[i].blue());
    m_globalSettings->setValue("Colors/Foreground", fgColorData);

    QByteArray bgColorData; bgColorData.resize(sizeof(COLORREF) * 16);
    COLORREF* bgColors = reinterpret_cast<COLORREF*>(bgColorData.data());
    for(int i=0; i<16; ++i) bgColors[i] = RGB(m_backgroundColors[i].red(), m_backgroundColors[i].green(), m_backgroundColors[i].blue());
    m_globalSettings->setValue("Colors/Background", bgColorData);
    m_globalSettings->setValue("Colors/DarkOnly", m_bDarkOnly);

    m_globalSettings->setValue("Options/Scroll", nScrollSize);
    m_globalSettings->setValue("Options/CommandChar", static_cast<uint>(QChar(m_cCommandChar).unicode()));
    m_globalSettings->setValue("Options/SplitOnBackscroll", m_bSplitOnBackscroll);

    // Save EditBar options to [Main]
    m_globalSettings->setValue("Main/History", m_inputHistorySize);
    m_globalSettings->setValue("Main/ClearInput", m_clearInputAfterSend);
    m_globalSettings->setValue("Main/TokenInput", m_inputBarTokenInput);
    m_globalSettings->setValue("Main/KillOneToken", m_inputBarKillOneToken);
    m_globalSettings->setValue("Main/ScrollEnd", m_inputBarScrollEnd);
    m_globalSettings->setValue("Main/CursorWileList", m_inputBarCursorPosWhileListing);
    m_globalSettings->setValue("Main/MinStrLen", m_inputBarMinStrLenForHistory);

    // Save Scripting global options to [Script]
    // m_globalSettings->setValue("Script/LANGGUID", m_scriptLanguageGuid);
    m_globalSettings->setValue("Script/AllowDebug", m_bAllowScriptDebug);
    m_globalSettings->setValue("Script/ErrOutput", m_nScriptErrorOutputWnd);

    m_globalSettings->sync(); // Ensure changes are written
    qDebug() << "Global settings saved.";
}


QString ProfileManager::currentProfileName() const
{
    return m_currentProfileName;
}

bool ProfileManager::loadProfile(const QString& profileName)
{
    if (profileName.isEmpty()) {
        qWarning() << "Profile name cannot be empty.";
        return false;
    }
    qDebug() << "Attempting to load profile:" << profileName;

    m_profileOptPath = QDir(m_settingsDir).filePath(profileName + ".opt");
    if (m_profileSettings) {
        delete m_profileSettings;
    }
    m_profileSettings = new QSettings(m_profileOptPath, QSettings::IniFormat, this);

    if (m_profileSettings->status() != QSettings::NoError) {
        qWarning() << "Failed to load profile settings file:" << m_profileOptPath
                   << "Error:" << m_profileSettings->status();
        // Keep global settings (font, colors) but profile specific ones will be default
        // This behavior matches original JMC which would create a new default .opt if missing.
        // For a port, it might be better to signal an error or create a new default .opt explicitly.
        m_currentProfileName = profileName; // Still set the name
        // Initialize profile specific settings to defaults
        loadProfileSpecificSettings(profileName); // This will load defaults if file is bad/new
    } else {
         m_currentProfileName = profileName;
         loadProfileSpecificSettings(profileName);
    }

    loadTabKeywords(); // Load from tabwords.txt (global for now as in original)

    // TODO: Initialize/Reload MudEngineWorker with profile-specific scripts etc.
    // For now, just emit signal
    emit profileLoaded(m_currentProfileName);
    qInfo() << "Profile" << profileName << "loaded. Path:" << m_profileOptPath;
    return true;
}

void ProfileManager::saveProfile()
{
    if (m_currentProfileName.isEmpty() || !m_profileSettings) {
        qWarning() << "No profile loaded or settings object invalid, cannot save.";
        return;
    }
    qDebug() << "Saving profile:" << m_currentProfileName << "to" << m_profileOptPath;
    saveProfileSpecificSettings();
    saveTabKeywords(); // Save tabwords.txt
    m_profileSettings->sync(); // Ensure changes are written
    qInfo() << "Profile" << m_currentProfileName << "saved.";
}

void ProfileManager::loadProfileSpecificSettings(const QString& profileName)
{
    Q_UNUSED(profileName); // Name is used for file path, already set up in m_profileSettings
    if (!m_profileSettings) return;

    // Options from CSmcDoc::OnNewDocument and DoProfileSave that are profile-specific
    // Paths for settinsg/macro files
    m_profileSetPath = makeAbsolutePath(m_profileSettings->value("Options/AutoLoadFile", m_currentProfileName + ".set").toString(), m_baseDir);
    m_profileSavePath = makeAbsolutePath(m_profileSettings->value("Options/AutoSaveFile", m_profileSetPath).toString(), m_baseDir);
    m_profileSaveCommand = m_profileSettings->value("Options/AutoSaveCommand", "").toString();
    QString delimiterStr = m_profileSettings->value("Options/CommandDelimiter", ";").toString();
    m_commandDelimiter = delimiterStr.isEmpty() ? L';' : QChar(delimiterStr[0].unicode()).toLatin1();


    // Display options (some were global in CSmcDoc constructor, some per profile)
    // Prioritize profile, then global, then default. For now, simple load.
    m_bRectangleSelection = m_profileSettings->value("Options/RectangleSelection", m_globalSettings->value("Options/RectangleSelection", false).toBool()).toBool();
    m_bRemoveESCSelection = m_profileSettings->value("Options/RemoveESCSelection", m_globalSettings->value("Options/RemoveESCSelection", true).toBool()).toBool();
    m_bLineWrap = m_profileSettings->value("Options/LineWrap", m_globalSettings->value("Options/LineWrap", true).toBool()).toBool();
    m_bShowTimestamps = m_profileSettings->value("Options/LineTimeStamps", m_globalSettings->value("Options/LineTimeStamps", false).toBool()).toBool();
    m_bShowHiddenText = m_profileSettings->value("Options/ShowHiddenText", m_globalSettings->value("Options/ShowHiddenText", true).toBool()).toBool();

    // ANSI/Log settings from CSmcDoc
    // bRMASupport = m_profileSettings->value("ANSI/RMAsupport", false).toBool();
    // bAppendLogTitle = m_profileSettings->value("ANSI/AppendLogTitle", false).toBool();
    // ... many others ... these need careful mapping to Qt logging or custom features.

    // Substitution settings from CSmcDoc (example, full implementation later)
    // bool bSubstitution = m_profileSettings->value("Options/bSubstitution", false).toBool();
    // QByteArray substCharsData = m_profileSettings->value("Options/charsSubstitution").toByteArray();
    // if (substCharsData.size() == SUBST_ARRAY_SIZE) { /* load substChars */ }
    m_bSubstitutionEnabled = m_profileSettings->value("Options/bSubstitution", false).toBool();
    QByteArray loadedSubstData = m_profileSettings->value("Options/charsSubstitution").toByteArray();
    if (loadedSubstData.size() == ProfileManager::SUBST_CHARS_DATA_SIZE) {
        m_substCharsData = loadedSubstData;
    } else {
        m_substCharsData.clear();
        m_substCharsData.resize(ProfileManager::SUBST_CHARS_DATA_SIZE); // Initialize with zeros if not found or wrong size
        qWarning() << "SubstChars data not found or wrong size in profile, initialized to default (zeros).";
    }


    // IAC (Telnet options from [Substitution] section in original .opt)
    m_bIacSendSingle = m_profileSettings->value("Substitution/IACSendSingle", false).toBool();
    m_bIacReceiveSingle = m_profileSettings->value("Substitution/IACReciveSingle", false).toBool();

    // Logging options (from [ANSI] section in original .opt)
    m_bRmaSupport = m_profileSettings->value("ANSI/RMAsupport", false).toBool();
    m_bAppendLogTitle = m_profileSettings->value("ANSI/AppendLogTitle", false).toBool();
    m_bAnsiLog = m_profileSettings->value("ANSI/ANSILog", false).toBool();
    m_bDefaultLogMode = m_profileSettings->value("ANSI/AppendMode", false).toBool(); // true is Append
    m_bHtmlLog = m_profileSettings->value("ANSI/HTMLLog", false).toBool();
    m_bHtmlLogTimestamps = m_profileSettings->value("ANSI/HTMLLogTimestamps", false).toBool();
    m_bLogAsUserSeen = m_profileSettings->value("ANSI/LogAsUserSeen", false).toBool();
    m_logCodePage = m_profileSettings->value("ANSI/LogCodePage", 0).toInt();


    qDebug() << "Profile specific settings for" << m_currentProfileName << "loaded.";
    emit displayOptionsChanged(); // Signal that view-related options might have changed
    // emit commandCharChanged(m_cCommandChar); // Already done in loadGlobal if it's global, or here if per-profile
}

void ProfileManager::saveProfileSpecificSettings()
{
    if (!m_profileSettings || m_currentProfileName.isEmpty()) return;

    // Paths for settings/macro files
    m_profileSettings->setValue("Options/AutoLoadFile", makeLocalPath(m_profileSetPath, m_baseDir));
    m_profileSettings->setValue("Options/AutoSaveFile", makeLocalPath(m_profileSavePath, m_baseDir));
    m_profileSettings->setValue("Options/AutoSaveCommand", m_profileSaveCommand);
    m_profileSettings->setValue("Options/CommandDelimiter", QString(QChar(m_commandDelimiter)));

    // Display options
    m_profileSettings->setValue("Options/RectangleSelection", m_bRectangleSelection);
    m_profileSettings->setValue("Options/RemoveESCSelection", m_bRemoveESCSelection);
    m_profileSettings->setValue("Options/LineWrap", m_bLineWrap);
    m_profileSettings->setValue("Options/LineTimeStamps", m_bShowTimestamps);
    m_profileSettings->setValue("Options/ShowHiddenText", m_bShowHiddenText);

    // Save other profile-specific settings like logging, substitutions, IAC here...
    m_profileSettings->setValue("Options/bSubstitution", m_bSubstitutionEnabled);
    if (m_substCharsData.size() == ProfileManager::SUBST_CHARS_DATA_SIZE) { // Ensure correct size before saving
        m_profileSettings->setValue("Options/charsSubstitution", m_substCharsData);
    } else {
        // Handle error or save a default empty array of correct size
        QByteArray defaultSubstData(ProfileManager::SUBST_CHARS_DATA_SIZE, 0);
        m_profileSettings->setValue("Options/charsSubstitution", defaultSubstData);
        qWarning() << "m_substCharsData was not of the correct size when saving. Saved default empty data.";
    }

    // IAC options
    m_profileSettings->setValue("Substitution/IACSendSingle", m_bIacSendSingle);
    m_profileSettings->setValue("Substitution/IACReciveSingle", m_bIacReceiveSingle);

    // Logging options
    m_profileSettings->setValue("ANSI/RMAsupport", m_bRmaSupport);
    m_profileSettings->setValue("ANSI/AppendLogTitle", m_bAppendLogTitle);
    m_profileSettings->setValue("ANSI/ANSILog", m_bAnsiLog);
    m_profileSettings->setValue("ANSI/AppendMode", m_bDefaultLogMode);
    m_profileSettings->setValue("ANSI/HTMLLog", m_bHtmlLog);
    m_profileSettings->setValue("ANSI/HTMLLogTimestamps", m_bHtmlLogTimestamps);
    m_profileSettings->setValue("ANSI/LogAsUserSeen", m_bLogAsUserSeen);
    m_profileSettings->setValue("ANSI/LogCodePage", m_logCodePage);


    qDebug() << "Profile specific settings for" << m_currentProfileName << "saved to" << m_profileOptPath;
}

void ProfileManager::loadTabKeywords() {
    m_keywords.clear();
    QString tabWordsPath = QDir(m_baseDir).filePath("tabwords.txt");
    QFile file(tabWordsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while(!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                m_keywords.append(line);
            }
        }
        file.close();
        qDebug() << "Loaded" << m_keywords.size() << "keywords from tabwords.txt";
    } else {
        qDebug() << "tabwords.txt not found or could not be opened:" << tabWordsPath;
    }

    // Original CSmcDoc also added script commands to tab words via FillTabWords(NULL) -> GetCommandsList().
    // This part will require the ttcoreex script engine to be active to get its command list.
    // For now, we only load from tabwords.txt.
    // Later, MudEngineWorker could provide a list of script commands to append here.

    emit keywordsChanged(m_keywords);
}

void ProfileManager::saveTabKeywords() {
     QString tabWordsPath = QDir(m_baseDir).filePath("tabwords.txt");
    QFile file(tabWordsPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&file);
        // In the original CSmcDoc, keywords starting with the command character
        // (which were script commands) were not saved to tabwords.txt because
        // they would be re-added dynamically from GetCommandsList().
        // We should replicate this filtering if we add dynamic script commands later.
        for (const QString& keyword : m_keywords) {
            if (keyword.isEmpty() || keyword.at(0) == QChar(m_cCommandChar)) { // Don't save commands added dynamically
                // This assumes m_cCommandChar is up-to-date.
                // This check is a placeholder; proper filtering depends on how script commands are merged.
                // For now, if we only load from file, saving all is fine.
                // If script commands are added to m_keywords, filter them out here.
            }
            out << keyword << "\n";
        }
        file.close();
        qDebug() << "Saved" << m_keywords.size() << "keywords to tabwords.txt";
    } else {
        qWarning() << "Could not open tabwords.txt for writing:" << tabWordsPath;
    }
}


QStringList ProfileManager::keywords() const
{
    return m_keywords;
}

void ProfileManager::sendCommandToMud(const QString& command)
{
    qDebug() << "Command to MUD (ProfileManager):" << command;
    // This will eventually queue the command to MudEngineWorker
    // For now, maybe echo it to one of the views for testing
    emit textAddedToBuffer(0, "Sent: " + command); // Echo to main view (wndCode 0)
}

// --- Getters for display options ---
QList<QColor> ProfileManager::getForegroundColors() const { return m_foregroundColors; }
QList<QColor> ProfileManager::getBackgroundColors() const { return m_backgroundColors; }
QFont ProfileManager::getCurrentFont() const { return m_font; }
bool ProfileManager::getLineWrap() const { return m_bLineWrap; }
bool ProfileManager::getShowTimestamps() const { return m_bShowTimestamps; }
bool ProfileManager::getRectangleSelection() const { return m_bRectangleSelection; }
bool ProfileManager::getRemoveEscFromSelection() const { return m_bRemoveESCSelection; }
bool ProfileManager::getShowHiddenText() const { return m_bShowHiddenText; }
bool ProfileManager::getDarkOnlyColors() const { return m_bDarkOnly; }
wchar_t ProfileManager::getCommandChar() const { return m_cCommandChar; }

// --- Path helpers (simplified from original CSmcApp, may need refinement) ---
QString ProfileManager::makeAbsolutePath(const QString& relativeOrAbsoluteFile, const QString& baseDir) {
    QDir base(baseDir);
    if (QDir::isAbsolutePath(relativeOrAbsoluteFile)) {
        return QDir::cleanPath(relativeOrAbsoluteFile);
    }
    return QDir::cleanPath(base.filePath(relativeOrAbsoluteFile));
}

QString ProfileManager::makeLocalPath(const QString& absoluteFile, const QString& baseDir) {
    QDir base(baseDir);
    return base.relativeFilePath(absoluteFile);
}

// Placeholder for nScrollSize, which was global in CSmcDoc.
// It should be a setting for AnsiViewWidget, potentially loaded via ProfileManager.
int nScrollSize = 300; // Default value

// Slots for MudEngineWorker signals (to be implemented when MudEngineWorker is)
void ProfileManager::onTextReceivedFromWorker(int wndCode, const QString& text) {
    // Process text if needed (e.g. timestamping if not done by view)
    emit textAddedToBuffer(wndCode, text);
}

void ProfileManager::onClearDisplayFromWorker(int wndCode) {
    emit bufferCleared(wndCode);
}

void ProfileManager::reloadScripts() {
    qDebug() << "ProfileManager::reloadScripts() called. To be implemented.";
    // This would involve:
    // 1. Reading commonlib.scr
    // 2. Reading profile-specific .scr file
    // 3. Reading other script files defined in JMC objects
    // 4. Concatenating them
    // 5. Sending to MudEngineWorker to reload script engine

    startMudEngineThread(); // Start the worker thread after profile is loaded/initialized
}

void ProfileManager::startMudEngineThread() {
    if (m_workerThread && m_workerThread->isRunning()) {
        qWarning() << "MudEngineWorker thread already running.";
        return;
    }

    if (!m_workerThread) {
        m_workerThread = new QThread(this); // Parent to ProfileManager for auto-cleanup if PM is deleted
    }
    if (!m_mudEngineWorker) {
        m_mudEngineWorker = new MudEngineWorker(); // No parent, will be moved to thread
    }

    m_mudEngineWorker->moveToThread(m_workerThread);

    // Connect signals for thread management
    connect(m_workerThread, &QThread::started, m_mudEngineWorker, [this](){
        qDebug() << "MudEngineWorker thread started. Worker preparing to initialize.";
        // Pass a WId. For now, 0, as ttcoreex might not need a real HWND if callbacks are function pointers.
        // A more robust way would be for MainWindow to provide its winId().
        WId mainWindowId = 0; // Placeholder
        QMetaObject::invokeMethod(m_mudEngineWorker, "initializeEngine", Qt::QueuedConnection, Q_ARG(WId, mainWindowId));
    });
    connect(m_mudEngineWorker, &MudEngineWorker::finished, m_workerThread, &QThread::quit);
    connect(m_mudEngineWorker, &MudEngineWorker::finished, m_mudEngineWorker, &MudEngineWorker::deleteLater); // Worker cleans itself up
    connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater); // Thread cleans itself up
    // If worker is deleted, and thread is still parented to ProfileManager, then thread might not be deleted by deleteLater if PM is deleted first.
    // Better: connect(m_workerThread, &QThread::finished, this, [this](){ m_workerThread = nullptr; m_mudEngineWorker = nullptr; });


    // Connect communication signals
    // ProfileManager -> MudEngineWorker (command sending is via invokeMethod)
    // MudEngineWorker -> ProfileManager
    connect(m_mudEngineWorker, &MudEngineWorker::textReceived, this, &ProfileManager::onTextReceivedFromWorker);
    connect(m_mudEngineWorker, &MudEngineWorker::clearDisplayRequested, this, &ProfileManager::onClearDisplayFromWorker);
    connect(m_mudEngineWorker, &MudEngineWorker::errorOccurred, this, [this](const QString& errorString){
        qWarning() << "Error from MudEngineWorker:" << errorString;
        // Optionally emit another signal for the UI to display this error
    });


    m_workerThread->start();
    qDebug() << "MudEngineWorker thread start requested.";
}

void ProfileManager::stopMudEngineThread() {
    if (m_workerThread && m_workerThread->isRunning() && m_mudEngineWorker) {
        qDebug() << "Requesting MudEngineWorker to stop...";
        // Signal the worker to quit its operations.
        // The worker should handle this by stopping loops and emitting finished().
        // For now, we directly invoke quit on its thread if it has a #quit command,
        // or we can add a specific quit slot to MudEngineWorker.
        // If MudEngineWorker::processCommand handles "#quit" by emitting finished(), that's one way.
        // Alternatively:
        // QMetaObject::invokeMethod(m_mudEngineWorker, "quitSafely", Qt::QueuedConnection);
        // Then MudEngineWorker::quitSafely() would set a flag and emit finished().

        // For now, assuming a #quit command or similar will make it emit finished().
        // If not, the thread needs to be quit more directly.
        m_workerThread->quit(); // Ask the event loop to exit
        if (!m_workerThread->wait(3000)) { // Wait up to 3 seconds
            qWarning() << "MudEngineWorker thread did not quit gracefully, terminating...";
            m_workerThread->terminate();
            m_workerThread->wait(); // Wait for termination
        } else {
            qDebug() << "MudEngineWorker thread finished gracefully.";
        }
    }
    // Objects should be cleaned up by deleteLater connections.
    // Nullify pointers to prevent reuse if ProfileManager itself isn't being destroyed.
    // If ProfileManager is being destroyed, QObject parenting handles some of this.
    m_mudEngineWorker = nullptr;
    m_workerThread = nullptr;
}

void ProfileManager::sendCommandToMud(const QString& command)
{
    if (m_mudEngineWorker && m_workerThread && m_workerThread->isRunning()) {
        // Use QMetaObject::invokeMethod to ensure processCommand is called in the worker's thread
        bool success = QMetaObject::invokeMethod(m_mudEngineWorker, "processCommand", Qt::QueuedConnection,
                                     Q_ARG(QString, command));
        if (!success) {
            qWarning() << "Failed to invoke processCommand on MudEngineWorker.";
        } else {
            qDebug() << "Command queued for MudEngineWorker:" << command;
        }
    } else {
        qWarning() << "Mud engine worker not running. Command not sent:" << command;
        // Optionally, echo to main view that engine is not active
        emit textAddedToBuffer(0, "[System: MUD Engine not active. Command not sent: " + command + "]");
    }
}

// Slots for MudEngineWorker signals (to be implemented when MudEngineWorker is)
void ProfileManager::onTextReceivedFromWorker(int wndCode, const QString& text) {
    qDebug() << "ProfileManager::onTextReceivedFromWorker - WndCode:" << wndCode << "Text:" << text;
    // Process text if needed (e.g. timestamping if not done by view)
    emit textAddedToBuffer(wndCode, text);
}

void ProfileManager::onClearDisplayFromWorker(int wndCode) {
    qDebug() << "ProfileManager::onClearDisplayFromWorker - WndCode:" << wndCode;
    emit bufferCleared(wndCode);
}

void ProfileManager::reloadScripts() {
    qDebug() << "ProfileManager::reloadScripts() called.";
    if (m_mudEngineWorker && m_workerThread && m_workerThread->isRunning()) {
        // TODO: Gather all script text as in CSmcDoc::OnScriptingReload
        QString allScriptText = "// Placeholder for combined scripts from commonlib.scr, profile.scr, etc.";
        // QByteArray langGuid; // If needed for script engine
        // QMetaObject::invokeMethod(m_mudEngineWorker, "reloadScripts", Qt::QueuedConnection,
        //                           Q_ARG(QString, allScriptText), Q_ARG(QByteArray, langGuid));
        qDebug() << "Reload scripts command would be queued for MudEngineWorker.";
        emit textAddedToBuffer(0, "[System: Script reload requested (not fully implemented).]");

    } else {
        qWarning() << "Mud engine worker not running. Cannot reload scripts.";
        emit textAddedToBuffer(0, "[System: MUD Engine not active. Cannot reload scripts.]");
    }
    // This would involve:
    // 1. Reading commonlib.scr
    // 2. Reading profile-specific .scr file
    // 3. Reading other script files defined in JMC objects
    // 4. Concatenating them
    // 5. Sending to MudEngineWorker to reload script engine
}


// --- Command History Management ---
void ProfileManager::loadCommandHistory() {
    if (m_globalSettings) {
        m_commandHistory = m_globalSettings->value("History/entries").toStringList();
        // Max history size is m_inputHistorySize, which is also loaded from global settings.
        // We'll let InputBarWidget primarily manage trimming to this size when it loads the history.
        // However, we can do a preliminary trim here too.
        int maxHist = m_inputHistorySize; // getInputHistorySize() isn't const, m_inputHistorySize is available
        if (maxHist <= 0) maxHist = 20; // Fallback if not loaded yet or invalid

        while(m_commandHistory.size() > maxHist) {
            m_commandHistory.removeFirst();
        }
        qDebug() << "Loaded" << m_commandHistory.size() << "command history entries. Max size:" << maxHist;
    } else {
        qWarning() << "Global settings not available, cannot load command history.";
    }
}

void ProfileManager::saveCommandHistory() {
    if (m_globalSettings) {
        // InputBarWidget is the source of truth for current history during session.
        // ProfileManager's m_commandHistory should be updated from InputBarWidget before saving.
        // This is handled by MainWindow calling setCommandHistory before triggering global save.
        int maxHist = m_inputHistorySize;
        if (maxHist <= 0) maxHist = 20;

        while(m_commandHistory.size() > maxHist) {
            m_commandHistory.removeFirst(); // Trim if it somehow grew too large
        }
        m_globalSettings->setValue("History/entries", m_commandHistory);
        qDebug() << "Saved" << m_commandHistory.size() << "command history entries to jmc.ini.";
    } else {
        qWarning() << "Global settings not available, cannot save command history.";
    }
}

QStringList ProfileManager::getCommandHistory() const {
    return m_commandHistory;
}

void ProfileManager::setCommandHistory(const QStringList& history) {
    m_commandHistory = history;
    // Optionally, trim again here if rules are strict, though InputBarWidget should also manage its size.
    int maxHist = m_inputHistorySize;
    if (maxHist <= 0) maxHist = 20; // Fallback

    while(m_commandHistory.size() > maxHist) {
        m_commandHistory.removeFirst();
    }
}


// --- Getters for InputBarWidget global settings ---
int ProfileManager::getInputHistorySize() const { return m_inputHistorySize; }
bool ProfileManager::getClearInputAfterSend() const { return m_clearInputAfterSend; }
bool ProfileManager::getInputBarTokenInput() const { return m_inputBarTokenInput; }
bool ProfileManager::getInputBarKillOneToken() const { return m_inputBarKillOneToken; }
bool ProfileManager::getInputBarScrollEnd() const { return m_inputBarScrollEnd; }
int ProfileManager::getInputBarCursorPosWhileListing() const { return m_inputBarCursorPosWhileListing; }
int ProfileManager::getInputBarMinStrLenForHistory() const { return m_inputBarMinStrLenForHistory; }
bool ProfileManager::getAllowScriptDebug() const { return m_bAllowScriptDebug; }

void ProfileManager::createNewProfile(const QString& newName, bool copyCurrentSettingsFrom, const QString& sourceProfileName)
{
    if (newName.isEmpty()) {
        qWarning() << "New profile name cannot be empty.";
        return;
    }
    qDebug() << "Creating new profile:" << newName;

    QString newOptFilePath = QDir(m_settingsDir).filePath(newName + ".opt");
    if (QFile::exists(newOptFilePath)) {
        // Or ask user if they want to overwrite, for now, just warn and return
        qWarning() << "Profile already exists:" << newName;
        // Consider emitting an error signal or returning a status
        return;
    }

    // Save current profile first (if one is loaded)
    if (!m_currentProfileName.isEmpty() && m_profileSettings) {
        saveProfile();
    }

    if (copyCurrentSettingsFrom && !sourceProfileName.isEmpty()) {
        QString sourceOptFilePath = QDir(m_settingsDir).filePath(sourceProfileName + ".opt");
        if (QFile::exists(sourceOptFilePath)) {
            if (QFile::copy(sourceOptFilePath, newOptFilePath)) {
                qDebug() << "Copied settings from" << sourceProfileName << "to" << newName;
                // Original JMC also copied .hot file and potentially .set file (AutoLoadFile)
                // Hotkey file
                QString sourceHotPath = QDir(m_settingsDir).filePath(sourceProfileName + ".hot");
                QString newHotPath = QDir(m_settingsDir).filePath(newName + ".hot");
                if (QFile::exists(sourceHotPath)) QFile::copy(sourceHotPath, newHotPath);

                // AutoLoadFile (.set) - need to read from source .opt, then copy
                QSettings sourceOpt(sourceOptFilePath, QSettings::IniFormat);
                QString sourceSetFile = sourceOpt.value("Options/AutoLoadFile").toString();
                QString sourceSaveFile = sourceOpt.value("Options/AutoSaveFile").toString(); // Also from original logic
                QString sourceCommand = sourceOpt.value("Options/AutoSaveCommand", "").toString();


                if (!sourceSetFile.isEmpty()) {
                     QString newSetFile = newName + ".set"; // Default new name
                     // Copy the content of sourceSetFile to a file named newSetFile (local to settings dir)
                     QString absSourceSetPath = makeAbsolutePath(sourceSetFile, m_baseDir);
                     QString absNewSetPath = makeAbsolutePath(newSetFile, m_settingsDir); // Save new .set in settings
                     if(QFile::exists(absSourceSetPath)) QFile::copy(absSourceSetPath, absNewSetPath);

                     // Update the new .opt file to point to this new .set file
                     QSettings newOpt(newOptFilePath, QSettings::IniFormat);
                     newOpt.setValue("Options/AutoLoadFile", makeLocalPath(absNewSetPath, m_baseDir));
                     // If AutoSaveFile was same as AutoLoadFile, update it too, else copy original AutoSaveFile
                     if (sourceSaveFile == sourceSetFile && !sourceSetFile.isEmpty()) {
                         newOpt.setValue("Options/AutoSaveFile", makeLocalPath(absNewSetPath, m_baseDir));
                     } else if (!sourceSaveFile.isEmpty()){
                         // If different, copy the original AutoSaveFile to a new name or keep path if absolute
                         // This part of logic from original CSmcApp was complex. For now, simple copy or new default.
                         // Let's assume a new default save file name for simplicity here.
                         QString newSaveFile = newName + ".sav"; // Example
                         newOpt.setValue("Options/AutoSaveFile", newSaveFile); // Point to a new default
                     }
                     newOpt.setValue("Options/AutoSaveCommand", sourceCommand); // Copy command
                     newOpt.sync();
                }
            } else {
                qWarning() << "Failed to copy settings from" << sourceProfileName;
                // Fallback to creating a default new profile
            }
        } else {
            qWarning() << "Source profile for copy does not exist:" << sourceProfileName;
             // Fallback to creating a default new profile
        }
    } else {
        // Create a minimal .opt file if not copying, or let QSettings create it on first write
        QSettings newOpt(newOptFilePath, QSettings::IniFormat);
        newOpt.setValue("Options/AutoLoadFile", newName + ".set"); // Default .set file
        newOpt.setValue("Options/AutoSaveFile", newName + ".set"); // Default save file
        newOpt.setValue("Options/AutoSaveCommand", "");
        // Add any other minimal default settings required for a new profile
        newOpt.sync();
        qDebug() << "Created new default .opt file for profile:" << newName;
    }

    // Load the newly created profile
    loadProfile(newName);
}
