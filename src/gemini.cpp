#include "gemini.h"
#include "image_gen.h"
#include "ai.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

const char* PRIMARY_MODEL = "gemini-3.8-flash";
const char* BACKUP_MODEL = "gemini-3.5-flash-lite";

static size_t gemini_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total_size);
    return total_size;
}

bool call_gemini_api(const std::string& model_endpoint, const std::string& api_key,
                     const json& payload, std::string& response_body, long& http_code) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" +
                      model_endpoint + ":generateContent";
    std::string key_header = "x-goog-api-key: " + api_key;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, key_header.c_str());

    std::string payload_str = payload.dump();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gemini_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    } else {
        std::cerr << "[gemini.cpp] Network error: " << curl_easy_strerror(res) << "\n";
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

std::string clean_json_text(const std::string& raw) {
    std::string s = raw;
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start != std::string::npos) s = s.substr(start);

    if (s.rfind("```json", 0) == 0) {
        s = s.substr(7);
    } else if (s.rfind("```", 0) == 0) {
        s = s.substr(3);
    }

    size_t end = s.rfind("```");
    if (end != std::string::npos) {
        s = s.substr(0, end);
    }

    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    if (b != std::string::npos && e != std::string::npos && e >= b) {
        return s.substr(b, e - b + 1);
    }
    return s;
}

}

namespace gemini {

bool generate_prompt_json(const std::string& title, const std::string& blog_text) {
    std::string api_key = ai::get_api_key();
    if (api_key.empty()) {
        std::cerr << "[gemini.cpp] Warning: No API key found in .env or environment." << std::endl;
    }

    std::string system_instruction =
        "You are an expert AI prompt engineer. Given the blog title and blog text, "
        "create a detailed prompt for an image generation AI (flux-1-schnell) to produce a cover image for this blog.\n"
        "Return ONLY a JSON object with this exact structure:\n"
        "{\n"
        "  \"meta-data\": {\n"
        "    \"blog name\": \"" + title + "\",\n"
        "    \"title\": \"" + title + "\"\n"
        "  },\n"
        "  \"main\": {\n"
        "    \"prompt for AI\": \"<detailed, descriptive prompt for generating the cover image>\"\n"
        "  }\n"
        "}";

    std::string user_content = "Blog Title: " + title + "\n\nBlog Content:\n" + blog_text;

    json payload = {
        {"contents", json::array({
            {
                {"role", "user"},
                {"parts", json::array({
                    {{"text", system_instruction + "\n\n" + user_content}}
                })}
            }
        })},
        {"generationConfig", {
            {"responseMimeType", "application/json"}
        }}
    };

    std::string response_body;
    long http_code = 0;
    bool api_ok = false;

    std::cout << "[gemini.cpp] Sending request to model-endpoint: " << PRIMARY_MODEL << "..." << std::endl;

    if (!api_key.empty()) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            response_body.clear();
            http_code = 0;
            if (call_gemini_api(PRIMARY_MODEL, api_key, payload, response_body, http_code)) {
                if (http_code == 200) {
                    api_ok = true;
                    break;
                } else if (http_code == 503) {
                    std::cout << "[gemini.cpp] " << PRIMARY_MODEL << " returned 503 (high demand), retrying..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                } else {
                    std::cerr << "[gemini.cpp] API returned HTTP " << http_code << std::endl;
                    break;
                }
            }
        }

