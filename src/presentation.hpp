#pragma once
#include <QtWidgets>
namespace cl {
// Presentation only: persisted reservations and wire messages keep their original values.
class Presentation final : public QTranslator {
    struct Pattern {
        QRegularExpression expression;
        QString translated;
        QList<int> arguments;
    };
    QHash<QString, QString> english_;
    QList<Pattern> patterns_;
    QString language_ = "es", theme_ = "light";
    void styleWindowFrame(QWidget *widget) const;
    bool eventFilter(QObject *watched, QEvent *event) override;

  public:
    explicit Presentation(QObject *parent = nullptr);
    bool isEmpty() const override {
        return false;
    }
    QString translate(const char *context, const char *source, const char *disambiguation = nullptr,
                      int n = -1) const override;
    QString message(const QString &source) const;
    QString language() const {
        return language_;
    }
    QString theme() const {
        return theme_;
    }
    void setLanguage(QString language);
    void setTheme(QString theme);
    void loadPreferences();
    QColor conflictColor() const {
        return theme_ == "dark" ? QColor("#ff9d8f") : QColor("#a63225");
    }
};
Presentation &presentation();
QString text(const QString &source);
QString message(const QString &source);
void rememberUiText(QWidget *root);
void translateUiText(QWidget *root);
} // namespace cl
