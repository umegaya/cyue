this.yue || (function(_G) {
	function trace(msg) {
		console.log(msg);
	}
	if (!this.msgpack) {
		trace("msgpack.js required");
		return;
	}
	_G.yue = {
		open: open,
		connections: {},
	};
	/* CONSTANT */
	var REQUEST = 0;
	var RESPONSE = 1;
	var NOTIFY = 2;
	
	var KEEP_ALIVE = 0;
	var CALL_PROC = 1;
	var CALL_METHOD = 2;
	
	/* class Dispatcher */
	function Dispatcher() {
		var self = this;
		self.msgid_seed = 1;
		self.dispatch_table = {}
	}
	Dispatcher.prototype.send = function(conn, cmd, method, args, callback) {
		var self = this;
		rpc = [cmd, this.msgid_seed, method, args];
		self.dispatch_table[this.msgid_seed] = callback;
		self.msgid_seed++;
		conn.send(_G.msgpack.pack(rpc));
	}
	Dispatcher.prototype.fetch = function(env, data) {
		return [nil, "not implemented"];
	}
	Dispatcher.prototype.recv = function(conn, e) {
		var self = this;
		var m = _G.msgpack.unpack(new Uint8Array(e.data));
		if (m[0] == RESPONSE) {
			var cb = self.dispatch_table[m[1]];
			self.dispatch_table[m[1]] = null;
			cb(conn, m[3], m[2]);
		}
		else if (m[0] == REQUEST) {
			/* TODO: resolve function with _G and m[1] and call it */
			resp = [RESPONSE, m[2]].concat(self.fetch(_G, m));
			conn.send(_G.msgpack.pack(resp))
		}
	}
	var dispatcher = new Dispatcher();
	/* class Connection */
	function Connection(host) {
		var self = this;
		self.socket = new WebSocket("ws://" + host + "/");
		self.socket.binaryType = "arraybuffer";
		self.wbuf = [];
		self.established = false;
		self.socket.onopen = function(e){
			trace("open connection: " + host);
			self.established = true;
			if (self.wbuf.length > 0) {
				for (k in self.wbuf) {
					var buf = self.wbuf[k];
					trace("pre-sent " + buf.length + " byte sending");
					trace(JSON.stringify(buf));
					self.socket.send(buf.buffer);
				}
				self.wbuf = [];
			}
		}
		self.socket.onmessage = function(e){
			trace("recv data");
			dispatcher.recv(self, e)
		}
		self.socket.onerror = function(e){
			trace("connect error: " + host);
			this.established = false;
		}
		self.socket.onclose = function(e){
			trace("connection close: " + host);
			this.established = false;
		}
	}
	Connection.prototype.call = function(method) {
		var args = [];
		for (i = 0; i < arguments.length - 1; i++) {
			args.push(arguments[i])
		}
		dispatcher.send(this, REQUEST, CALL_PROC, args, arguments[arguments.length - 1]);
	}
	Connection.prototype.send = function(data) {
		var self = this;
		var bin = new Uint8Array(data);
		if (!self.established) {
			self.wbuf.push(bin);
			return;
		}
		self.socket.send(bin.buffer);
	}
	function open(host) {
		if (_G.yue.connections[host] != null) {
			return _G.yue.connections[host];
		}
		_G.yue.connections[host] = new Connection(host);
		return _G.yue.connections[host];
	}
})(this);