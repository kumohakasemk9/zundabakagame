const importobj = {
	env : {
		wasm_login_to_smp : wasm_login_to_smp,
		make_tcp_socket : make_tcp_socket,
		close_tcp_socket : close_tcp_socket,
		send_tcp_socket : send_tcp_socket,
		console_put : console_put,
		clear_screen : clear_screen,
		chcolor : chcolor,
		restore_color : restore_color,
		draw_polygon : draw_polygon,
		drawline : drawline,
		fillrect : fillrect,
		hollowrect : hollowrect,
		fillcircle : fillcircle,
		drawimage : drawimage,
		drawimage_scale : drawimage_scale,
		get_image_size : get_image_size,
		drawstring : drawstring,
		get_string_width : get_string_width,
		get_font_height : get_font_height,
		loadfont : loadfont,
		set_font_size : set_font_size
	}
};

var G;
var CV;
var ZundaGame;
var MemView;
var PRXBuffer;
var IMGS;
var CurrentFont = "monospace";
var CurrentFontSize = "18pt";
var IMAGE_COUNT;
var loadedimgcnt = 0;
var SavedColor;
var GTTime = 0, GTTimeCnt = 0, GTTimeAvg = 0;
var DTime = 0, DTimeCnt = 0, DTimeAvg = 0;
var WS = null;

addEventListener("load", function() {
	//Check window size
	if(window.clientWidth < 850 || window.clientHeight < 650) {
		alert("You need at least 850x 650 window space");
		return;
	}
	
	//Obtain graphics context, init
	CV = document.getElementById("cv");
	G = CV.getContext("2d");
	G.font = "14pt monospace";
	G.textBaseline = "top";
	
	updateStatus("Loading WASM...", 0, 0);
	const XHR = new XMLHttpRequest();
	XHR.open("GET", "zundagame.wasm");
	XHR.addEventListener("load", wasm_load_cb);
	XHR.responseType = 'arraybuffer';
	XHR.send();
});

async function wasm_load_cb(ev) {
	//Compile WASM
	updateStatus("Compiling WASM...");
	const wasm = await WebAssembly.instantiate(ev.target.response, importobj);
	
	//Get WASM exported functions and memory
	ZundaGame = wasm.instance.exports;
	MemView = new DataView(ZundaGame.memory.buffer);
	PRXBuffer = ZundaGame.getPtr_RXBuffer();

	let la = navigator.language;
	if(la.includes("ja") ) {
		ZundaGame.set_language(1);
	}
	ZundaGame.gameinit();
	
	//Load images
	updateStatus("Loading images...");
	IMGS = new Array();
	let l = new Array();
	const b = ZundaGame.getIMGPATHES();
	IMAGE_COUNT = ZundaGame.getImageCount();
	console.log(`${IMAGE_COUNT} images need to be loaded.`);
	for(let i = 0; i < IMAGE_COUNT; i++) {
		const eptr = MemView.getUint32(b + (i * 4), true);
		const s = pointer_to_str(eptr);
		l.push(s);
		let img = new Image();
		img.addEventListener("load", img_load_cb);
		img.src = s;
		IMGS.push(img);
	}
	console.log(l);
	console.log(IMGS);
}

function img_load_cb(evt) {
	console.log(`${evt.target.src} loaded.`);
	loadedimgcnt++;
	updateStatus(`Loading images (${loadedimgcnt}/${IMAGE_COUNT})...`);
	
	//start main process when all images loaded.
	if(loadedimgcnt == IMAGE_COUNT) {
		console.log("All Images loaded. Starting game.");
		setInterval(gametick, 10);
		setInterval(drawgame, 30);
		CV.addEventListener("mousemove", mousemove_cb);
		CV.addEventListener("mousedown", mousedown_cb);
		CV.addEventListener("wheel", mousewheel_cb);
		document.addEventListener("keydown", keydown_cb);
		document.addEventListener("keyup", keyup_cb);
	}
}

function gametick() {
	if(ZundaGame.isProgramExiting() == 0) {
		const tbefore = performance.now();
		ZundaGame.gametick();
		const tafter = performance.now();
		GTTime += tafter - tbefore;
		GTTimeCnt++;
		if(GTTimeCnt >= 10) {
			GTTimeAvg = GTTime / GTTimeCnt;
			GTTime = 0;
			GTimeAvg = 0;
		}
	}
}

function drawgame() {
	const p = document.getElementById('profiler');
	if(ZundaGame.isProgramExiting() == 0) {
		const tbefore = performance.now();
		ZundaGame.game_paint();
		const tafter = performance.now();
		DTime += tafter - tbefore;
		DTimeCnt++;
		if(DTimeCnt >= 10) {
			DTimeAvg = DTime / DTimeCnt;
			DTime = 0;
			DTimeCnt = 0;
		}
		const t = GTTimeAvg.toFixed(3);
		const t2 = DTimeAvg.toFixed(3);
		p.innerText = `running Tick: ${t} Draw: ${t2}`;
	} else {
		updateStatus("Error");
		p.innerText = "error";
	}
}

