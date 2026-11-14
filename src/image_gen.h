#ifndef IMAGE_GEN_H
#define IMAGE_GEN_H

#include <string>

namespace image_gen {

extern const char* MODEL_ID;
extern const char* MODEL_NAME;

std::string get_api_key();
std::string get_account_id();

bool generate_image_from_json(const std::string& title);

bool process_json_file(const std::string& json_filepath, const std::string& output_filepath);

bool generate_image_from_prompt(const std::string& prompt, const std::string& output_filepath);

}

#endif
