/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2012 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MainWindow.h"
#include "QSynergyApplication.h"
#include "SetupWizard.h"
#include "SetupWizardBlocker.h"
#include "gui/AppConfig.h"
#include "gui/dotenv.h"

#include <QMessageBox>
#include <QtCore>
#include <QtGui>

#if defined(Q_OS_MAC)
#include <Carbon/Carbon.h>
#include <cstdlib>
#endif

using namespace synergy::gui;

class QThreadImpl : public QThread {
public:
  static void msleep(unsigned long msecs) { QThread::msleep(msecs); }
};

QString getSystemSettingPath();

#if defined(Q_OS_MAC)
bool checkMacAssistiveDevices();
#endif

int main(int argc, char *argv[]) {
#ifdef Q_OS_DARWIN
  /* Workaround for QTBUG-40332 - "High ping when QNetworkAccessManager is
   * instantiated" */
  ::setenv("QT_BEARER_POLL_TIMEOUT", "-1", 1);
#endif

  dotenv(".env");

  QCoreApplication::setOrganizationName("Synergy");
  QCoreApplication::setOrganizationDomain("http://symless.com/");
  QCoreApplication::setApplicationName("Synergy");

  QSynergyApplication app(argc, argv);

#if defined(Q_OS_MAC)

  if (app.applicationDirPath().startsWith("/Volumes/")) {
    QMessageBox::information(
        NULL, "Synergy",
        "Please drag Synergy to the Applications folder, "
        "and open it from there.");
    return 1;
  }

  if (!checkMacAssistiveDevices()) {
    return 1;
  }
#endif

  AppConfig appConfig;
  qRegisterMetaType<Edition>("Edition");

  MainWindow mainWindow(appConfig);

  QObject::connect(
      &app, &QSynergyApplication::aboutToQuit, &mainWindow,
      &MainWindow::onAppAboutToQuit);

  std::unique_ptr<SetupWizardBlocker> setupBlocker;
  if (qgetenv("XDG_SESSION_TYPE") == "wayland") {
    setupBlocker.reset(new SetupWizardBlocker(
        mainWindow, SetupWizardBlocker::qBlockerType::waylandDetected));
    setupBlocker->show();
    return QApplication::exec();
  }

  std::unique_ptr<SetupWizard> setupWizard;
  if (appConfig.wizardShouldRun()) {
    setupWizard.reset(new SetupWizard(mainWindow));
    setupWizard->show();
  } else {
    mainWindow.open();
  }

  return QSynergyApplication::exec();
}

#if defined(Q_OS_MAC)
bool checkMacAssistiveDevices() {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090 // mavericks

  // new in mavericks, applications are trusted individually
  // with use of the accessibility api. this call will show a
  // prompt which can show the security/privacy/accessibility
  // tab, with a list of allowed applications. synergy should
  // show up there automatically, but will be unchecked.

  if (AXIsProcessTrusted()) {
    return true;
  }

  const void *keys[] = {kAXTrustedCheckOptionPrompt};
  const void *trueValue[] = {kCFBooleanTrue};
  CFDictionaryRef options =
      CFDictionaryCreate(NULL, keys, trueValue, 1, NULL, NULL);

  bool result = AXIsProcessTrustedWithOptions(options);
  CFRelease(options);
  return result;

#else

  // now deprecated in mavericks.
  bool result = AXAPIEnabled();
  if (!result) {
    QMessageBox::information(
        NULL, "Synergy",
        "Please enable access to assistive devices "
        "System Preferences -> Security & Privacy -> "
        "Privacy -> Accessibility, then re-open Synergy.");
  }
  return result;

#endif
}
#endif
