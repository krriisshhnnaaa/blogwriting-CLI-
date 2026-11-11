#ifndef IMAGE_GEN_H
#define IMAGE_GEN_H

#include <string>

namespace image_gen {

extern const char* MODEL_ID;
extern const char* MODEL_NAME;

// Retrieves API token and Account ID (checks .env file or environment variables)
std::string get_api_key();
std::string get_account_id();

// Generates an image using Cloudflare Workers AI flux-1-schnell model
// by reading the prompt from <title>.json (or temp/<title>.json),
// and saving the resulting image to assets/<title>.jpg.
bool generate_image_from_json(const std::string& title);

// Direct function taking path to a JSON file and output image path
bool process_json_file(const std::string& json_filepath, const std::string& output_filepath);

// Direct function taking prompt string and output image path
bool generate_image_from_prompt(const std::string& prompt, const std::string& output_filepath);

} // namespace image_gen

#endif // IMAGE_GEN_H
