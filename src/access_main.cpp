#include "access_native.hpp"
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 5 || args[1] != "--pipe" || args[3] != "--parent" ||
        !args[2].startsWith("\\\\.\\pipe\\ConsoleLAN-Access-"))
        return 1;
    bool ok;
    auto parent = args[4].toUInt(&ok);
    if (!ok || !parent)
        return 1;
    return cl::accessHelperMain(args[2], parent);
}