        if (!api_ok && http_code == 503) {
            std::cout << "[gemini.cpp] " << PRIMARY_MODEL << " high demand spike (503). Trying " << BACKUP_MODEL << "..." << std::endl;
            response_body.clear();
            http_code = 0;
            if (call_gemini_api(BACKUP_MODEL, api_key, payload, response_body, http_code) && http_code == 200) {
                api_ok = true;
            }
        }
    }

    json result_json;
    bool parsed_ok = false;

    if (api_ok && !response_body.empty()) {
        try {
            json resp = json::parse(response_body);
            if (resp.contains("candidates") && resp["candidates"].is_array() && !resp["candidates"].empty()) {
                auto& candidate = resp["candidates"][0];
                if (candidate.contains("content") && candidate["content"].contains("parts")) {
                    for (auto& part : candidate["content"]["parts"]) {
                        if (part.contains("text")) {
                            std::string raw_text = part["text"].get<std::string>();
                            std::string cleaned = clean_json_text(raw_text);
                            result_json = json::parse(cleaned);
                            parsed_ok = true;
                            break;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[gemini.cpp] Warning: Could not parse model response as JSON: " << e.what() << std::endl;
        }
    }

    if (!parsed_ok || !result_json.contains("main")) {
        std::cout << "[gemini.cpp] Using structured fallback prompt for " << title << "..." << std::endl;
        std::string prompt_text = "A high-quality, beautiful, cinematic landscape digital art cover illustration for a blog titled '" +
                                  title + "', 1.5:1 rectangular aspect ratio, vivid colors, modern aesthetic";
        if (!blog_text.empty()) {
            std::string snippet = blog_text.substr(0, std::min<size_t>(blog_text.size(), 140));
            for (char& c : snippet) { if (c == '\n' || c == '\r') c = ' '; }
            prompt_text += ", inspired by: " + snippet;
        }

        result_json = {
            {"meta-data", {
                {"blog name", title},
                {"title", title}
            }},
            {"main", {
                {"prompt for AI", prompt_text}
            }}
        };
    } else {
        if (!result_json.contains("meta-data")) {
            result_json["meta-data"] = {
                {"blog name", title},
                {"title", title}
            };
        } else {
            if (!result_json["meta-data"].contains("blog name")) {
                result_json["meta-data"]["blog name"] = title;
            }
            if (!result_json["meta-data"].contains("title")) {
                result_json["meta-data"]["title"] = title;
            }
        }

        if (result_json["main"].is_string()) {
            std::string prompt_str = result_json["main"].get<std::string>();
            result_json["main"] = {
                {"prompt for AI", prompt_str}
            };
        } else if (result_json["main"].is_object()) {
            if (!result_json["main"].contains("prompt for AI")) {
                if (result_json["main"].contains("prompt")) {
                    result_json["main"]["prompt for AI"] = result_json["main"]["prompt"];
                }
            }
        }
    }

    fs::create_directories("temp");
    std::string temp_json_path = "temp/" + title + ".json";
    std::string root_json_path = title + ".json";

    std::ofstream out_temp(temp_json_path);
    out_temp << result_json.dump(2) << std::endl;
    out_temp.close();

    std::ofstream out_root(root_json_path);
    out_root << result_json.dump(2) << std::endl;
    out_root.close();

    std::cout << "[gemini.cpp] Saved " << root_json_path << " and " << temp_json_path << std::endl;
    return true;
}

bool process(const std::string& title) {
    std::string filename = title + ".txt";
    std::string temp_path = "temp/" + filename;

    std::string source;
    if (fs::exists(temp_path)) {
        source = temp_path;
    } else if (fs::exists(filename)) {
        fs::create_directories("temp");
        fs::copy_file(filename, temp_path, fs::copy_options::overwrite_existing);
        source = temp_path;
    } else {
        std::cerr << "[gemini.cpp] Error: " << filename
                  << " not found in temp/ or the project root. Run 'write " << title
                  << "' first." << std::endl;
        return false;
    }

    std::ifstream in(source);
    std::stringstream buffer;
    buffer << in.rdbuf();

    std::cout << "[gemini.cpp] Parsing " << filename << " through gemini.cpp..." << std::endl;

    if (!generate_prompt_json(title, buffer.str())) {
        std::cerr << "[gemini.cpp] Failed to generate " << title << ".json" << std::endl;
        return false;
    }

    std::cout << "[gemini.cpp] Parsing " << title << ".json through image_gen.cpp..." << std::endl;
    bool img_ok = image_gen::generate_image_from_json(title);
    if (!img_ok) {
        std::cerr << "[gemini.cpp] Warning: image generation failed in image_gen.cpp, writing placeholder..." << std::endl;
        fs::create_directories("assets");
        ai::write_placeholder_png("assets/" + title + ".png");
        return false;
    }

    return true;
}

}
