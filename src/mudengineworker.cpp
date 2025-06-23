#include "mudengineworker.h"
#include <QDebug>
#include <QThread> // For QThread::currentThreadId()

MudEngineWorker* MudEngineWorker::s_instance = nullptr; // Initialize static member

MudEngineWorker::MudEngineWorker(QObject *parent)
    : QObject(parent), m_quitSignaled(false)
{
    s_instance = this;
    qDebug() << "MudEngineWorker constructor on thread:" << QThread::currentThreadId() << "s_instance set:" << s_instance;
}

MudEngineWorker::~MudEngineWorker()
{
    qDebug() << "MudEngineWorker destructor on thread:" << QThread::currentThreadId() << "s_instance was:" << s_instance;

    // #include "ttcoreex.h" // Would be needed
    // ttcoreex::CloseState();
    qInfo() << "ttcoreex::CloseState() would be called here in MudEngineWorker destructor.";

    if (s_instance == this) { // Should always be true if constructor/destructor are paired
        s_instance = nullptr;
    }
}

// --- Static Callback Proxies ---
void __stdcall MudEngineWorker::ttCoreOutputCallbackProxy(const wchar_t* text, int wndCode) {
    if (s_instance) {
        // This callback is called by ttcoreex, potentially from its own thread.
        // The handleTtCoreOutput method will emit a signal, which Qt will handle
        // correctly across threads (queued connection if necessary).
        s_instance->handleTtCoreOutput(text, wndCode);
    } else {
        qWarning() << "ttCoreOutputCallbackProxy called but s_instance is null!";
    }
}

void __stdcall MudEngineWorker::ttCoreClearCallbackProxy(int wndCode) {
    if (s_instance) {
        s_instance->handleTtCoreClear(wndCode);
    } else {
        qWarning() << "ttCoreClearCallbackProxy called but s_instance is null!";
    }
}

// --- Instance Methods to Handle Proxied Calls ---
void MudEngineWorker::handleTtCoreOutput(const wchar_t* text, int wndCode) {
    // This method is called by the static proxy, so it's in whatever thread ttcoreex used.
    // Emit the signal. Qt will ensure it's delivered appropriately to the ProfileManager's thread.
    emit textReceived(wndCode, QString::fromWCharArray(text));
}

void MudEngineWorker::handleTtCoreClear(int wndCode) {
    emit clearDisplayRequested(wndCode);
}


void MudEngineWorker::processCommand(const QString& command)
{
    if (m_quitSignaled) return;

    qDebug() << "MudEngineWorker::processCommand:" << command << "on thread:" << QThread::currentThreadId();

    // For now, just echo the command back to the main window (wndCode 0)
    // Later, this will call ttcoreex->CompileInput(command.toStdWString().c_str());
    // And text output will come via the ttCoreOutputCallback.

    // #include "ttcoreex.h" // Would be needed here
    // Assuming ttcoreex::CompileInput is declared as:
    // void __stdcall CompileInput(const wchar_t* command);

    // std::wstring wCommand = command.toStdWString();
    // ttcoreex::CompileInput(wCommand.c_str());
    qInfo() << "ttcoreex::CompileInput would be called with:" << command;


    // For testing the callback path without real ttcoreex, simulate output:
    if (command.startsWith("sim_output ")) {
        QString output = command.mid(11);
        handleTtCoreOutput(output.toStdWString().c_str(), 0); // Simulate output to window 0
    } else if (command == "sim_clear") {
        handleTtCoreClear(0); // Simulate clear for window 0
    } else {
        // Default echo if not a simulation command (can be removed once ttcoreex is active)
         QString echoedCommand = QString("Echo from Worker (CompileInput called): %1").arg(command);
         emit textReceived(0, echoedCommand); // 0 is main window
    }


    if (command.toLower() == "#quit") { // Example of a command worker might handle
        m_quitSignaled = true;
        qDebug() << "MudEngineWorker: Quit command received, preparing to finish.";
        emit finished(); // Signal that this worker is done
    }
}

/*
void MudEngineWorker::initializeEngine() {
    qDebug() << "MudEngineWorker::initializeEngine() on thread:" << QThread::currentThreadId();
    // This is where InitState(ttCoreOutputCallback, ttCoreClearCallback, mainWindowHandle) would be called.
    // mainWindowHandle would need to be passed in or retrieved carefully.
    // InitState from ttcoreex.h:
    // void InitState(TEXTOUTFUNC TextOut, CLEARTEXTAREA ClearTextArea, HWND hParent)
}
*/

/*
void MudEngineWorker::reloadScripts(const QString& allScriptsText, const QByteArray& scriptEngineGuid) {
    if (m_quitSignaled) return;
    qDebug() << "MudEngineWorker::reloadScripts() on thread:" << QThread::currentThreadId();
    // Call ttcoreex's ReloadScriptEngine here.
    // ReloadScriptEngine(allScriptsText.toStdWString(), guidFromByteArray(scriptEngineGuid), currentProfileName.toStdWString());
}
*/

/*
void MudEngineWorker::doWork() {
    qDebug() << "MudEngineWorker::doWork() starting on thread:" << QThread::currentThreadId();
    // This would be the equivalent of the loop in ClientThread that calls ReadMud()
    // while (!m_quitSignaled) {
    //    ReadMud(); // From ttcoreex, assuming it's blocking or has a timeout
    //    QThread::msleep(10); // Or some other delay to prevent tight loop if ReadMud is non-blocking
    // }
    // qDebug() << "MudEngineWorker::doWork() finishing.";
    // emit finished();
}
*/

