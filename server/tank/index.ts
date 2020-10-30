import * as SocketIO from 'socket.io';
import Config from './config';

interface Position {
    x: number,
    y: number,
}

interface Tank {
    pos: Position,
    blood: number,
    name: string,
    can_safe_fire: boolean,
    dire: number,
    is_moving: boolean,
    socket: SocketIO.Socket,
}

interface Bullet {
    pos: Position,
    dire: number,
    level: number,
}

function get_random_position(): Position {
    let x = (Math.random() * 10000) % Config.space.width;
    let y = (Math.random() * 10000) % Config.space.height;
    return { x: x, y: y };
}

function get_random_direction(): number {
    return (Math.random() * 10000) % 360;
}

function create_tank(socket: SocketIO.Socket): Tank {
    return {
        pos: get_random_position(),
        blood: 1.0,
        name: "<unnamed> " + socket.id,
        can_safe_fire: true,
        dire: get_random_direction(),
        is_moving: false,
        socket: socket,
    };
}

function create_bullet(tank: Tank, level: number) {
    bullets.push({
        pos: {
            x: tank.pos.x,
            y: tank.pos.y,
        },
        dire: tank.dire,
        level: level,
    });
}

function check_crash_bullet(bullet: Bullet) {
    for (let id in tanks) {
        let this_tank = tanks[id];
        if (bullet.pos.x > this_tank.pos.x - Config.tanks.size / 2
            && bullet.pos.x < this_tank.pos.x + Config.tanks.size / 2
            && bullet.pos.y > this_tank.pos.y - Config.tanks.size / 2
            && bullet.pos.y < this_tank.pos.y + Config.tanks.size / 2) {
            this_tank.blood -= Config.bullet.damage[bullet.level];
            return true;
        }
    }

    return false;
}

function check_outof_space(pos: Position) {
    if (pos.x < 0
        || pos.y < 0
        || pos.x > Config.space.width
        || pos.y > Config.space.height) {
        return true;
    }
    return false;
}

const io = new SocketIO();
let bullets = new Array<Bullet>();
let tanks = new Array<Tank>();

io.on('connection', (socket: SocketIO.Socket) => {
    let this_tank = create_tank(socket);
    tanks[socket.id] = this_tank;

    socket.on('disconnect', () => {
        console.log("One tank disconnected");
    });

    socket.on('turn', (target: number) => {
        let once_update: number = Config.game.update / Config.tanks.turn_speed;
        if (target - this_tank.dire < 0) {
            once_update *= -1;
        }

        let iid = setInterval(() => {
            if (this_tank.dire == target) {
                clearInterval(iid);
                return;
            }

            if (Math.abs(target - this_tank.dire) < Math.abs(once_update)) {
                this_tank.dire = target;
            } else {
                this_tank.dire += once_update;
            }
        }, Config.game.update);
    });

    socket.on('fire', (level: number) => {
        if (!this_tank.can_safe_fire) {
            this_tank.blood -= Config.tanks.fire_too_much_damage;
            return;
        }

        this_tank.can_safe_fire = false;
        setTimeout(() => { this_tank.can_safe_fire = true; }, Config.tanks.fire_speed[level]);
        create_bullet(this_tank, level);
    });

    socket.on('move', (state: boolean) => {
        this_tank.is_moving = state;
    });
});


setInterval(() => {
    for (let id in bullets) {
        let this_bullet = bullets[id];

        this_bullet.pos.x += Config.bullet.speed * Math.cos(this_bullet.dire);
        this_bullet.pos.y += Config.bullet.speed * Math.sin(this_bullet.dire);
        if (check_crash_bullet(this_bullet) || check_outof_space(this_bullet.pos)) {
            bullets[id] = undefined;
        }
    }

    for (let id in tanks) {
        let this_tank = tanks[id];
        if (this_tank.blood <= 0) {
            this_tank.socket.disconnect();
            tanks[id] = undefined;
            continue;
        }

        if (!this_tank.is_moving) {
            continue;
        }

        this_tank.pos.x += Config.tanks.max_speed * Math.cos(this_tank.dire);
        this_tank.pos.y += Config.tanks.max_speed * Math.sin(this_tank.dire);

        if (check_outof_space(this_tank.pos)) {
            this_tank.blood -= Config.tanks.crash_damage;

            this_tank.pos.x = Math.min(Config.space.width, this_tank.pos.x);
            this_tank.pos.y = Math.min(Config.space.width, this_tank.pos.y);

            this_tank.pos.x = Math.max(0, this_tank.pos.x);
            this_tank.pos.y = Math.max(0, this_tank.pos.y);
        }
    }

    io.emit("update", { tanks: tanks, bullets: bullets });
}, Config.game.update);

io.listen(3000);
console.log("Server started.");
