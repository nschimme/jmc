#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>

/**
 * @brief The Application class represents the main Qt application instance.
 *
 * It is derived from QApplication and is responsible for managing global application-level
 * settings, event loop, and overall application lifecycle.
 * In the context of porting from the MFC CSmcApp, this class will handle
 * aspects like initializing the application, potentially managing global settings
 * (like the path to jmc.ini or last used profile if not handled by MainWindow directly),
 * and setting application-wide properties (e.g., application name, organization name).
 *
 * Porting Notes:
 * - Replaces CSmcApp from the MFC version.
 * - main() in main.cpp will instantiate this class.
 * - Global settings previously read directly from jmc.ini in CSmcApp::InitInstance
 *   (e.g., script language GUID, last profile) can be managed here or by MainWindow
 *   using QSettings.
 * - Socket initialization (AfxSocketInit) is handled implicitly by Qt's network module
 *   when QNetworkAccessManager or QTcpSocket are used. No explicit global init needed typically.
 * - COM initialization (CoInitialize) is not directly needed unless specific COM objects
 *   are being used that Qt doesn't abstract. For ActiveScripting (if retained), it would be necessary.
 *   However, the plan is to use Qt equivalents (e.g. QJSEngine), reducing COM dependency.
 * - Version information loading for an "About" dialog is typically handled by accessing
 *   application metadata or by embedding it directly.
 */
class Application : public QApplication
{
    Q_OBJECT
public:
    explicit Application(int &argc, char **argv);

signals:

};

#endif // APPLICATION_H
