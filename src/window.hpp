#pragma once
#include <QtWidgets>
#include "worker.hpp"
namespace cl {
class Window : public QMainWindow {
    Q_OBJECT
    QString dataDir_;
    std::unique_ptr<Worker> worker_;
    QComboBox *vpn_, *ethernet_;
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
    void refreshAdapters();
    QJsonObject command(QString op) const;
    void post(QJsonObject c);
    void updateView(QJsonObject v);
    void selectionChanged();
    void joinDirect();
    void exportDiagnostic();
    QString configuration() const;

  public:
    explicit Window(QString dataDir);
    ~Window();
};
} // namespace cl
