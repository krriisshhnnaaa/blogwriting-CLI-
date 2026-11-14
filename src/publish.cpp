#include "publish.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace publish {

bool process(const std::string& title) {
    std::string script = "./publish.sh";
    if (!fs::exists(script)) {
        if (fs::exists("src/publish.sh")) {
            script = "src/publish.sh";
        } else if (fs::exists("./src/publish.sh")) {
            script = "./src/publish.sh";
        }
    }

    std::cout << "[publish.cpp] Executing publish script (" << script << " " << title << ")..." << std::endl;

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[publish.cpp] Error: fork failed for " << script << std::endl;
        return false;
    }
    if (pid == 0) {
        execlp("/bin/bash", "bash", script.c_str(), title.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::cerr << "[publish.cpp] Error: waitpid failed for " << script << std::endl;
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}

#ifdef PUBLISH_STANDALONE
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <title>\n";
        return 1;
    }
    return publish::process(argv[1]) ? 0 : 1;
}
#endif
