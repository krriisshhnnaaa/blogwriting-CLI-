#include "writer.h"
#include "gemini.h"
#include "renderer.h"
#include "mark.h"
#include "publish.h"
#include "upload.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <map>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

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

}

namespace writer {

void handle_write(const std::string& title) {
    fs::create_directories("temp");
    std::string root_filename = title + ".txt";
    std::string temp_filename = "temp/" + root_filename;

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

void handle_mark(const std::string& title) {
    if (mark::process(title)) {
        std::cout << "[writer.cpp] mark done: markdown created and saved for '" << title << "'." << std::endl;
    } else {
        std::cerr << "[writer.cpp] mark failed for '" << title << "'." << std::endl;
    }
}

void handle_publish(const std::string& title) {
    if (publish::process(title)) {
        std::cout << "[writer.cpp] publish done: published '" << title << "' successfully." << std::endl;
    } else {
        std::cerr << "[writer.cpp] publish failed for '" << title << "'." << std::endl;
    }
}

void handle_upload(const std::string& title) {
    if (upload::process(title)) {
        std::cout << "[writer.cpp] upload done: pushed '" << title << "' to GitHub." << std::endl;
    } else {
        std::cerr << "[writer.cpp] upload failed for '" << title << "'." << std::endl;
    }
}

struct BlogStatus {
    bool has_txt = false;
    bool has_img = false;
    bool has_html = false;
    bool has_published = false;
};

void handle_list() {
    std::map<std::string, BlogStatus> blogs;

    if (fs::exists("temp") && fs::is_directory("temp")) {
        for (const auto& entry : fs::directory_iterator("temp")) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                blogs[entry.path().stem().string()].has_txt = true;
            }
        }
    }

    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            std::string title = entry.path().stem().string();
            if (title == "CMakeLists" || title == "CMakeLists.txt") continue;
            blogs[title].has_txt = true;
        }
    }

    if (fs::exists("blogs") && fs::is_directory("blogs")) {
        for (const auto& entry : fs::directory_iterator("blogs")) {
            if (entry.is_regular_file() && entry.path().extension() == ".html") {
                blogs[entry.path().stem().string()].has_html = true;
            }
        }
    }

    std::string site_blog_dir = "/home/krishnang/krishnang.dev-personal-website-project/blog";
    if (fs::exists(site_blog_dir) && fs::is_directory(site_blog_dir)) {
        for (const auto& entry : fs::directory_iterator(site_blog_dir)) {
            if (entry.is_directory()) {
                blogs[entry.path().filename().string()].has_published = true;
            }
        }
    }

    for (auto& [title, info] : blogs) {
        if (fs::exists("assets/" + title + ".jpg") || fs::exists("assets/" + title + ".png") ||
            fs::exists("blogs/" + title + ".jpg") || fs::exists("blogs/" + title + ".png") ||
            fs::exists(site_blog_dir + "/" + title + "/" + title + ".jpg")) {
            info.has_img = true;
        }
        if (fs::exists("blogs/" + title + ".html") ||
            fs::exists(site_blog_dir + "/" + title + "/" + title + ".html")) {
            info.has_html = true;
        }
        if (fs::exists(site_blog_dir + "/" + title)) {
            info.has_published = true;
        }
        if (fs::exists("temp/" + title + ".txt") || fs::exists(title + ".txt")) {
            info.has_txt = true;
        }
    }

    if (blogs.empty()) {
        std::cout << "No written blogs found." << std::endl;
        return;
    }

    std::cout << "\nBlogs written up until now (" << blogs.size() << " total):\n" << std::endl;
    int idx = 1;
    for (const auto& [title, info] : blogs) {
        std::cout << "  " << idx++ << ". " << title;
        std::vector<std::string> tags;
        if (info.has_txt) tags.push_back("draft");
        if (info.has_img) tags.push_back("image");
        if (info.has_html) tags.push_back("html");
        if (info.has_published) tags.push_back("published");
        if (!tags.empty()) {
            std::cout << " (";
            for (size_t i = 0; i < tags.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << tags[i];
            }
            std::cout << ")";
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}

}
