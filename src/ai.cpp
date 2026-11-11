#include "ai.h"
#include <iostream>
#include <fstream>
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

const char* MODEL = "gemini-2.5-flash-image";

void write_be32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back(val & 0xFF);
}

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

size_t write_data_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Remove any previously saved image for this title so only one format exists.
void remove_old_assets(const std::string& title) {
    std::error_code ec;
    for (const char* ext : {".png", ".jpg", ".jpeg"}) {
        fs::remove("assets/" + title + ext, ec);
    }
}

// POSTs the payload; fills response_body and http_code. Returns an error string
// (empty on success).
std::string post_to_gemini(const std::string& api_key, const std::string& payload,
                           std::string& response_body, long& http_code) {
    CURL* curl = curl_easy_init();
    if (!curl) return "could not initialise libcurl";

    std::string url = std::string("https://generativelanguage.googleapis.com/v1beta/models/") +
                      MODEL + ":generateContent";
    std::string key_header = "x-goog-api-key: " + api_key;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, key_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L); // image generation can be slow

    CURLcode res = curl_easy_perform(curl);
    std::string err;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    } else {
        err = std::string("network error: ") + curl_easy_strerror(res);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return err;
}

// Tries to pull an image out of a Gemini response and save it. Returns an empty
// string on success, otherwise a human-readable reason.
std::string save_image_from_response(const std::string& title, const std::string& body, long http_code) {
    json resp;
    try {
        resp = json::parse(body);
    } catch (const std::exception& e) {
        return "could not parse API response (HTTP " + std::to_string(http_code) + ")";
    }

    if (resp.contains("error")) {
        return "API error (HTTP " + std::to_string(http_code) + "): " +
               resp["error"].value("message", "unknown error");
    }

    std::string model_text;
    if (resp.contains("candidates") && resp["candidates"].is_array() && !resp["candidates"].empty()) {
        const auto& cand = resp["candidates"][0];
        if (cand.contains("content") && cand["content"].contains("parts")) {
            for (const auto& part : cand["content"]["parts"]) {
                if (part.contains("inlineData") && part["inlineData"].contains("data")) {
                    std::vector<uint8_t> bytes =
                        base64_decode(part["inlineData"]["data"].get<std::string>());
                    if (bytes.empty()) continue;

                    std::string mime = part["inlineData"].value("mimeType", "image/png");
                    std::string ext = (mime.find("jpeg") != std::string::npos ||
                                       mime.find("jpg") != std::string::npos) ? ".jpg" : ".png";
                    fs::create_directories("assets");
                    remove_old_assets(title);
                    std::ofstream out("assets/" + title + ext, std::ios::binary);
                    if (!out) return "could not write assets/" + title + ext;
                    out.write(reinterpret_cast<const char*>(bytes.data()),
                              static_cast<std::streamsize>(bytes.size()));
                    return "";
                }
                if (part.contains("text")) model_text += part["text"].get<std::string>();
            }
        }
    }

    if (!model_text.empty()) {
        return "the model replied with text instead of an image: " + model_text.substr(0, 200);
    }
    if (resp.contains("promptFeedback")) {
        return "the request was blocked: " + resp["promptFeedback"].dump();
    }
    return "the response contained no image";
}

} // namespace