function updateStatus(s) {
	G.fillStyle = "white";
	G.clearRect(0, 0, 800, 600);
	G.fillText(s, 0, 0);
}

function pointer_to_str(p) {
	let d = new Array();
	let i = 0;
	while(i < 1024) {
		e = MemView.getUint8(p + i);
		if(e != 0) {
			d.push(e);
		} else {
			break;
		}
		i++;
	}
	const uidata = new Uint8Array(d);
	return new TextDecoder().decode(uidata);
}

function setcolor(c) {
	const a = (c >> 24) & 0xff;
	const _c = '#' + ('000000' + (c & 0xffffff).toString(16) ).slice(-6);
	G.globalAlpha = a / 255;
	G.fillStyle = _c;
	G.strokeStyle = _c;
}

function mousemove_cb(evt) {
	//MemView.setInt32(PCursorX, evt.x, true);
	//MemView.setInt32(PCursorY, evt.y, true);
	ZundaGame.mousemotion_handler(evt.x, evt.y);
}

function mousedown_cb(evt) {
	if(evt.buttons == 1) {
		ZundaGame.switch_character_move();
	}
}

function mousewheel_cb(evt) {
	if(evt.deltaY > 0) {
		ZundaGame.select_next_item();
	} else {
		ZundaGame.select_prev_item();
	}
}

function keydown_cb(evt) {
	const c = evt.key.codePointAt(0);
	if(ZundaGame.is_cmd_mode() ) {
		if(evt.key == "Enter") {
			ZundaGame.cmd_enter();
		} else if(evt.key == "Escape") {
			ZundaGame.cmd_cancel();
		} else if(evt.key == "Backspace") {
			ZundaGame.cmd_backspace();
		} else if(evt.key == "ArrowLeft") {
			ZundaGame.cmd_cursor_back();
		} else if(evt.key == "ArrowRight") {
			ZundaGame.cmd_cursor_forward();
		} else if(evt.key.length == 1 && 0x20 <= c && c <= 0x7e) {
			ZundaGame.cmd_putch(c);
			evt.preventDefault();
		}
	} else {
		if(evt.key == "q" || evt.key == "Q") {
			ZundaGame.modifyKeyFlags(1, true);
		} else if(evt.key == "w" || evt.key == "W") {
			ZundaGame.modifyKeyFlags(2, true);
		} else if(evt.key == "e" || evt.key == "E") {
			ZundaGame.modifyKeyFlags(4, true);
		} else if(evt.key == "d" || evt.key == "D") {
			ZundaGame.use_item();
		} else if(evt.key == "t" || evt.key == "T") {
			ZundaGame.start_command_mode(0);
		} else if(evt.key == "/") {
			ZundaGame.start_command_mode(1);
			evt.preventDefault();
		}
	}
}

function keyup_cb(evt) {
	if(!ZundaGame.is_cmd_mode() ) {
		if(evt.key == "q" || evt.key == "Q") {
			ZundaGame.modifyKeyFlags(1, false);
		} else if(evt.key == "w" || evt.key == "W") {
			ZundaGame.modifyKeyFlags(2, false);
		} else if(evt.key == "e" || evt.key == "E") {
			ZundaGame.modifyKeyFlags(4, false);
		}
	}
}

function ws_msg_cb(evt) {
	//let binstr = "";
	const data = new DataView(evt.data);
	let len = data.byteLength;
	for(let i = 0; i < len; i++) {
		//binstr += ("00" + data.getUint8(i).toString(16) ).slice(-2) + " ";
		MemView.setUint8(PRXBuffer + i, data.getUint8(i) );
	}
	//console.log(`WS recv (${len}): ${binstr}`);
	ZundaGame.pkt_recv_handler(PRXBuffer, len);
	return false;
}

function ws_open_cb(evt) {
	console.log("Websocket connected.");
	ZundaGame.connection_establish_handler();
}

function ws_close_cb(evt) {
	console.log("Websocket closed.");
	ZundaGame.connection_close_handler();
} 

function ws_error_cb(evt) {
	console.warn("Websocket error: " + evt);
}

function sha512_cb(digest) {
	console.log("sha512 hash callback");
	if(WS == null) {
		console.warn("Websocket not connected, this function will return.");
		return;
	}
	let v = new DataView(digest);
	let senddata = [1];
	for(let i = 0; i < v.byteLength; i++) {
		senddata.push(v.getUint8(i) );
	}
	WS.send(new Uint8Array(senddata) );
}

/* zundgame.wasm imported functions */
function clear_screen() {
	setcolor(0xff000000);
	fillrect(0, 0, 800, 600);
}

//uint32_t, int32_t
function chcolor(c, s) {
	if(s == 1) {
		SavedColor = c;
	}
	setcolor(c);
}

function restore_color() {
	setcolor(SavedColor);
}

