#ifndef UPLOAD_H
#define UPLOAD_H

#include <string>

namespace upload {
    // Executes: git add . ; git commit -m "added <title>" ; git push
    bool process(const std::string& title);
}

#endif // UPLOAD_H
