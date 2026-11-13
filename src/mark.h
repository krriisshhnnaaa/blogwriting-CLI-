#ifndef MARK_H
#define MARK_H

#include <string>

namespace mark {
    // Main processing function called when user commands `mark <title>`:
    // 1. Reads <title>.txt (from temp/ or project root)
    // 2. Parses through Gemini API (gemini-3.8-flash) requesting <title>.md metadata
    // 3. Generates <title>.md with frontmatter (title, date, description, cover, url)
    // 4. Saves <title>.md to project root and temp/
    // 5. Sleeps 1 second
    // 6. Executes save.sh to copy <title>.md to personal website project blog directory
    bool process(const std::string& title);

    // Creates the markdown content with metadata
    bool generate_markdown(const std::string& title, const std::string& blog_text, std::string& output_md);
}

#endif // MARK_H
