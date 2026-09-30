#include <string>

#include "app.hpp"

int main(int argc, char **argv) {
    std::string asset_root;
    std::string character = "ecto";
    if (argc > 1) {
        asset_root = argv[1];
    }
    if (argc > 2) {
        character = argv[2];
    }

    de::App app;
    if (!app.init(asset_root, character)) {
        app.shutdown();
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
