#ifndef MARK_H
#define MARK_H

#include <string>

namespace mark {
    bool process(const std::string& title);

    bool generate_markdown(const std::string& title, const std::string& blog_text, std::string& output_md);
}

#endif
