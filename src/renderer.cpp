#include "renderer.h"
#include "ai.h"
#include "util.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

std::string read_file_to_string(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void copy_template_backgrounds() {
    for (const char* name : {"wood.jpg", "bg.jpg"}) {
        std::string src = std::string("template/") + name;
        std::string dst = std::string("blogs/") + name;
        if (fs::exists(src) && !fs::exists(dst)) {
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        }
    }
}

std::string prepare_title_image(const std::string& title) {
    for (const char* ext : {".png", ".jpg", ".jpeg"}) {
        std::string asset = "assets/" + title + ext;
        if (fs::exists(asset)) {
            fs::copy_file(asset, "blogs/" + title + ext, fs::copy_options::overwrite_existing);
            return title + ext;
        }
    }
    std::cerr << "[renderer.cpp] Warning: no image in assets/ for '" << title
              << "' (run 'prep " << title << "'); using a placeholder." << std::endl;
    std::string name = title + ".png";
    ai::write_placeholder_png("blogs/" + name);
    return name;
}

const char* DEFAULT_TEMPLATE = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title><topic></title>
    <style>
        body { margin: 0; padding: 20px; font-family: sans-serif; background: #ddd; }
        .image-container { position: relative; display: inline-block; }
        .image-container img { display: block; width: 100%; height: auto; }
        .overlay-text { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%);
                        color: white; font-size: 24px; text-align: center; }
        pre { color: #2C2C2C; font-family: inherit; white-space: pre-wrap; word-wrap: break-word;
              font-size: 18px; line-height: 1.6; }
    </style>
</head>
<body>

<div class="image-container">
  <img src="<title>.jpg" alt="<title>">
  <div class="overlay-text"> <title> </div>
</div>

<pre>
{ content from <title>.txt }
</pre>

</body>
</html>
)";

}

namespace renderer {

bool render(const std::string& title) {
    std::cout << "[renderer.cpp] Rendering '" << title << "'" << std::endl;

    std::string txt_filename = title + ".txt";
    std::string temp_txt_path = "temp/" + txt_filename;
    std::string blog_content;
    if (fs::exists(temp_txt_path)) {
        blog_content = read_file_to_string(temp_txt_path);
    } else if (fs::exists(txt_filename)) {
        blog_content = read_file_to_string(txt_filename);
        fs::create_directories("temp");
        fs::copy_file(txt_filename, temp_txt_path, fs::copy_options::overwrite_existing);
    } else {
        std::cerr << "[renderer.cpp] Error: " << txt_filename
                  << " not found. Run 'write " << title << "' first." << std::endl;
        return false;
    }

    std::string html = read_file_to_string("template/template.html");
    if (html.empty()) {
        std::cerr << "[renderer.cpp] Warning: template/template.html missing, using built-in template"
                  << std::endl;
        html = DEFAULT_TEMPLATE;
    }

    fs::create_directories("blogs");
    copy_template_backgrounds();
    std::string image_name = prepare_title_image(title);

    std::string safe_title = util::html_escape(title);
    replace_all(html, "<img src=\"<title>.jpg\" alt=\"<title>\">",
                "<img src=\"" + util::url_encode(image_name) + "\" alt=\"" + safe_title + "\">");
    replace_all(html, "<div class=\"overlay-text\"> <title> </div>",
                "<div class=\"overlay-text\"> " + safe_title + " </div>");
    replace_all(html, "<topic>", safe_title);
    replace_all(html, "{ content from <title>.txt }", util::html_escape(blog_content));

    std::string output_path = "blogs/" + title + ".html";
    std::ofstream out_file(output_path);
    if (!out_file.is_open()) {
        std::cerr << "[renderer.cpp] Error: failed to create " << output_path << std::endl;
        return false;
    }
    out_file << html;
    out_file.close();

    std::cout << "[renderer.cpp] Created " << output_path << std::endl;
    return true;
}

}
