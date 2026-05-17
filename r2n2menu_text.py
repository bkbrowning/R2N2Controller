# R2N2 Text-based Control Menu for use with Portable RPi5

import time
import sys
import select
import termios
import tty

import board
import busio
import adafruit_rfm69
import adafruit_ssd1306

from digitalio import DigitalInOut
from PIL import Image, ImageDraw, ImageFont


RADIO_FREQ_MHZ = 915.0
TX_POWER = 14

PI_NODE = 99
BODY_NODE = 10
FRONT_NODE = 20
REAR_NODE = 30
DOME_NODE = 40

ACTION_SERVO_GROUP_MOVE = 2
ACTION_DOME_ALL_OPEN = 5
ACTION_DOME_ALL_CLOSE = 6
ACTION_STEALTH_SOUND = 0x30

GROUP_ALL_SERVOS = 255
SERVO_POS_OPEN = 1
SERVO_POS_CLOSED = 2

SOUND_BANKS = {
    1: "General",
    2: "Chatty",
    3: "Sad",
    4: "Burp",
    5: "Whistle",
    6: "Scream",
    7: "Warning",
    8: "Short",
    9: "Leia",
    10: "Imperial",
    11: "Star Wars",
    12: "Dance",
    13: "Cantina",
}

i2c = busio.I2C(board.SCL, board.SDA)
oled_reset = DigitalInOut(board.D4)
display = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c, reset=oled_reset)
oled_font = ImageFont.load_default()

spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
cs = DigitalInOut(board.CE1)
reset = DigitalInOut(board.D25)

rfm69 = adafruit_rfm69.RFM69(spi, cs, reset, RADIO_FREQ_MHZ)
rfm69.tx_power = TX_POWER
rfm69.node = PI_NODE
rfm69.destination = BODY_NODE
rfm69.encryption_key = None

msg_id = 1


def oled(line1="", line2="", line3=""):
    image = Image.new("1", (128, 32))
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), line1[:21], font=oled_font, fill=255)
    draw.text((0, 10), line2[:21], font=oled_font, fill=255)
    draw.text((0, 20), line3[:21], font=oled_font, fill=255)
    display.image(image)
    display.show()


def sound_label(bank):
    return SOUND_BANKS.get(bank, f"Custom {bank}")


def payload_group_open():
    return bytes([ACTION_SERVO_GROUP_MOVE, GROUP_ALL_SERVOS, SERVO_POS_OPEN, 0])


def payload_group_close():
    return bytes([ACTION_SERVO_GROUP_MOVE, GROUP_ALL_SERVOS, SERVO_POS_CLOSED, 0])


def payload_dome_open():
    return bytes([ACTION_DOME_ALL_OPEN, GROUP_ALL_SERVOS, SERVO_POS_OPEN, 0])


def payload_dome_close():
    return bytes([ACTION_DOME_ALL_CLOSE, GROUP_ALL_SERVOS, SERVO_POS_CLOSED, 0])


def payload_sound_bank(bank):
    return bytes([ACTION_STEALTH_SOUND, bank, 0x00, 0x00])


def send_radio_command(label, dest, payload):
    global msg_id

    print(f"TX {label} -> node {dest}: {payload.hex(' ')}")
    oled("TX", label, f"to {dest} id {msg_id}")

    ok = rfm69.send(
        payload,
        destination=dest,
        node=PI_NODE,
        identifier=msg_id,
        flags=0,
        keep_listening=True,
    )

    print(f"send returned: {ok}")

    msg_id = (msg_id + 1) & 0xFF
    if msg_id == 0:
        msg_id = 1

    time.sleep(0.15)


def receive_once():
    pkt = rfm69.receive(timeout=0.05, with_header=True)
    if pkt is None:
        return

    if len(pkt) >= 4:
        header = pkt[:4]
        body = pkt[4:]
        print(
            f"RX RSSI {rfm69.rssi} | "
            f"to={header[0]} from={header[1]} id={header[2]} "
            f"flags=0x{header[3]:02X} | body={body.hex(' ')}"
        )
        oled("RX", f"from {header[1]}", f"RSSI {rfm69.rssi}")


def clear_screen():
    print("\033c", end="")


def show_menu():
    clear_screen()
    print("================================")
    print("        R2N2 FIELD CONTROL      ")
    print("================================")
    print()
    print("Panels:")
    print("  F = Front Open      G = Front Close")
    print("  R = Rear Open       T = Rear Close")
    print("  D = Dome Open       E = Dome Close")
    print("  O = Open All        C = Close All")
    print()
    print("Sounds:")
    print("  1-9 = Sound banks 1-9")
    print("  A = Sound 10 Imperial")
    print("  B = Sound 11 Star Wars")
    print("  L = Sound 12 Dance")
    print("  M = Sound 13 Cantina")
    print()
    print("System:")
    print("  Q = Quit")
    print()
    print("WARNING: commands can move real servos.")
    print()


def open_all():
    send_radio_command("Front Open", FRONT_NODE, payload_group_open())
    send_radio_command("Rear Open", REAR_NODE, payload_group_open())
    send_radio_command("Dome Open", DOME_NODE, payload_dome_open())


def close_all():
    send_radio_command("Dome Close", DOME_NODE, payload_dome_close())
    send_radio_command("Rear Close", REAR_NODE, payload_group_close())
    send_radio_command("Front Close", FRONT_NODE, payload_group_close())


def play_sound(bank):
    send_radio_command(f"Sound {bank}: {sound_label(bank)}", BODY_NODE, payload_sound_bank(bank))


def handle_key(ch):
    ch = ch.lower()

    if ch == "f":
        send_radio_command("Front Open", FRONT_NODE, payload_group_open())
    elif ch == "g":
        send_radio_command("Front Close", FRONT_NODE, payload_group_close())
    elif ch == "r":
        send_radio_command("Rear Open", REAR_NODE, payload_group_open())
    elif ch == "t":
        send_radio_command("Rear Close", REAR_NODE, payload_group_close())
    elif ch == "d":
        send_radio_command("Dome Open", DOME_NODE, payload_dome_open())
    elif ch == "e":
        send_radio_command("Dome Close", DOME_NODE, payload_dome_close())
    elif ch == "o":
        open_all()
    elif ch == "c":
        close_all()
    elif ch in "123456789":
        play_sound(int(ch))
    elif ch == "a":
        play_sound(10)
    elif ch == "b":
        play_sound(11)
    elif ch == "l":
        play_sound(12)
    elif ch == "m":
        play_sound(13)
    elif ch == "?":
        show_menu()
    elif ch == "q":
        return False

    return True


def main():
    old_term_settings = None

    try:
        if sys.stdin.isatty():
            old_term_settings = termios.tcgetattr(sys.stdin)
            tty.setcbreak(sys.stdin.fileno())

        oled("R2N2 Text", "Radio ready", "")
        show_menu()

        running = True
        while running:
            receive_once()

            if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                ch = sys.stdin.read(1)
                running = handle_key(ch)

            time.sleep(0.02)

    except KeyboardInterrupt:
        pass

    finally:
        if old_term_settings is not None:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_term_settings)

        oled("R2N2 Text", "Stopped", "")


if __name__ == "__main__":
    main()
