#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "writer.h"

const char* DISPLAY_TEXT = R"(
 _  _______  _____  _____ _    _ _   _          _   _  _____ 
 | |/ /  __ \|_   _|/ ____| |  | | \ | |   /\   | \ | |/ ____|
 | ' /| |__) | | | | (___ | |__| |  \| |  /  \  |  \| | |  __ 
 |  < |  _  /  | |  \___ \|  __  | . ` | / /\ \ | . ` | | |_ |
 | . \| | \ \ _| |_ ____) | |  | | |\  |/ ____ \| |\  | |__| |
 |_|\_\_|  \_\_____|_____/|_|  |_|_| \_/_/    \_\_| \_|\_____|
                                                               

)";

void print_help() {
    std::cout << "\navailable commands {\n\n"
              << "help \n\n"
              << "write <title> : writes a file with filename : <title>.txt \n\n"
              << "exit : exit CLI \n\n"
              << "prep <title> : parses file with filename : <title>.txt through gemini.cpp\n\n"
              << "make <title> : parses file with filename : <title.txt> through renderer.cpp\n"
              << "}\n" << std::endl;
}

bool execute_command(const std::string& input) {
    std::istringstream iss(input);
    std::string cmd;
    if (!(iss >> cmd)) {
        return true; // Empty line, continue
    }

    if (cmd == "exit" || cmd == "quit") {
        std::cout << "Exiting CLI." << std::endl;
        return false;
    } else if (cmd == "help") {
        print_help();
    } else if (cmd == "write") {
        std::string title;
        if (iss >> title) {
            writer::handle_write(title);
        } else {
            std::cout << "Usage: write <title>" << std::endl;
        }
    } else if (cmd == "prep") {
        std::string title;
        if (iss >> title) {
            writer::handle_prep(title);
        } else {
            std::cout << "Usage: prep <title>" << std::endl;
        }
    } else if (cmd == "make") {
        std::string title;
        if (iss >> title) {
            writer::handle_make(title);
        } else {
            std::cout << "Usage: make <title>" << std::endl;
        }
    } else {
        std::cout << "Unknown command: '" << cmd << "'. Type 'help' for available commands." << std::endl;
    }

    return true;
}

int main(int argc, char* argv[]) {
    // CLI tool display_text banner
    std::cout << DISPLAY_TEXT << std::endl;

    // Support non-interactive command line execution
    if (argc > 1) {
        std::string command_line;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) command_line += " ";
            command_line += argv[i];
        }
        execute_command(command_line);
        return 0;
    }

    // Interactive CLI loop
    print_help();
    std::string line;
    while (true) {
        std::cout << "blogWRITER> ";
        if (!std::getline(std::cin, line)) {
            break; // EOF
        }
        if (!execute_command(line)) {
            break;
        }
    }

    return 0;
}
