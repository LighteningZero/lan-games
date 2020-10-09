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

using namespace std;
using namespace seasocks;

auto str_replace_all(string& source, string f, string t) {
    string res;
    size_t match = 0;
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

bool str_start_with(string source, string x) {
    if (source.size() < x.size())
        return false;
    return source.substr(0, x.size()) == x;
}

bool str_end_with(string source, string x) {
    if (source.size() < x.size())
        return false;
    return source.substr(source.size() - x.size(), x.size()) == x;
}

set<string> online_users;

string get_username(string name) {
    name = name.substr(0, min(name.size(), 80UL));

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

string format_msg(string msg, string from, bool is_private = false) {
    stringstream ss;
    ss << "{";
    jsonKeyPairToStream(ss, "from", from);
    ss << ",";
    jsonKeyPairToStream(ss, "msg", msg);
    ss << ",";
    jsonKeyPairToStream(ss, "is_private", is_private);
    ss << ",";
    jsonKeyPairToStream(ss, "time", time(nullptr));
    ss << "}";

    string res;
    getline(ss, res);
    return res;
}

bool user_match(string expr, string username, string from) {
    if (expr == "@a")
        return true;

    if (str_start_with(expr, "@r")) {
        expr = expr.substr(3);
        expr.pop_back();
        regex r(expr);
        return regex_match(username, r);
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

vector<string> disabled_commands;

string rename(const shared_ptr<Credentials>& source, const string& new_name) {
    string username = new_name;
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

void send_message_cb(WebSocket* s, const string& msg) {
    s->send(format_msg(msg, "Command Block"));
}

void send_message_sys(WebSocket* s, const string& msg) {
    s->send(format_msg(msg, "System"));
}

void send_message_cmd_disabled(WebSocket* s) {
    s->send(format_msg("This command has been disabled.", "System"));
}

void send_message_cmd_not_perm(WebSocket* s) {
    s->send(format_msg("You have no permission to preform this action.", "System"));
}

string private_msg(const string& from, const string& expr, const string& msg, const set<WebSocket*>& all_socks) {
    for (auto x : all_socks) {
        if (user_match(expr, x->credentials()->username, from)) {
            x->send(format_msg(msg, from, true));
        }
    }

    return fmt::format("Private message sent. Content:\n{}.", msg);
}

string kill_user(const string& from, const string& expr, const set<WebSocket*>& all_socks) {
    string killed_users = "[";
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

string make_user_silence(const string& from, const string& expr, const set<WebSocket*>& all_socks) {
    string matched_users = "[";
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

string list_online_user(const string& from, const string& expr) {
    string matched_users = "[";

    for (auto x : online_users) {
        if (user_match(expr, x, from)) {
            matched_users += x + ",";
        }
    }

    if (matched_users.size() > 1)
        matched_users.pop_back();

    matched_users.push_back(']');
    return fmt::format("Now user {} are on line.", matched_users);
}

bool check_command(const string& cmd) {
    auto p = find(disabled_commands.begin(), disabled_commands.end(), cmd);
    if (p != disabled_commands.end())
        return false;
    return true;
}

string enable_command(const string& cmd) {
    auto p = find(disabled_commands.begin(), disabled_commands.end(), cmd);
    if (p != disabled_commands.end())
        disabled_commands.erase(p);

    return fmt::format("Command {} enabled.", cmd);
}

string disable_command(const string& cmd) {
    auto p = find(disabled_commands.begin(), disabled_commands.end(), cmd);
    if (p == disabled_commands.end())
        disabled_commands.push_back(cmd);

    return fmt::format("Command {} disabled.", cmd);
}

void run_command(const string& c, WebSocket* s, const set<WebSocket*>& all_socks) {
    string from = s->credentials()->username;
    stringstream ss;
    ss << c;
    string command_mark;
    ss >> command_mark;
    if (command_mark == "/rename") {
        if (!check_command("rename")) {
            send_message_cmd_disabled(s);
            return;
        }

        string new_name;
        ss.get();
        getline(ss, new_name);
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

        string to, msg;
        ss >> to;
        ss.get();
        getline(ss, msg);
        msg = msg.substr(1, msg.size() - 2);
        send_message_cb(s, cmd::private_msg(from, to, msg, all_socks));
        return;
    }

    if (command_mark == "/list") {
        if (!check_command("list")) {
            send_message_cmd_disabled(s);
            return;
        }

        string target;
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

        string target;
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

        string target;
        ss >> target;
        send_message_cb(s, cmd::make_user_silence(from, target, all_socks));
        return;
    }

    if (command_mark == "/enable") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        string target;
        ss >> target;
        send_message_cb(s, cmd::enable_command(target));
        return;
    }

    if (command_mark == "/disable") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            send_message_cmd_not_perm(s);
            return;
        }

        string target;
        ss >> target;
        send_message_cb(s, cmd::disable_command(target));
        return;
    }
}

} // namespace cmd

class ChatHandler : public WebSocket::Handler {
public:
    void onConnect(WebSocket* s) override {
        _connections.insert(s);
        online_users.insert(s->credentials()->username);
    }

    void onData(WebSocket* s, const char* data) override {
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

    void onDisconnect(WebSocket* s) override {
        _connections.erase(s);
        online_users.erase(s->credentials()->username);
        this->_say(fmt::format("User {} left the chatroom.", s->credentials()->username), "System");
    }

private:
    set<WebSocket*> _connections;

    void _say(string msg, string from) {
        string fmsg = format_msg(msg, from);
        for (auto c : this->_connections) {
            c->send(fmsg);
        }
    }
};

struct ChatroomAuthHandler : PageHandler {
    std::shared_ptr<Response> handle(const Request& request) {
        request.credentials()->username = formatAddress(request.getRemoteAddress());
        request.credentials()->attributes["can_talk"] = "yes";
        request.credentials()->attributes["is_admin"] = "no";
        return Response::unhandled();
    }
};

struct NotFoundHandler : PageHandler {
    std::shared_ptr<Response> handle(const Request& request) {
        if (request.getRequestUri() != "/chat") {
            return Response::textResponse("Server Status: Online");
        }
        return Response::unhandled();
    }
};

int main() {
    Server server(make_shared<PrintfLogger>(Logger::Level::Warning));
    server.addPageHandler(make_shared<ChatroomAuthHandler>());
    server.addPageHandler(make_shared<NotFoundHandler>());
    server.addWebSocketHandler("/chat", make_shared<ChatHandler>(), true);
    server.startListening(8010);
    server.loop();
    return 0;
}