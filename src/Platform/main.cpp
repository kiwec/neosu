// cross-platform entry point

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <filesystem>

#include "BaseEnvironment.h"

#include "fmt/format.h"

#include "LinuxMain.h"
#include "WindowsMain.h"

int Main::ret = 1;
EnvironmentImpl *baseEnv = nullptr;

namespace {
void setcwdexe(const char *argv0) noexcept {
    // Fix path in case user is running it from the wrong folder.
    // We only do this if MCENGINE_DATA_DIR is set to its default value, since if it's changed,
    // the packager clearly wants the executable in a different location.
    UString dataDir{MCENGINE_DATA_DIR};
    if(dataDir != "./" && dataDir != ".\\") {
        return;
    }
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::path exe_path{};
    if constexpr(Env::cfg(OS::LINUX))
        exe_path = fs::canonical("/proc/self/exe", ec);
    else if constexpr(Env::cfg(OS::WINDOWS))
        exe_path = fs::canonical(fs::path(argv0), ec);

    if(ec || !exe_path.has_parent_path()) return;

    fs::current_path(exe_path.parent_path(), ec);
}
}  // namespace

int main(int argc, char *argv[]) {
    if constexpr(!Env::cfg(OS::WASM))
        setcwdexe(argv[0]);  // set the current working directory to the executable directory, so that relative paths
                             // work as expected

    std::string lowerPackageName = PACKAGE_NAME;
    std::ranges::transform(lowerPackageName, lowerPackageName.begin(), [](char c) { return std::tolower(c); });

    // set up some common app metadata (SDL says these should be called as early as possible)
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, PACKAGE_NAME);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, PACKAGE_VERSION);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING,
                               fmt::format("com.mcengine.{}", lowerPackageName).c_str());
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "kiwec/spectator/McKay");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "MIT/GPL3");  // neosu is gpl3, mcengine is mit
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, PACKAGE_URL);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

    // parse args
    auto argMap = [&]() -> std::unordered_map<UString, std::optional<UString>> {
        // example usages:
        // args.contains("-file")
        // auto filename = args["-file"].value_or("default.txt");
        // if (args["-output"].has_value())
        // 	auto outfile = args["-output"].value();
        std::unordered_map<UString, std::optional<UString>> args;
        for(int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if(arg.starts_with('-'))
                if(i + 1 < argc && !(argv[i + 1][0] == '-')) {
                    args[UString(arg)] = argv[i + 1];
                    ++i;
                } else
                    args[UString(arg)] = std::nullopt;
            else
                args[UString(arg)] = std::nullopt;
        }
        return args;
    }();

    // simple vector representation of the whole cmdline including the program name (as the first element)
    auto cmdLine = std::vector<UString>(argv, argv + argc);

    auto ml = Main(argc, argv, cmdLine, argMap);
    return ml.ret;
}
