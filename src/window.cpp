#include "window.hpp"
#include "access_policy.hpp"
namespace cl {
// Hidden pages must not reserve space for tables that are not currently displayed.
class AdaptiveStack final : public QStackedWidget {
  public:
    AdaptiveStack() {
        layout()->setSizeConstraint(QLayout::SetNoConstraint);
        connect(this, &QStackedWidget::currentChanged, this, [this] { scheduleFit(); });
    }
    bool event(QEvent *event) override {
        const bool result = QStackedWidget::event(event);
        if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize ||
            event->type() == QEvent::Show)
            scheduleFit();
        return result;
    }
    void scheduleFit() {
        QTimer::singleShot(0, this, [this] {
            auto page = currentWidget();
            if (!page)
                return;
            int height =
                page->hasHeightForWidth() ? page->heightForWidth(width()) : page->sizeHint().height();
            height = std::clamp(height, page->minimumSizeHint().height(), page->maximumHeight());
            if (minimumHeight() != height || maximumHeight() != height) {
                setFixedHeight(height);
                updateGeometry();
            }
        });
    }
    QSize sizeHint() const override {
        return currentWidget() ? currentWidget()->sizeHint().boundedTo(currentWidget()->maximumSize())
                               : QSize();
    }
    bool hasHeightForWidth() const override {
        return currentWidget() && currentWidget()->hasHeightForWidth();
    }
    int heightForWidth(int width) const override {
        return currentWidget() ? currentWidget()->heightForWidth(width) : -1;
    }
    QSize minimumSizeHint() const override {
        return currentWidget() ? currentWidget()->minimumSizeHint() : QSize();
    }
};
static QPushButton *button(const QString &label, QBoxLayout *layout) {
    auto b = new QPushButton(label);
    b->setMinimumHeight(34);
    layout->addWidget(b);
    return b;
}
static void cell(QTableWidget *t, int row, int column, const QString &text, const QString &key = {}) {
    auto item = new QTableWidgetItem(text);
    item->setToolTip(text);
    if (!key.isEmpty())
        item->setData(Qt::UserRole, key);
    t->setItem(row, column, item);
}
// Keep engine statuses intact; use clearer wording only in the console list.
static QString consoleStatus(const QString &source) {
    static const QHash<QString, QString> captions{
        {"Tráfico observado · conectividad sin verificar", "Actividad detectada · prueba el modo LAN"},
        {"MAC registrada · conectividad sin verificar", "Consola identificada · prueba el modo LAN"},
        {"MAC recordada · esperando observación en esta sesión", "Consola guardada · esperando actividad"},
        {"Esperando tráfico con IP reservada", "IP asignada · esperando actividad"},
        {"Recordada · sin sala", "Consola guardada · entra en una sala"},
        {"Solicitando reserva…", "Asignando una IP…"},
        {"CONFLICTO IP/MAC: revise la consola o use Reaprender MAC",
         "Conflicto de IP: revisa la consola o usa Volver a detectar la consola"}};
    auto caption = captions.constFind(source);
    return caption == captions.cend() ? message(source) : text(*caption);
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

    setWindowTitle("ConsoleLAN 1.3.0 — LAN entre casas");
    resize(1280, 920);
    setMinimumSize(1000, 700);
    auto central = new QWidget;
    auto shell = new QHBoxLayout(central);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);
    setCentralWidget(central);

    auto sidebar = new QFrame;
    sidebar->setProperty("role", "sidebar");
    sidebar->setFixedWidth(206);
    auto side = new QVBoxLayout(sidebar);
    side->setContentsMargins(20, 28, 20, 22);
    side->setSpacing(10);
    auto brand = new QHBoxLayout;
    auto mark = new QLabel("C");
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(34, 34);
    mark->setProperty("role", "brandMark");
    auto title = new QLabel("ConsoleLAN");
    title->setProperty("role", "brandTitle");
    brand->addWidget(mark);
    brand->addWidget(title);
    brand->addStretch();
    side->addLayout(brand);
    auto tagline = new QLabel("LAN entre casas");
    tagline->setProperty("role", "muted");
    side->addWidget(tagline);
    side->addSpacing(30);
    auto navigation = new QButtonGroup(this);
    navigation->setExclusive(true);
    const QStringList sections{"Jugar en LAN", "Diagnóstico", "Reservas"};
    const QStringList identifiers{"playNav", "diagnosticsNav", "reservationsNav"};
    for (int i = 0; i < sections.size(); ++i) {
        auto nav = new QPushButton(sections[i]);
        nav->setObjectName(identifiers[i]);
        nav->setProperty("role", "nav");
        nav->setCheckable(true);
        navigation->addButton(nav, i);
        side->addWidget(nav);
        if (i == 0)
            nav->setChecked(true);
    }
    side->addStretch();
    auto manual = new QPushButton("Manual y ayuda");
    manual->setObjectName("manualButton");
    manual->setProperty("role", "ghost");
    side->addWidget(manual);
    side->addSpacing(16);
    auto preferences = new QFormLayout;
    preferences->setHorizontalSpacing(8);
    preferences->setVerticalSpacing(10);
    theme_ = new QComboBox;
    theme_->setObjectName("themeSelector");
    theme_->addItem("Claro", "light");
    theme_->addItem("Oscuro", "dark");
    theme_->setCurrentIndex(theme_->findData(presentation().theme()));
    language_ = new QComboBox;
    language_->setObjectName("languageSelector");
    language_->addItem("Español", "es");
    language_->addItem("English", "en");
    language_->setCurrentIndex(language_->findData(presentation().language()));
    preferences->addRow(new QLabel("Tema"), theme_);
    preferences->addRow(new QLabel("Idioma"), language_);
    side->addLayout(preferences);
    side->addSpacing(18);
    auto version = new QLabel(QString("v%1 · Windows x64").arg(CONSOLELAN_VERSION));
    version->setProperty("role", "version");
    side->addWidget(version);
    shell->addWidget(sidebar);

    pages_ = new QStackedWidget;
    pages_->setObjectName("mainPages");
    shell->addWidget(pages_, 1);
    connect(navigation, &QButtonGroup::idClicked, pages_, &QStackedWidget::setCurrentIndex);
    auto makeLabel = [](QString value, const char *role) {
        auto label = new QLabel(value);
        label->setProperty("role", role);
        label->setWordWrap(true);
        return label;
    };
    auto makeCard = [] {
        auto frame = new QFrame;
        frame->setProperty("role", "card");
        return frame;
    };
    auto stepHeading = [makeLabel](const QString &number, const QString &title) {
        auto row = new QHBoxLayout;
        row->setSpacing(10);
        auto marker = makeLabel(number, "stepNumber");
        marker->setAlignment(Qt::AlignCenter);
        marker->setFixedSize(30, 30);
        row->addWidget(marker);
        row->addWidget(makeLabel(title, "cardTitle"), 1);
        return row;
    };
    auto makeField = [](QComboBox *box) {
        box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        box->setMinimumContentsLength(16);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    };
    playScroll_ = new QScrollArea;
    playScroll_->setObjectName("playScroll");
    playScroll_->setWidgetResizable(true);
    playScroll_->setFrameShape(QFrame::NoFrame);
    auto play = new QWidget;
    auto flow = new QVBoxLayout(play);
    flow->setContentsMargins(28, 28, 28, 20);
    flow->setSpacing(16);
    auto heading = new QHBoxLayout;
    heading->addWidget(makeLabel("Prepara tu partida LAN", "pageTitle"), 1);
    stateBadge_ = makeLabel("Sin sala", "badge");
    stateBadge_->setObjectName("roomBadge");
    heading->addWidget(stateBadge_, 0, Qt::AlignTop);
    flow->addLayout(heading);
    nextStep_ = makeLabel("Selecciona tu VPN y crea una sala, o únete a la de tus amigos.", "muted");
    nextStep_->setObjectName("nextStep");
    flow->addWidget(nextStep_);
    status_ = makeLabel("Sin sala", "notice");
    flow->addWidget(status_);
    errors_ = makeLabel({}, "error");
    flow->addWidget(errors_);

    connectionGrid_ = new QGridLayout;
    connectionGrid_->setSpacing(18);
    connectionGrid_->setColumnStretch(0, 1);
    connectionGrid_->setColumnStretch(1, 1);
    networkCard_ = makeCard();
    auto network = new QVBoxLayout(networkCard_);
    network->setContentsMargins(22, 20, 22, 20);
    network->setSpacing(10);
    auto networkHeading = stepHeading("1", "Tu conexión");
    auto refresh = new QPushButton("Actualizar");
    refresh->setProperty("role", "ghost");
    networkHeading->addWidget(refresh);
    network->addLayout(networkHeading);
    network->addWidget(makeLabel("VPN de este PC", "fieldLabel"));
    vpn_ = new QComboBox;
    makeField(vpn_);
    network->addWidget(vpn_);
    network->addWidget(
        makeLabel("Elige la VPN que usas con tus amigos, como ZeroTier o WireGuard.", "helper"));
    network->addSpacing(4);
    network->addWidget(makeLabel("Ethernet de las consolas", "fieldLabel"));
    ethernet_ = new QComboBox;
    makeField(ethernet_);
    network->addWidget(ethernet_);
    network->addWidget(makeLabel("Elige la conexión por cable que comparte tu PC con las consolas. Si no "
                                 "sabes cuál es, deja la detección automática.",
                                 "helper"));
    auto advanced = new QToolButton;
    advanced->setObjectName("advancedToggle");
    advanced->setText("Opciones avanzadas");
    advanced->setCheckable(true);
    advanced->setArrowType(Qt::RightArrow);
    advanced->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    network->addWidget(advanced, 0, Qt::AlignLeft);
    auto advancedBody = new QWidget;
    advancedBody->setObjectName("advancedOptions");
    auto advancedForm = new QFormLayout(advancedBody);
    advancedForm->setContentsMargins(0, 4, 0, 0);
    subnet_ = new QLineEdit("10.253.42.0");
    budget_ = new QSpinBox;
    budget_->setRange(512, 1400);
    budget_->setValue(1100);
    budget_->setSuffix(" bytes UDP");
    advancedForm->addRow(new QLabel("Subred de la sala (/24)"), subnet_);
    advancedForm->addRow(new QLabel("Tamaño máximo de paquete UDP"), budget_);
    advancedForm->addRow(makeLabel(
        "Si no necesitas una configuración especial, conserva los valores predeterminados.", "helper"));
    network->addWidget(advancedBody);
    advancedBody->hide();
    connect(advanced, &QToolButton::toggled, this, [advanced, advancedBody](bool shown) {
        advancedBody->setVisible(shown);
        advanced->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
    });
    network->addStretch();

    roomCard_ = makeCard();
    auto room = new QVBoxLayout(roomCard_);
    room->setContentsMargins(22, 20, 22, 22);
    room->setSpacing(14);
    room->addLayout(stepHeading("2", "Tu sala"));
    roomPanels_ = new AdaptiveStack;
    roomPanels_->setObjectName("roomPanels");
    auto roomSetup = new QWidget;
    auto setup = new QVBoxLayout(roomSetup);
    setup->setContentsMargins(0, 0, 0, 0);
    setup->setSpacing(14);
    auto modes = new QHBoxLayout;
    modes->setSpacing(6);
    auto createMode = new QPushButton("Crear una sala");
    auto joinMode = new QPushButton("Unirme a una sala");
    createMode->setObjectName("createMode");
    joinMode->setObjectName("joinMode");
    auto modeGroup = new QButtonGroup(this);
    for (auto entry : {std::pair{createMode, 0}, std::pair{joinMode, 1}}) {
        entry.first->setCheckable(true);
        entry.first->setProperty("role", "segment");
        modeGroup->addButton(entry.first, entry.second);
        modes->addWidget(entry.first, 1);
    }
    createMode->setChecked(true);
    setup->addLayout(modes);
    roomForms_ = new AdaptiveStack;
    auto createPage = new QWidget;
    auto create = new QVBoxLayout(createPage);
    create->setContentsMargins(0, 0, 0, 0);
    create->setSpacing(12);
    create->addWidget(makeLabel("Crea la sala y comparte la IP de tu VPN con tus amigos.", "helper"));
    roomName_ = new QLineEdit("Mi sala LAN");
    create->addWidget(new QLabel("Nombre de la sala"));
    create->addWidget(roomName_);
    host_ = new QPushButton("Crear sala");
    host_->setProperty("role", "primary");
    host_->setMinimumHeight(42);
    create->addWidget(host_);
    create->addStretch();
    roomForms_->addWidget(createPage);
    auto joinPage = new QWidget;
    auto joinLayout = new QVBoxLayout(joinPage);
    joinLayout->setContentsMargins(0, 0, 0, 0);
    joinLayout->setSpacing(12);
    joinLayout->addWidget(
        makeLabel("Pide a quien ha creado la sala la IP de su VPN e introdúcela aquí.", "helper"));
    direct_ = new QLineEdit;
    direct_->setPlaceholderText("Ejemplo: 10.147.0.8");
    joinLayout->addWidget(new QLabel("IP VPN del anfitrión"));
    joinLayout->addWidget(direct_);
    join_ = new QPushButton("Unirme a la sala");
    join_->setProperty("role", "primary");
    join_->setMinimumHeight(42);
    joinLayout->addWidget(join_);
    auto search = new QPushButton("Buscar salas en mi VPN");
    search->setObjectName("searchRooms");
    search->setProperty("role", "ghost");
    joinLayout->addWidget(search);
    discoveryPanels_ = new AdaptiveStack;
    discoveryPanels_->setObjectName("discoveryPanels");
    auto noRooms = makeLabel("También puedes buscar salas si tu VPN lo permite.", "helper");
    discoveryHint_ = noRooms;
    discoveryPanels_->addWidget(noRooms);
    auto found = new QWidget;
    auto foundLayout = new QVBoxLayout(found);
    foundLayout->setContentsMargins(0, 0, 0, 0);
    offers_ = table({"Sala disponible", "IP del anfitrión", "Participantes"});
    offers_->setMaximumHeight(170);
    foundLayout->addWidget(offers_);
    joinFound_ = new QPushButton("Entrar en la sala seleccionada");
    joinFound_->setEnabled(false);
    foundLayout->addWidget(joinFound_);
    discoveryPanels_->addWidget(found);
    joinLayout->addWidget(discoveryPanels_);
    joinLayout->addStretch();
    roomForms_->addWidget(joinPage);
    setup->addWidget(roomForms_);
    roomPanels_->addWidget(roomSetup);
    auto connectedPage = new QWidget;
    auto connected = new QVBoxLayout(connectedPage);
    connected->setContentsMargins(0, 0, 0, 0);
    connected->setSpacing(16);
    connected->addWidget(makeLabel("Conectado a la sala", "connectedTitle"));
    roomInfo_ = makeLabel("Sin participantes", "body");
    roomInfo_->setTextFormat(Qt::PlainText);
    connected->addWidget(roomInfo_);
    connected->addWidget(makeLabel(
        "Comparte la IP del anfitrión para invitar a tus amigos. Después, probad el modo LAN del juego.",
        "helper"));
    leave_ = new QPushButton("Salir de la sala");
    connected->addStretch();
    leave_->setObjectName("leaveRoom");
    roomPanels_->addWidget(connectedPage);
    room->addWidget(roomPanels_, 0, Qt::AlignTop);
    room->addWidget(leave_);
    room->addStretch();
    connect(modeGroup, &QButtonGroup::idClicked, roomForms_, &QStackedWidget::setCurrentIndex);
    connectionGrid_->addWidget(networkCard_, 0, 0);
    connectionGrid_->addWidget(roomCard_, 0, 1);
    flow->addLayout(connectionGrid_);

    auto consolesCard = makeCard();
    auto consolesLayout = new QVBoxLayout(consolesCard);
    consolesLayout->setContentsMargins(22, 20, 22, 20);
    consolesLayout->setSpacing(14);
    auto consolesHeading = new QHBoxLayout;
    auto consolesTitles = new QVBoxLayout;
    auto consoleStep = new QWidget;
    consoleStep->setObjectName("consoleStepHeading");
    consoleStep->setLayout(stepHeading("3", "Tus consolas"));
    consoleStep->layout()->setContentsMargins(0, 0, 0, 0);
    consoleStep->setFixedHeight(30);
    consolesTitles->addWidget(consoleStep);
    consolesTitles->addWidget(makeLabel(
        "Añade cada consola y usa los datos que aparecen aquí para configurar su conexión por cable.",
        "helper"));
    consolesHeading->addLayout(consolesTitles, 1);
    add_ = new QPushButton("Añadir consola");
    auto add = add_;
    add->setObjectName("addConsole");
    add->setProperty("role", "primary");
    consolesHeading->addWidget(add);
    consolesLayout->addLayout(consolesHeading);
    consolePanels_ = new AdaptiveStack;
    consolePanels_->setObjectName("consolePanels");
    auto empty = new QWidget;
    auto emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(20, 24, 20, 24);
    emptyLayout->setSpacing(10);
    auto emptyTitle = makeLabel("Añade tu primera consola", "emptyTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto emptyHint =
        makeLabel("Ponle un nombre para reconocerla. Al entrar en una sala, recibirá una IP.", "muted");
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyHint);
    auto firstConsole = new QPushButton("Añadir consola");
    firstConsole->setObjectName("addFirstConsole");
    firstConsole->setProperty("role", "primary");
    emptyLayout->addWidget(firstConsole, 0, Qt::AlignCenter);
    connect(firstConsole, &QPushButton::clicked, add, &QPushButton::click);
    consolePanels_->addWidget(empty);
    consoles_ = table({"Consola", "Sede", "IP reservada", "MAC", "Ethernet", "Estado"});
    consoles_->setShowGrid(false);
    consoles_->verticalHeader()->setDefaultSectionSize(46);
    for (int column : {1, 3, 4})
        consoles_->setColumnHidden(column, true);
    consoles_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    consoles_->setColumnWidth(0, 190);
    consoles_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    consoles_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    consoles_->setMinimumHeight(86);
    consoles_->setMaximumHeight(220);
    consolePanels_->addWidget(consoles_);
    consolesLayout->addWidget(consolePanels_);
    consoleDetails_ = new QFrame;
    consoleDetails_->setObjectName("consoleDetails");
    consoleDetails_->setProperty("role", "inset");
    auto detail = new QVBoxLayout(consoleDetails_);
    detail->setContentsMargins(18, 16, 18, 16);
    detail->setSpacing(10);
    auto detailHeading = new QHBoxLayout;
    consoleDetailTitle_ = makeLabel("Configuración de la consola", "detailTitle");
    consoleDetailTitle_->setTextFormat(Qt::PlainText);
    detailHeading->addWidget(consoleDetailTitle_, 1);
    copy_ = new QPushButton("Copiar configuración");
    auto copy = copy_;
    copy_->setProperty("role", "primary");
    copy_->setObjectName("copyConfiguration");
    copy_->setProperty("dynamicText", true);
    copy_->setMinimumWidth(166);
    detailHeading->addWidget(copy_);
    manage_ = new QPushButton("Gestionar");
    manage_->setObjectName("manageConsole");
    auto consoleMenu = new QMenu(manage_);
    consoleMenu->setToolTipsVisible(true);
    applyEthernet_ = consoleMenu->addAction("Usar la conexión por cable seleccionada");
    auto apply = applyEthernet_;
    relearnMac_ = consoleMenu->addAction("Volver a detectar la consola");
    auto reset = relearnMac_;
    consoleMenu->addSeparator();
    forgetConsole_ = consoleMenu->addAction("Quitar consola");
    auto remove = forgetConsole_;
    manage_->setMenu(consoleMenu);
    detailHeading->addWidget(manage_);
    detail->addLayout(detailHeading);
    auto addresses = new QGridLayout;
    addresses->setHorizontalSpacing(10);
    auto addressField = [&](int column, const QString &title, const QString &value) {
        auto tile = new QFrame;
        tile->setProperty("role", "addressField");
        auto contents = new QVBoxLayout(tile);
        contents->setContentsMargins(12, 10, 12, 10);
        contents->setSpacing(6);
        contents->addWidget(makeLabel(title, "helper"));
        auto field = makeLabel(value, column == 0 ? "ipValue" : "addressValue");
        field->setTextFormat(Qt::PlainText);
        field->setTextInteractionFlags(Qt::TextSelectableByMouse);
        contents->addWidget(field);
        addresses->addWidget(tile, 0, column);
        addresses->setColumnStretch(column, 1);
        return field;
    };
    ipInfo_ = addressField(0, "Dirección IP", {});
    ipInfo_->setObjectName("consoleIpAddress");
    addressField(1, "Máscara de red", "255.255.255.0");
    gatewayInfo_ = addressField(2, "Puerta de enlace", {});
    gatewayInfo_->setObjectName("consoleGateway");
    dnsInfo_ = addressField(3, "DNS", {});
    dnsInfo_->setObjectName("consoleDns");
    detail->addLayout(addresses);
    consoleHelp_ = makeLabel({}, "helper");
    consoleHelp_->setObjectName("consoleConfigurationHelp");
    detail->addWidget(consoleHelp_);
    consoleMeta_ = makeLabel({}, "helper");
    consoleMeta_->setTextFormat(Qt::PlainText);
    detail->addWidget(consoleMeta_);
    consolesLayout->addWidget(consoleDetails_);
    consoleDetails_->hide();
    flow->addWidget(consolesCard);
    auto accessCard = makeCard();
    accessCard->setObjectName("localAccessCard");
    auto accessLayout = new QVBoxLayout(accessCard);
    accessLayout->setContentsMargins(22, 20, 22, 20);
    accessLayout->setSpacing(10);
    auto accessHeading = new QHBoxLayout;
    accessHeading->addWidget(makeLabel("Acceso desde este PC · opcional", "cardTitle"), 1);
    accessToggle_ = new QPushButton;
    accessToggle_->setObjectName("localAccessToggle");
    accessToggle_->setProperty("dynamicText", true);
    accessHeading->addWidget(accessToggle_);
    accessLayout->addLayout(accessHeading);
    accessLayout->addWidget(makeLabel("Accede por FTP o a cualquier puerto TCP de tus consolas locales sin "
                                      "cambiar la IP de tu Ethernet doméstica.",
                                      "helper"));
    accessStatus_ = makeLabel({}, "muted");
    accessStatus_->setObjectName("localAccessStatus");
    accessLayout->addWidget(accessStatus_);
    accessLayout->addWidget(
        makeLabel("Windows pedirá permiso de administrador. ConsoleLAN creará un adaptador propio para "
                  "acceder a tus consolas y lo retirará al desactivar el acceso o cerrar la app. "
                  "La Ethernet y la VPN conservan sus ajustes.",
                  "helper"));
    connect(accessToggle_, &QPushButton::clicked, this, [this] {
        post({{"op", "localAccess"}, {"enabled", !view_["localAccess"].toObject()["requested"].toBool()}});
    });
    auto internetHeading = new QHBoxLayout;
    internetHeading->addWidget(makeLabel("Internet para tus consolas · opcional", "cardTitle"), 1);
    internetToggle_ = new QPushButton;
    internetToggle_->setObjectName("internetToggle");
    internetToggle_->setProperty("dynamicText", true);
    internetHeading->addWidget(internetToggle_);
    accessLayout->addLayout(internetHeading);
    internetStatus_ = makeLabel({}, "helper");
    internetStatus_->setObjectName("internetStatus");
    accessLayout->addWidget(internetStatus_);
    accessLayout->addWidget(
        makeLabel("Comparte la salida a Internet de este PC con sus consolas locales. "
                  "El PC debe seguir encendido y conectado. No cambia la Ethernet ni la VPN. "
                  "No abre puertos entrantes ni garantiza un tipo de NAT para PlayStation Network.",
                  "helper"));
    connect(internetToggle_, &QPushButton::clicked, this, [this] {
        post({{"op", "internet"},
              {"enabled", !view_["localAccess"].toObject()["internetRequested"].toBool()}});
    });
    flow->addWidget(accessCard);
    auto footer = new QHBoxLayout;
    auto lanOnly = makeLabel("La sala LAN y la salida opcional a Internet funcionan por separado.", "helper");
    footer->addWidget(lanOnly, 1);
    quickStats_ = makeLabel({}, "helper");
    quickStats_->setWordWrap(false);
    footer->addWidget(quickStats_);
    flow->addLayout(footer);
    flow->addStretch();
    playScroll_->setWidget(play);
    pages_->addWidget(playScroll_);

    auto diagPage = new QWidget;
    auto d = new QVBoxLayout(diagPage);
    d->setContentsMargins(28, 28, 28, 24);
    d->setSpacing(16);
    d->addWidget(makeLabel("Diagnóstico", "pageTitle"));
    d->addWidget(
        makeLabel("Comprueba por separado la VPN, la captura Ethernet y el tráfico observado.", "muted"));
    counters_ = makeLabel({}, "surface");
    d->addWidget(counters_);
    diagnostics_ = new QPlainTextEdit;
    diagnostics_->setReadOnly(true);
    logs_ = new QPlainTextEdit;
    logs_->setReadOnly(true);
    logs_->setMaximumBlockCount(1000);
    auto splitter = new QSplitter(Qt::Vertical);
    auto diagnosticSection = [&](const QString &title, QWidget *content) {
        auto panel = new QWidget;
        auto layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(makeLabel(title, "cardTitle"));
        layout->addWidget(content, 1);
        splitter->addWidget(panel);
    };
    diagnosticSection("Estado de la conexión", diagnostics_);
    diagnosticSection("Actividad reciente", logs_);
    d->addWidget(splitter, 1);
    auto da = new QHBoxLayout;
    auto exportButton = button("Exportar diagnóstico y registros…", da);
    auto openData = button("Abrir carpeta de datos", da);
    da->addStretch();
    d->addLayout(da);
    pages_->addWidget(diagPage);
    auto reservationsPage = new QWidget;
    auto reservationsLayout = new QVBoxLayout(reservationsPage);
    reservationsLayout->setContentsMargins(28, 28, 28, 24);
    reservationsLayout->setSpacing(16);
    reservationsLayout->addWidget(makeLabel("Reservas de la sala", "pageTitle"));
    reservationsLayout->addWidget(makeLabel("Las consolas activas se gestionan desde su PC. Aquí el "
                                            "anfitrión puede liberar reservas desconectadas.",
                                            "muted"));
    reservationPanels_ = new QStackedWidget;
    reservationsEmpty_ = makeLabel("Aquí verás las direcciones IP de las consolas de tu sala.", "emptyTitle");
    reservationsEmpty_->setAlignment(Qt::AlignCenter);
    reservationPanels_->addWidget(reservationsEmpty_);
    reservations_ = table({"Consola", "IP", "MAC", "Identificador del PC", "Estado"});
    reservations_->setShowGrid(false);
    reservations_->verticalHeader()->setDefaultSectionSize(46);
    reservationPanels_->addWidget(reservations_);
    reservationsLayout->addWidget(reservationPanels_, 1);
    auto release = new QPushButton("Liberar reserva desconectada");
    reservationsLayout->addWidget(release, 0, Qt::AlignLeft);
    pages_->addWidget(reservationsPage);
    connect(joinFound_, &QPushButton::clicked, this, [this] {
        auto selected = offers_->selectionModel()->selectedRows();
        if (!selected.isEmpty()) {
            direct_->setText(offers_->item(selected.first().row(), 1)->text());
            joinDirect();
        }
    });
    connect(offers_, &QTableWidget::itemSelectionChanged, this, [this] {
        joinFound_->setEnabled(!offers_->selectionModel()->selectedRows().isEmpty() &&
                               view_["mode"].toString() == "offline");
    });
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
    connect(search, &QPushButton::clicked, this, [this] {
        discoverySearching_ = discoverySearched_ = true;
        const int generation = ++searchGeneration_;
        updateGuidance();
        post(command("search"));
        QTimer::singleShot(2500, this, [this, generation] {
            if (generation != searchGeneration_)
                return;
            discoverySearching_ = false;
            updateGuidance();
        });
    });
    connect(offers_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        auto item = offers_->item(row, 1);
        if (item) {
            direct_->setText(item->text());
            joinDirect();
        }
    });
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok;
        auto name =
            QInputDialog::getText(this, text("Añadir consola"),
                                  text("¿Cómo quieres llamar a esta consola? (Por ejemplo, PS5 del salón)"),
                                  QLineEdit::Normal, {}, &ok);
        if (ok && !name.trimmed().isEmpty()) {
            auto c = command("add");
            c["name"] = name;
            c["adapter"] = ethernet_->currentData().toString();
            post(c);
        }
    });
    connect(remove, &QAction::triggered, this, [this] {
        if (selectedConsole_.isEmpty() || selectedOwner_ != view_["owner"].toString())
            return;
        auto c = command("remove");
        c["key"] = selectedConsole_;
        post(c);
    });
    connect(apply, &QAction::triggered, this, [this] {
        if (selectedConsole_.isEmpty() || selectedOwner_ != view_["owner"].toString())
            return;
        auto c = command("adapter");
        c["key"] = selectedConsole_;
        c["adapter"] = ethernet_->currentData().toString();
        post(c);
    });
    connect(reset, &QAction::triggered, this, [this] {
        if (selectedConsole_.isEmpty() || selectedOwner_ != view_["owner"].toString())
            return;
        auto c = command("resetMac");
        c["key"] = selectedConsole_;
        post(c);
    });
    connect(consoles_, &QTableWidget::itemSelectionChanged, this, &Window::selectionChanged);
    connect(copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(configuration());
        copiedConsole_ = selectedConsole_;
        copiedOwner_ = selectedOwner_;
        const int generation = ++copyGeneration_;
        updateConsoleDetails();
        QTimer::singleShot(2200, this, [this, generation] {
            if (copyGeneration_ != generation)
                return;
            copiedConsole_.clear();
            copiedOwner_.clear();
            updateConsoleDetails();
        });
    });
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
        dialog.setWindowTitle(text("Manual de ConsoleLAN 1.3.0"));
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
                                     text("Esta consola sigue conectada. Para quitarla, usa ConsoleLAN en el "
                                          "PC al que pertenece."));
            return;
        }
        if (QMessageBox::question(
                this, text("Liberar reserva"),
                text("Esta dirección quedará libre. Si la consola vuelve a conectarse, quizá tengas que "
                     "configurar su IP de nuevo. ¿Quieres liberar la dirección?")) != QMessageBox::Yes)
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
    for (auto label : {status_, roomInfo_, counters_, ipInfo_, errors_, stateBadge_, nextStep_, consoleMeta_,
                       consoleDetailTitle_, quickStats_, reservationsEmpty_, discoveryHint_, consoleHelp_,
                       accessStatus_, internetStatus_, gatewayInfo_, dnsInfo_})
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
void Window::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!uiReady_)
        return;
    const bool wide = width() >= 1180;
    if (wide == wideLayout_)
        return;
    wideLayout_ = wide;
    connectionGrid_->removeWidget(networkCard_);
    connectionGrid_->removeWidget(roomCard_);
    connectionGrid_->setColumnStretch(1, wide ? 1 : 0);
    if (wide) {
        connectionGrid_->addWidget(networkCard_, 0, 0);
        connectionGrid_->addWidget(roomCard_, 0, 1);
    } else {
        connectionGrid_->addWidget(networkCard_, 0, 0, 1, 2);
        connectionGrid_->addWidget(roomCard_, 1, 0, 1, 2);
    }
}
void Window::updateConsoleDetails() {
    bool found = false, assigned = false;
    Console selected;
    for (auto value : view_["consoles"].toArray()) {
        const auto c = consoleFromJson(value.toObject());
        if (c.key == selectedConsole_ && c.owner == selectedOwner_) {
            selected = c;
            found = true;
            assigned = c.ip != 0;
            break;
        }
    }
    if (!found)
        for (auto value : view_["remembered"].toArray()) {
            auto c = consoleFromJson(value.toObject());
            if (c.key == selectedConsole_ && c.owner == selectedOwner_) {
                c.ip = 0;
                selected = c;
                found = true;
                break;
            }
        }
    consoleDetails_->setVisible(found);
    copy_->setEnabled(assigned);
    const bool local = found && selected.owner == view_["owner"].toString();
    manage_->setEnabled(local);
    for (auto action : {applyEthernet_, relearnMac_, forgetConsole_})
        action->setEnabled(local);
    if (!found)
        return;
    consoleDetailTitle_->setText(selected.name);
    ipInfo_->setText(assigned ? ipText(selected.ip) : text("Pendiente"));
    const bool internet = local && view_["localAccess"].toObject()["internetActive"].toBool();
    const auto gateway = ipText((selected.ip & 0xffffff00) | 1);
    gatewayInfo_->setText(internet ? gateway : text("Vacía o 0.0.0.0"));
    dnsInfo_->setText(internet ? gateway : text("Sin configurar"));
    const bool copied = assigned && copiedConsole_ == selectedConsole_ && copiedOwner_ == selectedOwner_;
    copy_->setText(text(copied ? "Copiado" : "Copiar configuración"));
    copy_->setToolTip(text(copied ? "Configuración copiada al portapapeles."
                                  : "Copia estos datos para consultarlos al configurar tu consola."));
    consoleHelp_->setText(
        assigned
            ? internet
                  ? text("En tu consola, elige conexión por cable y configuración manual. "
                         "Usa estos valores de puerta de enlace y DNS para salir a Internet por este PC.")
                  : text("En tu consola, elige conexión por cable y configuración manual. Si "
                         "exige otros valores de puerta de enlace o DNS, consulta la ayuda.")
            : text(view_["mode"].toString() == "offline" ? "Crea una sala o únete a una para recibir una IP."
                                                         : "Esperando una IP para esta consola…"));
    QString adapter = selected.adapter;
    for (const auto &a : adapters_)
        if (a.id == adapter) {
            adapter = a.name;
            break;
        }
    consoleMeta_->setText(text("MAC: %1   ·   Ethernet: %2   ·   %3")
                              .arg(selected.mac.isEmpty() ? text("Por detectar") : selected.mac,
                                   adapter.isEmpty() ? text("Detección automática") : adapter,
                                   text(local ? "Este PC" : "Remota")));
}
void Window::updateGuidance() {
    const auto access = view_["localAccess"].toObject();
    const bool accessRequested = access["requested"].toBool();
    QString eligibility;
    const auto plan = accessPlan(view_, &eligibility);
    accessToggle_->setEnabled(accessRequested || plan.has_value());
    accessToggle_->setText(text(accessRequested ? "Desactivar acceso local" : "Activar acceso local"));
    const auto accessState = access["state"].toString();
    const bool internetRequested = access["internetRequested"].toBool();
    internetToggle_->setEnabled(internetRequested ||
                                (accessState == "active" && access["internetCapable"].toBool()));
    internetToggle_->setText(text(internetRequested ? "Desactivar Internet" : "Activar Internet"));
    internetStatus_->setText(
        access["internetActive"].toBool() ? text("Puerta de enlace y DNS activos: %1. Configura ambos en tu "
                                                 "consola; la IP y la máscara no cambian.")
                                                .arg(access["pcIp"].toString())
        : !access["internetError"].toString().isEmpty() ? message(access["internetError"].toString())
        : internetRequested                             ? text("Preparando la salida a Internet…")
        : accessState == "active"
            ? text("Desactivado. Puedes activarlo y copiar los valores de puerta de enlace y DNS.")
            : text("Desactivado. Activa primero el acceso local; después podrás compartir Internet."));
    accessStatus_->setText(
        accessState == "active" ? text("Acceso TCP/IP activo · IP de este PC: %1. Usa la IP de la consola "
                                       "y el puerto de su servicio.")
                                      .arg(access["pcIp"].toString())
        : accessState == "starting"
            ? text("Preparando el adaptador propio. Acepta el permiso de Windows para continuar…")
        : accessState == "error" ? message(access["error"].toString())
        : plan                   ? text("Desactivado. Puedes activarlo mientras sigues jugando en LAN.")
                                 : message(eligibility));
    const auto mode = view_["mode"].toString();
    const bool connected = mode == "host" || mode == "client";
    const QString tone = connected ? "connected" : mode == "joining" ? "pending" : "neutral";
    if (stateBadge_->property("tone").toString() != tone) {
        stateBadge_->setProperty("tone", tone);
        stateBadge_->style()->unpolish(stateBadge_);
        stateBadge_->style()->polish(stateBadge_);
    }
    stateBadge_->setText(text(connected ? "En sala" : mode == "joining" ? "Conectando…" : "Sin sala"));
    roomPanels_->setCurrentIndex(connected ? 1 : 0);
    consolePanels_->setCurrentIndex(consoles_->rowCount() ? 1 : 0);
    consoles_->setFixedHeight(std::clamp(38 + consoles_->rowCount() * 46, 86, 220));
    add_->setVisible(consoles_->rowCount() > 0);
    discoveryPanels_->setCurrentIndex(offers_->rowCount() ? 1 : 0);
    reservationPanels_->setCurrentIndex(reservations_->rowCount() ? 1 : 0);
    reservationsEmpty_->setText(
        text(mode == "host"
                 ? "Todavía no hay direcciones reservadas. Añade una consola desde Jugar en LAN."
                 : "Las direcciones IP las gestiona quien crea la sala. Puedes verlas aquí al conectarte."));
    discoveryHint_->setText(text(discoverySearching_ ? "Buscando salas en tu VPN…"
                                 : discoverySearched_
                                     ? "No encontramos salas. Comprueba que tus amigos estén en la misma VPN "
                                       "o introduce la IP del anfitrión."
                                     : "También puedes buscar salas si tu VPN lo permite."));
    roomCard_->layout()->invalidate();
    networkCard_->layout()->invalidate();
    connectionGrid_->invalidate();
    playScroll_->widget()->layout()->invalidate();
    playScroll_->widget()->updateGeometry();
    joinFound_->setEnabled(mode == "offline" && !offers_->selectionModel()->selectedRows().isEmpty());
    const auto sourceStatus = view_["status"].toString();
    const bool routine =
        sourceStatus.isEmpty() || sourceStatus == "Sin sala" || sourceStatus.startsWith("Anfitrión activo") ||
        sourceStatus.startsWith("Conectado al maestro") || sourceStatus.startsWith("Negociando con");
    status_->setVisible(!routine);
    errors_->setVisible(!view_["error"].toString().isEmpty());
    if (mode == "joining")
        nextStep_->setText(text("Contactando con el anfitrión. Espera un momento…"));
    else if (!connected)
        nextStep_->setText(text("Selecciona tu VPN y crea una sala, o únete a la de tus amigos."));
    else if (view_["remembered"].toArray().isEmpty())
        nextStep_->setText(text("Ya puedes añadir tu consola para ver cómo configurarla."));
    else {
        const auto consoles = view_["consoles"].toArray();
        const bool assigned = std::any_of(consoles.cbegin(), consoles.cend(), [](const QJsonValue &value) {
            return !value.toObject()["ip"].toString().isEmpty();
        });
        nextStep_->setText(text(assigned ? "Introduce estos datos en tu consola y abre el modo LAN del juego."
                                         : "Espera a que aparezca la IP de tu consola para configurarla."));
    }
    quickStats_->setVisible(connected);
    const int rtt = view_["rtt"].toInt(-1);
    quickStats_->setText(
        mode == "host"
            ? text("Este PC es el anfitrión")
            : text("Latencia al anfitrión: %1").arg(rtt < 0 ? QString("—") : QString::number(rtt) + " ms"));
    quickStats_->setToolTip(text("Latencia al anfitrión; no es el ping entre consolas."));
    updateConsoleDetails();
}
void Window::retranslate() {
    if (!uiReady_)
        return;
    setWindowTitle(text("ConsoleLAN 1.3.0 — LAN entre casas"));
    translateUiText(this);
    vpn_->setAccessibleName(text("VPN de este PC"));
    ethernet_->setAccessibleName(text("Ethernet de las consolas"));
    direct_->setAccessibleName(text("IP VPN del anfitrión"));
    roomName_->setAccessibleName(text("Nombre de la sala"));
    ipInfo_->setAccessibleName(text("Dirección IP"));
    logs_->setPlaceholderText(text("Aquí aparecerán los mensajes de conexión y la actividad de ConsoleLAN."));
    applyEthernet_->setToolTip(text("Usa para esta consola el adaptador Ethernet elegido en Tu conexión."));
    relearnMac_->setToolTip(
        text("Olvida la dirección MAC aprendida. Se detectará de nuevo cuando la consola envíe datos."));
    forgetConsole_->setToolTip(text("Quita esta consola de la lista de este PC."));
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
    ethernet_->addItem(text("Detectar automáticamente"), QString());
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
            text("Escribe la IP de la VPN de quien creó la sala, por ejemplo 10.147.0.8. Si te ha indicado "
                 "un puerto, añádelo así: 10.147.0.8:32190."));
        return;
    }
    auto c = command("join");
    c["ip"] = parts[0];
    c["port"] = port;
    post(c);
}
void Window::selectionChanged() {
    auto rows = consoles_->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        selectedConsole_.clear();
        selectedOwner_.clear();
        updateConsoleDetails();
        return;
    }
    auto item = consoles_->item(rows[0].row(), 0);
    selectedConsole_ = item->data(Qt::UserRole).toString();
    selectedOwner_ = item->data(Qt::UserRole + 1).toString();
    updateConsoleDetails();
}
QString Window::configuration() const {
    for (auto v : view_["consoles"].toArray()) {
        auto c = consoleFromJson(v.toObject());
        if (c.key == selectedConsole_ && c.owner == selectedOwner_) {
            if (c.owner == view_["owner"].toString() &&
                view_["localAccess"].toObject()["internetActive"].toBool())
                return text("%1\nIP: %2    Máscara: 255.255.255.0\nPuerta de enlace: %3    DNS primario: %3\n"
                            "Configura la conexión por cable manualmente. Mantén ConsoleLAN, el acceso local "
                            "e Internet activos en este PC.")
                    .arg(c.name, ipText(c.ip), ipText((c.ip & 0xffffff00) | 1));
            return text("%1\nIP: %2    Máscara: 255.255.255.0\nGateway: vacío / 0.0.0.0    DNS: sin "
                        "configurar (no se proporciona servicio DNS)\nConfigure la red por cable "
                        "manualmente. Si la consola exige gateway/DNS, consulte el manual; la prueba de "
                        "Internet no certifica el modo LAN.")
                .arg(c.name, ipText(c.ip));
        }
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
    leave_->setVisible(active);
    leave_->setText(text(mode == "joining" ? "Cancelar conexión" : "Salir de la sala"));
    roomName_->setEnabled(!active);
    subnet_->setEnabled(!active);
    status_->setText(message(view_["status"].toString()));
    errors_->setText(view_["error"].toString().isEmpty()
                         ? QString()
                         : text("Último error: %1").arg(message(view_["error"].toString())));
    QStringList people;
    for (auto v : view_["members"].toArray())
        people << v.toObject()["name"].toString();
    roomInfo_->setText(text("Sala: %1\nAnfitrión: %2\nPC conectados (%3): %4")
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
            cell(reservations_, row, 4, text(o["active"].toBool() ? "PC conectado" : "PC desconectado"));
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
        cell(consoles_, row, 5, consoleStatus(c.status));
        consoles_->item(row, 5)->setToolTip(message(c.status));
        if (c.status.contains("CONFLICTO"))
            for (int j = 0; j < 6; ++j)
                consoles_->item(row, j)->setForeground(presentation().conflictColor());
        if (c.key == selectedConsole_ && c.owner == selectedOwner_)
            consoles_->selectRow(row);
    }
    if (consoles_->selectionModel()->selectedRows().isEmpty() && consoles_->rowCount()) {
        int first = 0;
        for (int row = 0; row < consoles_->rowCount(); ++row)
            if (consoles_->item(row, 0)->data(Qt::UserRole + 1).toString() == own) {
                first = row;
                break;
            }
        consoles_->selectRow(first);
        selectedConsole_ = consoles_->item(first, 0)->data(Qt::UserRole).toString();
        selectedOwner_ = consoles_->item(first, 0)->data(Qt::UserRole + 1).toString();
    }
    updateConsoleDetails();
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
        text("ConsoleLAN 1.3.0 · diagnóstico\n\nVPN / socket: %1\nNpcap: %2\nEstado: %3\nTráfico Ethernet "
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
    updateGuidance();
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
