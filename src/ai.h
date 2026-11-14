#ifndef AI_H
#define AI_H

#include <string>

namespace ai {
    std::string get_api_key();

    bool generate_image_for_blog(const std::string& title, const std::string& blog_text);

    bool write_placeholder_png(const std::string& filepath);
}

#endif
