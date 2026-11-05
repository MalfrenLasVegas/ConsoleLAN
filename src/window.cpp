#include "window.hpp"
namespace cl {
static QPushButton *button(const QString &label, QBoxLayout *layout) {
    auto b = new QPushButton(label);
    b->setMinimumHeight(34);
    layout->addWidget(b);
    return b;
}
static void cell(QTableWidget *t, int row, int column, const QString &text, const QString &key = {}) {
    auto item = new QTableWidgetItem(text);
    if (!key.isEmpty())
        item->setData(Qt::UserRole, key);
    t->setItem(row, column, item);
}
static QTableWidget *table(QStringList headings) {
    auto t = new QTableWidget(0, headings.size());
    t->setHorizontalHeaderLabels(headings);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->hide();
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setAlternatingRowColors(true);
    return t;
}
Window::Window(QString dataDir) : dataDir_(std::move(dataDir)) {
    setWindowTitle("ConsoleLAN 1.0 — LAN entre casas");
    resize(1180, 880);
    setMinimumSize(960, 720);
    auto central = new QWidget;
    auto root = new QVBoxLayout(central);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(10);
    setCentralWidget(central);
    auto top = new QHBoxLayout;
    auto title = new QLabel("ConsoleLAN <span style='color:#718299;font-size:17px'>1.0</span>");
    title->setStyleSheet("font-size:28px;font-weight:700;");
    top->addWidget(title);
    top->addStretch();
    auto badge = new QLabel("ETHERNET  →  VPN  →  ETHERNET");
    badge->setStyleSheet("color:#52667d;font-weight:600");
    top->addWidget(badge);
    root->addLayout(top);
    auto subtitle =
        new QLabel("Conecta consolas en una sala LAN a través de tu VPN. Un PC puente por cada red física.");
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);
    status_ = new QLabel("Sin sala");
    status_->setWordWrap(true);
    status_->setStyleSheet("background:#e9f1ff;color:#183c69;border-radius:7px;padding:10px;font-weight:600");
    root->addWidget(status_);
    auto tabs = new QTabWidget;
    root->addWidget(tabs, 1);
    auto page = new QWidget;
    auto layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);
    tabs->addTab(page, "Sala y consolas");
    auto network = new QGroupBox("1   Interfaces · funciones separadas");
    auto networkForm = new QGridLayout(network);
    vpn_ = new QComboBox;
    vpn_->setMinimumWidth(360);
    ethernet_ = new QComboBox;
    networkForm->addWidget(new QLabel("VPN · transporte UDP"), 0, 0);
    networkForm->addWidget(vpn_, 0, 1);
    auto refresh = new QPushButton("Actualizar adaptadores");
    networkForm->addWidget(refresh, 0, 2);
    networkForm->addWidget(new QLabel("Ethernet · consolas y Npcap"), 1, 0);
    networkForm->addWidget(ethernet_, 1, 1, 1, 2);
    networkForm->setColumnStretch(1, 1);
    layout->addWidget(network);
    auto roomBox = new QGroupBox("2   Sala · anfitrión como coordinador y relay");
    auto r = new QGridLayout(roomBox);
    roomName_ = new QLineEdit("Mi sala LAN");
    subnet_ = new QLineEdit("10.253.42.0");
    subnet_->setMaximumWidth(150);
    budget_ = new QSpinBox;
    budget_->setRange(512, 1400);
    budget_->setValue(1100);
    budget_->setSuffix(" bytes UDP");
    r->addWidget(new QLabel("Nombre"), 0, 0);
    r->addWidget(roomName_, 0, 1);
    r->addWidget(new QLabel("Subred /24"), 0, 2);
    r->addWidget(subnet_, 0, 3);
    r->addWidget(budget_, 0, 4);
    host_ = new QPushButton("Crear sala");
    r->addWidget(host_, 0, 5);
    direct_ = new QLineEdit;
    direct_->setPlaceholderText("IP VPN del anfitrión (ej. 10.147.0.8)");
    r->addWidget(direct_, 1, 0, 1, 3);
    join_ = new QPushButton("Conectar / reconectar");
    r->addWidget(join_, 1, 3, 1, 2);
    leave_ = new QPushButton("Salir");
    r->addWidget(leave_, 1, 5);
    offers_ = table({"Sala disponible", "Anfitrión VPN", "Participantes"});
    offers_->setMaximumHeight(125);
    offers_->setMinimumHeight(75);
    r->addWidget(offers_, 2, 0, 1, 5);
    auto search = new QPushButton("Buscar salas");
    r->addWidget(search, 2, 5);
    roomInfo_ = new QLabel("Sin participantes");
    roomInfo_->setWordWrap(true);
    r->addWidget(roomInfo_, 3, 0, 1, 6);
    layout->addWidget(roomBox);
    auto consolesBox = new QGroupBox("3   Consolas · reserva primero, configuración manual después");
    auto c = new QVBoxLayout(consolesBox);
    consoles_ = table({"Consola", "Sede", "IP reservada", "MAC", "Ethernet", "Estado"});
    consoles_->setMinimumHeight(140);
    c->addWidget(consoles_, 1);
    auto actions = new QHBoxLayout;
    auto add = button("Añadir consola", actions);
    auto apply = button("Aplicar Ethernet elegida", actions);
    auto reset = button("Reaprender MAC", actions);
    auto remove = button("Eliminar / olvidar", actions);
    actions->addStretch();
    auto copy = button("Copiar configuración IP", actions);
    c->addLayout(actions);
    ipInfo_ = new QLabel("Seleccione una consola para ver su configuración IP.");
    ipInfo_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ipInfo_->setWordWrap(true);
    ipInfo_->setStyleSheet("padding:9px;background:#f1f5fa;border-radius:5px");
    c->addWidget(ipInfo_);
    layout->addWidget(consolesBox, 1);
    auto note = new QLabel(
        "La prueba de Internet puede fallar: esta aplicación ofrece LAN, sin Internet ni DNS. Las consolas "
        "del mismo switch se ven directamente, incluso si sus PC están en salas distintas.");
    note->setWordWrap(true);
    note->setStyleSheet("color:#52667d");
    layout->addWidget(note);
    auto diagPage = new QWidget;
    auto d = new QVBoxLayout(diagPage);
    diagnostics_ = new QPlainTextEdit;
    diagnostics_->setReadOnly(true);
    logs_ = new QPlainTextEdit;
    logs_->setReadOnly(true);
    logs_->setMaximumBlockCount(1000);
    auto splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(diagnostics_);
    splitter->addWidget(logs_);
    d->addWidget(splitter);
    auto da = new QHBoxLayout;
    auto exportButton = button("Exportar diagnóstico y registros…", da);
    auto openData = button("Abrir carpeta de datos", da);
    auto manual = button("Abrir manual", da);
    da->addStretch();
    d->addLayout(da);
    tabs->addTab(diagPage, "Diagnóstico y registros");
    auto reservationsPage = new QWidget;
    auto reservationsLayout = new QVBoxLayout(reservationsPage);
    reservationsLayout->addWidget(new QLabel(
        "Reservas persistentes del maestro. Las consolas activas se eliminan desde su PC propietario."));
    reservations_ = table({"Consola", "IP", "MAC", "Identidad de sede", "Estado"});
    reservationsLayout->addWidget(reservations_);
    auto release = new QPushButton("Liberar reserva desconectada");
    reservationsLayout->addWidget(release);
    tabs->addTab(reservationsPage, "Reservas del maestro");
    counters_ = new QLabel("TX 0   ·   RX 0   ·   Latencia al maestro: —");
    root->addWidget(counters_);
    errors_ = new QLabel;
    errors_->setWordWrap(true);
    errors_->setStyleSheet("color:#a03827");
    root->addWidget(errors_);
    setStyleSheet(
        "QMainWindow,QWidget{font-family:'Segoe UI';font-size:12px;color:#20334b} "
        "QMainWindow{background:#f6f8fb} QGroupBox{font-weight:600;border:1px solid "
        "#d8e1ec;border-radius:7px;margin-top:10px;padding-top:12px;background:white} "
        "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 5px} "
        "QPushButton{background:#edf2f8;border:1px solid #ccd7e5;border-radius:5px;padding:6px 10px} "
        "QPushButton:hover{background:#dce8fa} QPushButton:disabled{color:#9aa5b4} "
        "QLineEdit,QComboBox,QSpinBox{background:white;border:1px solid "
        "#c8d3e0;border-radius:4px;padding:6px} "
        "QTableWidget{background:white;alternate-background-color:#f5f8fc;border:1px solid "
        "#dbe3ed;gridline-color:#eef2f7} "
        "QHeaderView::section{background:#eef3f9;border:0;padding:6px;font-weight:600} "
        "QPlainTextEdit{background:#f9fbfd;border:1px solid #d6e0ed;font-family:Consolas;font-size:12px} "
        "QTabWidget::pane{border:1px solid #d9e2ed;background:white} QTabBar::tab{padding:8px 16px} "
        "QTabBar::tab:selected{color:#155ec0;background:white}");
    host_->setStyleSheet("background:#2166ce;color:white;border:0;padding:7px 14px;font-weight:600");
    QSettings settings;
    roomName_->setText(settings.value("roomName", "Mi sala LAN").toString());
    subnet_->setText(settings.value("subnet", "10.253.42.0").toString());
    direct_->setText(settings.value("direct").toString());
    budget_->setValue(settings.value("budget", 1100).toInt());
    refreshAdapters();
    worker_ = std::make_unique<Worker>(dataDir_);
    connect(worker_.get(), &Worker::updated, this, &Window::updateView);
    connect(worker_.get(), &Worker::logged, logs_, &QPlainTextEdit::appendPlainText);
    connect(refresh, &QPushButton::clicked, this, &Window::refreshAdapters);
    connect(host_, &QPushButton::clicked, this, [this] {
        auto c = command("host");
        c["name"] = roomName_->text();
        c["subnet"] = subnet_->text().trimmed();
        post(c);
    });
    connect(join_, &QPushButton::clicked, this, &Window::joinDirect);
    connect(direct_, &QLineEdit::returnPressed, this, &Window::joinDirect);
    connect(leave_, &QPushButton::clicked, this, [this] { post(command("leave")); });
    connect(search, &QPushButton::clicked, this, [this] { post(command("search")); });
    connect(offers_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        auto item = offers_->item(row, 1);
        if (item) {
            direct_->setText(item->text());
            joinDirect();
        }
    });
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok;
        auto name = QInputDialog::getText(this, "Añadir consola", "Nombre de la consola:", QLineEdit::Normal,
                                          {}, &ok);
        if (ok && !name.trimmed().isEmpty()) {
            auto c = command("add");
            c["name"] = name;
            c["adapter"] = ethernet_->currentData().toString();
            post(c);
        }
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (selectedConsole_.isEmpty() || selectedOwner_ != view_["owner"].toString())
            return;
        auto c = command("remove");
        c["key"] = selectedConsole_;
        post(c);
    });
    connect(apply, &QPushButton::clicked, this, [this] {
        if (selectedConsole_.isEmpty() || selectedOwner_ != view_["owner"].toString())
            return;
        auto c = command("adapter");
        c["key"] = selectedConsole_;
        c["adapter"] = ethernet_->currentData().toString();
        post(c);
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        if (selectedConsole_.isEmpty() || selectedOwner_ != view_["owner"].toString())
            return;
        auto c = command("resetMac");
        c["key"] = selectedConsole_;
        post(c);
    });
    connect(consoles_, &QTableWidget::itemSelectionChanged, this, &Window::selectionChanged);
    connect(copy, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(configuration()); });
    connect(exportButton, &QPushButton::clicked, this, &Window::exportDiagnostic);
    connect(openData, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(dataDir_)); });
    connect(manual, &QPushButton::clicked, this, [this] {
        QString p = QCoreApplication::applicationDirPath() + "/docs/MANUAL.md";
        if (!QFile::exists(p))
            p = QCoreApplication::applicationDirPath() + "/../docs/MANUAL.md";
        QFile file(p);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, "Manual", "No se pudo abrir el manual: " + file.errorString());
            return;
        }
        QDialog dialog(this);
        dialog.setWindowTitle("Manual de ConsoleLAN 1.0");
        dialog.resize(820, 700);
        auto layout = new QVBoxLayout(&dialog);
        auto browser = new QTextBrowser;
        browser->setOpenExternalLinks(true);
        browser->document()->setBaseUrl(QUrl::fromLocalFile(QFileInfo(p).absolutePath() + "/"));
        browser->setMarkdown(QString::fromUtf8(file.readAll()));
        layout->addWidget(browser);
        auto close = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(close);
        dialog.exec();
    });
    connect(release, &QPushButton::clicked, this, [this] {
        auto rows = reservations_->selectionModel()->selectedRows();
        if (rows.isEmpty() || view_["mode"].toString() != "host")
            return;
        auto item = reservations_->item(rows.first().row(), 0);
        if (item->data(Qt::UserRole + 2).toBool()) {
            QMessageBox::information(this, "Reserva activa", "Elimine la consola desde su PC propietario.");
            return;
        }
        if (QMessageBox::question(this, "Liberar reserva",
                                  "La IP dejará de estar reservada. Si esa consola vuelve, podría necesitar "
                                  "reconfiguración. ¿Liberar?") != QMessageBox::Yes)
            return;
        auto c = command("release");
        c["key"] = item->data(Qt::UserRole).toString();
        c["owner"] = item->data(Qt::UserRole + 1).toString();
        post(c);
    });
}
Window::~Window() {
    QSettings s;
    s.setValue("vpn", vpn_->currentData().toString());
    s.setValue("ethernet", ethernet_->currentData().toString());
    s.setValue("roomName", roomName_->text());
    s.setValue("subnet", subnet_->text());
    s.setValue("direct", direct_->text());
    s.setValue("budget", budget_->value());
    worker_.reset();
}
void Window::refreshAdapters() {
    QSettings s;
    auto oldVpn = vpn_->currentData().toString();
    if (oldVpn.isEmpty())
        oldVpn = s.value("vpn").toString();
    auto oldEth = ethernet_->currentData().toString();
    if (oldEth.isEmpty())
        oldEth = s.value("ethernet").toString();
    adapters_ = adapters();
    vpn_->clear();
    ethernet_->clear();
    ethernet_->addItem("Automática · aprender por IP de origen", QString());
    int recommended = -1;
    for (auto &a : adapters_) {
        auto label =
            a.name + " · " + (a.ip ? ipText(a.ip) : "sin IPv4") + " · " + (a.up ? "activa" : "desconectada");
        vpn_->addItem(label, a.id + "|" + ipText(a.ip));
        auto description = (a.name + " " + a.description).toLower();
        bool vpnHint = description.contains("zerotier") || description.contains("wireguard") ||
                       description.contains("openvpn") || description.contains("tap-windows");
        if (a.up && a.ip && vpnHint &&
            (recommended < 0 || adapters_.value(recommended).description.contains("Hyper-V")))
            recommended = vpn_->count() - 1;
        ethernet_->addItem(label, a.id);
    }
    int i = vpn_->findData(oldVpn);
    if (i < 0)
        i = recommended;
    vpn_->setCurrentIndex(i);
    i = ethernet_->findData(oldEth);
    ethernet_->setCurrentIndex(i < 0 ? 0 : i);
}
QJsonObject Window::command(QString op) const {
    auto parts = vpn_->currentData().toString().split('|');
    return {{"op", op}, {"vpn", parts.value(0)}, {"vpnIp", parts.value(1)}, {"budget", budget_->value()}};
}
void Window::post(QJsonObject c) {
    worker_->post(std::move(c));
}
void Window::joinDirect() {
    auto text = direct_->text().trimmed();
    auto parts = text.split(':');
    auto ip = parseIp(parts.value(0));
    bool ok = true;
    int port = parts.size() == 2 ? parts[1].toInt(&ok) : 32190;
    if (!ip || !ok || port < 1 || port > 65535 || parts.size() > 2) {
        QMessageBox::information(this, "Conexión directa",
                                 "Introduzca la IPv4 VPN del anfitrión, opcionalmente seguida de :puerto.");
        return;
    }
    auto c = command("join");
    c["ip"] = parts[0];
    c["port"] = port;
    post(c);
}
void Window::selectionChanged() {
    auto rows = consoles_->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return;
    auto item = consoles_->item(rows[0].row(), 0);
    selectedConsole_ = item->data(Qt::UserRole).toString();
    selectedOwner_ = item->data(Qt::UserRole + 1).toString();
    ipInfo_->setText(configuration());
}
QString Window::configuration() const {
    for (auto v : view_["consoles"].toArray()) {
        auto c = consoleFromJson(v.toObject());
        if (c.key == selectedConsole_ && c.owner == selectedOwner_)
            return QString("%1\nIP: %2    Máscara: 255.255.255.0\nGateway: vacío / 0.0.0.0    DNS: sin "
                           "configurar (no se proporciona servicio DNS)\nConfigure la red por cable "
                           "manualmente. Si la consola exige gateway/DNS, consulte el manual; la prueba de "
                           "Internet no certifica el modo LAN.")
                .arg(c.name, ipText(c.ip));
    }
    return "Seleccione una consola con reserva de sala. Las consolas recordadas reciben IP al crear o entrar "
           "en una sala.";
}
void Window::updateView(QJsonObject nextView) {
    auto previousOffers = view_["offers"];
    auto previousReservations = view_["reservations"];
    view_ = std::move(nextView);
    QString mode = view_["mode"].toString();
    bool active = mode != "offline";
    vpn_->setEnabled(!active);
    host_->setEnabled(!active);
    join_->setEnabled(!active);
    leave_->setEnabled(active);
    roomName_->setEnabled(!active);
    subnet_->setEnabled(!active);
    status_->setText(view_["status"].toString());
    errors_->setText(view_["error"].toString().isEmpty() ? QString()
                                                         : "Último error: " + view_["error"].toString());
    QStringList people;
    for (auto v : view_["members"].toArray())
        people << v.toObject()["name"].toString();
    roomInfo_->setText(QString("Sala: %1   ·   Maestro: %2\nParticipantes (%3): %4")
                           .arg(view_["name"].toString().isEmpty() ? "—" : view_["name"].toString(),
                                active ? view_["master"].toString() : "—")
                           .arg(people.size())
                           .arg(people.join(", ")));
    if (previousOffers != view_["offers"]) {
        offers_->setRowCount(0);
        for (auto v : view_["offers"].toArray()) {
            auto o = v.toObject();
            int row = offers_->rowCount();
            offers_->insertRow(row);
            cell(offers_, row, 0, o["name"].toString());
            cell(offers_, row, 1, o["endpoint"].toString());
            cell(offers_, row, 2, QString::number(o["members"].toInt()));
        }
    }
    if (previousReservations != view_["reservations"]) {
        reservations_->setRowCount(0);
        for (auto value : view_["reservations"].toArray()) {
            auto o = value.toObject();
            int row = reservations_->rowCount();
            reservations_->insertRow(row);
            cell(reservations_, row, 0, o["name"].toString(), o["key"].toString());
            reservations_->item(row, 0)->setData(Qt::UserRole + 1, o["owner"].toString());
            reservations_->item(row, 0)->setData(Qt::UserRole + 2, o["active"].toBool());
            cell(reservations_, row, 1, o["ip"].toString());
            cell(reservations_, row, 2, o["mac"].toString());
            cell(reservations_, row, 3, o["owner"].toString());
            cell(reservations_, row, 4, o["active"].toBool() ? "Sede activa" : "Sede desconectada");
        }
    }
    QSignalBlocker block(consoles_);
    consoles_->setRowCount(0);
    QJsonArray cs = view_["consoles"].toArray();
    auto own = view_["owner"].toString();
    for (auto v : view_["remembered"].toArray()) {
        auto c = consoleFromJson(v.toObject());
        bool exists = false;
        for (auto cv : cs)
            if (cv.toObject()["key"] == v.toObject()["key"] && cv.toObject()["owner"].toString() == own)
                exists = true;
        if (!exists) {
            auto o = consoleJson(c);
            o["status"] = active ? "Solicitando reserva…" : "Recordada · sin sala";
            o["ip"] = QString();
            cs.append(o);
        }
    }
    for (auto v : cs) {
        auto c = consoleFromJson(v.toObject());
        bool local = c.owner == own;
        int row = consoles_->rowCount();
        consoles_->insertRow(row);
        cell(consoles_, row, 0, c.name, c.key);
        consoles_->item(row, 0)->setData(Qt::UserRole + 1, c.owner);
        cell(consoles_, row, 1, local ? "Este PC" : "Remota");
        cell(consoles_, row, 2, c.ip ? ipText(c.ip) : "Pendiente");
        cell(consoles_, row, 3, c.mac.isEmpty() ? "Esperando" : c.mac);
        QString adapter = c.adapter;
        for (auto &a : adapters_)
            if (a.id == c.adapter) {
                adapter = a.name;
                break;
            }
        cell(consoles_, row, 4, adapter.isEmpty() ? "Automática" : adapter);
        cell(consoles_, row, 5, c.status);
        if (c.status.contains("CONFLICTO"))
            for (int j = 0; j < 6; ++j)
                consoles_->item(row, j)->setForeground(QColor("#b13a25"));
        if (c.key == selectedConsole_ && c.owner == selectedOwner_)
            consoles_->selectRow(row);
    }
    ipInfo_->setText(configuration());
    int rtt = view_["rtt"].toInt(-1);
    counters_->setText(
        QString(
            "TX %1   ·   RX %2   ·   Incompletas %3   ·   Saltos estimados %4   ·   Latencia al maestro: %5")
            .arg(view_["tx"].toDouble(), 0, 'f', 0)
            .arg(view_["rx"].toDouble(), 0, 'f', 0)
            .arg(view_["incomplete"].toDouble(), 0, 'f', 0)
            .arg(view_["gaps"].toDouble(), 0, 'f', 0)
            .arg(mode == "host" ? "este PC"
                 : rtt < 0      ? "—"
                                : QString::number(rtt) + " ms"));
    QString diag =
        QString("ConsoleLAN 1.0 · diagnóstico\n\nVPN / socket: %1\nNpcap: %2\nEstado: %3\nTráfico Ethernet "
                "observado: %4\nInyecciones realizadas: %5\nErrores de envío: %6\nRechazos de protocolo: "
                "%7\nFiltradas: %8\nDescartes por cola: %9\n\nUn paquete observado no demuestra conectividad "
                "entre consolas.\nLa latencia mide ida y vuelta al maestro, no el ping del juego.\n\n")
            .arg(view_["bound"].toString(), view_["npcap"].toString(), view_["status"].toString())
            .arg(view_["observed"].toDouble(), 0, 'f', 0)
            .arg(view_["injected"].toDouble(), 0, 'f', 0)
            .arg(view_["sendErrors"].toDouble(), 0, 'f', 0)
            .arg(view_["invalid"].toDouble(), 0, 'f', 0)
            .arg(view_["filtered"].toDouble(), 0, 'f', 0)
            .arg(view_["queueDrops"].toDouble(), 0, 'f', 0);
    for (auto v : view_["captures"].toArray()) {
        auto o = v.toObject();
        diag += "Ethernet " + o["adapter"].toString() + ": " +
                (o["blocked"].toBool() ? "BLOQUEADA: PC duplicado" : "captura abierta") + "\n";
    }
    for (auto v : view_["captureErrors"].toArray())
        diag += "ERROR Ethernet: " + v.toString() + "\n";
    diag += "\nDatos: " + dataDir_;
    diagnostics_->setPlainText(diag);
}
void Window::exportDiagnostic() {
    auto path = QFileDialog::getSaveFileName(
        this, "Exportar diagnóstico", QDir::homePath() + "/ConsoleLAN-diagnostico.txt", "Texto (*.txt)");
    if (path.isEmpty())
        return;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Exportación", f.errorString());
        return;
    }
    f.write(diagnostics_->toPlainText().toUtf8());
    f.write("\n\nESTADO\n");
    f.write(QJsonDocument(view_).toJson());
    f.write("\n\nREGISTROS\n");
    QFile log(dataDir_ + "/consolelan.log");
    if (log.open(QIODevice::ReadOnly))
        f.write(log.readAll());
    if (!f.commit())
        QMessageBox::warning(this, "Exportación", f.errorString());
}
} // namespace cl
