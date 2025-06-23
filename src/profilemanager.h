#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include <QObject>
#include <QStringList>
#include <QFont>
#include <QColor>
#include <QList>

// Forward declaration for the worker class that will handle ttcoreex
class MudEngineWorker;
class QThread;

/**
 * @brief The ProfileManager class is responsible for managing MUD profiles, settings,
 * and interaction with the MUD engine (ttcoreex).
 *
 * It replaces the data management aspects of CSmcDoc from the MFC version.
 * This includes loading and saving profile settings (from .opt files), global settings
 * (like default font, colors from jmc.ini), managing tab-completion keywords,
 * and interfacing with the MUD communication thread (MudEngineWorker).
 * It will emit signals when data changes so that UI components can update.
 *
 * Porting Notes:
 * - Handles data previously in CSmcDoc (profile name, file paths, options, ANSI colors, font).
 * - Uses QSettings for loading/saving .ini and .opt files.
 * - Manages the MudEngineWorker thread which will encapsulate ttcoreex calls.
 * - Callbacks from ttcoreex (OutTextFrom, ClearContents) will be translated into
 *   signals emitted by MudEngineWorker, which ProfileManager will connect to.
 * - ProfileManager then processes this data (e.g., updates internal state or buffers if any)
 *   and emits its own signals (e.g., textAddedToBuffer, bufferCleared) for the UI views.
 * - Scripting commands like "#read", "#write", "#killall" will be sent to MudEngineWorker.
 * - Manages lists for tab-completion words (m_keywords).
 * - Stores and provides access to display options like line wrapping, timestamps, etc.
 */
class ProfileManager : public QObject
{
    Q_OBJECT
public:
    explicit ProfileManager(QObject *parent = nullptr);
    ~ProfileManager();

    static const int MAX_OUTPUT = 10; // Matching MAX_OUTPUT from original CSmcDoc/ttcoreex
    static const int SUBST_CHARS_DATA_SIZE = 256; // As per original WriteProfileBinary usage for substChars

    QString currentProfileName() const;
    bool loadProfile(const QString& profileName);
    void saveProfile();
    void createNewProfile(const QString& newName, bool copyCurrentSettingsFrom = false, const QString& sourceProfileName = QString());


    // Accessors for settings needed by views/dialogs
    QList<QColor> getForegroundColors() const;
    QList<QColor> getBackgroundColors() const;
    QFont getCurrentFont() const;
    bool getLineWrap() const;
    bool getShowTimestamps() const;
    bool getRectangleSelection() const;
    bool getRemoveEscFromSelection() const;
    bool getShowHiddenText() const;
    bool getDarkOnlyColors() const; // m_bDarkOnly from CSmcDoc
    wchar_t getCommandChar() const; // CSmcDoc::m_cCommandChar

    QStringList keywords() const; // For tab completion

    // Methods to change settings (will emit signals)
    void setAnsiColors(const QList<QColor>& foregrounds, const QList<QColor>& backgrounds, bool darkOnly);
    void setCurrentFont(const QFont& font);
    void setDisplayOption(const QString& optionName, bool value); // For lineWrap, timestamps etc.
    void setKeywords(const QStringList& keywords);
    void setCommandChar(wchar_t commandChar);


signals:
    void profileLoaded(const QString& profileName);
    void textAddedToBuffer(int wndCode, const QString& line); // wndCode 0 for main, 1+ for aux
    void bufferCleared(int wndCode);

    void keywordsChanged(const QStringList& keywords);
    void ansiColorsChanged();
    void fontChanged();
    void displayOptionsChanged(); // Catch-all for lineWrap, timestamps, etc.
    void commandCharChanged(wchar_t newChar);

public slots:
    void sendCommandToMud(const QString& command); // User input from InputBarWidget
    void reloadScripts(); // Triggered by menu action

private slots:
    // Slots to receive signals from MudEngineWorker
    void onTextReceivedFromWorker(int wndCode, const QString& text);
    void onClearDisplayFromWorker(int wndCode);
    // void onMudEngineStateChanged(...); // e.g. connected, disconnected

private:
    QString m_currentProfileName;
    QString m_baseDir; // Path to application's root or user data directory
    QString m_settingsDir; // Path to "settings" subdirectory

