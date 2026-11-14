#include "image_gen.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace image_gen {

const char* MODEL_ID = "@cf/black-forest-labs/flux-1-schnell";
const char* MODEL_NAME = "flux-1-schnell";

static size_t write_cb(void* data, size_t size, size_t nmemb, void* userp) {
    static_cast<std::string*>(userp)->append(static_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

static std::vector<unsigned char> base64_decode(const std::string& in) {
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> table(256, -1);
    for (int i = 0; i < 64; i++) table[static_cast<unsigned char>(chars[i])] = i;

    std::vector<unsigned char> out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        if (table[c] == -1) continue;
        val = (val << 6) + table[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<unsigned char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

static bool run_model(const std::string& account_id, const std::string& token,
                      const std::string& model, const json& body,
                      std::string& response, long& http_code) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "https://api.cloudflare.com/client/v4/accounts/" + account_id +
                      "/ai/run/" + model;
    std::string auth = "Authorization: Bearer " + token;
    std::string payload = body.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    } else {
        std::cerr << "[image_gen.cpp] Network error: " << curl_easy_strerror(res) << "\n";
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

std::string get_api_key() {
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

            if (key == "CF_API_TOKEN" || key == "CF_API_KEY" || key == "CLOUDFLARE_API_TOKEN") {
                std::string val = line.substr(eq + 1);
                size_t v_start = val.find_first_not_of(" \t\"'");
                size_t v_end = val.find_last_not_of(" \t\"'\r\n");
                if (v_start != std::string::npos && v_end != std::string::npos && v_end >= v_start) {
                    return val.substr(v_start, v_end - v_start + 1);
                }
            }
        }
    }

    for (const char* name : {"CF_API_TOKEN", "CF_API_KEY", "CLOUDFLARE_API_TOKEN"}) {
        const char* val = std::getenv(name);
        if (val && *val) return std::string(val);
    }

    return "";
}

std::string get_account_id() {
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

            if (key == "CF_ACCOUNT_ID" || key == "CLOUDFLARE_ACCOUNT_ID") {
                std::string val = line.substr(eq + 1);
                size_t v_start = val.find_first_not_of(" \t\"'");
                size_t v_end = val.find_last_not_of(" \t\"'\r\n");
                if (v_start != std::string::npos && v_end != std::string::npos && v_end >= v_start) {
                    return val.substr(v_start, v_end - v_start + 1);
                }
            }
        }
    }

    for (const char* name : {"CF_ACCOUNT_ID", "CLOUDFLARE_ACCOUNT_ID"}) {
        const char* val = std::getenv(name);
        if (val && *val) return std::string(val);
    }

    return "";
}

static std::string extract_prompt(const json& j) {
    if (j.contains("main")) {
        if (j["main"].is_object()) {
            if (j["main"].contains("prompt for AI") && j["main"]["prompt for AI"].is_string()) {
                return j["main"]["prompt for AI"].get<std::string>();
            }
            if (j["main"].contains("prompt") && j["main"]["prompt"].is_string()) {
                return j["main"]["prompt"].get<std::string>();
            }
            for (auto& [k, v] : j["main"].items()) {
                if (v.is_string()) return v.get<std::string>();
            }
        } else if (j["main"].is_string()) {
            return j["main"].get<std::string>();
        }
    }
    if (j.contains("prompt") && j["prompt"].is_string()) {
        return j["prompt"].get<std::string>();
    }
    if (j.contains("prompt for AI") && j["prompt for AI"].is_string()) {
        return j["prompt for AI"].get<std::string>();
    }
    return "";
}

