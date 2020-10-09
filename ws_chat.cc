#include <regex>
#include <set>
#include <sstream>
#include <fmt/core.h>
#include <seasocks/PageHandler.h>
#include <seasocks/PrintfLogger.h>
#include <seasocks/Server.h>
#include <seasocks/StringUtil.h>
#include <seasocks/WebSocket.h>
#include <seasocks/util/Json.h>

auto str_replace_all(std::string& source, std::string f, std::string t) {
    std::string res;
    std::size_t match = 0;
    for (auto i = 0; i < source.size(); ++i) {
        if (source[i] == f[match]) {
            match += 1;
            if (match == f.size()) {
                res += t;
                match = 0;
            }
        } else {
            for (auto j = i - match; j <= i; ++j) {
                res += source[i];
            }
            match = 0;
        }
    }

    for (auto i = source.size() - match; i < source.size(); ++i) {
        res += source[i];
    }

    source = res;
}

bool str_start_with(std::string source, std::string x) {
    if (source.size() < x.size())
        return false;
    return source.substr(0, x.size()) == x;
}

bool str_end_with(std::string source, std::string x) {
    if (source.size() < x.size())
        return false;
    return source.substr(source.size() - x.size(), x.size()) == x;
}

std::set<std::string> online_users;

std::string get_username(std::string name) {
    name = name.substr(0, std::min(name.size(), 80UL));

    str_replace_all(name, "*", "+");
    str_replace_all(name, "@sudo", "**");
    str_replace_all(name, " ", "_");
    str_replace_all(name, "@", "#");

    bool flag = false;
    for (auto x : online_users) {
        if (x == name)
            flag = true;
    }

    if (flag)
        return "";
    else
        return name;
}

std::string format_msg(std::string msg, std::string from, bool is_private = false) {
    std::stringstream ss;
    ss << "{";
    seasocks::jsonKeyPairToStream(ss, "from", from);
    ss << ",";
    seasocks::jsonKeyPairToStream(ss, "msg", msg);
    ss << ",";
    seasocks::jsonKeyPairToStream(ss, "is_private", is_private);
    ss << ",";
    seasocks::jsonKeyPairToStream(ss, "time", time(nullptr));
    ss << "}";

    std::string res;
    std::getline(ss, res);
    return res;
}

bool user_match(std::string expr, std::string username, std::string from) {
    if (expr == "@a")
        return true;

    if (str_start_with(expr, "@r")) {
        expr = expr.substr(3);
        expr.pop_back();
        std::regex r(expr);
        return std::regex_match(username, r);
    }

    if (str_start_with(expr, "@i")) {
        return username == from;
    }

    if (str_start_with(expr, "@o")) {
        return username != from;
    }

    if (str_start_with(expr, "@p")) {
        return rand() % 3 == 0;
    }

    if (expr == username)
        return true;

    return false;
}

