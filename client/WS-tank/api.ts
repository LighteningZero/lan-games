import Tank from '../../shared/tank/tanks';
import Config from '../../shared/tank/config';
import 'socket.io-client';

let socket: SocketIOClient.Socket;
let tanks: Array<Tank>;

class tank {
    private this_tank: Tank
    on_scan_callback: Function

    constructor(this_tank: Tank) {
        this.this_tank = this_tank;
        this.on_scan_callback = () => { this.do_nothing(); };
    }

    get_x() {
        return this.this_tank.pos.x;
    }

    get_y() {
        return this.this_tank.pos.y;
    }

    get_pos() {
        return this.this_tank.pos;
    }

    get_direction() {
        return this.this_tank.tank_dire;
    }

    get_gun_direction() {
        return this.this_tank.gun_dire;
    }

    get_radar_direction() {
        return this.this_tank.radar_dire;
    }

    can_fire() {
        return this.this_tank.can_safe_fire;
    }

    fire(level: number) {
        socket.emit('fire', level - 1);
    }

    turn_to(target: number) {
        socket.emit('turn-tank', target);
    }

    turn_gun_to(target: number) {
        socket.emit('turn-gun', target);
    }

    turn_radar_to(target: number) {
        socket.emit('turn-radar', target);
    }

    move(): boolean {
        if (this.this_tank.is_moving) { return false; }
        socket.emit("move", true);
        return true;
    }

    stop(): boolean {
        if (!this.this_tank.is_moving) { return false; }
        socket.emit('move', false);
        return false;
    }

    on_scan(callback: Function): void {
        this.on_scan_callback = callback;
    }

    scan(): void {
        let upper_slope = get_line_slope(this.this_tank.radar_dire + Config.tanks.radar_size);
        let lower_slope = get_line_slope(this.this_tank.radar_dire - Config.tanks.radar_size);
        for (let id in tanks) {
            let element = tanks[id];
            if (element.id == this.this_tank.id) { return; }

            let target_slope = element.pos.x / element.pos.y;
            if (lower_slope >= target_slope && upper_slope <= target_slope) {
                // scanned
                this.on_scan_callback({
                    x: element.pos.x,
                    y: element.pos.y,
                    name: element.name,
                });
            }
        };
    }

    do_nothing() { }

    get_config() {
        return JSON.parse(JSON.stringify(Config));
    }

    set_name(name: string) {
        socket.emit('set-name', name);
    }

    loop(callback: Function): void {
        setInterval(() => {
            callback();
        }, Config.game.update);
    }
}

let t: tank;

// covert angle to radian
function covert_degree(x: number): number {
    return x * Math.PI / 180;
}

function get_line_slope(d: number): number {
    return 1 / Math.tan(covert_degree(d));
}

function scan_tanks() {
    if (t === undefined) { return; }
    t.scan();
}

function start_code(parsed_code: Function, _tanks: Tank[], _socket: SocketIOClient.Socket) {
    tanks = _tanks;
    socket = _socket;

    let this_tank = tanks[socket.id];
    t = new tank(this_tank);
    parsed_code(t);
}

export {
    start_code,
    scan_tanks
};