namespace ai {

bool write_placeholder_png(const std::string& filepath) {
    const int width = 300;
    const int height = 200; // 1.5:1
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<uint8_t> ihdr;
    write_be32(ihdr, width);
    write_be32(ihdr, height);
    ihdr.push_back(8); // bit depth
    ihdr.push_back(2); // RGB
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    add_png_chunk(png, "IHDR", ihdr.data(), ihdr.size());

    size_t row_size = 1 + width * 3;
    std::vector<uint8_t> raw(row_size * height);
    for (int y = 0; y < height; ++y) {
        raw[y * row_size] = 0; // filter: none
        for (int x = 0; x < width; ++x) {
            raw[y * row_size + 1 + x * 3 + 0] = static_cast<uint8_t>(50 + (x * 120) / width);
            raw[y * row_size + 1 + x * 3 + 1] = static_cast<uint8_t>(90 + (y * 100) / height);
            raw[y * row_size + 1 + x * 3 + 2] = static_cast<uint8_t>(160 + ((x + y) * 80) / (width + height));
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
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return static_cast<bool>(out);
}

std::string get_api_key() {
    // 1. Try reading from .env in current directory, then near executable
    std::vector<fs::path> candidate_paths = { ".env", "../.env" };
    try {
        fs::path exe_dir = fs::canonical("/proc/self/exe").parent_path();
        candidate_paths.push_back(exe_dir / ".env");
        candidate_paths.push_back(exe_dir.parent_path() / ".env");
    } catch (...) {}

    for (const auto& path : candidate_paths) {
        std::ifstream env_file(path);
        if (!env_file.is_open()) continue;

        std::string line;
        while (std::getline(env_file, line)) {
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos || line[start] == '#') continue;

            size_t eq = line.find('=', start);
            if (eq == std::string::npos) continue;

            std::string key = line.substr(start, eq - start);
            size_t key_end = key.find_last_not_of(" \t");
            if (key_end != std::string::npos) key = key.substr(0, key_end + 1);

            if (key == "API_KEY" || key == "GEMINI_API_KEY" || key == "GOOGLE_API_KEY") {
                std::string val = line.substr(eq + 1);
                size_t v_start = val.find_first_not_of(" \t\"'");
                size_t v_end = val.find_last_not_of(" \t\"'\r\n");
                if (v_start != std::string::npos && v_end != std::string::npos && v_end >= v_start) {
                    return val.substr(v_start, v_end - v_start + 1);
                }
            }
        }
    }

    // 2. Fallback to environment variables
    for (const char* name : {"GEMINI_API_KEY", "GOOGLE_API_KEY", "API_KEY"}) {
        const char* env_var = std::getenv(name);
        if (env_var && *env_var) return std::string(env_var);
    }
    return "";
}

bool generate_image_for_blog(const std::string& title, const std::string& blog_text) {
    std::string failure;

    std::string api_key = get_api_key();
    if (api_key.empty()) {
        failure = "no API key found (set GEMINI_API_KEY in .env or the environment)";
    } else {
        // The blog text is sent inline as a normal text part. (A local file name
        // is not something the API can fetch.)
        std::string prompt =
            "Make an image for the blog post below. The image should be a rectangular "
            "landscape image with a 3:2 (1.5:1) aspect ratio. If the post contains "
            "instructions about the image, follow them.\n\n--- BLOG POST ---\n" + blog_text;

        json payload = {
            {"contents", json::array({
                {{"role", "user"}, {"parts", json::array({ {{"text", prompt}} })}}
            })},
            {"generationConfig", {{"imageConfig", {{"aspectRatio", "3:2"}}}}}
        };

        std::cout << "[ai.cpp] Requesting image from " << MODEL << "..." << std::endl;
        std::string response_body;
        long http_code = 0;
        std::string net_err = post_to_gemini(api_key, payload.dump(), response_body, http_code);
        failure = net_err.empty() ? save_image_from_response(title, response_body, http_code) : net_err;
    }

    if (failure.empty()) {
        std::cout << "[ai.cpp] Saved generated image to assets/" << title << std::endl;
        return true;
    }

    std::cerr << "[ai.cpp] WARNING: image generation failed: " << failure << std::endl;
    fs::create_directories("assets");
    remove_old_assets(title);
    if (write_placeholder_png("assets/" + title + ".png")) {
        std::cerr << "[ai.cpp] Wrote a PLACEHOLDER image to assets/" << title
                  << ".png (not AI generated)." << std::endl;
    } else {
        std::cerr << "[ai.cpp] Could not write a placeholder image either." << std::endl;
    }
    return false;
}

} // namespace ai
