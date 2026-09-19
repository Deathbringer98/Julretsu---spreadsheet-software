#pragma once
#include "GridCore.hpp"
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>
namespace julretsu {
// Optional "Connect Your AI": the user's own provider account, key kept in the OS credential store,
// proposals previewed before anything touches the sheet.
enum class AiProvider { Anthropic, OpenAICompatible };
struct AiSettings {
    AiProvider provider = AiProvider::Anthropic;
    std::string model = "claude-opus-5";
    std::string endpoint = "https://api.anthropic.com/v1/messages";
};
[[nodiscard]] AiSettings default_ai_settings(AiProvider provider);
struct AiContext { std::string text; std::size_t cells{}, total{}; bool truncated{}; };
struct AiEdit {
    CellCoord coord; Input input; std::string address, before, after;
    bool lua{}, selected = true;
};
struct AiProposal { std::string summary; std::vector<AiEdit> edits; std::vector<std::string> warnings; };
struct HttpRequest { std::string url; std::vector<std::pair<std::string, std::string>> headers; std::string body; };
struct HttpResponse { long status{}; std::string body; };

// Empty on success; otherwise a user-facing reason. Keys never travel over plain HTTP except to this machine.
[[nodiscard]] std::string validate_ai_endpoint(std::string_view url);
[[nodiscard]] std::string endpoint_host(std::string_view url);
[[nodiscard]] AiContext describe_sheet(const Sheet&, CellCoord active, CellCoord anchor,
                                       std::size_t max_cells = 2000, std::size_t max_bytes = 200 * 1024);
[[nodiscard]] HttpRequest build_ai_request(const AiSettings&, std::string_view key, std::string_view instruction, const AiContext&);
// Returns the model's JSON text, or throws std::runtime_error with a user-facing message.
[[nodiscard]] std::string extract_ai_text(const AiSettings&, const HttpResponse&);
[[nodiscard]] AiProposal parse_ai_proposal(std::string_view text, const Sheet&, std::size_t max_edits = 1000);
void refresh_proposal(AiProposal&, const Sheet&);
[[nodiscard]] Batch proposal_batch(const AiProposal&);

// Non-secret settings live beside the appearance preference; the API key lives in Windows Credential Manager.
[[nodiscard]] AiSettings load_ai_settings();
bool save_ai_settings(const AiSettings&);
bool store_ai_key(AiProvider, std::string_view key);
[[nodiscard]] std::optional<std::string> load_ai_key(AiProvider);
bool delete_ai_key(AiProvider);

// One background HTTPS request at a time; cancel() aborts the connection promptly.
class AiSession {
public:
    struct State; // defined in Ai.cpp
private:
    std::shared_ptr<State> state_;
    std::thread worker_;
public:
    AiSession();
    ~AiSession();
    AiSession(const AiSession&) = delete;
    AiSession& operator=(const AiSession&) = delete;
    void start(HttpRequest request);
    void cancel();
    [[nodiscard]] bool busy() const;
    // Completed response, or an error message; nullopt while running or idle.
    [[nodiscard]] std::optional<std::pair<HttpResponse, std::string>> poll();
};
}
