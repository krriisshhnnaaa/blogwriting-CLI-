#include "renderer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <cstdlib>

namespace fs = std::filesystem;

namespace {

void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string read_file_to_string(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void ensure_image_assets(const std::string& title) {
    fs::create_directories("blogs");
    fs::create_directories("assets");

    // Copy template background images to blogs/
    if (fs::exists("template/wood.jpg") && !fs::exists("blogs/wood.jpg")) {
        fs::copy_file("template/wood.jpg", "blogs/wood.jpg", fs::copy_options::overwrite_existing);
    }
    if (fs::exists("template/bg.jpg") && !fs::exists("blogs/bg.jpg")) {
        fs::copy_file("template/bg.jpg", "blogs/bg.jpg", fs::copy_options::overwrite_existing);
    }
    if (fs::exists("template/wood.jpg") && !fs::exists("blogs/bg.jpg")) {
        fs::copy_file("template/wood.jpg", "blogs/bg.jpg", fs::copy_options::overwrite_existing);
    }
    if (fs::exists("template/bg.jpg") && !fs::exists("blogs/wood.jpg")) {
        fs::copy_file("template/bg.jpg", "blogs/wood.jpg", fs::copy_options::overwrite_existing);
    }

    // Ensure title image is available in blogs/
    std::string asset_jpg = "assets/" + title + ".jpg";
    std::string asset_png = "assets/" + title + ".png";
    std::string blog_jpg = "blogs/" + title + ".jpg";

    if (fs::exists(asset_jpg)) {
        fs::copy_file(asset_jpg, blog_jpg, fs::copy_options::overwrite_existing);
    } else if (fs::exists(asset_png)) {
        // Convert asset_png to asset_jpg & blog_jpg using python PIL if available
        std::string py_cmd = "python3 -c \"from PIL import Image; img=Image.open('" + asset_png + "').convert('RGB'); img.save('" + asset_jpg + "'); img.save('" + blog_jpg + "')\" 2>/dev/null";
        int ret = std::system(py_cmd.c_str());
        if (ret != 0 || !fs::exists(blog_jpg)) {
            fs::copy_file(asset_png, blog_jpg, fs::copy_options::overwrite_existing);
        }
    } else {
        // Generate fallback rectangular image (1.5:1, 300x200)
        std::string py_cmd = "python3 -c \"from PIL import Image, ImageDraw; img = Image.new('RGB', (300, 200), color=(52, 101, 164)); draw = ImageDraw.Draw(img); draw.rectangle([10, 10, 290, 190], outline=(255, 255, 255), width=2); img.save('" + asset_jpg + "'); img.save('" + blog_jpg + "')\" 2>/dev/null";
        int ret = std::system(py_cmd.c_str());
        (void)ret;
    }
}

} // namespace

namespace renderer {

bool render(const std::string& title) {
    std::cout << "[renderer.cpp] Activated with input: " << title << " by main.cpp" << std::endl;

    // 1. Pick <title.txt> from path : /temp
    std::string txt_filename = title + ".txt";
    std::string temp_txt_path = "temp/" + txt_filename;
    std::string root_txt_path = txt_filename;

    std::string blog_content;
    if (fs::exists(temp_txt_path)) {
        std::cout << "[renderer.cpp] Picking " << txt_filename << " from path: /temp" << std::endl;
        blog_content = read_file_to_string(temp_txt_path);
    } else if (fs::exists(root_txt_path)) {
        std::cout << "[renderer.cpp] Picking " << txt_filename << " from project root and syncing to /temp" << std::endl;
        blog_content = read_file_to_string(root_txt_path);
        fs::create_directories("temp");
        fs::copy_file(root_txt_path, temp_txt_path, fs::copy_options::overwrite_existing);
    } else {
        std::cerr << "[renderer.cpp] Warning: Could not find " << txt_filename << " in /temp" << std::endl;
    }

    // 2. Pick template.html from path /template
    std::string template_path = "template/template.html";
    std::string html_template;
    if (fs::exists(template_path)) {
        std::cout << "[renderer.cpp] Picking template.html from path: /template" << std::endl;
        html_template = read_file_to_string(template_path);
    }

    if (html_template.empty()) {
        std::cerr << "[renderer.cpp] Warning: template.html not found, using default specification template" << std::endl;
        html_template = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title><topic></title>
    <style>
        body {
            margin: 0;
            padding: 20px;
            font-family: sans-serif;
            background: linear-gradient(rgba(0, 0, 0, 0.3), rgba(0, 0, 0, 0.3)), url('wood.jpg');
            background-size: cover;
            background-repeat: no-repeat;
            background-attachment: fixed;
        }

        .image-container {
            position: relative;
            display: inline-block; /* Keeps the container the exact same size as the image */
        }

        .image-container img {
            display: block;
            width: 100%;
            height: auto;
        }

        .overlay-text {
            position: absolute;
            top: 50%;                 /* Moves the top edge of the text to the vertical center */
            left: 50%;                /* Moves the left edge of the text to the horizontal center */
            transform: translate(-50%, -50%); /* Shifts the text back by half its own width and height */

            color: white;             
            font-size: 24px;
            font-family: sans-serif;
            text-align: center;
        }

        pre {
            color: #2C2C2C; /* Charcoal (#2C2C2C) */
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.4);
            font-family: inherit;
            white-space: pre-wrap;
            word-wrap: break-word;
            font-size: 18px;
            line-height: 1.6;
        }
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

    // 3. Populate template with title and content
    // Replace <title> = <topic>
    replace_all(html_template, "<title><topic></title>", "<title>" + title + "</title>");
    replace_all(html_template, "<title><title></title>", "<title>" + title + "</title>");
    replace_all(html_template, "<title>TEMPLATE</title>", "<title>" + title + "</title>");
    replace_all(html_template, "<topic>", title);

    // Replace image container placeholders
    replace_all(html_template, "<img src=\"<title>.jpg\" alt=\"<title>\">", "<img src=\"" + title + ".jpg\" alt=\"" + title + "\">");
    replace_all(html_template, "<div class=\"overlay-text\"> <title> </div>", "<div class=\"overlay-text\"> " + title + " </div>");
    replace_all(html_template, "<title>.jpg", title + ".jpg");

    // Replace blog text content in <pre>
    replace_all(html_template, "{ content from <title>.txt }", blog_content);
    replace_all(html_template, "{content}", blog_content);

    // 4. Create a new html file as : <title>.html
    // 5. Add to path : /blogs
    fs::create_directories("blogs");
    std::string output_path = "blogs/" + title + ".html";
    std::ofstream out_file(output_path);
    if (!out_file.is_open()) {
        std::cerr << "[renderer.cpp] Error: Failed to create " << output_path << std::endl;
        return false;
    }

    out_file << html_template;
    out_file.close();

    // Ensure assets are present in blogs/ for rendering
    ensure_image_assets(title);

    std::cout << "[renderer.cpp] Created a new html file as : " << title << ".html" << std::endl;
    std::cout << "[renderer.cpp] Added to path : /blogs (" << output_path << ")" << std::endl;
    return true;
}

} // namespace renderer