    // Core settings objects
    QSettings* m_globalSettings; // For jmc.ini (global app settings)
    QSettings* m_profileSettings; // For current [profileName].opt (profile-specific settings)

    // Data members mirroring CSmcDoc
    QList<QColor> m_foregroundColors;
    QList<QColor> m_backgroundColors;
    QFont m_font;
    QStringList m_keywords; // From tabwords.txt

    // Options
    wchar_t m_cCommandChar;
    // bool m_bSplitOnBackscroll; // This was global in CSmcDoc constructor, loaded in ProfileManager::loadGlobalSettings
    bool m_bRectangleSelection;  // Loaded from profile .opt
    bool m_bRemoveESCSelection;  // Loaded from profile .opt
    bool m_bLineWrap;            // Loaded from profile .opt
    bool m_bShowTimestamps;
    bool m_bShowHiddenText;
    bool m_bDarkOnly; // From global Colors section, but applies to profile's view of them

    // Logging options (from CSmcDoc, saved in profile .opt's [ANSI] section)
    bool m_bRmaSupport;
    bool m_bAppendLogTitle;
    bool m_bAnsiLog;
    bool m_bDefaultLogMode; // True for append, False for overwrite
    bool m_bHtmlLog;
    bool m_bHtmlLogTimestamps;
    bool m_bLogAsUserSeen;
    int m_logCodePage; // 0 for current, 1 for ANSI, 2 for OEM etc. (needs mapping)

    // Substitution & IAC options (from CSmcDoc, saved in profile .opt)
    bool m_bSubstitutionEnabled;
    QByteArray m_substCharsData; // Represents the substChars data (fixed size: SUBST_CHARS_DATA_SIZE)
    bool m_bIacSendSingle;
    bool m_bIacReceiveSingle;


    // Paths for current profile
    QString m_profileOptPath;
    QString m_profileSetPath;   // AutoLoadFile
    QString m_profileSavePath;  // AutoSaveFile
    QString m_profileSaveCommand;
    wchar_t m_commandDelimiter; // From CSmcDoc, used with cCommandChar

    // Scripting related (placeholders for now)
    // QByteArray m_scriptLanguageGuid; // From global jmc.ini [Script] LANGGUID (if needed for QJSEngine options)
    bool m_bAllowScriptDebug;       // From global jmc.ini [Script] AllowDebug
    int m_nScriptErrorOutputWnd;    // From global jmc.ini [Script] ErrOutput

    // EditBar options (from global jmc.ini [Main] section, used by InputBarWidget)
    int m_inputHistorySize;
    bool m_clearInputAfterSend;
    bool m_inputBarTokenInput;
    bool m_inputBarKillOneToken;
    bool m_inputBarScrollEnd;
    int m_inputBarCursorPosWhileListing; // Original: nCursorPosWhileListing
    int m_inputBarMinStrLenForHistory;   // Original: nMinStrLen

    // Logging options (bANSILog, bHTML, etc.) - to be mapped to Qt logging features or custom logic
    // These are already declared above under profile-specific, which is correct as per CSmcDoc::OnNewDocument

    MudEngineWorker* m_mudEngineWorker;
    QThread* m_workerThread;

    void initPaths();
    void loadGlobalSettings();      // For default font, colors, last profile etc. from jmc.ini
    void saveGlobalSettings();
    bool loadProfileSpecificSettings(const QString& profileName); // From .opt file
    void saveProfileSpecificSettings();
    void loadTabKeywords();
    void saveTabKeywords();
    void initializeDefaultAnsiColors();
    void startMudEngineThread();
    void stopMudEngineThread();

    // Helpers to convert between MFC profile path logic and Qt absolute paths
    QString makeAbsolutePath(const QString& relativeOrAbsoluteFile, const QString& baseDir);
    QString makeLocalPath(const QString& absoluteFile, const QString& baseDir);
};

#endif // PROFILEMANAGER_H
