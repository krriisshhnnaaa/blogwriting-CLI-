#include "mark.h"
#include "ai.h"
#include "util.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sys/wait.h>
#include <unistd.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

const char* PRIMARY_MODEL = "gemini-3.8-flash";
const char* BACKUP_MODEL = "gemini-3.5-flash-lite";

static size_t mark_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mark_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    } else {
        std::cerr << "[mark.cpp] Network error: " << curl_easy_strerror(res) << "\n";
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

std::string get_current_date() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

bool run_save_script(const std::string& title) {
    std::string script = "./src/save.sh";
    if (!fs::exists(script)) {
        if (fs::exists("src/save.sh")) {
            script = "src/save.sh";
        } else if (fs::exists("./save.sh")) {
            script = "./save.sh";
        }
    }

    std::cout << "[mark.cpp] Executing save script (" << script << " " << title << ")..." << std::endl;

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[mark.cpp] Error: fork failed for " << script << std::endl;
        return false;
    }
    if (pid == 0) {
        execlp("/bin/bash", "bash", script.c_str(), title.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::cerr << "[mark.cpp] Error: waitpid failed for " << script << std::endl;
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

}

namespace mark {

bool generate_markdown(const std::string& title, const std::string& blog_text, std::string& output_md) {
    std::string api_key = ai::get_api_key();
    std::string date_str = get_current_date();
    std::string default_url = "/blog/" + title + "/" + title + ".html";
    std::string default_cover = "cover.jpg";

    std::string system_instruction =
        "You are an expert technical editor. Given the blog title and blog text, "
        "generate metadata for the markdown post.\n"
        "Return ONLY a JSON object with this exact structure:\n"
        "{\n"
        "  \"title\": \"" + title + "\",\n"
        "  \"description\": \"<Summary or excerpt of the blog content>\",\n"
        "  \"cover\": \"cover.jpg\",\n"
        "  \"url\": \"/blog/" + title + "/" + title + ".html\"\n"
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

    std::cout << "[mark.cpp] Sending request to model-endpoint: " << PRIMARY_MODEL << "..." << std::endl;

    if (!api_key.empty()) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            response_body.clear();
            http_code = 0;
            if (call_gemini_api(PRIMARY_MODEL, api_key, payload, response_body, http_code)) {
                if (http_code == 200) {
                    api_ok = true;
                    break;
                } else if (http_code == 503) {
                    std::cout << "[mark.cpp] " << PRIMARY_MODEL << " returned 503, retrying..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                } else {
                    std::cerr << "[mark.cpp] API returned HTTP " << http_code << std::endl;
                    break;
                }
            }
        }

        if (!api_ok && http_code == 503) {
            std::cout << "[mark.cpp] " << PRIMARY_MODEL << " high demand spike (503). Trying " << BACKUP_MODEL << "..." << std::endl;
            response_body.clear();
            http_code = 0;
            if (call_gemini_api(BACKUP_MODEL, api_key, payload, response_body, http_code) && http_code == 200) {
                api_ok = true;
            }
        }
    }

    std::string desc;
    std::string cover = default_cover;
    std::string url = default_url;
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
                            json meta = json::parse(cleaned);
                            if (meta.contains("description") && meta["description"].is_string()) {
                                desc = meta["description"].get<std::string>();
                                parsed_ok = true;
                            }
                            if (meta.contains("cover") && meta["cover"].is_string()) {
                                cover = meta["cover"].get<std::string>();
                            }
                            if (meta.contains("url") && meta["url"].is_string()) {
                                url = meta["url"].get<std::string>();
                            }
                            break;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[mark.cpp] Warning: Could not parse model response as JSON: " << e.what() << std::endl;
        }
    }

    if (!parsed_ok || desc.empty()) {
        std::cout << "[mark.cpp] Extracting description directly from blog text..." << std::endl;
        std::istringstream stream(blog_text);
        std::string line;
        while (std::getline(stream, line)) {
            line = util::trim(line);
            if (!line.empty() && line[0] != '#') {
                desc = line.substr(0, std::min<size_t>(line.size(), 140));
                break;
            }
        }
        if (desc.empty()) {
            desc = "Blog post about " + title;
        }
    }

    std::ostringstream md;
    md << "---\n";
    md << "title: \"" << title << "\"\n";
    md << "date: \"" << date_str << "\"\n";
    md << "description: \"" << desc << "\"\n";
    md << "cover: \"" << cover << "\"\n";
    md << "url: \"" << url << "\"\n";
    md << "---\n\n";
    md << blog_text;
    if (blog_text.empty() || blog_text.back() != '\n') {
        md << "\n";
    }

    output_md = md.str();
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
        std::cerr << "[mark.cpp] Error: " << filename
                  << " not found in temp/ or project root. Run 'write " << title << "' first." << std::endl;
        return false;
    }

    std::ifstream in(source);
    std::stringstream buffer;
    buffer << in.rdbuf();

    std::cout << "[mark.cpp] Parsing " << filename << " through gemini api..." << std::endl;

    std::string md_content;
    if (!generate_markdown(title, buffer.str(), md_content)) {
        std::cerr << "[mark.cpp] Error: Failed to generate markdown for " << title << std::endl;
        return false;
    }

    fs::create_directories("temp");
    std::string temp_md_path = "temp/" + title + ".md";
    std::string root_md_path = title + ".md";

    std::ofstream out_temp(temp_md_path);
    out_temp << md_content;
    out_temp.close();

    std::ofstream out_root(root_md_path);
    out_root << md_content;
    out_root.close();

    std::cout << "[mark.cpp] Saved " << root_md_path << " and " << temp_md_path << std::endl;

    std::cout << "[mark.cpp] Waiting 1 second..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!run_save_script(title)) {
        std::cerr << "[mark.cpp] Warning: save.sh reported non-zero status." << std::endl;
        return false;
    }

    return true;
}

}

#ifdef MARK_STANDALONE
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <title>\n";
        return 1;
    }
    return mark::process(argv[1]) ? 0 : 1;
}
#endif
