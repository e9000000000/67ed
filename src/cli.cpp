#include "cli.h"

#include <cstdlib>

LaunchOptions ParseOptions(const std::vector<std::wstring>& args) {
    LaunchOptions options;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::wstring& arg = args[i];
        if (arg == L"--selftest") {
            options.selfTest = true;
        } else if (arg == L"--bench-startup") {
            options.startupBench = true;
        } else if (arg == L"--bench" && i + 1 < args.size()) {
            options.bench = true;
            options.benchFile = args[++i];
        } else if (arg == L"--make-fixture" && i + 1 < args.size()) {
            options.makeFixture = true;
            options.fixturePath = args[++i];
        } else if (arg == L"--mb" && i + 1 < args.size()) {
            options.megabytes = static_cast<int>(wcstol(args[++i].c_str(), nullptr, 10));
        } else if (arg == L"--one-line") {
            options.oneLine = true;
        } else if (arg == L"--log" && i + 1 < args.size()) {
            options.logPath = args[++i];
        } else if (arg == L"--new-tab" || arg == L"--new") {
            // Compatible no-op: no-argument launches already open a new tab.
        } else if (!arg.empty() && arg[0] == L'-') {
            // Unknown option.
        } else if (!arg.empty()) {
            options.paths.push_back(arg);
        }
    }
    if (options.megabytes < 1) {
        options.megabytes = 100;
    }
    if (options.megabytes > 2048) {
        options.megabytes = 2048;
    }
    return options;
}
