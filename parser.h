#ifndef PARSER_H
#define PARSER_H

#include <string>

namespace parser {

struct Command {
    std::string name;
    std::string argument;
    bool valid = false;
};

Command parse_line(const std::string& line);

} // namespace parser

#endif // PARSER_H
