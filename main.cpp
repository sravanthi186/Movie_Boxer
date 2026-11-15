#include "App.h"
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

int main() {
    // Create the SQLite parent directory when running on a fresh Codespace.
    mkdir("data", 0755);
    try {
        App app("data/cinema.db");
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Application stopped: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
