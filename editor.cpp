#include "editor.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace editor {

void open_nano(const std::string& title) {
    fs::create_directories("temp");
    std::string root_filename = title + ".txt";
    std::string temp_filename = "temp/" + root_filename;

    if (fs::exists(root_filename) && !fs::exists(temp_filename)) {
        fs::copy_file(root_filename, temp_filename, fs::copy_options::overwrite_existing);
    } else if (fs::exists(temp_filename) && !fs::exists(root_filename)) {
        fs::copy_file(temp_filename, root_filename, fs::copy_options::overwrite_existing);
    }

    std::cout << "[editor.cpp] Executing: nano " << root_filename << std::endl;
    std::string cmd = "nano \"" + root_filename + "\"";
    int ret = std::system(cmd.c_str());
    (void)ret;

    if (fs::exists(root_filename)) {
        fs::copy_file(root_filename, temp_filename, fs::copy_options::overwrite_existing);
    } else if (fs::exists(temp_filename)) {
        fs::copy_file(temp_filename, root_filename, fs::copy_options::overwrite_existing);
    } else {
        std::ofstream create_file(temp_filename);
        create_file.close();
        fs::copy_file(temp_filename, root_filename, fs::copy_options::overwrite_existing);
    }
}

} // namespace editor
