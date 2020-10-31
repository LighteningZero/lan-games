import Bullet from "../../shared/tank/bullet";
import Config from "../../shared/tank/config";
import Tank from "../../shared/tank/tanks";

export default function (tanks: Tank[], bullets: Bullet[], socket) {
    const canvas = <HTMLCanvasElement>document.getElementById('space');
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = "#fff";
    ctx.fillRect(0, 0, Config.space.width, Config.space.height);

    ctx.fillStyle = "brown";
    for (let id in bullets) {
        let this_bullet = bullets[id];
        ctx.fillRect(this_bullet.pos.x, this_bullet.pos.y, 5, 5);
    }

    for (let id in tanks) {
        let this_tank = tanks[id];
        if (id === socket.id) { ctx.fillStyle = "green"; }
        else { ctx.fillStyle = "orange"; }

        ctx.fillRect(this_tank.pos.x - Config.tanks.size / 2,
            this_tank.pos.y - Config.tanks.size / 2,
            Config.tanks.size,
            Config.tanks.size);

        ctx.fillStyle = "black";
        ctx.fillText(Math.ceil(this_tank.blood * 100).toString() + '%', this_tank.pos.x, this_tank.pos.y);
    }
};