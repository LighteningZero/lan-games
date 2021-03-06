#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <fmt/core.h>
#include <seasocks/PageHandler.h>
#include <seasocks/PrintfLogger.h>
#include <seasocks/Server.h>
#include <seasocks/StringUtil.h>
#include <seasocks/WebSocket.h>
#include <seasocks/util/Json.h>

auto str_replace_all(std::string& source, const std::string& f, const std::string& t) {
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

bool str_start_with(const std::string& source, const std::string& x) {
    if (source.size() < x.size())
        return false;
    return source.substr(0, x.size()) == x;
}

bool str_end_with(const std::string& source, const std::string& x) {
    if (source.size() < x.size())
        return false;
    return source.substr(source.size() - x.size(), x.size()) == x;
}

std::set<std::string> online_users;
std::string admin_suffix;
int port;

std::string get_username(std::string name) {
    name = name.substr(0, std::min(name.size(), 80UL));

    str_replace_all(name, "*", "+");
    str_replace_all(name, admin_suffix, "**");
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

std::string format_msg(const std::string& msg, const std::string& from, bool is_private = false) {
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

bool user_match(std::string expr, const std::string& username, const std::string& from) {
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

void send_msg(const std::string& msg, const std::string& from, const std::set<seasocks::WebSocket*>& all_socks) {
    for (auto x : all_socks)
        x->send(format_msg(msg, from, false));
}

namespace cmd {

std::vector<std::string> disabled_commands;

std::string rename(const std::shared_ptr<seasocks::Credentials>& source, const std::string& new_name) {
    std::string username = new_name;
    if (str_end_with(username, admin_suffix))
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
    return fmt::format("Login success. Now your name is '{}'", username);
}

void send_message_cb(seasocks::WebSocket* s, const std::string& msg) {
    s->send(format_msg(msg, "Command Block"));
}

void send_message_sys(seasocks::WebSocket* s, const std::string& msg) {
    s->send(format_msg(msg, "System Message"));
}

void send_message_cmd_disabled(seasocks::WebSocket* s) {
    send_message_cb(s, "This command has been disabled.");
}

void send_message_cmd_not_perm(seasocks::WebSocket* s) {
    send_message_cb(s, "You have no permission to preform this action.");
}

void send_message_cmd_res(seasocks::WebSocket* s, const std::string& command, const std::string& res) {
    send_message_cb(s, fmt::format(">> {}<br><- {}", command, res));
}

std::string private_msg(const std::string& from, const std::string& expr, const std::string& msg,
                        const std::set<seasocks::WebSocket*>& all_socks) {
    for (auto x : all_socks) {
        if (user_match(expr, x->credentials()->username, from)) {
            x->send(format_msg(msg, from, true));
        }
    }

    return "Sent";
}

std::string kill_user(const std::string& from, const std::string& expr,
                      const std::set<seasocks::WebSocket*>& all_socks) {
    std::string killed_users = "[";
    for (auto x : all_socks) {
        if (user_match(expr, x->credentials()->username, from)) {
            send_message_sys(x, "You were killed.");
            killed_users += x->credentials()->username + ",";
            x->close();
        }
    }

    if (killed_users.size() > 1)
        killed_users.pop_back();

    killed_users.push_back(']');
    return killed_users;
}

std::string make_user_silence(const std::string& from, const std::string& expr,
                              const std::set<seasocks::WebSocket*>& all_socks) {
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
    return matched_users;
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
    return matched_users;
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

    return "Enabled.";
}

std::string disable_command(const std::string& cmd) {
    auto p = find(disabled_commands.begin(), disabled_commands.end(), cmd);
    if (p == disabled_commands.end())
        disabled_commands.push_back(cmd);

    return "Disabled.";
}

std::string set_user_attributes(const std::string& from, const std::string& expr, const std::string& key,
                                const std::string& val, const std::set<seasocks::WebSocket*>& all_socks) {
    std::string matched_users("[");
    for (auto s : all_socks) {
        if (user_match(expr, s->credentials()->username, from)) {
            s->credentials()->attributes[key] = val;
            matched_users += s->credentials()->username + ",";
        }
    }

    if (matched_users.size() > 1)
        matched_users.pop_back();

    matched_users.push_back(']');
    return matched_users;
}

void run_command(const std::string& c, seasocks::WebSocket* s, const std::set<seasocks::WebSocket*>& all_socks);

std::string run_script(seasocks::WebSocket* s, const std::set<seasocks::WebSocket*>& all_socks,
                       std::stringstream& ss) {
    std::string now_cmd, res;
    res = "[";
    while (ss.eof()) {
        getline(ss, now_cmd);
        run_command(now_cmd, s, all_socks);
        if (s->verb() == seasocks::Request::Verb::Invalid)
            return "";
    }

    if (res.size() > 1)
        res.pop_back();

    res.push_back(']');
    return res;
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
        send_message_cmd_res(s, c, cmd::private_msg(from, to, msg, all_socks));
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

        send_message_cmd_res(s, c, cmd::list_online_user(from, target));
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
        send_message_cmd_res(s, c, cmd::kill_user(from, target, all_socks));
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
        send_message_cmd_res(s, c, cmd::make_user_silence(from, target, all_socks));
        return;
    }

    if (command_mark == "/enable") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        std::string target;
        ss >> target;
        send_message_cmd_res(s, c, cmd::enable_command(target));
        return;
    }

    if (command_mark == "/disable") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        std::string target;
        ss >> target;

        if (target == "disable") {
            send_message_cmd_res(s, c, "Can not disable /disable");
            return;
        }

        send_message_cmd_res(s, c, cmd::disable_command(target));
        return;
    }

    if (command_mark == "/anon") {
        if (!check_command("list")) {
            send_message_cmd_disabled(s);
            return;
        }

        std::string msg;
        ss.get();
        std::getline(ss, msg);
        msg = msg.substr(1, msg.size() - 2);
        send_msg(msg, "Anonymous User", all_socks);
    }

    if (command_mark == "/set") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        if (!check_command("set")) {
            send_message_cmd_disabled(s);
            return;
        }

        std::string expr, key, val;
        ss >> expr >> key >> val;
        send_message_cmd_res(s, c, set_user_attributes(from, expr, key, val, all_socks));
    }

    if (command_mark == "/script") {
        if (!check_command("script")) {
            send_message_cmd_disabled(s);
            return;
        }

        std::string res = cmd::run_script(s, all_socks, ss);
        if (res == "") return;
        send_message_cmd_res(s, c, res);
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
        this->_say(fmt::format("User {} left the chatroom.", s->credentials()->username), "System Message");
    }

private:
    std::set<seasocks::WebSocket*> _connections;

    void _say(const std::string& msg, const std::string& from) {
        send_msg(msg, from, this->_connections);
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
    std::cout << "Port and admin suffix: " << std::endl;
    std::cin >> port;
    std::cin >> admin_suffix;
    server.startListening(port);
    server.setStaticPath("web");
    std::cout << "Starting Server.." << std::endl;
    server.loop();
    return 0;
}