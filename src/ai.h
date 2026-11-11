#ifndef AI_H
#define AI_H

#include <string>

namespace ai {
    std::string get_api_key();

    // Asks Gemini for a 3:2 image illustrating the blog text and saves it to
    // assets/<title>.png (or .jpg). Returns true ONLY if a real generated image
    // was saved. On any failure it prints the reason, writes a placeholder
    // image so later steps still work, and returns false.
    bool generate_image_for_blog(const std::string& title, const std::string& blog_text);

    // Writes a simple 300x200 gradient PNG (pure C++/zlib, no external tools).
    bool write_placeholder_png(const std::string& filepath);
}

#endif // AI_H
