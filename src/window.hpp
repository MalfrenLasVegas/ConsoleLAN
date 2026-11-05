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
    QStackedWidget *pages_, *roomForms_, *roomPanels_, *consolePanels_, *discoveryPanels_,
        *reservationPanels_;
    QScrollArea *playScroll_;
    QGridLayout *connectionGrid_;
    QFrame *networkCard_, *roomCard_, *consoleDetails_;
    QLabel *stateBadge_, *nextStep_, *consoleMeta_, *consoleDetailTitle_, *quickStats_, *reservationsEmpty_,
        *discoveryHint_, *consoleHelp_, *accessStatus_;
    QPushButton *copy_, *manage_, *joinFound_, *add_, *accessToggle_;
    QAction *applyEthernet_, *relearnMac_, *forgetConsole_;
    bool wideLayout_ = true;
    bool discoverySearching_ = false, discoverySearched_ = false;
    int searchGeneration_ = 0, copyGeneration_ = 0;
    QJsonObject view_;
    QList<Adapter> adapters_;
    QString selectedConsole_, selectedOwner_, copiedConsole_, copiedOwner_;
    QStringList rawLogs_;
    bool uiReady_ = false, retranslating_ = false;
    void refreshAdapters(bool enumerate = true);
    void retranslate();
    void appendLog(QString line);
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void updateGuidance();
    void updateConsoleDetails();
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
