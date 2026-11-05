#include "presentation.hpp"
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

static void initializePresentationResources() {
    Q_INIT_RESOURCE(presentation);
}
namespace cl {
Presentation::Presentation(QObject *parent) : QTranslator(parent) {
    qApp->installEventFilter(this);
    initializePresentationResources();
    QFile file(":/i18n/en.json");
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Missing embedded English translation catalog");
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        qFatal("Invalid translation catalog");
    const auto translations = document.object();
    for (auto it = translations.begin(); it != translations.end(); ++it) {
        english_.insert(it.key(), it.value().toString());
        if (!it.key().contains(QRegularExpression("%[1-9]")))
            continue;
        QString expression = "^";
        QList<int> arguments;
        auto matches = QRegularExpression("%([1-9])").globalMatch(it.key());
        qsizetype position = 0;
        while (matches.hasNext()) {
            auto match = matches.next();
            expression +=
                QRegularExpression::escape(it.key().mid(position, match.capturedStart() - position));
            expression += "(.*?)";
            arguments.append(match.captured(1).toInt());
            position = match.capturedEnd();
        }
        expression += QRegularExpression::escape(it.key().mid(position)) + "$";
        patterns_.append({QRegularExpression(expression, QRegularExpression::DotMatchesEverythingOption),
                          it.value().toString(), arguments});
    }
}
QString Presentation::translate(const char *context, const char *source, const char *, int) const {
    if (qstrcmp(context, "ConsoleLAN") == 0) {
        if (language_ == "en")
            return english_.value(QString::fromUtf8(source));
        return {};
    }
    if (language_ == "es") {
        static const QHash<QString, QString> buttons = {{"&Yes", "&Sí"},
                                                        {"Yes", "Sí"},
                                                        {"&No", "&No"},
                                                        {"No", "No"},
                                                        {"Cancel", "Cancelar"},
                                                        {"&Cancel", "&Cancelar"},
                                                        {"Close", "Cerrar"},
                                                        {"&Close", "&Cerrar"},
                                                        {"OK", "Aceptar"},
                                                        {"Open", "Abrir"},
                                                        {"Save", "Guardar"},
                                                        {"&Save", "&Guardar"},
                                                        {"&Open", "&Abrir"},
                                                        {"Look in:", "Buscar en:"},
                                                        {"&Look in:", "&Buscar en:"},
                                                        {"File &name:", "&Nombre del archivo:"},
                                                        {"Files of type:", "Tipo de archivo:"},
                                                        {"File name:", "Nombre del archivo:"},
                                                        {"Save As", "Guardar como"},
                                                        {"Name", "Nombre"},
                                                        {"Size", "Tamaño"},
                                                        {"Type", "Tipo"},
                                                        {"Date Modified", "Modificado"},
                                                        {"Date modified", "Modificado"},
                                                        {"Computer", "Equipo"},
                                                        {"Back", "Atrás"},
                                                        {"Forward", "Adelante"},
                                                        {"Parent Directory", "Carpeta superior"},
                                                        {"Create New Folder", "Crear carpeta"},
                                                        {"New Folder", "Nueva carpeta"},
                                                        {"List View", "Vista de lista"},
                                                        {"Detail View", "Vista detallada"},
                                                        {"Show hidden files", "Mostrar archivos ocultos"},
                                                        {"&Rename", "Cambia&r nombre"},
                                                        {"&Delete", "&Eliminar"},
                                                        {"Show ", "Mostrar "},
                                                        {"Folder", "Carpeta"},
                                                        {"&Undo", "&Deshacer"},
                                                        {"&Redo", "&Rehacer"},
                                                        {"Cu&t", "Cor&tar"},
                                                        {"&Copy", "&Copiar"},
                                                        {"&Paste", "&Pegar"},
                                                        {"Delete", "Eliminar"},
                                                        {"Select All", "Seleccionar todo"}};
        const QByteArray qtContext(context);
        if (qtContext.startsWith("QFile") || qtContext == "QSidebar" || qtContext == "QPlatformTheme" ||
            qtContext == "QDialogButtonBox" || qtContext == "QWidgetTextControl" || qtContext == "QLineEdit")
            return buttons.value(QString::fromUtf8(source));
    }
    return {};
}
QString Presentation::message(const QString &source) const {
    if (language_ != "en" || source.isEmpty())
        return source;
    const auto exact = english_.constFind(source);
    if (exact != english_.cend())
        return *exact;
    // Logs keep timestamps and user-entered names/data untouched.
    const auto separator = source.indexOf("  ");
    if (source.front().isDigit() && separator >= 20 && separator <= 40)
        return source.left(separator + 2) + message(source.mid(separator + 2));
    for (const auto &pattern : patterns_) {
        const auto match = pattern.expression.match(source);
        if (!match.hasMatch())
            continue;
        QString result = pattern.translated;
        // Replace from the original translated format; captured user data is never reinterpreted.
        QString output;
        auto placeholders = QRegularExpression("%([1-9])").globalMatch(result);
        qsizetype position = 0;
        while (placeholders.hasNext()) {
            const auto placeholder = placeholders.next();
            output += result.mid(position, placeholder.capturedStart() - position);
            auto argument = pattern.arguments.indexOf(placeholder.captured(1).toInt());
            output += argument >= 0 ? match.captured(argument + 1) : placeholder.captured();
            position = placeholder.capturedEnd();
        }
        return output + result.mid(position);
    }
    return source; // Native Npcap/Windows errors retain their original language.
}
void Presentation::setLanguage(QString language) {
    language = language == "en" ? "en" : "es";
    language_ = language;
    QLocale::setDefault(QLocale(language == "en" ? QLocale::English : QLocale::Spanish));
    QSettings settings;
    settings.setValue("appearance/language", language_);
    settings.sync();
    QCoreApplication::removeTranslator(this);
    QCoreApplication::installTranslator(this);
}
void Presentation::setTheme(QString theme) {
    theme_ = theme == "dark" ? "dark" : "light";
    const bool dark = theme_ == "dark";
    const QColor background(dark ? "#101824" : "#f6f8fb"), surface(dark ? "#192435" : "#ffffff"),
        foreground(dark ? "#e7edf6" : "#20334b"), muted(dark ? "#a9b8cc" : "#52667d"),
        border(dark ? "#35465d" : "#ccd7e5"), alternate(dark ? "#202e42" : "#f1f5fa"),
        button(dark ? "#25364c" : "#edf2f8"), disabled(dark ? "#7f91aa" : "#7a8797"),
        accent(dark ? "#69b5ff" : "#2166ce");
    QPalette palette;
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        auto fg = group == QPalette::Disabled ? disabled : foreground;
        palette.setColor(group, QPalette::Window, background);
        palette.setColor(group, QPalette::WindowText, fg);
        palette.setColor(group, QPalette::Base, surface);
        palette.setColor(group, QPalette::AlternateBase, alternate);
        palette.setColor(group, QPalette::Text, fg);
        palette.setColor(group, QPalette::Button, button);
        palette.setColor(group, QPalette::ButtonText, fg);
        palette.setColor(group, QPalette::Highlight, QColor(dark ? "#294f7b" : "#d8e8ff"));
        palette.setColor(group, QPalette::HighlightedText, foreground);
        palette.setColor(group, QPalette::PlaceholderText, muted);
        palette.setColor(group, QPalette::ToolTipBase, surface);
        palette.setColor(group, QPalette::ToolTipText, foreground);
        palette.setColor(group, QPalette::Link, accent);
        palette.setColor(group, QPalette::Accent, accent);
        palette.setColor(group, QPalette::Light, border.lighter());
        palette.setColor(group, QPalette::Mid, border);
        palette.setColor(group, QPalette::Dark, background.darker());
    }
    qApp->setPalette(palette);
    const auto css =
        QString(R"(
        QWidget { font-family: 'Segoe UI'; font-size: 12px; color: %1; }
        QMainWindow,QDialog { background: %2; }
        QLabel { background: transparent; }
        QLabel[role='heading'] { font-size: 28px; font-weight: 700; }
        QLabel[role='muted'] { color: %6; }
        QLabel[role='status'] { background: %4; border: 1px solid %5; border-radius: 7px; padding: 10px; font-weight: 600; }
        QLabel[role='surface'] { background: %4; padding: 9px; border-radius: 5px; }
        QLabel[role='error'] { color: %9; }
        QGroupBox { background: %3; border: 1px solid %5; border-radius: 7px; margin-top: 10px; padding-top: 12px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; }
        QPushButton { background: %7; border: 1px solid %5; border-radius: 5px; padding: 6px 10px; }
        QPushButton:hover { background: %4; border-color: %8; }
        QPushButton:focus,QComboBox:focus,QLineEdit:focus,QSpinBox:focus { border: 1px solid %8; }
        QPushButton[role='primary'] { background: #2166ce; color: #ffffff; font-weight: 600; }
        QPushButton:disabled,QComboBox:disabled,QLineEdit:disabled { color: %10; }
        QLineEdit,QComboBox,QSpinBox { background: %3; border: 1px solid %5; border-radius: 4px; padding: 6px; }
        QComboBox QAbstractItemView { background: %3; color: %1; selection-background-color: %4; }
        QTableWidget { background: %3; alternate-background-color: %4; border: 1px solid %5; gridline-color: %5; selection-background-color: %4; }
        QHeaderView::section { background: %4; color: %1; border: 0; padding: 6px; font-weight: 600; }
        QTableCornerButton::section { background: %4; border: 0; }
        QPlainTextEdit,QTextBrowser { background: %3; color: %1; border: 1px solid %5; selection-background-color: %4; }
        QPlainTextEdit { font-family: Consolas; }
        QTabWidget::pane { background: %3; border: 1px solid %5; }
        QTabBar::tab { background: %7; padding: 8px 14px; border: 1px solid %5; }
        QTabBar::tab:selected { background: %3; color: %8; }
        QToolTip { background: %3; color: %1; border: 1px solid %5; }
    )")
            .arg(foreground.name(), background.name(), surface.name(), alternate.name(), border.name(),
                 muted.name(), button.name(), accent.name(), conflictColor().name())
            .arg(disabled.name());
    qApp->setStyleSheet(css);
    for (auto window : QApplication::topLevelWidgets())
        styleWindowFrame(window);
    QSettings settings;
    settings.setValue("appearance/theme", theme_);
    settings.sync();
}
void Presentation::styleWindowFrame(QWidget *widget) const {
#ifdef Q_OS_WIN
    if (widget->isWindow() && widget->windowHandle()) {
        const BOOL dark = theme_ == "dark";
        DwmSetWindowAttribute(reinterpret_cast<HWND>(widget->winId()), 20, &dark, sizeof(dark));
    }
#else
    Q_UNUSED(widget);
#endif
}
bool Presentation::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Show)
        if (auto widget = qobject_cast<QWidget *>(watched))
            styleWindowFrame(widget);
    return QTranslator::eventFilter(watched, event);
}
void Presentation::loadPreferences() {
    if (qApp->style()->objectName() != "fusion")
        qApp->setStyle(QStyleFactory::create("Fusion"));
    QSettings settings;
    setLanguage(
        settings.value("appearance/language", QLocale::system().language() == QLocale::Spanish ? "es" : "en")
            .toString());
    setTheme(settings.value("appearance/theme", "light").toString());
}
Presentation &presentation() {
    static auto *instance = new Presentation(qApp);
    return *instance;
}
QString text(const QString &source) {
    const auto utf8 = source.toUtf8();
    return QCoreApplication::translate("ConsoleLAN", utf8.constData());
}
QString message(const QString &source) {
    return presentation().message(source);
}
void rememberUiText(QWidget *root) {
    for (auto widget : root->findChildren<QWidget *>()) {
        if (widget->property("dynamicText").toBool())
            continue;
        if (auto label = qobject_cast<QLabel *>(widget))
            widget->setProperty("sourceText", label->text());
        else if (auto button = qobject_cast<QAbstractButton *>(widget))
            widget->setProperty("sourceText", button->text());
        else if (auto group = qobject_cast<QGroupBox *>(widget))
            widget->setProperty("sourceText", group->title());
        if (auto edit = qobject_cast<QLineEdit *>(widget))
            widget->setProperty("sourcePlaceholder", edit->placeholderText());
        if (auto tabs = qobject_cast<QTabWidget *>(widget)) {
            QStringList labels;
            for (int i = 0; i < tabs->count(); ++i)
                labels << tabs->tabText(i);
            widget->setProperty("sourceTabs", labels);
        }
        if (auto table = qobject_cast<QTableWidget *>(widget)) {
            QStringList headers;
            for (int i = 0; i < table->columnCount(); ++i)
                headers << table->horizontalHeaderItem(i)->text();
            widget->setProperty("sourceHeaders", headers);
        }
    }
}
void translateUiText(QWidget *root) {
    for (auto widget : root->findChildren<QWidget *>()) {
        if (widget->property("sourceText").isValid()) {
            const auto translated = text(widget->property("sourceText").toString());
            if (auto label = qobject_cast<QLabel *>(widget))
                label->setText(translated);
            else if (auto button = qobject_cast<QAbstractButton *>(widget))
                button->setText(translated);
            else if (auto group = qobject_cast<QGroupBox *>(widget))
                group->setTitle(translated);
        }
        if (auto edit = qobject_cast<QLineEdit *>(widget))
            edit->setPlaceholderText(text(widget->property("sourcePlaceholder").toString()));
        if (auto tabs = qobject_cast<QTabWidget *>(widget)) {
            const auto labels = widget->property("sourceTabs").toStringList();
            for (int i = 0; i < labels.size(); ++i)
                tabs->setTabText(i, text(labels[i]));
        }
        if (auto table = qobject_cast<QTableWidget *>(widget)) {
            auto headers = widget->property("sourceHeaders").toStringList();
            for (auto &header : headers)
                header = text(header);
            table->setHorizontalHeaderLabels(headers);
        }
    }
}
} // namespace cl
