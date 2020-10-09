let ws;
let username;
let time_stamp = 0;
let ip = new String("ws://172.32.5.177:8010/chat");

function clear_message() {
    $("#message").empty();
}

function clear_send() {
    $("#send").val("");
}

function format_time(time) {
    let res;
    res = (time.getMonth() + 1) + "-" + time.getDate() + " " + time.getHours() + ":" + time.getMinutes() + ":" + time.getSeconds();
    return res;
}

if (String.prototype.replaceAll === undefined) {
    String.prototype.replaceAll = function (before, after) {
        let res = new String();
        let matched = 0;
        for (let i = 0; i < this.length; i += 1) {
            if (this[i] === before[matched]) {
                matched += 1;
                if (matched === before.length) {
                    res += after;
                    matched = 0;
                }
            } else {
                for (let j = i - matched; j <= i; j += 1) {
                    res += this[j];
                }
                matched = 0;

            }
        }

        for (let i = this.length - matched; i < this.length; ++i) {
            res += this[i];
        }

        return res;
    };
}

function write_message(data) {
    let message = DOMPurify.sanitize(data.msg);

    $("#message").append(
        "<div class='msg'>" +

        "<span class='msg-from " +
        (data.from == "INFO" ? "msg-from-info " : "") +
        (data.from == "Command Block" ? "msg-from-cb " : "") +
        (data.is_private ? "msg-private " : "") +
        "'>" +

        data.from +
        "<span class='msg-time'> " +
        (data.time == -1 ? "" : format_time(new Date(data.time * 1000))) +
        "</span>" +
        "</span>" +
        "<br>" +
        "<span class='msg-content'>" +
        message +
        "</span>" +

        "</div>"
    );
    scroll_to_bottom();
}

function init() {
    clear_message();
    login();
    ws = new WebSocket(ip);

    ws.onopen = function () {
        write_message({
            from: "INFO",
            msg: "Connected.",
            is_private: true,
            time: -1
        });
        ws.send("/rename " + username);
    };

    ws.onmessage = function (evt) {
        let data = JSON.parse(evt.data);
        write_message(data);
    };

    ws.onclose = function () {
        write_message({
            from: "INFO",
            msg: "Disconnected.",
            is_private: true,
            time: -1
        });
    };
}

function login() {
    let input_ip = window.prompt("[Connect] Input server");
    if (input_ip !== null) {
        ip = input_ip;
    }
    username = window.prompt("[Login] Input your name");
}

function send() {
    let data = $("#send").val();
    if (data == "/clear" || data == "/cls") {
        clear_message();
        clear_send();
        return;
    }

    if (ws.readyState == 1) {
        if (data[0] === '/') {
            ws.send(data);
        } else {
            ws.send("<pre>" + data + "</pre>");
        }
    } else {
        write_message({
            from: "INFO",
            msg: "Unknown error while sending message.",
            is_private: true,
            time: -1
        });
    }

    clear_send();
}

function scroll_to_bottom() {
    if ($("#scroll-option").is(":checked")) {
        document.getElementById("message").scrollTop = document.getElementById("message").scrollHeight;
    }
}

$(document).ready(
    function () {
        document.onkeydown = function () {
            let key_event = window.event;
            if (key_event.keyCode == 13 && key_event.ctrlKey) {
                send();
            }
        }
    }
);