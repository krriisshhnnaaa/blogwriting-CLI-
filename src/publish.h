#ifndef PUBLISH_H
#define PUBLISH_H

#include <string>

namespace publish {
    // Executes the publish workflow for <title>:
    // 1. Runs make to ensure fresh HTML and images
    // 2. Copies rendered assets to personal website
    // 3. Creates post.md with frontmatter metadata
    // 4. Registers post in blog-index.json
    bool process(const std::string& title);
}

#endif // PUBLISH_H
