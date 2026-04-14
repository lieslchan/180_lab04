import socket
import threading
import time

BUFFER_SIZE = 8192

#the list of blocked hosts
blocked_hosts = []

def main():
    with open('blocked.txt','r') as f:
        global blocked_hosts
        blocked_hosts = [
            line.strip()
            for line in f.readlines()
            if line.strip()
        ]

    start_proxy()

#handles a client
#checks whether the request is http or https depending on the request method
def handle_client(client_socket):
    request = client_socket.recv(BUFFER_SIZE)
    first_line = request.split(b'\r\n')[0]
    parts = first_line.split(b' ')
    
    url = parts[1] if len(parts) > 1 else b""

    if b'CONNECT' in first_line:
        #extract host:port
        handle_https_tunnel(client_socket, url)
    else:
        handle_http(client_socket, request)
        
    client_socket.close()

#handles http
def handle_http(client_socket, request):
    lines = request.split(b'\r\n')
    host = None
    for line in lines:
        if line.startswith(b'Host:'):
            host = line.split(b': ')[1].decode().strip()
            break

    if host in blocked_hosts:
        print(f"[BLOCKED] {host}")
        client_socket.send(
            b'HTTP/1.1 200 OK\r\n\r\n'
            b'<h2>THIS SITE IS BLOCKED</h2>'
        )
        return

    remote = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        remote.connect((host, 80))
        print(f"[HTTP] GET {request.split(b' ')[1].decode()} -> {host}:80")
        remote.sendall(request)
        forward(remote, client_socket)
    finally:
        remote.close()
    

    

#handles https
def handle_https_tunnel(client_socket, target_address):
    host, port = target_address.split(b':')
    host = host.decode()
    print(f"[HTTPS] CONNECT {host}:{port.decode()}")

    if host in blocked_hosts:
        print(f"[BLOCKED] {host}")
        client_socket.send(b'HTTP/1.1 403 Forbidden\r\n\r\n')
        return

    remote = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    remote.connect((host, int(port)))

    client_socket.send(
        b'HTTP/1.1 200 Connection established\r\n\r\n'
    )

    try:
        t1 = threading.Thread(target=forward, args=(client_socket, remote))
        t2 = threading.Thread(target=forward, args=(remote, client_socket))
        t1.start()
        t2.start()
        t1.join()
        t2.join()
    finally:
        remote.close()

#thread function to receive data from source and send to client
#used in https
def forward(source, destination):
    try:
        while True:
            data = source.recv(BUFFER_SIZE)
            if not data:
                break
            destination.sendall(data)
    except:
        pass


#starts the proxy
#creates the server socket and begins accepting connections
#where each connection is handled by a thread
def start_proxy(host='127.0.0.1', port=3128):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    server_socket.bind((host, port))

    server_socket.listen(5)

    print(f"Server listening on {host}:{port}...")

    # Accept a new connection
    try:
        while True:
            conn, address = server_socket.accept() 
            print(f"Connection from: {str(address)}")

            threading.Thread(
                target=handle_client, 
                args=(conn,),
                daemon=True
            ).start()
    
    finally:
        # Once communication is complete, close the connection
        conn.close()
        server_socket.close()

#main program which reads blocked.txt to populate blocked_hosts
#and calls start_proxy
if __name__ == "__main__":
    main()