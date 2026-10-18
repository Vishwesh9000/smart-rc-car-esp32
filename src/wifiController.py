import socket, time, keyboard

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("192.168.4.1", 8080))


command = b'S'
speed = 1
speed_vals = [0,100,0,-100]
s_index = 0

def updateSpeed(s):
    sock.sendall(b'S' + s.to_bytes(1,'big', signed=True))
    print(f"Sent Packet {b'S'}{s}")

def updateTurn(t):
    sock.sendall(b'T' + t.to_bytes(1,'big', signed=True))
    print(f"Sent Packet {b'T'}{t}")


key_status = {'up': False, 'down': False, 'right': False, 'left': False}
def on_key_event(event):
    if event.name not in key_status:
        return
    if key_status[event.name] == (event.event_type == 'down'):
        return
    else:
        key_status[event.name] = (event.event_type == 'down')


    if event.name == 'up':
        if event.event_type == 'down':
            updateSpeed(100)
        else:
            if keyboard.is_pressed('down'):
                updateSpeed(-100)
            else:
                updateSpeed(0)

    if event.name == 'down':
        if event.event_type == 'down':
            updateSpeed(-100)
        else:
            if keyboard.is_pressed('up'):
                updateSpeed(100)
            else:
                updateSpeed(0)


    if event.name == 'right':
        if event.event_type == 'down':
            updateTurn(100)
        else:
            if keyboard.is_pressed('left'):
                updateTurn(-100)
            else:
                updateTurn(0)
    if event.name == 'left':
        if event.event_type == 'down':
            updateTurn(-100)
        else:
            if keyboard.is_pressed('right'):
                updateTurn(100)
            else:
                updateTurn(0)

keyboard.hook(on_key_event)
print("Listening for arrow keys")

while True:
    time.sleep(10)
