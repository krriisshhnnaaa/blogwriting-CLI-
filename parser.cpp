#include "parser.h"
#include <sstream>

namespace parser {

Command parse_line(const std::string& line) {
    Command cmd;
    std::istringstream iss(line);
    if (iss >> cmd.name) {
        cmd.valid = true;
        iss >> cmd.argument;
    }
    return cmd;
}

} // namespace parser
