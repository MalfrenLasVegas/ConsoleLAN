#pragma once
#include <QtWidgets>
#include "worker.hpp"
#include "presentation.hpp"
namespace cl {
class Window : public QMainWindow {
    Q_OBJECT
    QString dataDir_;
    std::unique_ptr<Worker> worker_;
    QComboBox *vpn_, *ethernet_;
    QComboBox *language_, *theme_;
    QLineEdit *roomName_, *subnet_, *direct_;
    QSpinBox *budget_;
    QLabel *status_, *roomInfo_, *counters_, *ipInfo_, *errors_;
    QTableWidget *offers_, *consoles_;
    QTableWidget *reservations_;
    QPlainTextEdit *logs_, *diagnostics_;
    QPushButton *host_, *join_, *leave_;
    QJsonObject view_;
    QList<Adapter> adapters_;
    QString selectedConsole_, selectedOwner_;
    QStringList rawLogs_;
    bool uiReady_ = false, retranslating_ = false;
    void refreshAdapters(bool enumerate = true);
    void retranslate();
    void appendLog(QString line);
    void changeEvent(QEvent *event) override;
    QJsonObject command(QString op) const;
    void post(QJsonObject c);
    Q_INVOKABLE void updateView(QJsonObject v);
    void selectionChanged();
    void joinDirect();
    void exportDiagnostic();
    QString configuration() const;

  public:
    explicit Window(QString dataDir, bool startWorker = true);
    ~Window();
};
} // namespace cl
