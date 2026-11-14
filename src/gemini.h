#ifndef GEMINI_H
#define GEMINI_H

#include <string>

namespace gemini {
    bool process(const std::string& title);

    bool generate_prompt_json(const std::string& title, const std::string& blog_text);
}

#endif
