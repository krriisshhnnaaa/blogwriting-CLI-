#include "upload.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace upload {

bool process(const std::string& title) {
    std::string script = "./upload.sh";
    if (!fs::exists(script)) {
        if (fs::exists("src/upload.sh")) {
            script = "src/upload.sh";
        } else if (fs::exists("./src/upload.sh")) {
            script = "./src/upload.sh";
        }
    }

    std::cout << "[upload.cpp] Executing upload for '" << title << "' via " << script << "..." << std::endl;

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[upload.cpp] Error: fork failed." << std::endl;
        return false;
    }
    if (pid == 0) {
        execlp("/bin/bash", "bash", script.c_str(), title.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::cerr << "[upload.cpp] Error: waitpid failed." << std::endl;
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}

#ifdef UPLOAD_STANDALONE
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <title>\n";
        return 1;
    }
    return upload::process(argv[1]) ? 0 : 1;
}
#endif
