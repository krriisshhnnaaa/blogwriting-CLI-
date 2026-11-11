#include "writer.h"
#include "gemini.h"
#include "renderer.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

// Launches $EDITOR (default: nano) on a file WITHOUT going through a shell, so
// file names can never be interpreted as commands.
bool run_editor(const std::string& file) {
    const char* env = std::getenv("EDITOR");
    std::string editor = (env && *env) ? env : "nano";

    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execlp(editor.c_str(), editor.c_str(), file.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) != 127;
}

} // namespace

namespace writer {

void handle_write(const std::string& title) {
    fs::create_directories("temp");
    std::string root_filename = title + ".txt";
    std::string temp_filename = "temp/" + root_filename;

    // Make sure both copies exist if either does.
    if (fs::exists(root_filename) && !fs::exists(temp_filename)) {
        fs::copy_file(root_filename, temp_filename, fs::copy_options::overwrite_existing);
    } else if (fs::exists(temp_filename) && !fs::exists(root_filename)) {
        fs::copy_file(temp_filename, root_filename, fs::copy_options::overwrite_existing);
    }

    if (!run_editor(root_filename)) {
        std::cerr << "[writer.cpp] Could not launch the editor (set $EDITOR or install nano)." << std::endl;
        return;
    }

    if (fs::exists(root_filename)) {
        fs::copy_file(root_filename, temp_filename, fs::copy_options::overwrite_existing);
        std::cout << "[writer.cpp] Saved " << root_filename << " (synced to " << temp_filename << ")" << std::endl;
    } else {
        std::cout << "[writer.cpp] No file was saved." << std::endl;
    }
}

void handle_prep(const std::string& title) {
    if (gemini::process(title)) {
        std::cout << "[writer.cpp] prep done: image generated for '" << title << "'." << std::endl;
    } else {
        std::cout << "[writer.cpp] prep finished WITHOUT a generated image (see messages above)." << std::endl;
    }
}

void handle_make(const std::string& title) {
    renderer::render(title);
}

} // namespace writer
