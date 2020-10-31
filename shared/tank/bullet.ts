import Position from './positions';

interface Bullet {
    pos: Position,
    dire: number,
    level: number,
    source: string,
}

export default Bullet;