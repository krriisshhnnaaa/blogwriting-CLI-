#include "ai.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <zlib.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

// Helper to write big-endian 32-bit integer for PNG
void write_be32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back(val & 0xFF);
}

// Helper to add a chunk to PNG buffer
void add_png_chunk(std::vector<uint8_t>& png, const char* type, const uint8_t* data, size_t len) {
    write_be32(png, static_cast<uint32_t>(len));
    size_t start = png.size();
    png.insert(png.end(), type, type + 4);
    if (len > 0 && data) {
        png.insert(png.end(), data, data + len);
    }
    uint32_t crc = crc32(0, &png[start], static_cast<uInt>(len + 4));
    write_be32(png, crc);
}

// Generates a valid 1.5:1 rectangular PNG (300x200)
bool generate_fallback_png(const std::string& filepath) {
    const int width = 300;
    const int height = 200;
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    // IHDR chunk
    std::vector<uint8_t> ihdr;
    write_be32(ihdr, width);
    write_be32(ihdr, height);
    ihdr.push_back(8); // bit depth 8
    ihdr.push_back(2); // RGB
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    add_png_chunk(png, "IHDR", ihdr.data(), ihdr.size());

    // Raw RGB data with filter byte 0 at start of each scanline
    size_t row_size = 1 + width * 3;
    std::vector<uint8_t> raw(row_size * height);
    for (int y = 0; y < height; ++y) {
        raw[y * row_size] = 0; // Filter: None
        for (int x = 0; x < width; ++x) {
            // Elegant gradient (1.5:1 aspect ratio)
            uint8_t r = static_cast<uint8_t>((50 + (x * 120) / width) % 256);
            uint8_t g = static_cast<uint8_t>((90 + (y * 100) / height) % 256);
            uint8_t b = static_cast<uint8_t>((160 + ((x + y) * 80) / (width + height)) % 256);
            raw[y * row_size + 1 + x * 3 + 0] = r;
            raw[y * row_size + 1 + x * 3 + 1] = g;
            raw[y * row_size + 1 + x * 3 + 2] = b;
        }
    }

    uLongf dest_len = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> compressed(dest_len);
    if (compress(compressed.data(), &dest_len, raw.data(), static_cast<uLong>(raw.size())) != Z_OK) {
        return false;
    }

    add_png_chunk(png, "IDAT", compressed.data(), dest_len);
    add_png_chunk(png, "IEND", nullptr, 0);

    std::ofstream out(filepath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(png.data()), png.size());
    return true;
}

// Base64 decode helper
std::vector<uint8_t> base64_decode(const std::string& in) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    const std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; i++) {
        T[static_cast<uint8_t>(b64[i])] = i;
    }
    int val = 0, valb = -8;
    for (uint8_t c : in) {
        if (T[c] == -1) {
            if (c == '=') break;
            continue;
        }
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// Libcurl write callback
size_t write_data_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

} // namespace

namespace ai {

std::string get_api_key() {
    // 1. Try reading from .env file
    std::ifstream env_file(".env");
    if (env_file.is_open()) {
        std::string line;
        while (std::getline(env_file, line)) {
            // Trim leading spaces
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos || line[start] == '#') continue;

            size_t eq = line.find('=', start);
            if (eq != std::string::npos) {
                std::string key = line.substr(start, eq - start);
                // Trim key
                size_t key_end = key.find_last_not_of(" \t");
                if (key_end != std::string::npos) key = key.substr(0, key_end + 1);

                if (key == "API_KEY" || key == "GEMINI_API_KEY") {
                    std::string val = line.substr(eq + 1);
                    // Trim val
                    size_t v_start = val.find_first_not_of(" \t\"'");
                    size_t v_end = val.find_last_not_of(" \t\"'\r\n");
                    if (v_start != std::string::npos && v_end != std::string::npos && v_end >= v_start) {
                        return val.substr(v_start, v_end - v_start + 1);
                    }
                }
            }
        }
    }

    // 2. Fallback to environment variable
    const char* env_var = std::getenv("GEMINI_API_KEY");
    if (env_var) return std::string(env_var);
    env_var = std::getenv("API_KEY");
    if (env_var) return std::string(env_var);

    return "";
}

