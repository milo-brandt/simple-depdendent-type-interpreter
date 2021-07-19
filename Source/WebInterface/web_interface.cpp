/*
#include <iostream>
#include <variant>
#include <memory>
#include <vector>

#include <httpserver.hpp>
#include <json.hpp>
#include <shared_mutex>
#include <mutex>
#include <chrono>
#include <sstream>
#include "../Utility/overloaded.hpp"

#include "session.hpp"

using namespace httpserver;

template <>
struct nlohmann::adl_serializer<Web::Request::Any> {
  static void to_json(json& j, const Web::Request::Any& request) {
    throw std::runtime_error("Cannot serialized requests.");
  }
  static void from_json(const json& j, Web::Request::Any& request) {
    auto action = j.at("action").get<std::string>();
    if(action == "simplify") {
      auto index = j.at("index").get<std::size_t>();
      request = Web::Request::Simplify{index};
    } else if(action == "initialize") {
      request = Web::Request::Initialize{};
    } else {
      throw std::runtime_error("Unknown request action: \"" + action + "\"");
    }
  }
};
namespace format {
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Span, start, end)
}
namespace combinator {
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Match, trigger, area, matches)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InterfaceOutput, string, matches)
}
namespace Web {
  namespace {
    struct SessionEntry {
      std::timed_mutex session_mutex;
      Session session;
      template<class OkayCallback>
      decltype(auto) modify(OkayCallback&& okay) {
        std::unique_lock lock(session_mutex, std::defer_lock);
        if(lock.try_lock_for(std::chrono::milliseconds{50})) {
          return okay(session);
        } else {
          throw std::runtime_error("Session is busy.");
        }
      }
    };
    struct SessionVector {
      std::shared_timed_mutex vector_mutex;
      std::vector<std::shared_ptr<SessionEntry> > entries;
      template<class OkayCallback>
      decltype(auto) get_session(std::size_t index, OkayCallback&& okay) {
        std::shared_lock lock(vector_mutex, std::defer_lock);
        if(lock.try_lock_for(std::chrono::milliseconds{50})) {
          if(index > entries.size()) {
            std::stringstream err;
            err << "Session " << index << " could not be found.";
            throw std::runtime_error(err.str());
          }
          auto& pos = entries[index];
          if(pos) {
            return okay(*pos);
          } else {
            std::stringstream err;
            err << "Session " << index << " could not be found.";
            throw std::runtime_error(err.str());
          }
        } else {
          throw std::runtime_error("Server is busy. Could not lookup session.");
        }
      }
      std::size_t create_session() {
        std::unique_lock lock(vector_mutex, std::defer_lock);
        if(lock.try_lock_for(std::chrono::milliseconds{50})) {
          auto ret = entries.size();
          entries.push_back(std::make_shared<SessionEntry>());
          return ret;
        } else {
          throw std::runtime_error("Server is busy. Could not create new session.");
        }
      }
    };

    class IndexPage : public http_resource {
    public:
      const std::shared_ptr<http_response> render(const http_request& request) {
        return std::shared_ptr<http_response>(new file_response("Web/index.html", 200, "text/html"));
      }
    };
    class QueryHandler : public http_resource {
    public:
      SessionVector sessions;
      const std::shared_ptr<http_response> render(const http_request& request) {
        auto content = request.get_content();
        try {
          auto j = nlohmann::json::parse(content);
          auto action = j.at("action").get<std::string>();
          if(action == "new_session") {
            auto session_id = sessions.create_session();
            nlohmann::json response{
              {"ok", true},
              {"session_id", session_id}
            };
            return std::shared_ptr<http_response>(new string_response(response.dump()));
          } else {
            nlohmann::json response;
            sessions.get_session(j.at("session_id").get<long>(),
              [&](SessionEntry& entry) {
                entry.modify(
                  [&](Session& session) {
                    auto amt = j.get<Request::Any>();
                    auto ret = session.respond(std::move(amt));
                    response = nlohmann::json{
                      {"ok", true},
                      {"state", ret}
                    };
                  }
                );
              }
            );
            return std::shared_ptr<http_response>(new string_response(response.dump()));
          }
        } catch(std::runtime_error const& err) {
          nlohmann::json response{
            {"ok", false},
            {"error", err.what()}
          };
          return std::shared_ptr<http_response>(new string_response(response.dump()));
        }
      }
    };
  }
  class ResourceDirectory : public http_resource {
  public:
      const std::shared_ptr<http_response> render(const http_request& req) {
          auto mime_type = [&](){
            if(req.get_path().ends_with("css")) {
              return "text/css";
            } else if(req.get_path().ends_with("js")) {
              return "text/javascript";
            } else {
              return "text/plain";
            }
          }();
          return std::shared_ptr<http_response>(new file_response("Web" + req.get_path(), 200, mime_type));
      }
  };
  void run_server() {
    webserver ws = create_webserver(8080)
      .log_error([](std::string const& error) { std::cout << "Error: " << error << "\n"; });

    IndexPage index;
    QueryHandler query;
    ResourceDirectory resource_directory;
    ws.register_resource("/", &index);
    ws.register_resource("/query", &query);
    ws.register_resource("/resources", &resource_directory, true);
    ws.start();
    std::cout << "Web server started...\n";
    { //wait for input to cin
      std::string x;
      std::cin >> x;
    }
    std::cout << "Stopping webserver...\n";
    ws.sweet_kill();
  }
}
*/
