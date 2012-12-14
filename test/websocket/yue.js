this.yue || (function(_G) {
	function trace(msg) {
		console.log(msg);
	}
	if (!this.msgpack) {
		trace("msgpack.js required");
		return;
	}
	/*--------------------------------------------------------------------------*/
	/* module main 	 															*/
	/*--------------------------------------------------------------------------*/
	var yue = {
		open: open,
		connections: {},
		timer: timer,
	};
	
	
	
	/*--------------------------------------------------------------------------*/
	/* CONSTANT		 															*/
	/*--------------------------------------------------------------------------*/
	var REQUEST = 0;
	var RESPONSE = 1;
	var NOTIFY = 2;
	
	var KEEP_ALIVE = 0;
	var CALL_PROC = 1;
	var CALL_METHOD = 2;
	
	var DEFAULT_TIMEOUT = 5000;    //5000msec
	var TIMEOUT_CHECK_SPAN = 1000; //1000msec
	
	var TIMER_RESOLUTION = 100;        //100msec
	var PACKET_SEND_CHECK_SPAN = 100;  //100msec
	
	
	
	/*--------------------------------------------------------------------------*/
	/* class Dispatcher 														*/
	/*--------------------------------------------------------------------------*/
	function Dispatcher() {
		var self = this;
		self.msgid_seed = 1;
		self.last_check = 0;
		self.dispatch_table = {}
	}
	Dispatcher.prototype.send = function(conn, cmd, method, args, callback, timeout) {
		var self = this;
		rpc = [cmd, this.msgid_seed, method, args];
		trace(JSON.stringify(rpc));
		if (!timeout) { timeout = DEFAULT_TIMEOUT; }
		self.dispatch_table[this.msgid_seed] = {conn:conn, cb:callback, limit:(timer.now() + timeout)};
		self.msgid_seed++;
		conn.send(rpc);
	}
	Dispatcher.prototype.fetch = function(env, conn, mpk) {
		if (!/[a-zA-Z]/.test(mpk[2].charAt(0))) {
			return [[-34/* NBR_ERIGHT */, "try to invoke private symbol" + mpk[2]], null];
		}
		var f = conn.namespace.fetch(mpk[2]);
		if (typeof(f) != "function") {
			return [[-9/* NBR_ENOTFOUND */, "try to invoke non-function symbol:" + mpk[2]], null];
		}
		var r;
		args = mpk[3];
		switch (args.length) {
		case 0: r = f(); break;
		case 1: r = f(args[0]); break;
		case 2: r = f(args[0], args[1]); break;
		case 3: r = f(args[0], args[1], args[2]); break;
		case 4: r = f(args[0], args[1], args[2], args[3]); break;
		case 5: r = f(args[0], args[1], args[2], args[3], args[4]); break;
		case 6: r = f(args[0], args[1], args[2], args[3], args[4], args[5]); break;
		case 7: r = f(args[0], args[1], args[2], args[3], args[4], args[5], 
						args[6]); break;
		case 8: r = f(args[0], args[1], args[2], args[3], args[4], args[5], 
						args[6], args[7]); break;
		case 9: r = f(args[0], args[1], args[2], args[3], args[4], args[5], 
						args[6], args[7], args[8]); break;
		case 10: r = f(args[0], args[1], args[2], args[3], args[4], args[5], 
						args[6], args[7], args[8], args[9]); break;
		default: return [[-22/* NBR_ENOTSUPPORT */, "currently, upto 10 argument supported:" + args.length], null];
		}
		if (!(r instanceof Array)) {
			r = [r];
		}
		return [null, r];
	}
	Dispatcher.prototype.recv = function(conn, e) {
		var self = this;
		trace(JSON.stringify(new Uint8Array(e.data)));
		var m = conn.unpacker.unpack(new Uint8Array(e.data));
		while (m !== undefined) {
			if (m[0] == RESPONSE) {
				/* [RESPONSE,msgid,error,result] */
				var e = self.dispatch_table[m[1]];
				delete self.dispatch_table[m[1]];
				e.cb(conn, m[3], m[2]);
			}
			else if (m[0] == REQUEST) {
				/* REQUEST,msgid,method,[arg1,arg2,...,argN]] */
				trace(JSON.stringify(m));
				resp = [RESPONSE, m[1]].concat(self.fetch(_G, conn, m));
				trace(JSON.stringify(resp));
				conn.send_nocheck(_G.msgpack.pack(resp))
				/* TODO: should support 2? (NOTIFY) */
			}
			m = conn.unpacker.unpack(null);
		}
	}
	Dispatcher.prototype.raise = function(msgid, error) {
		var self = this;
		var e = self.dispatch_table[msgid];
		delete self.dispatch_table[msgid];
		e.cb(e.conn, null, error);
	}
	var dispatcher = new Dispatcher();
	
	
	
	/*--------------------------------------------------------------------------*/
	/* class Namespace 															*/
	/*--------------------------------------------------------------------------*/
	function Namespace() {
		var self = this;
		self.symbols = {}
	}
	Namespace.prototype.import = function(symbols) {
		var self = this;
		for (var k in symbols) {
			this.symbols[k] = symbols[k];
		}
	}
	Namespace.prototype.fetch = function(name) {
		return this.symbols[name];
	}



	/*--------------------------------------------------------------------------*/
	/* class Connection 														*/
	/*--------------------------------------------------------------------------*/
	function Connection(host, opt) {
		var self = this;
		self.wbuf = [];
		self.sendQ = [];
		self.unpacker = msgpack.stream_unpacker();
		self.namespace = new Namespace();
		self.namespace.import({
			accept__ : function (r) {
				trace("accepted__: " + r);
				var f = self.namespace.fetch("__accept");
				if (f && !f(self, r)) {
					self.close();
				}
				else if (self.state == self.WAITACCEPT) {
					trace("wait accept: now establish and send packet");
					self.establish(true);
				}
			}
		});
		self.last_sent = timer.now();
		self.state = self.CLOSED;
		self.host = host;
		self.opt = opt || {};
	}
	Connection.prototype.CLOSED = 1;
	Connection.prototype.HANDSHAKE = 2;
	Connection.prototype.WAITACCEPT = 3;
	Connection.prototype.ESTABLISHED = 4;
	Connection.prototype.establish = function (success) {
		var self = this;
		if (success) {
			self.state = self.ESTABLISHED;
			for (var i in self.sendQ) {
				trace("queued msg sent:" + JSON.stringify(self.sendQ[i]));
				self.send_nocheck(_G.msgpack.pack(self.sendQ[i]));
			}
			self.sendQ = [];
			self.rawsend();
		}
		else {
			self.state = self.CLOSED;
			for (var i in self.sendQ) {
				trace("error returns to queued msg sender:" + JSON.stringify(self.sendQ[i]));
				dispatcher.raise(self.sendQ[i][1], "connection closed");
			}
			self.sendQ = [];
		}
	}
	Connection.prototype.connect = function() {
		var self = this;
		if (self.state !=self.CLOSED) {
			return;
		}
		self.state = self.HANDSHAKE;
		self.socket = new WebSocket("ws://" + self.host + "/");
		self.socket.binaryType = "arraybuffer";
		self.socket.onopen = function(e){
			trace("open connection: " + self.host);
			self.state = self.WAITACCEPT;
		}
		self.socket.onmessage = function(e){
			trace("recv data" + JSON.stringify(e));
			dispatcher.recv(self, e)
		}
		self.socket.onerror = function(e){
			trace("connect error: " + self.host);
			self.establish(false);
		}
		self.socket.onclose = function(e){
			trace("connection close: " + self.host);
			self.establish(false);
			var f = self.namespace.fetch("__close");
			if (f) { f(self); }
		}
	}
	Connection.prototype.close = function() {
		var self = this;
		if (self.state == self.CLOSED) {
			return;
		}
		self.socket.close();
	}
	Connection.prototype.call = function(method) {
		var args = [];
		for (i = 1; i < arguments.length - 1; i++) {
			args.push(arguments[i])
		}
		dispatcher.send(this, REQUEST, method, args, arguments[arguments.length - 1]);
	}
	Connection.prototype.send = function(args) {
		var self = this;
		if (self.state != self.ESTABLISHED) {
			trace("connection not established: queue:" + JSON.stringify(args));
			self.sendQ.push(args);
			self.connect();
			return;
		}
		self.send_nocheck(_G.msgpack.pack(args));
	}
	Connection.prototype.send_nocheck = function(data) {
		var self = this;
		self.wbuf = self.wbuf.concat(data);
		trace("sent time: " + timer.now() + " vs " + self.last_sent);
		if ((timer.now() - self.last_sent) <= PACKET_SEND_CHECK_SPAN) {
			return;
		}
		self.rawsend();
	}
	Connection.prototype.rawsend = function() {
		var self = this;
		if (self.wbuf.length > 0) {
			trace(JSON.stringify(self.wbuf));
			var bin = new Uint8Array(self.wbuf);
			self.socket.send(bin.buffer);
			self.wbuf = [];
		}
	}



	/*--------------------------------------------------------------------------*/
	/* class Timer		 														*/
	/*--------------------------------------------------------------------------*/
	function Timer(msec) {
		var self = this;
		self.procs = [];
		setInterval(function () {
			for (var k in self.procs) {
				self.procs[k](self.now());
			}
		}, msec);
	}
	Timer.prototype.add = function(proc) {
		var self = this;
		self.procs.push(proc);
	}
	Timer.prototype.remove = function(proc) {
		var nprocs = [];
		for (var k in self.procs) {
			if (self.procs[k] != proc) { 
				nprocs.push(self.procs[k]);
			}
		}
		self.procs = nprocs;
	}
	Timer.prototype.now = function () {
	   return (new Date).getTime();
	}
	
	
	/*--------------------------------------------------------------------------*/
	/* init lib			 														*/
	/*--------------------------------------------------------------------------*/
	var timer = new Timer(TIMER_RESOLUTION);
	timer.add(function (now) {
		for (var k in _G.yue.connections) {
			var c = _G.yue.connections[k];
			if (now - c.last_sent >= PACKET_SEND_CHECK_SPAN) {
				c.rawsend();
			}
		}
	});	
    timer.add(function (now) {
        if ((now - dispatcher.last_check) < TIMEOUT_CHECK_SPAN) {
            return;
        }
        dispatcher.last_check = now;
        var t = dispatcher.dispatch_table;
        for (var msgid in t) {
            var d = t[msgid];
            if (now >= d.limit) {
                trace("query:" + msgid + " timeout: " + now + " vs " + d.limit);
                dispatcher.raise(msgid, "timeout");
            }
        }
    }); 
	function open(host, opt) {
		if (_G.yue.connections[host] != null) {
			return _G.yue.connections[host];
		}
		_G.yue.connections[host] = new Connection(host, opt);
		return _G.yue.connections[host];
	}
	
	_G.yue = yue;
})(this);
