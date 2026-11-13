#include <iostream>
#include <string>
#include <sstream>
#include "writer.h"
#include "util.h"

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
              << "list : lists all blogs written up until now\n\n"
              << "write <title> : writes a file with filename : <title>.txt \n\n"
              << "exit : exit CLI \n\n"
              << "prep <title> : sends <title>.txt to Gemini and saves an image to assets/\n\n"
              << "make <title> : renders <title>.txt into blogs/<title>.html\n\n"
              << "mark <title> : parses <title>.txt through Gemini and saves <title>.md to website\n\n"
              << "publish <title> : renders and publishes <title> to personal website and blog index\n\n"
              << "upload <title> : pushes changes to GitHub with commit message 'added <title>'\n\n"
              << "titles may contain letters, digits, spaces, '-' and '_'\n"
              << "}\n" << std::endl;
}

bool execute_command(const std::string& input) {
    std::istringstream iss(input);
    std::string cmd;
    if (!(iss >> cmd)) {
        return true; // empty line
    }

    if (cmd == "exit" || cmd == "quit") {
        std::cout << "Exiting CLI." << std::endl;
        return false;
    }
    if (cmd == "help") {
        print_help();
        return true;
    }
    if (cmd == "list") {
        writer::handle_list();
        return true;
    }

    if (cmd == "write" || cmd == "prep" || cmd == "make" || cmd == "mark" || cmd == "publish" || cmd == "upload") {
        // The title is the rest of the line, so titles can contain spaces.
        std::string title;
        std::getline(iss, title);
        title = util::trim(title);

        if (title.empty()) {
            std::cout << "Usage: " << cmd << " <title>" << std::endl;
        } else if (!util::is_valid_title(title)) {
            std::cout << "Invalid title. Use only letters, digits, spaces, '-' and '_' (max 100 chars)." << std::endl;
        } else if (cmd == "write") {
            writer::handle_write(title);
        } else if (cmd == "prep") {
            writer::handle_prep(title);
        } else if (cmd == "make") {
            writer::handle_make(title);
        } else if (cmd == "mark") {
            writer::handle_mark(title);
        } else if (cmd == "publish") {
            writer::handle_publish(title);
        } else if (cmd == "upload") {
            writer::handle_upload(title);
        }
        return true;
    }

    std::cout << "Unknown command: '" << cmd << "'. Type 'help' for available commands." << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << DISPLAY_TEXT << std::endl;

    // Non-interactive mode: ./blogwriter write my-post
    if (argc > 1) {
        std::string command_line;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) command_line += " ";
            command_line += argv[i];
        }
        execute_command(command_line);
        return 0;
    }

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
