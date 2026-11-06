#include "window.hpp"
#include <QtTest>
#include <QScopeGuard>

using namespace cl;
class UiTests : public QObject {
    Q_OBJECT
    QTemporaryDir preferences_;
    QJsonObject fixture() const {
        QJsonObject console{{"key", "console-123"},
                            {"owner", "owner-123"},
                            {"name", "Crear sala"},
                            {"ip", "10.253.42.2"},
                            {"mac", "02:00:00:00:00:01"},
                            {"adapter", "Ethernet"},
                            {"status", "Tráfico observado · conectividad sin verificar"}};
        auto reservation = console;
        reservation["active"] = true;
        return {{"mode", "client"},
                {"subnet", "10.253.42.0"},
                {"status", "Conectado al maestro · conectividad de consolas pendiente de prueba"},
                {"owner", "owner-123"},
                {"master", "10.147.0.8:32190"},
                {"name", "Sin sala"},
                {"members", QJsonArray{QJsonObject{{"name", "My PC"}}}},
                {"consoles", QJsonArray{console}},
                {"remembered", QJsonArray{console}},
                {"reservations", QJsonArray{reservation}},
                {"bound", "10.147.0.2:32190"},
                {"rtt", 17},
                {"tx", 12},
                {"rx", 9}};
    }
    void present(Window &window) {
        QVERIFY(QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection,
                                          Q_ARG(QJsonObject, fixture())));
    }
  private slots:
    void initTestCase() {
        QVERIFY(preferences_.isValid());
        QCoreApplication::setOrganizationName("ConsoleLAN-tests");
        QCoreApplication::setApplicationName("Presentation");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences_.path());
    }
    void init() {
        QSettings settings;
        settings.clear();
        settings.setValue("appearance/language", "es");
        settings.setValue("appearance/theme", "light");
        presentation().loadPreferences();
        QTest::qWait(100);
    }
    void switchLanguageDuringSession() {
        Window window(preferences_.path(), false);
        present(window);
        auto name = window.findChild<QLineEdit *>("roomName");
        auto address = window.findChild<QLineEdit *>("hostAddress");
        auto language = window.findChild<QComboBox *>("languageSelector");
        auto consoles = window.findChild<QTableWidget *>("consolesTable");
        auto status = window.findChild<QLabel *>("connectionStatus");
        auto diagnostics = window.findChild<QPlainTextEdit *>("diagnostics");
        QVERIFY(name && address && language && consoles && status && diagnostics);
        name->setText("Mi sala sin traducir");
        address->setText("10.147.0.8:32190");
        consoles->selectRow(0);
        language->setCurrentIndex(language->findData("en"));
        QTRY_COMPARE(window.findChild<QPushButton *>("createRoom")->text(), QString("Create room"));
        QCOMPARE(window.findChild<QPushButton *>("createRoom")->isEnabled(), false);
        QCOMPARE(name->text(), QString("Mi sala sin traducir"));
        QCOMPARE(address->text(), QString("10.147.0.8:32190"));
        QCOMPARE(consoles->item(0, 0)->text(), QString("Crear sala")); // A user-defined name, not UI text.
        QCOMPARE(consoles->item(0, 1)->text(), QString("This PC"));
        QCOMPARE(consoles->horizontalHeaderItem(2)->text(), QString("Reserved IP"));
        QVERIFY(consoles->item(0, 5)->text().startsWith("Activity detected"));
        QVERIFY(!consoles->selectionModel()->selectedRows().isEmpty());
        QVERIFY(status->text().startsWith("Connected to host"));
        QVERIFY(diagnostics->toPlainText().contains("Observed Ethernet packets"));
        QVERIFY(diagnostics->toPlainText().contains("connectivity still needs testing"));
        language->setCurrentIndex(language->findData("es"));
        QTRY_COMPARE(window.findChild<QPushButton *>("createRoom")->text(), QString("Crear sala"));
        QVERIFY(status->text().startsWith("Conectado al maestro"));
        QCOMPARE(consoles->item(0, 0)->text(), QString("Crear sala"));
    }
    void themesAndPersistence() {
        {
            Window window(preferences_.path(), false);
            present(window);
            const auto lightBase = qApp->palette().color(QPalette::Base);
            auto theme = window.findChild<QComboBox *>("themeSelector");
            theme->setCurrentIndex(theme->findData("dark"));
            QVERIFY(qApp->palette().color(QPalette::Window).lightness() < 60);
            QVERIFY(qApp->palette().color(QPalette::Base) != lightBase);
            QVERIFY(qApp->palette().color(QPalette::Text).lightness() > 180);
            QVERIFY(qApp->palette().color(QPalette::Disabled, QPalette::Text).lightness() > 90);
            auto language = window.findChild<QComboBox *>("languageSelector");
            language->setCurrentIndex(language->findData("en"));
            QTRY_COMPARE(window.findChild<QPushButton *>("createRoom")->text(), QString("Create room"));
            QDialog dialog(&window);
            QTextBrowser browser(&dialog);
            QVERIFY(browser.palette().color(QPalette::Base).lightness() < 70);
            QCOMPARE(QSettings().value("appearance/theme").toString(), QString("dark"));
            QCOMPARE(QSettings().value("appearance/language").toString(), QString("en"));
        }
        presentation().loadPreferences();
        Window reopened(preferences_.path(), false);
        QCOMPARE(reopened.findChild<QComboBox *>("themeSelector")->currentData().toString(), QString("dark"));
        QCOMPARE(reopened.findChild<QComboBox *>("languageSelector")->currentData().toString(),
                 QString("en"));
        QCOMPARE(reopened.findChild<QPushButton *>("createRoom")->text(), QString("Create room"));
        auto theme = reopened.findChild<QComboBox *>("themeSelector");
        theme->setCurrentIndex(theme->findData("light"));
        QVERIFY(qApp->palette().color(QPalette::Window).lightness() > 220);
    }
    void translationCatalogAndRuntimeMessages() {
        presentation().setLanguage("en");
        QFile file(":/i18n/en.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto catalog = QJsonDocument::fromJson(file.readAll()).object();
        QVERIFY(catalog.size() > 100);
        for (auto it = catalog.begin(); it != catalog.end(); ++it) {
            QVERIFY2(!it.value().toString().isEmpty(), qPrintable(it.key()));
            QCOMPARE(text(it.key()), it.value().toString());
            auto placeholders = [](QString s) {
                QStringList found;
                auto matches = QRegularExpression("%[1-9]").globalMatch(s);
                while (matches.hasNext())
                    found << matches.next().captured();
                found.sort();
                return found;
            };
            QCOMPARE(placeholders(it.key()), placeholders(it.value().toString()));
        }
        QCOMPARE(message("2026-11-05T12:00:00.000  Participante conectado: Crear sala"),
                 QString("2026-11-05T12:00:00.000  Member connected: Crear sala"));
        QCOMPARE(message("MAC aprendida por IP de origen: My PS5 = 02:00:00:00:00:01"),
                 QString("MAC learned from source IP: My PS5 = 02:00:00:00:00:01"));
        QCOMPARE(message("Tráfico observado · conectividad sin verificar"),
                 QString("Traffic observed · connectivity unverified"));
        QCOMPARE(message("native driver message"), QString("native driver message"));
    }
    void copyFeedbackAndPendingGuidance() {
        // Restore every clipboard format after exercising the existing copy action.
        auto clipboard = QApplication::clipboard();
        auto saved = new QMimeData;
        if (const auto current = clipboard->mimeData())
            for (const auto &format : current->formats())
                saved->setData(format, current->data(format));
        auto restore = qScopeGuard([saved, clipboard] { clipboard->setMimeData(saved); });
        Window window(preferences_.path(), false);
        present(window);
        auto copy = window.findChild<QPushButton *>("copyConfiguration");
        QVERIFY(copy->isEnabled());
        copy->click();
        QCOMPARE(copy->text(), QString("Copiado"));
        const auto copied = clipboard->text();
        QVERIFY(copied.contains("10.253.42.2"));
        QVERIFY(copied.contains("255.255.255.0"));
        present(window); // Status refreshes must not remove the feedback.
        QCOMPARE(copy->text(), QString("Copiado"));
        presentation().setLanguage("en");
        QTRY_COMPARE(copy->text(), QString("Copied"));
        QCOMPARE(clipboard->text(), copied); // Language changes only affect presentation.
        QTRY_COMPARE(copy->text(), QString("Copy settings"));
        auto pending = fixture();
        pending["consoles"] = QJsonArray{};
        QVERIFY(QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection,
                                          Q_ARG(QJsonObject, pending)));
        QVERIFY(!copy->isEnabled());
        QVERIFY(window.findChild<QLabel *>("nextStep")->text().startsWith("Wait for"));
        QVERIFY(window.findChild<QLabel *>("consoleConfigurationHelp")->text().startsWith("Waiting for"));
    }
    void internetPresentation() {
        Window window(preferences_.path(), false);
        auto toggle = window.findChild<QPushButton *>("internetToggle");
        auto gateway = window.findChild<QLabel *>("consoleGateway");
        auto dns = window.findChild<QLabel *>("consoleDns");
        QVERIFY(toggle && gateway && dns && !toggle->isEnabled());
        auto view = fixture();
        view["localAccess"] = QJsonObject{
            {"state", "active"}, {"requested", true}, {"internetCapable", true}, {"pcIp", "10.253.42.1"}};
        QVERIFY(
            QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection, Q_ARG(QJsonObject, view)));
        window.findChild<QTableWidget *>("consolesTable")->selectRow(0);
        QVERIFY(toggle->isEnabled());
        QCOMPARE(gateway->text(), QString("Vacía o 0.0.0.0"));
        auto access = view["localAccess"].toObject();
        access["internetRequested"] = true;
        access["internetActive"] = true;
        view["localAccess"] = access;
        QVERIFY(
            QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection, Q_ARG(QJsonObject, view)));
        QCOMPARE(toggle->text(), QString("Desactivar Internet"));
        QCOMPARE(gateway->text(), QString("10.253.42.1"));
        QCOMPARE(dns->text(), QString("10.253.42.1"));
        QString configuration;
        QVERIFY(QMetaObject::invokeMethod(&window, "configuration", Qt::DirectConnection,
                                          Q_RETURN_ARG(QString, configuration)));
        QVERIFY(configuration.contains("DNS primario: 10.253.42.1"));
        presentation().setLanguage("en");
        QTRY_COMPARE(toggle->text(), QString("Disable Internet"));
        QVERIFY(window.findChild<QLabel *>("internetStatus")->text().startsWith("Gateway and DNS enabled"));
        access["internetActive"] = false;
        access["internetRequested"] = false;
        view["localAccess"] = access;
        QVERIFY(
            QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection, Q_ARG(QJsonObject, view)));
        QCOMPARE(dns->text(), QString("Not set"));
    }
    void localAccessPresentation() {
        Window window(preferences_.path(), false);
        auto toggle = window.findChild<QPushButton *>("localAccessToggle");
        auto label = window.findChild<QLabel *>("localAccessStatus");
        QVERIFY(toggle && label);
        QVERIFY(!toggle->isEnabled());
        auto view = fixture();
        QVERIFY(
            QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection, Q_ARG(QJsonObject, view)));
        QVERIFY(toggle->isEnabled());
        QCOMPARE(toggle->text(), QString("Activar acceso local"));
        view["localAccess"] = QJsonObject{{"state", "active"}, {"requested", true}, {"pcIp", "10.253.42.1"}};
        QVERIFY(
            QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection, Q_ARG(QJsonObject, view)));
        QCOMPARE(toggle->text(), QString("Desactivar acceso local"));
        QVERIFY(label->text().contains("10.253.42.1"));
        presentation().setLanguage("en");
        QTRY_COMPARE(toggle->text(), QString("Disable local access"));
        QVERIFY(label->text().startsWith("Local TCP/IP access enabled"));
        view["localAccess"] = QJsonObject{
            {"state", "error"},
            {"requested", false},
            {"error", "Se canceló el permiso de administrador. No se ha activado el acceso local."}};
        QVERIFY(
            QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection, Q_ARG(QJsonObject, view)));
        QVERIFY(label->text().contains("permission was cancelled"));
        auto remote = view["consoles"].toArray()[0].toObject();
        remote["owner"] = "remote-owner";
        view["consoles"] = QJsonArray{remote};
        view["localAccess"] = QJsonObject{};
        QVERIFY(
            QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection, Q_ARG(QJsonObject, view)));
        QVERIFY(!toggle->isEnabled());
    }
    void renderLanguageThemeMatrix() {
        for (const auto &language : {QString("es"), QString("en")}) {
            for (const auto &theme : {QString("light"), QString("dark")}) {
                presentation().setLanguage(language);
                presentation().setTheme(theme);
                Window window(preferences_.path(), false);
                window.show();
                QVERIFY(QTest::qWaitForWindowExposed(&window));
                QTest::qWait(100);
                // Documentation previews use a synthetic adapter label, never the user's VPN IDs/IPs.
                auto vpn = window.findChild<QComboBox *>("vpnSelector");
                vpn->clear();
                vpn->addItem(language == "en" ? "Example VPN · 10.147.0.2 · up"
                                              : "VPN de ejemplo · 10.147.0.2 · activa",
                             "example|10.147.0.2");
                window.findChild<QLineEdit *>("roomName")->setText(text("Mi sala LAN"));
                QCOMPARE(window.findChild<QPushButton *>("createRoom")->text(),
                         language == "en" ? QString("Create room") : QString("Crear sala"));
                QCOMPARE(qApp->palette().color(QPalette::Window).lightness() < 60, theme == "dark");
                QVERIFY(window.grab().save("preview-" + theme + "-" + language + ".png"));
            }
        }
    }
    void guidedFlowAndContextualControls() {
        Window window(preferences_.path(), false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto panels = window.findChild<QStackedWidget *>("consolePanels");
        auto forms = window.findChild<QStackedWidget *>("roomPanels");
        QVERIFY(panels && forms);
        QCOMPARE(panels->currentIndex(), 0);
        QVERIFY(!window.findChild<QTableWidget *>("consolesTable")->isVisible());
        auto advanced = window.findChild<QToolButton *>("advancedToggle");
        QVERIFY(advanced);
        QVERIFY(!window.findChild<QWidget *>("advancedOptions")->isVisible());
        QTest::mouseClick(advanced, Qt::LeftButton);
        QVERIFY(window.findChild<QWidget *>("advancedOptions")->isVisible());
        QTest::mouseClick(window.findChild<QPushButton *>("joinMode"), Qt::LeftButton);
        QVERIFY(window.findChild<QLineEdit *>("hostAddress")->isVisible());
        QJsonObject joining{{"mode", "joining"}, {"status", "Conectando…"}};
        QVERIFY(QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection,
                                          Q_ARG(QJsonObject, joining)));
        QVERIFY(window.findChild<QPushButton *>("leaveRoom")->isVisible());
        QVERIFY(window.findChild<QPushButton *>("leaveRoom")->isEnabled());
        present(window);
        QCOMPARE(forms->currentIndex(), 1);
        QCOMPARE(panels->currentIndex(), 1);
        auto consoles = window.findChild<QTableWidget *>("consolesTable");
        consoles->selectRow(0);
        QVERIFY(window.findChild<QPushButton *>("copyConfiguration")->isEnabled());
        QVERIFY(window.findChild<QPushButton *>("manageConsole")->isEnabled());
        auto snapshot = fixture();
        auto list = snapshot["consoles"].toArray();
        auto remote = list[0].toObject();
        remote["owner"] = "remote-site";
        list[0] = remote;
        snapshot["consoles"] = list;
        snapshot["remembered"] = QJsonArray{};
        QVERIFY(QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection,
                                          Q_ARG(QJsonObject, snapshot)));
        consoles->selectRow(0);
        QVERIFY(!window.findChild<QPushButton *>("manageConsole")->isEnabled());
    }
    void renderConnectedAndCompactLayouts() {
        presentation().setTheme("dark");
        presentation().setLanguage("es");
        Window window(preferences_.path(), false);
        auto snapshot = fixture();
        snapshot["name"] = "Viernes LAN";
        auto list = snapshot["consoles"].toArray();
        auto console = list[0].toObject();
        console["name"] = "Consola del salón";
        list[0] = console;
        snapshot["consoles"] = list;
        snapshot["remembered"] = list;
        QVERIFY(QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection,
                                          Q_ARG(QJsonObject, snapshot)));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto vpn = window.findChild<QComboBox *>("vpnSelector");
        vpn->clear();
        vpn->addItem("VPN de ejemplo · 10.147.0.2", "example|10.147.0.2");
        window.findChild<QTableWidget *>("consolesTable")->selectRow(0);
        QTest::qWait(100);
        auto step = window.findChild<QWidget *>("consoleStepHeading");
        QVERIFY(step && step->isVisible());
        QCOMPARE(step->height(), 30);
        QVERIFY(window.grab().save("preview-session-dark-es.png"));
        window.resize(1000, 760);
        QTest::qWait(100);
        auto scroll = window.findChild<QScrollArea *>("playScroll");
        QVERIFY(scroll);
        QTRY_COMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        QVERIFY(window.grab().save("preview-compact-dark-es.png"));
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QTest::qWait(100);
        QVERIFY(window.grab().save("preview-compact-console-dark-es.png"));
        auto accessView = snapshot;
        accessView["localAccess"] =
            QJsonObject{{"state", "active"}, {"requested", true}, {"pcIp", "10.253.42.1"}};
        QVERIFY(QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection,
                                          Q_ARG(QJsonObject, accessView)));
        QTest::qWait(100);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QVERIFY(window.grab().save("preview-local-access-dark-es.png"));
        auto internetAccess = accessView["localAccess"].toObject();
        internetAccess["internetCapable"] = true;
        internetAccess["internetRequested"] = true;
        internetAccess["internetActive"] = true;
        accessView["localAccess"] = internetAccess;
        QVERIFY(QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection,
                                          Q_ARG(QJsonObject, accessView)));
        QTest::qWait(100);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QVERIFY(window.grab().save("preview-internet-dark-es.png"));
        auto details = window.findChild<QFrame *>("consoleDetails");
        QVERIFY(details->isVisible());
        QVERIFY(window.findChild<QLabel *>("consoleIpAddress")->text() == "10.253.42.2");
        scroll->verticalScrollBar()->setValue(0);
        window.resize(1280, 920);
        auto language = window.findChild<QComboBox *>("languageSelector");
        auto theme = window.findChild<QComboBox *>("themeSelector");
        language->setCurrentIndex(language->findData("en"));
        theme->setCurrentIndex(theme->findData("light"));
        QTest::qWait(100);
        vpn->clear();
        vpn->addItem("Example VPN · 10.147.0.2", "example|10.147.0.2");
        QVERIFY(window.grab().save("preview-session-light-en.png"));
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QVERIFY(window.grab().save("preview-internet-light-en.png"));
        language->setCurrentIndex(language->findData("es"));
        theme->setCurrentIndex(theme->findData("dark"));
        QTest::qWait(100);
        vpn->clear();
        vpn->addItem("VPN de ejemplo · 10.147.0.2", "example|10.147.0.2");
        QTest::mouseClick(window.findChild<QPushButton *>("diagnosticsNav"), Qt::LeftButton);
        QCOMPARE(window.findChild<QStackedWidget *>("mainPages")->currentIndex(), 1);
        QTest::qWait(100);
        QVERIFY(window.grab().save("preview-diagnostics-dark-es.png"));
        QTest::mouseClick(window.findChild<QPushButton *>("reservationsNav"), Qt::LeftButton);
        QCOMPARE(window.findChild<QStackedWidget *>("mainPages")->currentIndex(), 2);
        QTest::qWait(100);
        QVERIFY(window.grab().save("preview-reservations-dark-es.png"));
        QTest::mouseClick(window.findChild<QPushButton *>("playNav"), Qt::LeftButton);
        QJsonObject empty{{"mode", "offline"}, {"status", "Sin sala"}};
        QVERIFY(QMetaObject::invokeMethod(&window, "updateView", Qt::DirectConnection,
                                          Q_ARG(QJsonObject, empty)));
        QTest::mouseClick(window.findChild<QPushButton *>("joinMode"), Qt::LeftButton);
        QTest::qWait(100);
        QVERIFY(window.grab().save("preview-join-dark-es.png"));
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
