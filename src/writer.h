#ifndef WRITER_H
#define WRITER_H

#include <string>

namespace writer {
    void handle_list();
    void handle_write(const std::string& title);
    void handle_prep(const std::string& title);
    void handle_make(const std::string& title);
    void handle_mark(const std::string& title);
    void handle_publish(const std::string& title);
    void handle_upload(const std::string& title);
}

#endif // WRITER_H
