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
Window::Window(QString dataDir, bool startWorker) : dataDir_(std::move(dataDir)) {
    setWindowTitle("ConsoleLAN 1.1 — LAN entre casas");
    resize(1180, 880);
    setMinimumSize(960, 720);
    auto central = new QWidget;
    auto root = new QVBoxLayout(central);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(10);
    setCentralWidget(central);
    auto top = new QHBoxLayout;
    auto title = new QLabel("ConsoleLAN <span style='color:#718299;font-size:17px'>1.1</span>");
    title->setProperty("role", "heading");
    top->addWidget(title);
    top->addStretch();
    auto badge = new QLabel("ETHERNET  →  VPN  →  ETHERNET");
    badge->setProperty("role", "muted");
    top->addWidget(badge);
    top->addSpacing(14);
    top->addWidget(new QLabel("Tema"));
    theme_ = new QComboBox;
    theme_->setObjectName("themeSelector");
    theme_->addItem("Claro", "light");
    theme_->addItem("Oscuro", "dark");
    theme_->setCurrentIndex(theme_->findData(presentation().theme()));
    top->addWidget(theme_);
    top->addWidget(new QLabel("Idioma"));
    language_ = new QComboBox;
    language_->setObjectName("languageSelector");
    language_->addItem("Español", "es");
    language_->addItem("English", "en");
    language_->setCurrentIndex(language_->findData(presentation().language()));
    top->addWidget(language_);
    root->addLayout(top);
    auto subtitle =
        new QLabel("Conecta consolas en una sala LAN a través de tu VPN. Un PC puente por cada red física.");
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);
    status_ = new QLabel("Sin sala");
    status_->setWordWrap(true);
    status_->setProperty("role", "status");
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
    ipInfo_->setProperty("role", "surface");
    c->addWidget(ipInfo_);
    layout->addWidget(consolesBox, 1);
    auto note = new QLabel(
        "La prueba de Internet puede fallar: esta aplicación ofrece LAN, sin Internet ni DNS. Las consolas "
        "del mismo switch se ven directamente, incluso si sus PC están en salas distintas.");
    note->setWordWrap(true);
    note->setProperty("role", "muted");
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
    errors_->setProperty("role", "error");
    root->addWidget(errors_);
    host_->setProperty("role", "primary");
    QSettings settings;
    roomName_->setText(settings.value("roomName", text("Mi sala LAN")).toString());
    subnet_->setText(settings.value("subnet", "10.253.42.0").toString());
    direct_->setText(settings.value("direct").toString());
    budget_->setValue(settings.value("budget", 1100).toInt());
    refreshAdapters();
    if (startWorker) {
        worker_ = std::make_unique<Worker>(dataDir_);
        connect(worker_.get(), &Worker::updated, this, &Window::updateView);
        connect(worker_.get(), &Worker::logged, this, &Window::appendLog);
    }
    connect(refresh, &QPushButton::clicked, this, [this] { refreshAdapters(); });
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
        auto name = QInputDialog::getText(this, text("Añadir consola"), text("Nombre de la consola:"),
                                          QLineEdit::Normal, {}, &ok);
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
        QString p = QCoreApplication::applicationDirPath() +
                    (presentation().language() == "en" ? "/docs/MANUAL.en.md" : "/docs/MANUAL.md");
        if (!QFile::exists(p))
            p = QCoreApplication::applicationDirPath() +
                (presentation().language() == "en" ? "/../docs/MANUAL.en.md" : "/../docs/MANUAL.md");
        QFile file(p);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, text("Manual"),
                                 text("No se pudo abrir el manual: %1").arg(file.errorString()));
            return;
        }
        QDialog dialog(this);
        dialog.setWindowTitle(text("Manual de ConsoleLAN 1.1"));
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
            QMessageBox::information(this, text("Reserva activa"),
                                     text("Elimine la consola desde su PC propietario."));
            return;
        }
        if (QMessageBox::question(
                this, text("Liberar reserva"),
                text("La IP dejará de estar reservada. Si esa consola vuelve, podría necesitar "
                     "reconfiguración. ¿Liberar?")) != QMessageBox::Yes)
            return;
        auto c = command("release");
        c["key"] = item->data(Qt::UserRole).toString();
        c["owner"] = item->data(Qt::UserRole + 1).toString();
        post(c);
    });

    vpn_->setObjectName("vpnSelector");
    ethernet_->setObjectName("ethernetSelector");
    roomName_->setObjectName("roomName");
    direct_->setObjectName("hostAddress");
    host_->setObjectName("createRoom");
    consoles_->setObjectName("consolesTable");
    reservations_->setObjectName("reservationsTable");
    status_->setObjectName("connectionStatus");
    diagnostics_->setObjectName("diagnostics");
    logs_->setObjectName("logs");
    for (auto label : {status_, roomInfo_, counters_, ipInfo_, errors_})
        label->setProperty("dynamicText", true);
    rememberUiText(this);
    uiReady_ = true;
    view_ = {{"mode", "offline"}, {"status", "Sin sala"}, {"rtt", -1}};
    retranslate();
    connect(language_, &QComboBox::currentIndexChanged, this,
            [this] { presentation().setLanguage(language_->currentData().toString()); });
    connect(theme_, &QComboBox::currentIndexChanged, this, [this] {
        presentation().setTheme(theme_->currentData().toString());
        updateView(view_);
    });
}
void Window::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange && uiReady_)
        retranslate();
}
void Window::retranslate() {
    if (!uiReady_)
        return;
    setWindowTitle(text("ConsoleLAN 1.1 — LAN entre casas"));
    translateUiText(this);
    {
        QSignalBlocker block(theme_);
        theme_->setItemText(0, text("Claro"));
        theme_->setItemText(1, text("Oscuro"));
    }
    budget_->setSuffix(text(" bytes UDP"));
    refreshAdapters(false);
    retranslating_ = true;
    updateView(view_);
    retranslating_ = false;
    QStringList translated;
    for (const auto &line : rawLogs_)
        translated << message(line);
    logs_->setPlainText(translated.join('\n'));
}
void Window::appendLog(QString line) {
    rawLogs_.append(line);
    if (rawLogs_.size() > 1000)
        rawLogs_.removeFirst();
    logs_->appendPlainText(message(line));
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
void Window::refreshAdapters(bool enumerate) {
    QSettings s;
    auto oldVpn = vpn_->currentData().toString();
    if (oldVpn.isEmpty())
        oldVpn = s.value("vpn").toString();
    auto oldEth = ethernet_->currentData().toString();
    if (oldEth.isEmpty())
        oldEth = s.value("ethernet").toString();
    if (enumerate)
        adapters_ = adapters();
    vpn_->clear();
    ethernet_->clear();
    ethernet_->addItem(text("Automática · aprender por IP de origen"), QString());
    int recommended = -1;
    for (auto &a : adapters_) {
        auto label = a.name + " · " + (a.ip ? ipText(a.ip) : text("sin IPv4")) + " · " +
                     text(a.up ? "activa" : "desconectada");
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
    if (worker_)
        worker_->post(std::move(c));
}
void Window::joinDirect() {
    auto hostText = direct_->text().trimmed();
    auto parts = hostText.split(':');
    auto ip = parseIp(parts.value(0));
    bool ok = true;
    int port = parts.size() == 2 ? parts[1].toInt(&ok) : 32190;
    if (!ip || !ok || port < 1 || port > 65535 || parts.size() > 2) {
        QMessageBox::information(
            this, text("Conexión directa"),
            text("Introduzca la IPv4 VPN del anfitrión, opcionalmente seguida de :puerto."));
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
            return text("%1\nIP: %2    Máscara: 255.255.255.0\nGateway: vacío / 0.0.0.0    DNS: sin "
                        "configurar (no se proporciona servicio DNS)\nConfigure la red por cable "
                        "manualmente. Si la consola exige gateway/DNS, consulte el manual; la prueba de "
                        "Internet no certifica el modo LAN.")
                .arg(c.name, ipText(c.ip));
    }
    return text(
        "Seleccione una consola con reserva de sala. Las consolas recordadas reciben IP al crear o entrar "
        "en una sala.");
}
void Window::updateView(QJsonObject nextView) {
    // Own these values before replacing view_: QJsonValueRef would point at the old object.
    const QJsonValue previousOffers = view_.value("offers");
    const QJsonValue previousReservations = view_.value("reservations");
    view_ = std::move(nextView);
    QString mode = view_["mode"].toString();
    bool active = mode != "offline";
    vpn_->setEnabled(!active);
    host_->setEnabled(!active);
    join_->setEnabled(!active);
    leave_->setEnabled(active);
    roomName_->setEnabled(!active);
    subnet_->setEnabled(!active);
    status_->setText(message(view_["status"].toString()));
    errors_->setText(view_["error"].toString().isEmpty()
                         ? QString()
                         : text("Último error: %1").arg(message(view_["error"].toString())));
    QStringList people;
    for (auto v : view_["members"].toArray())
        people << v.toObject()["name"].toString();
    roomInfo_->setText(text("Sala: %1   ·   Maestro: %2\nParticipantes (%3): %4")
                           .arg(view_["name"].toString().isEmpty() ? "—" : view_["name"].toString(),
                                active ? message(view_["master"].toString()) : "—")
                           .arg(people.size())
                           .arg(people.join(", ")));
    if (previousOffers != view_["offers"] || retranslating_) {
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
    if (previousReservations != view_["reservations"] || retranslating_) {
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
            cell(reservations_, row, 4, text(o["active"].toBool() ? "Sede activa" : "Sede desconectada"));
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
        cell(consoles_, row, 1, text(local ? "Este PC" : "Remota"));
        cell(consoles_, row, 2, c.ip ? ipText(c.ip) : text("Pendiente"));
        cell(consoles_, row, 3, c.mac.isEmpty() ? text("Esperando") : c.mac);
        QString adapter = c.adapter;
        for (auto &a : adapters_)
            if (a.id == c.adapter) {
                adapter = a.name;
                break;
            }
        cell(consoles_, row, 4, adapter.isEmpty() ? text("Automática") : adapter);
        cell(consoles_, row, 5, message(c.status));
        if (c.status.contains("CONFLICTO"))
            for (int j = 0; j < 6; ++j)
                consoles_->item(row, j)->setForeground(presentation().conflictColor());
        if (c.key == selectedConsole_ && c.owner == selectedOwner_)
            consoles_->selectRow(row);
    }
    ipInfo_->setText(configuration());
    int rtt = view_["rtt"].toInt(-1);
    counters_->setText(
        text("TX %1   ·   RX %2   ·   Incompletas %3   ·   Saltos estimados %4   ·   Latencia al maestro: %5")
            .arg(view_["tx"].toDouble(), 0, 'f', 0)
            .arg(view_["rx"].toDouble(), 0, 'f', 0)
            .arg(view_["incomplete"].toDouble(), 0, 'f', 0)
            .arg(view_["gaps"].toDouble(), 0, 'f', 0)
            .arg(mode == "host" ? text("este PC")
                 : rtt < 0      ? "—"
                                : QString::number(rtt) + " ms"));
    QString diag =
        text("ConsoleLAN 1.1 · diagnóstico\n\nVPN / socket: %1\nNpcap: %2\nEstado: %3\nTráfico Ethernet "
             "observado: %4\nInyecciones realizadas: %5\nErrores de envío: %6\nRechazos de protocolo: "
             "%7\nFiltradas: %8\nDescartes por cola: %9\n\nUn paquete observado no demuestra conectividad "
             "entre consolas.\nLa latencia mide ida y vuelta al maestro, no el ping del juego.\n\n")
            .arg(message(view_["bound"].toString()), message(view_["npcap"].toString()),
                 message(view_["status"].toString()))
            .arg(view_["observed"].toDouble(), 0, 'f', 0)
            .arg(view_["injected"].toDouble(), 0, 'f', 0)
            .arg(view_["sendErrors"].toDouble(), 0, 'f', 0)
            .arg(view_["invalid"].toDouble(), 0, 'f', 0)
            .arg(view_["filtered"].toDouble(), 0, 'f', 0)
            .arg(view_["queueDrops"].toDouble(), 0, 'f', 0);
    for (auto v : view_["captures"].toArray()) {
        auto o = v.toObject();
        diag += "Ethernet " + o["adapter"].toString() + ": " +
                text(o["blocked"].toBool() ? "BLOQUEADA: PC duplicado" : "captura abierta") + "\n";
    }
    for (auto v : view_["captureErrors"].toArray())
        diag += text("ERROR Ethernet: %1\n").arg(message(v.toString()));
    diag += text("\nDatos: %1").arg(dataDir_);
    if (diagnostics_->toPlainText() != diag) {
        const auto scroll = diagnostics_->verticalScrollBar()->value();
        diagnostics_->setPlainText(diag);
        diagnostics_->verticalScrollBar()->setValue(scroll);
    }
}
void Window::exportDiagnostic() {
    auto path = QFileDialog::getSaveFileName(
        this, text("Exportar diagnóstico"), QDir::homePath() + "/ConsoleLAN-diagnostic.txt",
        text("Texto (*.txt)"), nullptr, QFileDialog::DontUseNativeDialog);
    if (path.isEmpty())
        return;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, text("Exportación"), f.errorString());
        return;
    }
    f.write(diagnostics_->toPlainText().toUtf8());
    f.write(text("\n\nESTADO\n").toUtf8());
    f.write(QJsonDocument(view_).toJson());
    f.write(text("\n\nREGISTROS\n").toUtf8());
    QFile log(dataDir_ + "/consolelan.log");
    if (log.open(QIODevice::ReadOnly))
        for (const auto &line : QString::fromUtf8(log.readAll()).split('\n'))
            f.write(message(line).toUtf8() + "\n");
    if (!f.commit())
        QMessageBox::warning(this, text("Exportación"), f.errorString());
}
} // namespace cl
