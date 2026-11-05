#include "window.hpp"
#include <QtTest>

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
        QCoreApplication::processEvents();
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
        QVERIFY(consoles->item(0, 5)->text().startsWith("Traffic observed"));
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
    void renderLanguageThemeMatrix() {
        for (const auto &language : {QString("es"), QString("en")}) {
            for (const auto &theme : {QString("light"), QString("dark")}) {
                presentation().setLanguage(language);
                presentation().setTheme(theme);
                Window window(preferences_.path(), false);
                window.show();
                QVERIFY(QTest::qWaitForWindowExposed(&window));
                QCoreApplication::processEvents();
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
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
