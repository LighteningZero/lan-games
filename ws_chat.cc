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
    time_t now = time(nullptr);
    char* now_time = new char[128];
    strftime(now_time, 128, "%m-%d %H:%M:%S", localtime(&now));
    return fmt::format("[{} @ {}{}] {}", from, now_time, is_private ? " (PRIVATE)" : "", msg);
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

string private_msg(const string& from, const string& expr, const string& msg, const set<WebSocket*>& all_socks) {
    for (auto x : all_socks) {
        if (user_match(expr, x->credentials()->username, from)) {
            x->send(format_msg(msg, from, true));
        }
    }

    return "Private message sent.";
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

void run_command(const string& c, WebSocket* s, const set<WebSocket*>& all_socks) {
    string from = s->credentials()->username;
    stringstream ss;
    ss << c;
    string command_mark;
    ss >> command_mark;
    if (command_mark == "/rename") {
        string new_name;
        ss.get();
        getline(ss, new_name);
        send_message_cb(s, cmd::rename(s->credentials(), new_name));
    }

    if (command_mark == "/disconnect") {
        s->close();
        return;
    }

    if (command_mark == "/msg") {
        string to, msg;
        ss >> to;
        ss.get();
        getline(ss, msg);
        msg = msg.substr(1, msg.size() - 2);
        send_message_cb(s, cmd::private_msg(from, to, msg, all_socks));
        return;
    }

    if (command_mark == "/list") {
        string target;
        ss >> target;
        if (target == "")
            target = "@a";

        send_message_cb(s, cmd::list_online_user(from, target));
    }

    if (command_mark == "/kill") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            s->send(format_msg("You have no permission to preform this action.", "Command Block"));
            return;
        }

        string target;
        ss >> target;
        send_message_cb(s, cmd::kill_user(from, target, all_socks));
        return;
    }

    if (command_mark == "/silence") {
        if (s->credentials()->attributes["is_admin"] != "yes") {
            s->send(format_msg("You have no permission to preform this action.", "Command Block"));
            return;
        }

        string target;
        ss >> target;
        send_message_cb(s, cmd::make_user_silence(from, target, all_socks));
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
        cmd::send_message_sys(s, fmt::format("User {} left the chatroom.", s->credentials()->username));
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