namespace cmd {

std::vector<std::string> disabled_commands;

std::string rename(const std::shared_ptr<seasocks::Credentials>& source, const std::string& new_name) {
    std::string username = new_name;
    if (str_end_with(username, "@sudo"))
        source->attributes["is_admin"] = "yes";
    else
        source->attributes["is_admin"] = "no";

    username = get_username(username);
    if (username == "") {
        return "Username not acceptable.";
    }
    online_users.erase(source->username);
    source->username = username;
    online_users.insert(source->username);
    return fmt::format("Login Success. Now you are {}.", username);
}

void send_message_cb(seasocks::WebSocket* s, const std::string& msg) {
    s->send(format_msg(msg, "Command Block"));
}

void send_message_sys(seasocks::WebSocket* s, const std::string& msg) {
    s->send(format_msg(msg, "System"));
}

void send_message_cmd_disabled(seasocks::WebSocket* s) {
    send_message_cb(s, "This command has been disabled.");
}

void send_message_cmd_not_perm(seasocks::WebSocket* s) {
    send_message_cb(s, "You have no permission to preform this action.");
}

std::string private_msg(const std::string& from, const std::string& expr, const std::string& msg,
                        const std::set<seasocks::WebSocket*>& all_socks) {
    for (auto x : all_socks) {
        if (user_match(expr, x->credentials()->username, from)) {
            x->send(format_msg(msg, from, true));
        }
    }

    return fmt::format("Private message sent. Content:\n{}.", msg);
}

std::string kill_user(const std::string& from, const std::string& expr, const std::set<seasocks::WebSocket*>& all_socks) {
    std::string killed_users = "[";
    for (auto x : all_socks) {
        if (user_match(expr, x->credentials()->username, from)) {
            send_message_sys(x, "You are killed.");
            killed_users += x->credentials()->username + ",";
            x->close();
        }
    }

    if (killed_users.size() > 1)
        killed_users.pop_back();

    killed_users.push_back(']');
    return fmt::format("User {} were killed.", killed_users);
}

std::string make_user_silence(const std::string& from, const std::string& expr, const std::set<seasocks::WebSocket*>& all_socks) {
    std::string matched_users = "[";
    for (auto x : all_socks) {
        if (user_match(expr, x->credentials()->username, from)) {
            x->credentials()->attributes["can_talk"] = "no";
            send_message_sys(x, "You could no longer talk or run command.");
            matched_users += x->credentials()->username + ",";
        }
    }

    if (matched_users.size() > 1)
        matched_users.pop_back();

    matched_users.push_back(']');
    return fmt::format("User {} now couldn't not say another word more.", matched_users);
}

std::string list_online_user(const std::string& from, const std::string& expr) {
    std::string matched_users = "[";

    for (auto x : online_users) {
        if (user_match(expr, x, from)) {
            matched_users += x + ",";
        }
    }

    if (matched_users.size() > 1)
        matched_users.pop_back();

    matched_users.push_back(']');
    return fmt::format("Now user {} are online.", matched_users);
}

bool check_command(const std::string& cmd) {
    auto p = find(disabled_commands.begin(), disabled_commands.end(), cmd);
    if (p != disabled_commands.end())
        return false;
    return true;
}

std::string enable_command(const std::string& cmd) {
    auto p = find(disabled_commands.begin(), disabled_commands.end(), cmd);
    if (p != disabled_commands.end())
        disabled_commands.erase(p);

    return fmt::format("Command {} enabled.", cmd);
}

std::string disable_command(const std::string& cmd) {
    auto p = find(disabled_commands.begin(), disabled_commands.end(), cmd);
    if (p == disabled_commands.end())
        disabled_commands.push_back(cmd);

    return fmt::format("Command {} disabled.", cmd);
}

void run_command(const std::string& c, seasocks::WebSocket* s, const std::set<seasocks::WebSocket*>& all_socks) {
    std::string from = s->credentials()->username;
    std::stringstream ss;
    ss << c;
    std::string command_mark;
    ss >> command_mark;
    if (command_mark == "/rename") {
        if (!check_command("rename")) {
            send_message_cmd_disabled(s);
            return;
        }

        std::string new_name;
        ss.get();
        std::getline(ss, new_name);
        send_message_cb(s, cmd::rename(s->credentials(), new_name));
    }

    if (command_mark == "/disconnect") {
        if (!check_command("disconnect")) {
            send_message_cmd_disabled(s);
            return;
        }

        s->close();
        return;
    }

    if (command_mark == "/msg") {
        if (!check_command("msg")) {
            send_message_cmd_disabled(s);
            return;
        }

        std::string to, msg;
        ss >> to;
        ss.get();
        std::getline(ss, msg);
        msg = msg.substr(1, msg.size() - 2);
        send_message_cb(s, cmd::private_msg(from, to, msg, all_socks));
        return;
    }

    if (command_mark == "/list") {
        if (!check_command("list")) {
            send_message_cmd_disabled(s);
            return;
        }

        std::string target;
        ss >> target;
        if (target == "")
            target = "@a";

        send_message_cb(s, cmd::list_online_user(from, target));
    }

    if (command_mark == "/kill") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        if (!check_command("kill")) {
            send_message_cmd_disabled(s);
            return;
        }

        std::string target;
        ss >> target;
        send_message_cb(s, cmd::kill_user(from, target, all_socks));
        return;
    }

    if (command_mark == "/silence") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        if (!check_command("silence")) {
            send_message_cmd_disabled(s);
            return;
        }

        std::string target;
        ss >> target;
        send_message_cb(s, cmd::make_user_silence(from, target, all_socks));
        return;
    }

    if (command_mark == "/enable") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        std::string target;
        ss >> target;
        send_message_cb(s, cmd::enable_command(target));
        return;
    }

    if (command_mark == "/disable") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        std::string target;
        ss >> target;
        send_message_cb(s, cmd::disable_command(target));
        return;
    }
}

} // namespace cmd

class ChatHandler : public seasocks::WebSocket::Handler {
public:
    void onConnect(seasocks::WebSocket* s) override {
        _connections.insert(s);
        online_users.insert(s->credentials()->username);
    }

    void onData(seasocks::WebSocket* s, const char* data) override {
        if (data[0] == '\0')
            return;

        if (s->credentials()->attributes["can_talk"] == "no")
            return;

        if (data[0] != '/') {
            this->_say(data, s->credentials()->username);
        } else {
            cmd::run_command(data, s, this->_connections);
        }
    }

    void onDisconnect(seasocks::WebSocket* s) override {
        _connections.erase(s);
        online_users.erase(s->credentials()->username);
        this->_say(fmt::format("User {} left the chatroom.", s->credentials()->username), "System");
    }

private:
    std::set<seasocks::WebSocket*> _connections;

    void _say(std::string msg, std::string from) {
        std::string fmsg = format_msg(msg, from);
        for (auto c : this->_connections) {
            c->send(fmsg);
        }
    }
};

struct ChatroomAuthHandler : seasocks::PageHandler {
    std::shared_ptr<seasocks::Response> handle(const seasocks::Request& request) {
        request.credentials()->username = seasocks::formatAddress(request.getRemoteAddress());
        request.credentials()->attributes["can_talk"] = "yes";
        request.credentials()->attributes["is_admin"] = "no";
        return seasocks::Response::unhandled();
    }
};

struct NotFoundHandler : seasocks::PageHandler {
    std::shared_ptr<seasocks::Response> handle(const seasocks::Request& request) {
        if (request.getRequestUri() != "/chat") {
            return seasocks::Response::textResponse("Server Status: Online");
        }
        return seasocks::Response::unhandled();
    }
};

int main() {
    seasocks::Server server(std::make_shared<seasocks::PrintfLogger>(seasocks::Logger::Level::Warning));
    server.addPageHandler(std::make_shared<ChatroomAuthHandler>());
    server.addPageHandler(std::make_shared<NotFoundHandler>());
    server.addWebSocketHandler("/chat", std::make_shared<ChatHandler>(), true);
    server.startListening(8010);
    server.loop();
    return 0;
}