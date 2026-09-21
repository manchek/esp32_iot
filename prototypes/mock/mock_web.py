#!/usr/bin/env python3

import http.server
import os
import time

token = '{0:x}'.format(int(time.time()*os.getpid()))

def extract_query(s):
	querydata = {}
	for qitem in s.split('&'):
		if '=' in qitem:
			k,v = qitem.split('=', 1)
			querydata[k] = v
	return querydata

class C(http.server.BaseHTTPRequestHandler):

	def get_cookies(self):
		self.cookies = {}
		if 'Cookie' in self.headers:
			for k, v in [e.split('=', 1) for e in self.headers['Cookie'].split('; ')]:
				self.cookies[k] = v

	def process_query(self):
		q = self.path.find('?')
		if q > 0:
			self.query = extract_query(self.path[q+1:])
			self.hier = self.path[0:q]
		else:
			self.query = extract_query('')
			self.hier = self.path

	def debug_request(self):
		a = self.client_address
		print('{0}:{1} {2}'.format(a[0], a[1], self.requestline))
		for h in self.headers:
			print('  {0}:{1}'.format(h, self.headers[h]))
		print('Path: {0}'.format(self.path))
		print('Cookies: {0}'.format(self.cookies))
		print('Hier: {0}'.format(self.hier))
		print('Query: {0}'.format(self.query))

	def do_GET(self):
		self.get_cookies()
		self.process_query()
		self.debug_request()
		path = self.hier
		if path == '/':
			path = '/default.html'
		if path == '/logout':
			self.serve_logout()
		elif self.cookies.get('acsession') != token:
			self.serve_login()
		else:
			self.serve_static(path)

	def do_POST(self):
		self.get_cookies()
		self.process_query()
		self.debug_request()

		if 'Content-Length' in self.headers:
			reqBodyLen = int(self.headers['Content-Length'])
			reqBody = self.rfile.read(reqBodyLen)
			print('Body [{0}] {1}'.format(len(reqBody), reqBody))
			args = extract_query(reqBody.decode())
			print('Args: {0}'.format(args))
		else:
			args = extract_query('')

		if args.get('user') == 'bob' and args.get('pw') == 'obo':
			self.send_response(200)
			self.send_header("Set-Cookie", "acsession=" + token + "; Path=/")
			self.serve_file('/login_success.html')
		else:
			self.serve_static('/deny.html')

	def serve_logout(self):
		self.send_response(200)
		self.send_header("Set-Cookie", "acsession=x; Max-Age=0")
		self.serve_file('/logout.html')

	def serve_login(self):
		self.serve_static('/login.html')

	def serve_file(self, path):
		try:
			with open('media/' + path) as f:
				data = f.read()
				body = bytes(data, 'utf-8')
				self.send_header("Content-type", "text/html")
				self.end_headers()
				self.wfile.write(body)
		except:
			self.send_header("Content-type", "text/plain")
			self.end_headers()
			self.wfile.write(bytes())

	def serve_static(self, path):
		try:
			with open('media/' + path) as f:
				data = f.read()
				body = bytes(data, 'utf-8')
				self.send_response(200)
				self.send_header("Content-type", "text/html")
				self.end_headers()
				self.wfile.write(body)
		except:
			self.send_response(404)
			self.send_header("Content-type", "text/plain")
			self.end_headers()
			self.wfile.write(bytes())

server = http.server.HTTPServer(('', 8001), C)
server.serve_forever()

