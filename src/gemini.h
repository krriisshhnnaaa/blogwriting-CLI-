#ifndef GEMINI_H
#define GEMINI_H

#include <string>

namespace gemini {
    // Main processing function called when user commands `prep <title>`:
    // 1. Reads <title>.txt
    // 2. Uses Gemini model-endpoint gemini-3.8-flash to generate an image prompt
    // 3. Saves the result as <title>.json with meta-data and main
    // 4. Passes <title>.json to image_gen.cpp (Cloudflare flux-1-schnell) to create the cover image in assets/
    bool process(const std::string& title);

    // Creates the prompt JSON from blog text using Gemini 3.8 flash and saves <title>.json
    bool generate_prompt_json(const std::string& title, const std::string& blog_text);
}

#endif // GEMINI_H
