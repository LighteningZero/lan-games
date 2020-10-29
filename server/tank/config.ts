const Config = {
    game: {
        update: 40,
    },
    space: {
        height: 800,
        width: 1000, 
    },
    tanks: {
        max_speed: 10,
        turn_speed: 10,
        fire_speed: [1500],
        fire_too_much_damage: 0.25,
        crash_damage: 0.1,
        size: 50,
    },
    bullet: {
        damage: [0.2]
    },
};

export default Config;