import $ = require('jquery');
import io = require('socket.io-client');
import Tank from '../../shared/tank/tanks';
import Bullet from '../../shared/tank/bullet';
import ui from './ui';
import Config from '../../shared/tank/config';
import { start_code, scan_tanks } from './api';

let socket: SocketIOClient.Socket;
let tanks: Array<Tank>;
let bullets: Array<Bullet>;

function message(x: string) {
	$("#message").text(x);
}

function start() {
	$("#stop-button").removeAttr("disabled");
	$('#start-button').attr("disabled", "true");

	let s: string = $('#server').val().toString();
	let code: string = $('#code').val().toString();
	let code_loaded: boolean = false;
	let load_tank_code = new Promise((resolve) => {
		let iid = setInterval(() => {
			if (code_loaded) {
				clearInterval(iid);
				resolve();
			}
		}, Config.game.update)
	});

	socket = io(s);
	socket.on('connection', function () {
		message('Connected');
	});

	socket.on('disconnect', function () {
		message('Killed. Disconnected.');
		on_stop();
	});

	socket.on('update', function (info) {
		tanks = info.tanks;
		bullets = info.bullets;
		ui(tanks, bullets);
		scan_tanks();
		code_loaded = true;
	});

	let parsed_code = new Function("tk", code);
	load_tank_code.then(() => {
		message('Tank Codes Loaded.');
		start_code(parsed_code, tanks, socket);
	});
}

function stop() {
	socket.disconnect();
	on_stop();
}

function on_stop() {
	$('#stop-button').attr("disabled", "true");
	$('#start-button').removeAttr("disabled");
}

$(function () {
	console.log("Init");
	$('#server').val('/');
	$('#start-button').on('click', start);
	$('#stop-button').on('click', stop);

	$('#stop-button').attr("disabled", "true");
});