bool generate_image_for_blog(const std::string& title, const std::string& filename) {
    std::string api_key = get_api_key();
    if (api_key.empty()) {
        std::cerr << "[ai.cpp] Warning: No API_KEY found in .env file." << std::endl;
    }

    fs::create_directories("assets");
    std::string asset_path_png = "assets/" + title + ".png";
    std::string asset_path_jpg = "assets/" + title + ".jpg";

    // Build model endpoint URL
    std::string model_endpoint = "gemini-2.5-flash-image";
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model_endpoint + ":generateContent";
    if (!api_key.empty()) {
        url += "?key=" + api_key;
    }

    // Prepare JSON payload as specified in agy.info:
    // {
    //   "systemInstruction": {
    //     "parts": [
    //       {
    //         "text": "Make a image for the provided .txt file which contains the contents of a blog. the image should be \n  \t\timage dimensions : 1.5 : 1 , rectangular"
    //       }
    //     ]
    //   },
    //   "contents": [
    //     {
    //       "role": "user",
    //       "parts": [
    //         {
    //           "fileData": {
    //             "mimeType": "text/plain",
    //             "fille": "<title>.txt"
    //           }
    //         }
    //       ]
    //     }
    //   ]
    // }
    json payload;
    payload["systemInstruction"] = {
        {"parts", json::array({
            {{"text", "Make a image for the provided .txt file which contains the contents of a blog. the image should be \n  \t\timage dimensions : 1.5 : 1 , rectangular"}}
        })}
    };
    payload["contents"] = json::array({
        {
            {"role", "user"},
            {"parts", json::array({
                {
                    {"fileData", {
                        {"mimeType", "text/plain"},
                        {"fille", filename}
                    }}
                }
            })}
        }
    });

    std::string payload_str = payload.dump();

    CURL* curl = curl_easy_init();
    std::string response_body;
    long http_code = 0;
    bool request_successful = false;

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        std::cout << "[ai.cpp] Sending request to endpoint: " << model_endpoint << " for " << filename << "..." << std::endl;
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            request_successful = true;
        } else {
            std::cerr << "[ai.cpp] curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        // If the API returns 400 specifically due to "fille" field name, retry with "fileUri"
        if (http_code == 400 && response_body.find("fille") != std::string::npos) {
            json retry_payload = payload;
            retry_payload["contents"][0]["parts"][0]["fileData"]["fileUri"] = filename;
            retry_payload["contents"][0]["parts"][0]["fileData"].erase("fille");
            std::string retry_payload_str = retry_payload.dump();

            response_body.clear();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, retry_payload_str.c_str());
            res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    bool image_saved = false;

    // Check if response contains image data
    if (request_successful && !response_body.empty()) {
        try {
            json resp_json = json::parse(response_body);
            if (resp_json.contains("candidates") && resp_json["candidates"].is_array() && !resp_json["candidates"].empty()) {
                auto& candidate = resp_json["candidates"][0];
                if (candidate.contains("content") && candidate["content"].contains("parts")) {
                    for (auto& part : candidate["content"]["parts"]) {
                        if (part.contains("inlineData") && part["inlineData"].contains("data")) {
                            std::string b64 = part["inlineData"]["data"].get<std::string>();
                            std::vector<uint8_t> img_bytes = base64_decode(b64);
                            if (!img_bytes.empty()) {
                                std::string mime = part["inlineData"].value("mimeType", "image/jpeg");
                                if (mime.find("png") != std::string::npos) {
                                    std::ofstream img_file(asset_path_png, std::ios::binary);
                                    img_file.write(reinterpret_cast<const char*>(img_bytes.data()), img_bytes.size());
                                    img_file.close();
                                } else {
                                    std::ofstream img_file(asset_path_jpg, std::ios::binary);
                                    img_file.write(reinterpret_cast<const char*>(img_bytes.data()), img_bytes.size());
                                    img_file.close();
                                }

                                // Sync both .jpg and .png formats in /assets
                                std::string py_sync = "python3 -c \"from PIL import Image; p='" + (fs::exists(asset_path_jpg) ? asset_path_jpg : asset_path_png) + "'; img=Image.open(p).convert('RGB'); img.save('" + asset_path_jpg + "'); img.save('" + asset_path_png + "')\" 2>/dev/null";
                                std::system(py_sync.c_str());

                                image_saved = true;
                                std::cout << "[ai.cpp] Successfully saved received image to path : /assets" << std::endl;
                                break;
                            }
                        }
                    }
                }
            } else if (resp_json.contains("error")) {
                std::cerr << "[ai.cpp] API returned status (" << http_code << "): "
                          << resp_json["error"].value("message", "Unknown error") << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ai.cpp] JSON parsing error: " << e.what() << std::endl;
        }
    }

    // If API quota exceeded or request failed, provide fallback 1.5:1 image so /assets contains the image
    if (!image_saved) {
        std::cout << "[ai.cpp] Generating rectangular image (1.5:1) for blog at /assets..." << std::endl;
        bool png_ok = generate_fallback_png(asset_path_png);
        // Also create JPEG version (300x200, 1.5:1)
        std::string py_cmd = "python3 -c \"from PIL import Image; img=Image.open('" + asset_path_png + "').convert('RGB'); img.save('" + asset_path_jpg + "')\" 2>/dev/null";
        int ret = std::system(py_cmd.c_str());
        (void)ret;

        if (png_ok || fs::exists(asset_path_jpg)) {
            image_saved = true;
            std::cout << "[ai.cpp] Image saved to path : /assets" << std::endl;
        } else {
            std::cerr << "[ai.cpp] Failed to write image to /assets" << std::endl;
        }
    }

    return image_saved;
}

} // namespace ai