void MudEngineWorker::initializeEngine(WId parentWindowId) {
    qDebug() << "MudEngineWorker::initializeEngine() on thread:" << QThread::currentThreadId()
             << "ParentWId:" << parentWindowId;

    // This is where InitState(ttCoreOutputCallbackProxy, ttCoreClearCallbackProxy, (HWND)parentWindowId) would be called.
    // HWND cast is platform specific. On Windows, WId can often be cast to HWND.
    // On other platforms, parentWindowId might be 0 or not directly usable by a Win32-style HWND parameter.
    // If ttcoreex is Windows-only, this is fine. If it's cross-platform, its InitState would take a more abstract handle or none.

    // #include "ttcoreex.h" // Would be needed here
    // Assuming ttcoreex::InitState is declared something like:
    // void __stdcall InitState(TEXTOUTFUNC TextOut, CLEARTEXTAREA ClearTextArea, HWND hParent);

    // HWND hwnd = reinterpret_cast<HWND>(parentWindowId); // This is Windows-specific
    // ttcoreex::InitState(MudEngineWorker::ttCoreOutputCallbackProxy,
    //                     MudEngineWorker::ttCoreClearCallbackProxy,
    //                     hwnd);

    qInfo() << "ttcoreex::InitState would be called here.";

    // After InitState, if ReadMud needs to be started in a loop, it could be triggered here.
    // For example, by starting a timer that calls a ReadMud wrapper, or by directly
    // invoking a method that contains the ReadMud loop if ReadMud itself is blocking.
    QMetaObject::invokeMethod(this, "startProcessing", Qt::QueuedConnection);
}

void MudEngineWorker::startProcessing() {
    qDebug() << "MudEngineWorker::startProcessing() starting on thread:" << QThread::currentThreadId();
    m_quitSignaled = false; // Ensure quit flag is reset if this method could be called multiple times (though not typical here)

    // This would be the equivalent of the loop in ClientThread that calls ReadMud()
    // while (!m_quitSignaled) {
    //    // #include "ttcoreex.h" // Would be needed
    //    // ttcoreex::ReadMud(); // Assuming this polls/processes MUD data and triggers callbacks
    //
    //    // Process Qt events for this thread (e.g., to handle queued method calls or quit signals)
    //    // QCoreApplication::processEvents(QEventLoop::AllEvents, 50); // If ReadMud is very short blocking
    //    // Or, more simply, just a sleep to yield:
    //    QThread::msleep(30); // Adjust sleep duration as needed (e.g., 10-50ms)
    //                          // This allows the event loop to process quit requests for the thread.
    // }

    qInfo() << "MudEngineWorker::startProcessing() loop would be here, calling ttcoreex::ReadMud(). Simulating work until #quit.";
    // For now, to keep the thread alive and responsive to #quit in processCommand:
    while(!m_quitSignaled) {
        // In a real scenario with a blocking ReadMud() that has its own timeout or event mechanism,
        // this loop might look different or ReadMud() itself would be the loop.
        // If ReadMud() is a poll, this loop is fine.
        // We also need to ensure that signals emitted from ttcoreex callbacks (other thread)
        // are processed by this thread's event loop if they connect to slots in this thread.
        // However, our callbacks emit signals that are handled by ProfileManager in the main thread.
        QThread::msleep(100); // Keep thread alive, check m_quitSignaled periodically.
                              // processCommand sets m_quitSignaled and emits finished.
    }

    qDebug() << "MudEngineWorker::startProcessing() loop finished due to m_quitSignaled.";
    // emit finished(); // Emitted by processCommand when it sets m_quitSignaled, or here if loop exits otherwise.
                     // If loop can exit for reasons other than #quit, ensure finished() is emitted.
}


// --- Scripting Related Slots ---

void MudEngineWorker::reloadAllScripts(const QString& scriptText, const QString& profileName) {
    if (m_quitSignaled) return;
    qDebug() << "MudEngineWorker::reloadAllScripts() for profile:" << profileName << "on thread:" << QThread::currentThreadId();
    // Text for scriptText is expected to be concatenated commonlib.scr, profile.scr, and other script files.
    // GUID for language: Original CSmcApp had m_guidScriptLang (CLSID_JScript or CLSID_VBScript).
    // If we standardize on QJSEngine, this GUID is less relevant, but ttcoreex::ReloadScriptEngine might still expect it.
    // For now, assume JScript GUID if ttcoreex needs one.
    // GUID JScript_GUID = {0xF414C260,0x6AC0,0x11CF,{0xB6, 0xD1, 0x00, 0xAA , 0x00, 0xBB, 0xBB,0x58}};

    // #include "ttcoreex.h" // Would be needed
    // ttcoreex::ReloadScriptEngine(scriptText.toStdWString().c_str(),
    //                              JScript_GUID, /* Or appropriate GUID handling */
    //                              profileName.toStdWString().c_str());
    qInfo() << "ttcoreex::ReloadScriptEngine would be called here.";
    emit textReceived(0, "[System: Scripts would be reloaded.]");
}

void MudEngineWorker::breakCurrentScript() {
    if (m_quitSignaled) return;
    qDebug() << "MudEngineWorker::breakCurrentScript() on thread:" << QThread::currentThreadId();
    // #include "ttcoreex.h" // Would be needed
    // ttcoreex::BreakScript();
    qInfo() << "ttcoreex::BreakScript would be called here.";
    emit textReceived(0, "[System: Script execution would be interrupted.]");
}

void MudEngineWorker::launchScriptDebugger() {
    if (m_quitSignaled) return;
    qDebug() << "MudEngineWorker::launchScriptDebugger() on thread:" << QThread::currentThreadId();
    // #include "ttcoreex.h" // Would be needed
    // ttcoreex::LunchDebugger(); // Assuming original spelling "LunchDebugger"
    qInfo() << "ttcoreex::LunchDebugger would be called here.";
    emit textReceived(0, "[System: Script debugger would be launched.]");
}
