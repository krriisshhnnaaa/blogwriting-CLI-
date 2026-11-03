#include "writer.h"
#include "gemini.h"
#include "renderer.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace writer {

void handle_write(const std::string& title) {
    fs::create_directories("temp");
    std::string root_filename = title + ".txt";
    std::string temp_filename = "temp/" + root_filename;

    // If root file already exists and temp doesn't, sync it first
    if (fs::exists(root_filename) && !fs::exists(temp_filename)) {
        fs::copy_file(root_filename, temp_filename, fs::copy_options::overwrite_existing);
    } else if (fs::exists(temp_filename) && !fs::exists(root_filename)) {
        fs::copy_file(temp_filename, root_filename, fs::copy_options::overwrite_existing);
    }

    // exec-once : nano <title>.txt
    std::cout << "[writer.cpp] Executing: nano " << root_filename << std::endl;
    std::string cmd = "nano \"" + root_filename + "\"";
    int ret = std::system(cmd.c_str());
    (void)ret;

    // Keep temp/<title>.txt in sync with <title>.txt
    if (fs::exists(root_filename)) {
        fs::copy_file(root_filename, temp_filename, fs::copy_options::overwrite_existing);
        std::cout << "[writer.cpp] Saved " << root_filename << " and synced to " << temp_filename << std::endl;
    } else if (fs::exists(temp_filename)) {
        fs::copy_file(temp_filename, root_filename, fs::copy_options::overwrite_existing);
        std::cout << "[writer.cpp] Synced " << temp_filename << " to " << root_filename << std::endl;
    } else {
        // Create an initial empty file if nano exited without writing
        std::ofstream create_file(temp_filename);
        create_file.close();
        fs::copy_file(temp_filename, root_filename, fs::copy_options::overwrite_existing);
        std::cout << "[writer.cpp] Initialized empty file: " << root_filename << std::endl;
    }
}

void handle_prep(const std::string& title) {
    // parse <title>.txt through gemini.cpp
    std::cout << "[writer.cpp] Parsing " << title << ".txt through gemini.cpp..." << std::endl;
    gemini::process(title);
}

void handle_make(const std::string& title) {
    // parse <title>.txt through renderer.cpp
    std::cout << "[writer.cpp] Parsing " << title << ".txt through renderer.cpp..." << std::endl;
    renderer::render(title);
}

} // namespace writer
