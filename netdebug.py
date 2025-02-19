import struct, socket, hashlib

global c

def start_server(portnum = 25566):
	global c
	s = socket.create_server( ("127.0.0.1", portnum) )
	c, a = s.accept()
	print(a)
	s.close()
	if recvall(1) != b'\n':
		print("Only for raw proto")
		c.close()

def let_client_join():
	send(b'\x03\x00' + (b'\0' * 16) ) #Greeter packet (cid0, salt all zero)
	recv()
	send(b'\x04\x00Debugger\0') #Login OK response (send userlist that has only Debugger user)

def start_client(portnum = 25566):
	global c
	c = socket.create_connection( ("127.0.0.1", portnum) )
	c.send(b'\n')

def server_send_credentials(uname, password):
	r = recv()
	if r[0] != 3:
		print("Wrong greeter response")
		return
	cid = r[1]
	salt = r[2:]
	if len(salt) != 16:
		print("Bad salt")
		return
	g = hashlib.sha512()
	g.update(uname)
	g.update(password)
	g.update(salt)
	r = server_cmd(b'\x01' + g.digest() )
	if r[0] != 4:
		print("Wrong response")
		return
	p = 1
	usrs = []
	while p < len(r):
		c = r[p]
		t = r.find(b'\0', p+1)
		usrs.append({"cid": c, "user": r[p+1:t]})
		p = t + 1
	return {"cid": cid, "salt": salt, "users": usrs}

def send(d):
	global c
	l = len(d)
	h = b""
	if l <= 0xfd:
		h = struct.pack("B", l)
	elif 0xfe <= l and l <= 0xffff:
		h = struct.pack("H", l)
	else:
		print("Too large packet")
	c.send(h + d)

def recvall(b):
	global c
	r = b""
	while len(r) < b:
		r += c.recv(b - len(r) )
	return r

def recv():
	l = struct.unpack("B", recvall(1) )[0]
	if l <= 0xfd:
		return recvall(l)
	elif l == 0xfe:
		return recvall(struct.unpack("!H", recvall(2) )[0])
	else:
		print("bad length header")

def server_cmd(c):
	send(c)
	return recv()