//double, double, double, double, double
function drawline(x1, y1, x2, y2, w) {
	G.lineWidth = w;
	G.beginPath();
	G.moveTo(x1, y1);
	G.lineTo(x2, y2);
	G.stroke();
	G.lineWidth = 1;
}

//double, double, double, double
function fillrect(x, y, w, h) {
	G.fillRect(x, y, w, h);
}

//double, double, double, double
function hollowrect(x, y, w, h) {
	G.strokeRect(x, y, w, h);
}

//double, double, double
function fillcircle(x, y, diam) {
	G.beginPath();
	G.arc(x, y, diam / 2, 0, 2 * Math.PI);
	G.fill();
}

//double, double, int32_t
function drawimage(x, y, iid) {
	G.globalAlpha = 1;
	if(iid > IMGS.length || iid < 0) {
		console.warn("drawimage(): bad iid passed");
		return;
	}
	G.drawImage(IMGS[iid], x, y);
}

function drawimage_scale(x, y, w, h, iid) {
	G.globalAlpha = 1;
	if(iid > IMGS.length || iid < 0) {
		console.warn("drawimage_scale(): bad iid passed");
		return;
	}
	G.drawImage(IMGS[iid], x, y, w, h);
}

//double, double, char*
function drawstring(x, y, pc) {
	const c = pointer_to_str(pc);
	G.fillText(c, x, y);
}

//int32_t
function set_font_size(s) {
	CurrentFontSize = `${s}pt`;
	G.font = `${CurrentFontSize} ${CurrentFont}`;
}

//char*
function loadfont(s) {
	CurrentFont = pointer_to_str(s);
	G.font = `${CurrentFontSize} ${CurrentFont}`;
}

//double, double, int32_t, double[]
function draw_polygon(x, y, npts, ppts) {
	G.beginPath();
	G.moveTo(x, y);
	let _x = x;
	let _y = y;
	for(var i = 0; i < npts; i++) {
		_x = _x + MemView.getFloat64(ppts + (i * 16), true);
		_y = _y + MemView.getFloat64(ppts + 8 + (i * 16), true);
		G.lineTo(_x, _y);
	}
	G.fill();
}

//char* => int32_t
function get_string_width(pc) {
	return G.measureText(pointer_to_str(pc) ).width;
}

//() => int32_t
function get_font_height() {
	const t = G.measureText(pointer_to_str("A") );
	return t.emHeightAscent + t.emHeightDescent;
}

//int32_t, double*, double*
function get_image_size(iid, px, py) {
	if(iid > IMGS.length || iid < 0) {
		console.warn("get_image_size(): bad iid passed");
		return;
	}
	MemView.setFloat64(px, IMGS[iid].width, true);
	MemView.setFloat64(py, IMGS[iid].height, true);
}

//char*, int
function console_put(a, l) {
	const s = "[WASM] " + pointer_to_str(a);
	if(l == 0) {
		console.log(s);
	} else if(l == 1) {
		console.warn(s);
	} else {
		console.err(s);
	}
}

//char*, char* => int32_t
function make_tcp_socket(peeraddr, port) {
	const s_addr = pointer_to_str(peeraddr);
	const s_port = pointer_to_str(port);
	const wsaddr = `ws://${s_addr}:${s_port}/`
	console.log(`Connecting to ${wsaddr}`);
	WS = new WebSocket(wsaddr);
	WS.binaryType = "arraybuffer";
	WS.addEventListener("open", ws_open_cb);
	WS.addEventListener("message", ws_msg_cb);
	WS.addEventListener("close", ws_close_cb);
	WS.addEventListener("error", ws_error_cb);
	return 0;
}

//char*, size_t => ssize_t
function send_tcp_socket(data, len) {
	if(WS == null) {
		console.warn("send_tcp_socket(): Websocket is not connected.");
	} else {
		//let binstr = "";
		let rdata = new Array();
		for(let i = 0; i < len; i++) {
			//binstr += ("00" + MemView.getUint8(data + i).toString(16)).slice(-2) + " ";
			rdata.push(MemView.getUint8(data + i) );
		}
		//console.log(`WS send: ${len}: ${binstr}`);
		WS.send(new Uint8Array(rdata) );
	}
	return len;
}

//() => int32_t
function close_tcp_socket() {
	console.log("WS close request");
	if(WS != null) {
		WS.close();
		ws = null;
		return 0;
	}
	return -1;
}

//char*, char* uint8_t*
function wasm_login_to_smp(usr, pwd, salt) {
	const ptrs = [usr, pwd, salt];
	const maxlens = [20, 16, 16];
	let rkba = new Array();
	for(let i = 0; i < 3; i++) {
		for(let j = 0; j < maxlens[i]; j++) {
			e = MemView.getUint8(ptrs[i] + j);
			if(e == 0 && i != 2) {break;}
			rkba.push(e);
		}
	}
	crypto.subtle.digest("SHA-512", new Uint8Array(rkba) ).then(sha512_cb);
}
