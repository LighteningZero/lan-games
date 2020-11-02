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
        let radar_angle = this.this_tank.radar_dire;
        let upper_slope = get_line_slope(radar_angle + Config.tanks.radar_size);
        let lower_slope = get_line_slope(radar_angle - Config.tanks.radar_size);
        for (let id in tanks) {
            let element = tanks[id];
            if (element.id == this.this_tank.id) { continue; }

            let target_slope = (this.this_tank.pos.x - element.pos.x) / (this.this_tank.pos.y - element.pos.y);
            if (upper_slope <= target_slope && target_slope <= lower_slope) {
                console.log(upper_slope, lower_slope, target_slope);
                // scanned
                this.on_scan_callback({
                    x: element.pos.x,
                    y: element.pos.y,
                    name: element.name,
                }, radar_angle);
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

    _update() {
        this.this_tank = tanks[socket.id];
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

function update_tanks(all_tanks: Tank[]) {
    tanks = all_tanks;
    if (t === undefined) { return; }
    t._update();
    t.scan();
}

function start_code(parsed_code: Function, _tanks: Tank[], _socket: SocketIOClient.Socket) {
    tanks = _tanks;
    socket = _socket;

    let this_tank = tanks[socket.id];
    t = new tank(this_tank);
    let costum_var = {};
    parsed_code(t, costum_var);
}

export {
    start_code,
    update_tanks
};