bool generate_image_from_prompt(const std::string& prompt, const std::string& output_filepath) {
    std::string account = get_account_id();
    std::string token = get_api_key();

    if (account.empty() || token.empty()) {
        std::cerr << "[image_gen.cpp] Error: CF_ACCOUNT_ID or CF_API_TOKEN is not configured.\n";
        return false;
    }

    json body = {{"prompt", prompt}, {"steps", 4}};
    std::string response;
    long http_code = 0;

    std::cout << "[image_gen.cpp] Requesting image from " << MODEL_ID << " (" << MODEL_NAME << ")..." << std::endl;
    if (!run_model(account, token, MODEL_ID, body, response, http_code)) {
        std::cerr << "[image_gen.cpp] Network request to Cloudflare failed." << std::endl;
        return false;
    }

    json resp;
    try {
        resp = json::parse(response);
    } catch (const std::exception& e) {
        std::cerr << "[image_gen.cpp] Could not parse response (HTTP " << http_code << "): " << e.what() << "\n"
                  << response.substr(0, 300) << "\n";
        return false;
    }

    if (!resp.value("success", false) || !resp.contains("result") || !resp["result"].contains("image")) {
        std::cerr << "[image_gen.cpp] Request failed (HTTP " << http_code << "): "
                  << resp.value("errors", json::array()).dump() << "\n";
        return false;
    }

    std::string b64 = resp["result"]["image"].get<std::string>();
    std::vector<unsigned char> bytes = base64_decode(b64);
    if (bytes.empty()) {
        std::cerr << "[image_gen.cpp] Decoded image data was empty.\n";
        return false;
    }

    fs::path out_path(output_filepath);
    if (out_path.has_parent_path()) {
        fs::create_directories(out_path.parent_path());
    }

    std::ofstream out(output_filepath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "[image_gen.cpp] Could not write " << output_filepath << "\n";
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();

    std::cout << "[image_gen.cpp] Saved " << output_filepath << " (" << bytes.size() << " bytes)\n";
    return true;
}

bool process_json_file(const std::string& json_filepath, const std::string& output_filepath) {
    std::ifstream in(json_filepath);
    if (!in.is_open()) {
        std::cerr << "[image_gen.cpp] Error: Could not open " << json_filepath << std::endl;
        return false;
    }

    json json_data;
    try {
        in >> json_data;
    } catch (const std::exception& e) {
        std::cerr << "[image_gen.cpp] JSON parse error in " << json_filepath << ": " << e.what() << std::endl;
        return false;
    }

    std::string prompt = extract_prompt(json_data);
    if (prompt.empty()) {
        std::cerr << "[image_gen.cpp] Error: No prompt found in " << json_filepath << std::endl;
        return false;
    }

    std::cout << "[image_gen.cpp] Extracted prompt from " << json_filepath << ": \"" << prompt << "\"" << std::endl;
    return generate_image_from_prompt(prompt, output_filepath);
}

bool generate_image_from_json(const std::string& title) {
    std::string filename = title + ".json";
    std::string temp_path = "temp/" + filename;
    std::string root_path = filename;
    std::string json_path;

    if (fs::exists(temp_path)) {
        json_path = temp_path;
    } else if (fs::exists(root_path)) {
        json_path = root_path;
    } else {
        std::cerr << "[image_gen.cpp] Error: " << filename << " not found in temp/ or root." << std::endl;
        return false;
    }

    fs::create_directories("assets");
    std::error_code ec;
    fs::remove("assets/" + title + ".png", ec);
    fs::remove("assets/" + title + ".jpg", ec);
    fs::remove("assets/" + title + ".jpeg", ec);

    std::string outfile = "assets/" + title + ".jpg";
    return process_json_file(json_path, outfile);
}

}

#ifdef IMAGE_GEN_STANDALONE
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <title.json | prompt> [output.jpg]\n";
        return 1;
    }

    std::string input = argv[1];
    std::string outfile = argc > 2 ? argv[2] : "output.jpg";

    if (input.size() > 5 && input.substr(input.size() - 5) == ".json") {
        if (!image_gen::process_json_file(input, outfile)) {
            return 1;
        }
    } else {
        if (!image_gen::generate_image_from_prompt(input, outfile)) {
            return 1;
        }
    }
    return 0;
}
#endif
