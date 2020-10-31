const Config = {
    game: {
        update: 40,
    },
    space: {
        height: 800,
        width: 1000, 
    },
    tanks: {
        max_speed: 5,
        turn_speed: {
            tank: 180,
            radar: 1080,
            gun: 720,
        },
        fire_speed: [1500, 2000, 2500, 6000],
        fire_too_much_damage: 0.05,
        crash_damage: 0.01,
        size: 50,
        radar_size: 15,
    },
    bullet: {
        damage: [0.2, 0.25, 0.35, 0.75],
        speed: 15.2,
    },
};

export default Config;
