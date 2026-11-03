#include "gemini.h"
#include "ai.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace gemini {

bool process(const std::string& title) {
    std::string filename = title + ".txt";
    std::string temp_path = "temp/" + filename;
    std::string root_path = filename;

    // Check if the blog text file exists in temp/ or root
    bool file_exists = false;
    if (fs::exists(temp_path)) {
        file_exists = true;
    } else if (fs::exists(root_path)) {
        file_exists = true;
        // Keep temp directory in sync
        fs::create_directories("temp");
        fs::copy_file(root_path, temp_path, fs::copy_options::overwrite_existing);
    }

    if (!file_exists) {
        std::cerr << "[gemini.cpp] Warning: " << filename << " not found in /temp or project root. Proceeding with filename reference." << std::endl;
    } else {
        std::cout << "[gemini.cpp] Parsing " << filename << " through gemini.cpp..." << std::endl;
    }

    // Call AI module to send request to Gemini API and save image
    return ai::generate_image_for_blog(title, filename);
}

} // namespace gemini
