# R2N2 Graphical Control Menu for RPi5 with attached HUD

import os
import time
import subprocess

import board
import busio
import pygame
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
ACTION_DOME_WAVE = 7
ACTION_FRONT_ARM_FLAIL = 8
ACTION_FRONT_CHARGE_TOGGLE = 9
ACTION_FRONT_DATA_TOGGLE = 10
ACTION_REAR_TOP_TOGGLE = 11
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

FPS = 30
STARTUP_DELAY_SECONDS = 2
COMMAND_DELAY_SECONDS = 0.15

BG = (8, 10, 14)
PANEL = (28, 32, 42)
PANEL_HEADER = (42, 48, 62)
PANEL_HEADER_OPEN = (34, 92, 54)
PANEL_HEADER_CLOSED = (45, 72, 112)

BUTTON_OPEN = (34, 110, 62)
BUTTON_CLOSE = (45, 82, 130)
BUTTON_SOUND = (95, 70, 130)
BUTTON_PRESET = (122, 88, 38)
BUTTON_SYSTEM = (105, 74, 45)
BUTTON_WIFI_ON = (38, 110, 76)
BUTTON_WIFI_OFF = (125, 70, 42)
BUTTON_DANGER = (130, 42, 42)
BUTTON_CONFIRM = (42, 120, 62)
BUTTON_CANCEL = (95, 95, 105)

SELECTED = (255, 220, 80)
TEXT = (235, 238, 245)
TEXT_DIM = (170, 178, 190)
STATUS_OK = (55, 150, 80)


def sound_label(bank):
    return SOUND_BANKS.get(bank, f"Custom {bank}")


# OLED
i2c = busio.I2C(board.SCL, board.SDA)
oled_reset = DigitalInOut(board.D4)
display = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c, reset=oled_reset)
oled_font = ImageFont.load_default()


def oled(line1="", line2="", line3=""):
    image = Image.new("1", (128, 32))
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), line1[:21], font=oled_font, fill=255)
    draw.text((0, 10), line2[:21], font=oled_font, fill=255)
    draw.text((0, 20), line3[:21], font=oled_font, fill=255)
    display.image(image)
    display.show()


# Radio
spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
cs = DigitalInOut(board.CE1)
reset = DigitalInOut(board.D25)

rfm69 = adafruit_rfm69.RFM69(spi, cs, reset, RADIO_FREQ_MHZ)
rfm69.tx_power = TX_POWER
rfm69.node = PI_NODE
rfm69.destination = BODY_NODE
rfm69.encryption_key = None

msg_id = 1


state = {
    "front": "unknown",
    "rear": "unknown",
    "dome": "unknown",
    "charge_bay": "closed",
    "data_panel": "closed",
    "rear_top": "closed",
    "selected_sound": 7,
    "last_command": "Ready",
    "last_rx": "None",
    "last_rssi": "",
    "status_message": "Radio ready",
    "wifi_status": "unknown",
    "confirm_shutdown": False,
    "confirm_exit": False,
    "confirm_wifi_off": False,
}


def run_cmd(cmd):
    try:
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=8,
        )
        return result.returncode, result.stdout.strip(), result.stderr.strip()
    except Exception as exc:
        return 1, "", str(exc)


def get_wifi_status():
    code, out, err = run_cmd(["nmcli", "radio", "wifi"])
    if code == 0:
        value = out.strip().lower()
        if value in ("enabled", "disabled"):
            return value
    return "unknown"


def update_wifi_status():
    state["wifi_status"] = get_wifi_status()


def wifi_off():
    code, out, err = run_cmd(["sudo", "nmcli", "radio", "wifi", "off"])
    update_wifi_status()
    if code == 0:
        state["status_message"] = "WiFi turned off"
        oled("WiFi", "OFF", "")
    else:
        state["status_message"] = f"WiFi OFF failed: {err[:50]}"
        oled("WiFi OFF failed", err[:21], "")


def wifi_on():
    code, out, err = run_cmd(["sudo", "nmcli", "radio", "wifi", "on"])
    update_wifi_status()
    if code == 0:
        state["status_message"] = "WiFi turned on"
        oled("WiFi", "ON", "")
    else:
        state["status_message"] = f"WiFi ON failed: {err[:50]}"
        oled("WiFi ON failed", err[:21], "")


def action_wifi_toggle():
    update_wifi_status()
    if state["wifi_status"] == "enabled":
        state["confirm_wifi_off"] = True
        state["confirm_exit"] = False
        state["confirm_shutdown"] = False
        state["status_message"] = "Confirm WiFi OFF"
    else:
        wifi_on()


def payload_group_open():
    return bytes([ACTION_SERVO_GROUP_MOVE, GROUP_ALL_SERVOS, SERVO_POS_OPEN, 0])


def payload_group_close():
    return bytes([ACTION_SERVO_GROUP_MOVE, GROUP_ALL_SERVOS, SERVO_POS_CLOSED, 0])


def payload_dome_open():
    return bytes([ACTION_DOME_ALL_OPEN, GROUP_ALL_SERVOS, SERVO_POS_OPEN, 0])


def payload_dome_close():
    return bytes([ACTION_DOME_ALL_CLOSE, GROUP_ALL_SERVOS, SERVO_POS_CLOSED, 0])


def payload_dome_wave():
    return bytes([ACTION_DOME_WAVE, GROUP_ALL_SERVOS, SERVO_POS_OPEN, 0])


def payload_front_arm_flail():
    return bytes([ACTION_FRONT_ARM_FLAIL, 0x00, 0x00, 0x00])


def payload_front_charge_toggle():
    return bytes([ACTION_FRONT_CHARGE_TOGGLE, 2, 0x00, 0x00])


def payload_front_data_toggle():
    return bytes([ACTION_FRONT_DATA_TOGGLE, 6, 0x00, 0x00])


def payload_rear_top_toggle():
    return bytes([ACTION_REAR_TOP_TOGGLE, 2, 0x00, 0x00])


def payload_sound_bank(bank):
    return bytes([ACTION_STEALTH_SOUND, bank, 0x00, 0x00])


def send_radio_command(label, dest, payload):
    global msg_id

    print(f"TX {label} -> node {dest}: {payload.hex(' ')}")
    oled("TX", label, f"to {dest} id {msg_id}")

    rfm69.send(
        payload,
        destination=dest,
        node=PI_NODE,
        identifier=msg_id,
        flags=0,
        keep_listening=True,
    )

    state["last_command"] = label
    state["status_message"] = f"Sent: {label}"

    msg_id = (msg_id + 1) & 0xFF
    if msg_id == 0:
        msg_id = 1

    time.sleep(COMMAND_DELAY_SECONDS)


def receive_once():
    pkt = rfm69.receive(timeout=0.01, with_header=True)
    if pkt is None:
        return

    if len(pkt) >= 4:
        header = pkt[:4]
        body = pkt[4:]

        state["last_rx"] = f"Node {header[1]}"
        state["last_rssi"] = str(rfm69.rssi)

        print(
            f"RX RSSI {rfm69.rssi} | "
            f"to={header[0]} from={header[1]} id={header[2]} "
            f"flags=0x{header[3]:02X} | body={body.hex(' ')}"
        )

        oled("RX", f"from {header[1]}", f"RSSI {rfm69.rssi}")


def action_front_open():
    send_radio_command("Front Open", FRONT_NODE, payload_group_open())
    state["front"] = "open"
    state["charge_bay"] = "open"
    state["data_panel"] = "open"


def action_front_close():
    send_radio_command("Front Close", FRONT_NODE, payload_group_close())
    state["front"] = "closed"
    state["charge_bay"] = "closed"
    state["data_panel"] = "closed"


def action_front_arm_flail():
    send_radio_command("Arm Flail", FRONT_NODE, payload_front_arm_flail())
    state["status_message"] = "Sent: Arm Flail"


def action_charge_bay_toggle():
    send_radio_command("Charge Bay Toggle", FRONT_NODE, payload_front_charge_toggle())
    state["charge_bay"] = "open" if state["charge_bay"] != "open" else "closed"


def action_data_panel_toggle():
    send_radio_command("Data Panel Toggle", FRONT_NODE, payload_front_data_toggle())
    state["data_panel"] = "open" if state["data_panel"] != "open" else "closed"


def action_rear_open():
    send_radio_command("Rear Open", REAR_NODE, payload_group_open())
    state["rear"] = "open"
    state["rear_top"] = "open"


def action_rear_close():
    send_radio_command("Rear Close", REAR_NODE, payload_group_close())
    state["rear"] = "closed"
    state["rear_top"] = "closed"


def action_rear_top_toggle():
    send_radio_command("Rear Top Toggle", REAR_NODE, payload_rear_top_toggle())
    state["rear_top"] = "open" if state["rear_top"] != "open" else "closed"


def action_dome_open():
    send_radio_command("Dome Open", DOME_NODE, payload_dome_open())
    state["dome"] = "open"


def action_dome_close():
    send_radio_command("Dome Close", DOME_NODE, payload_dome_close())
    state["dome"] = "closed"


def action_dome_wave():
    send_radio_command("Dome Wave", DOME_NODE, payload_dome_wave())
    state["dome"] = "wave"


def action_sound(bank):
    state["selected_sound"] = bank
    send_radio_command(f"Sound {bank}: {sound_label(bank)}", BODY_NODE, payload_sound_bank(bank))


def action_sound_minus():
    state["selected_sound"] -= 1
    if state["selected_sound"] < 1:
        state["selected_sound"] = 13
    bank = state["selected_sound"]
    state["status_message"] = f"Selected sound {bank}: {sound_label(bank)}"
    oled("Sound Select", f"{bank}: {sound_label(bank)}", "")


def action_sound_plus():
    state["selected_sound"] += 1
    if state["selected_sound"] > 13:
        state["selected_sound"] = 1
    bank = state["selected_sound"]
    state["status_message"] = f"Selected sound {bank}: {sound_label(bank)}"
    oled("Sound Select", f"{bank}: {sound_label(bank)}", "")


def action_play_selected_sound():
    bank = state["selected_sound"]
    send_radio_command(f"Sound {bank}: {sound_label(bank)}", BODY_NODE, payload_sound_bank(bank))


def action_open_all():
    action_front_open()
    action_rear_open()
    action_dome_open()
    state["status_message"] = "Sent: Open All"


def action_close_all():
    action_dome_close()
    action_rear_close()
    action_front_close()
    state["status_message"] = "Sent: Close All"


def action_wake_up():
    action_sound(1)
    action_dome_open()
    state["status_message"] = "Preset: Wake Up"


def action_warning_all_open():
    action_sound(7)
    action_open_all()
    state["status_message"] = "Preset: Warning + Open All"


def action_shutdown():
    state["confirm_shutdown"] = True
    state["confirm_exit"] = False
    state["confirm_wifi_off"] = False
    state["status_message"] = "Confirm shutdown"


def action_exit():
    state["confirm_exit"] = True
    state["confirm_shutdown"] = False
    state["confirm_wifi_off"] = False
    state["status_message"] = "Confirm UI exit"


class Button:
    def __init__(self, label_func, rect, action, color_func, group=None):
        self.label_func = label_func
        self.rect = pygame.Rect(rect)
        self.action = action
        self.color_func = color_func
        self.group = group

    @property
    def label(self):
        return self.label_func() if callable(self.label_func) else self.label_func

    @property
    def color(self):
        return self.color_func() if callable(self.color_func) else self.color_func


buttons = []
selected_index = 0


def add_button(label, rect, action, color, group=None):
    buttons.append(Button(label, rect, action, color, group))


def wifi_button_label():
    if state["wifi_status"] == "enabled":
        return "WiFi OFF"
    if state["wifi_status"] == "disabled":
        return "WiFi ON"
    return "WiFi ?"


def wifi_button_color():
    if state["wifi_status"] == "enabled":
        return BUTTON_WIFI_OFF
    if state["wifi_status"] == "disabled":
        return BUTTON_WIFI_ON
    return BUTTON_SYSTEM


def toggle_state_color(state_key):
    return BUTTON_OPEN if state.get(state_key) == "open" else BUTTON_CLOSE


def build_buttons(width, height):
    buttons.clear()

    margin = 30
    top = 120
    bottom_h = 135
    gap = 22

    col_w = (width - margin * 2 - gap * 3) // 4
    col_x = [margin + i * (col_w + gap) for i in range(4)]

    button_h = 66
    inner_gap = 14
    y0 = top + 82

    x = col_x[0]
    add_button("General", (x + 20, y0, col_w - 40, button_h), lambda: action_sound(1), BUTTON_SOUND, "body")
    add_button("Chatty", (x + 20, y0 + 1 * (button_h + inner_gap), col_w - 40, button_h), lambda: action_sound(2), BUTTON_SOUND, "body")
    add_button("Whistle", (x + 20, y0 + 2 * (button_h + inner_gap), col_w - 40, button_h), lambda: action_sound(5), BUTTON_SOUND, "body")
    add_button("Scream", (x + 20, y0 + 3 * (button_h + inner_gap), col_w - 40, button_h), lambda: action_sound(6), BUTTON_SOUND, "body")
    add_button("Warning", (x + 20, y0 + 4 * (button_h + inner_gap), col_w - 40, button_h), lambda: action_sound(7), BUTTON_SOUND, "body")
    add_button("Leia", (x + 20, y0 + 5 * (button_h + inner_gap), col_w - 40, button_h), lambda: action_sound(9), BUTTON_SOUND, "body")
    add_button("Sound -", (x + 20, y0 + 6 * (button_h + inner_gap), (col_w - 58) // 2, button_h), action_sound_minus, BUTTON_SOUND, "body")
    add_button("Sound +", (x + 20 + (col_w - 58) // 2 + 18, y0 + 6 * (button_h + inner_gap), (col_w - 58) // 2, button_h), action_sound_plus, BUTTON_SOUND, "body")
    add_button("Play Selected", (x + 20, y0 + 7 * (button_h + inner_gap), col_w - 40, button_h), action_play_selected_sound, BUTTON_SOUND, "body")

    x = col_x[1]
    add_button("Open Front", (x + 20, y0, col_w - 40, 66), action_front_open, BUTTON_OPEN, "front")
    add_button("Close Front", (x + 20, y0 + 80, col_w - 40, 66), action_front_close, BUTTON_CLOSE, "front")
    add_button("Arm Flail", (x + 20, y0 + 160, col_w - 40, 66), action_front_arm_flail, BUTTON_PRESET, "front")
    add_button("Charge Bay", (x + 20, y0 + 240, col_w - 40, 66), action_charge_bay_toggle, lambda: toggle_state_color("charge_bay"), "front")
    add_button("Data Panel", (x + 20, y0 + 320, col_w - 40, 66), action_data_panel_toggle, lambda: toggle_state_color("data_panel"), "front")
    add_button("Wake Up", (x + 20, y0 + 400, col_w - 40, 66), action_wake_up, BUTTON_PRESET, "front")

    x = col_x[2]
    add_button("Open Rear", (x + 20, y0, col_w - 40, 76), action_rear_open, BUTTON_OPEN, "rear")
    add_button("Close Rear", (x + 20, y0 + 92, col_w - 40, 76), action_rear_close, BUTTON_CLOSE, "rear")
    add_button("Rear Top", (x + 20, y0 + 184, col_w - 40, 76), action_rear_top_toggle, lambda: toggle_state_color("rear_top"), "rear")

    x = col_x[3]
    add_button("Open Dome", (x + 20, y0, col_w - 40, 86), action_dome_open, BUTTON_OPEN, "dome")
    add_button("Close Dome", (x + 20, y0 + 106, col_w - 40, 86), action_dome_close, BUTTON_CLOSE, "dome")
    add_button("Dome Wave", (x + 20, y0 + 232, col_w - 40, 86), action_dome_wave, BUTTON_PRESET, "dome")
    add_button("Warn + Open", (x + 20, y0 + 338, col_w - 40, 86), action_warning_all_open, BUTTON_PRESET, "dome")

    by = height - bottom_h + 25
    bw = (width - margin * 2 - gap * 5) // 6

    add_button("Open All", (margin, by, bw, 88), action_open_all, BUTTON_OPEN, "global")
    add_button("Close All", (margin + 1 * (bw + gap), by, bw, 88), action_close_all, BUTTON_CLOSE, "global")
    add_button(wifi_button_label, (margin + 2 * (bw + gap), by, bw, 88), action_wifi_toggle, wifi_button_color, "global")
    add_button("Status", (margin + 3 * (bw + gap), by, bw, 88), lambda: None, BUTTON_SYSTEM, "global")
    add_button("Shutdown", (margin + 4 * (bw + gap), by, bw, 88), action_shutdown, BUTTON_DANGER, "global")
    add_button("EXIT", (margin + 5 * (bw + gap), by, bw, 88), action_exit, BUTTON_DANGER, "global")


def panel_header_color(area):
    value = state.get(area, "unknown")
    if value == "open":
        return PANEL_HEADER_OPEN
    if value == "closed":
        return PANEL_HEADER_CLOSED
    return PANEL_HEADER


def draw_text(screen, text, font, color, center=None, topleft=None):
    surf = font.render(text, True, color)
    rect = surf.get_rect()
    if center:
        rect.center = center
    elif topleft:
        rect.topleft = topleft
    screen.blit(surf, rect)


def draw_panel(screen, rect, title, subtitle, font_title, font_small, header_color=PANEL_HEADER):
    pygame.draw.rect(screen, PANEL, rect, border_radius=22)
    header = pygame.Rect(rect.x, rect.y, rect.w, 58)
    pygame.draw.rect(screen, header_color, header, border_radius=22)
    pygame.draw.rect(screen, header_color, (rect.x, rect.y + 30, rect.w, 40))
    draw_text(screen, title, font_title, TEXT, center=(rect.centerx, rect.y + 28))
    draw_text(screen, subtitle, font_small, TEXT_DIM, center=(rect.centerx, rect.y + 62))


def draw_button(screen, button, font_button, selected=False):
    pygame.draw.rect(screen, button.color, button.rect, border_radius=18)
    if selected:
        pygame.draw.rect(screen, SELECTED, button.rect, width=6, border_radius=18)
    draw_text(screen, button.label, font_button, TEXT, center=button.rect.center)


def draw_confirm_dialog(screen, title, message, font_title, font_button, font_small):
    width, height = screen.get_size()

    overlay = pygame.Rect(width // 2 - 430, height // 2 - 160, 860, 320)
    pygame.draw.rect(screen, (68, 30, 34), overlay, border_radius=24)
    pygame.draw.rect(screen, SELECTED, overlay, width=6, border_radius=24)

    draw_text(screen, title, font_title, TEXT, center=(width // 2, height // 2 - 90))
    draw_text(screen, message, font_small, TEXT_DIM, center=(width // 2, height // 2 - 35))

    yes_rect = pygame.Rect(width // 2 - 260, height // 2 + 35, 220, 88)
    no_rect = pygame.Rect(width // 2 + 40, height // 2 + 35, 220, 88)

    pygame.draw.rect(screen, BUTTON_CONFIRM, yes_rect, border_radius=18)
    pygame.draw.rect(screen, BUTTON_CANCEL, no_rect, border_radius=18)

    draw_text(screen, "YES", font_button, TEXT, center=yes_rect.center)
    draw_text(screen, "NO", font_button, TEXT, center=no_rect.center)

    return yes_rect, no_rect


def draw_ui(screen, fonts):
    width, height = screen.get_size()
    screen.fill(BG)

    title_font, header_font, small_font, button_font, status_font = fonts

    draw_text(screen, "R2N2 FIELD CONTROL", title_font, TEXT, topleft=(30, 22))

    radio_status = (
        f"RADIO OK   WiFi: {state['wifi_status'].upper()}   "
        f"Last RX: {state['last_rx']}   RSSI: {state['last_rssi']}"
    )
    draw_text(screen, radio_status, status_font, STATUS_OK, topleft=(width - 860, 34))

    status_bar = pygame.Rect(30, 78, width - 60, 34)
    pygame.draw.rect(screen, (20, 24, 32), status_bar, border_radius=10)
    draw_text(
        screen,
        f"Last Command: {state['last_command']}     |     {state['status_message']}",
        status_font,
        TEXT_DIM,
        topleft=(48, 84),
    )

    margin = 30
    top = 120
    bottom_h = 135
    gap = 22
    col_w = (width - margin * 2 - gap * 3) // 4
    panel_h = height - top - bottom_h - margin
    col_x = [margin + i * (col_w + gap) for i in range(4)]

    selected_bank = state["selected_sound"]

    draw_panel(
        screen,
        pygame.Rect(col_x[0], top, col_w, panel_h),
        "BODY / SOUNDS",
        f"Selected: {selected_bank} {sound_label(selected_bank)}",
        header_font,
        small_font,
        PANEL_HEADER,
    )
    draw_panel(
        screen,
        pygame.Rect(col_x[1], top, col_w, panel_h),
        "FRONT",
        f"State: {state['front']}",
        header_font,
        small_font,
        panel_header_color("front"),
    )
    draw_panel(
        screen,
        pygame.Rect(col_x[2], top, col_w, panel_h),
        "REAR",
        f"State: {state['rear']}",
        header_font,
        small_font,
        panel_header_color("rear"),
    )
    draw_panel(
        screen,
        pygame.Rect(col_x[3], top, col_w, panel_h),
        "DOME",
        f"State: {state['dome']}",
        header_font,
        small_font,
        panel_header_color("dome"),
    )

    for i, button in enumerate(buttons):
        draw_button(screen, button, button_font, selected=(i == selected_index))

    yes_rect = None
    no_rect = None

    if state["confirm_shutdown"]:
        yes_rect, no_rect = draw_confirm_dialog(
            screen,
            "CONFIRM SHUTDOWN",
            "This will safely shut down the Raspberry Pi.",
            header_font,
            button_font,
            small_font,
        )
    elif state["confirm_exit"]:
        yes_rect, no_rect = draw_confirm_dialog(
            screen,
            "CONFIRM EXIT",
            "This exits the UI but leaves the Pi running.",
            header_font,
            button_font,
            small_font,
        )
    elif state["confirm_wifi_off"]:
        yes_rect, no_rect = draw_confirm_dialog(
            screen,
            "TURN WIFI OFF?",
            "SSH/Raspberry Pi Connect will disconnect.",
            header_font,
            button_font,
            small_font,
        )

    pygame.display.flip()
    return yes_rect, no_rect


def button_center(button):
    return button.rect.centerx, button.rect.centery


def move_selection(dx, dy):
    global selected_index

    if not buttons:
        return

    current = buttons[selected_index]
    cx, cy = button_center(current)

    # -----------------------------
    # UP / DOWN: stay in same column
    # -----------------------------
    if dy != 0:
        same_column = []

        for i, b in enumerate(buttons):
            if i == selected_index:
                continue

            bx, by = button_center(b)

            # same visual column tolerance
            if abs(bx - cx) < 90:
                if dy < 0 and by < cy:
                    same_column.append((cy - by, i))
                elif dy > 0 and by > cy:
                    same_column.append((by - cy, i))

        if same_column:
            same_column.sort()
            selected_index = same_column[0][1]
            return

        # If moving down out of a column, jump to first bottom-row item.
        if dy > 0:
            bottom_items = sorted(
                [(b.rect.y, b.rect.x, i) for i, b in enumerate(buttons) if b.group == "global"]
            )
            if bottom_items:
                selected_index = bottom_items[0][2]
            return

        # If moving up from bottom row, go to nearest item above in same-ish x range.
        if dy < 0 and current.group == "global":
            candidates = []

            for i, b in enumerate(buttons):
                if b.group == "global":
                    continue

                bx, by = button_center(b)
                if by < cy:
                    score = abs(bx - cx) * 3 + abs(cy - by)
                    candidates.append((score, i))

            if candidates:
                candidates.sort()
                selected_index = candidates[0][1]

            return

    # -----------------------------
    # LEFT / RIGHT: move across same row
    # -----------------------------
    if dx != 0:
        same_row = []

        for i, b in enumerate(buttons):
            if i == selected_index:
                continue

            bx, by = button_center(b)

            # same visual row tolerance
            if abs(by - cy) < 90:
                if dx < 0 and bx < cx:
                    same_row.append((cx - bx, i))
                elif dx > 0 and bx > cx:
                    same_row.append((bx - cx, i))

        if same_row:
            same_row.sort()
            selected_index = same_row[0][1]
            return

        # If there is nothing to the right on that row,
        # jump to the top item in the next column to the right.
        if dx > 0:
            right_columns = []

            for i, b in enumerate(buttons):
                bx, by = button_center(b)
                if bx > cx:
                    right_columns.append((bx, by, i))

            if right_columns:
                # find nearest column to the right, then top item in that column
                nearest_x = min(x for x, y, i in right_columns)
                column_items = [(y, i) for x, y, i in right_columns if abs(x - nearest_x) < 90]
                column_items.sort()
                selected_index = column_items[0][1]
            return

        # If there is nothing to the left on that row,
        # jump to the top item in the previous column to the left.
        if dx < 0:
            left_columns = []

            for i, b in enumerate(buttons):
                bx, by = button_center(b)
                if bx < cx:
                    left_columns.append((bx, by, i))

            if left_columns:
                nearest_x = max(x for x, y, i in left_columns)
                column_items = [(y, i) for x, y, i in left_columns if abs(x - nearest_x) < 90]
                column_items.sort()
                selected_index = column_items[0][1]
            return


def activate_selected():
    if buttons:
        buttons[selected_index].action()


def handle_confirm_yes():
    if state["confirm_shutdown"]:
        oled("R2N2 Control", "Shutting down", "")
        pygame.quit()
        os.system("sudo shutdown now")
        return "shutdown"

    if state["confirm_exit"]:
        return "exit"

    if state["confirm_wifi_off"]:
        state["confirm_wifi_off"] = False
        wifi_off()

    return None


def cancel_confirm():
    state["confirm_shutdown"] = False
    state["confirm_exit"] = False
    state["confirm_wifi_off"] = False
    state["status_message"] = "Cancelled"


def handle_mouse_click(pos, yes_rect, no_rect):
    global selected_index

    if state["confirm_shutdown"] or state["confirm_exit"] or state["confirm_wifi_off"]:
        if yes_rect and yes_rect.collidepoint(pos):
            return handle_confirm_yes()
        if no_rect and no_rect.collidepoint(pos):
            cancel_confirm()
        return None

    for i, b in enumerate(buttons):
        if b.rect.collidepoint(pos):
            selected_index = i
            activate_selected()
            break

    return None


def main():
    global selected_index

    time.sleep(STARTUP_DELAY_SECONDS)
    oled("R2N2 GUI", "Starting", "Radio ready")
    update_wifi_status()

    pygame.init()
    pygame.mouse.set_visible(True)

    screen = pygame.display.set_mode((0, 0), pygame.FULLSCREEN)
    pygame.display.set_caption("R2N2 Field Control")

    width, height = screen.get_size()
    build_buttons(width, height)
    selected_index = 0

    title_font = pygame.font.SysFont(None, 64)
    header_font = pygame.font.SysFont(None, 42)
    small_font = pygame.font.SysFont(None, 32)
    button_font = pygame.font.SysFont(None, 34)
    status_font = pygame.font.SysFont(None, 30)
    fonts = (title_font, header_font, small_font, button_font, status_font)

    clock = pygame.time.Clock()
    running = True
    yes_rect = None
    no_rect = None
    last_wifi_status_check = 0

    while running:
        receive_once()

        now = time.monotonic()
        if now - last_wifi_status_check > 5:
            update_wifi_status()
            last_wifi_status_check = now

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.KEYDOWN:
                key = event.key

                # Twiddler navigation only:
                # A = left, E = right, B = up, C = down, D = select/confirm
                if key in (pygame.K_a, pygame.K_LEFT):
                    if state["confirm_shutdown"] or state["confirm_exit"] or state["confirm_wifi_off"]:
                        cancel_confirm()
                    else:
                        move_selection(-1, 0)

                elif key in (pygame.K_e, pygame.K_RIGHT):
                    if not (state["confirm_shutdown"] or state["confirm_exit"] or state["confirm_wifi_off"]):
                        move_selection(1, 0)

                elif key in (pygame.K_b, pygame.K_UP):
                    if state["confirm_shutdown"] or state["confirm_exit"] or state["confirm_wifi_off"]:
                        cancel_confirm()
                    else:
                        move_selection(0, -1)

                elif key in (pygame.K_c, pygame.K_DOWN):
                    if not (state["confirm_shutdown"] or state["confirm_exit"] or state["confirm_wifi_off"]):
                        move_selection(0, 1)

                elif key in (pygame.K_d, pygame.K_RETURN, pygame.K_KP_ENTER, pygame.K_SPACE):
                    if state["confirm_shutdown"] or state["confirm_exit"] or state["confirm_wifi_off"]:
                        result = handle_confirm_yes()
                        if result in ("exit", "shutdown"):
                            running = False
                    else:
                        activate_selected()

            elif event.type == pygame.MOUSEBUTTONDOWN:
                result = handle_mouse_click(event.pos, yes_rect, no_rect)
                if result in ("exit", "shutdown"):
                    running = False

        yes_rect, no_rect = draw_ui(screen, fonts)
        clock.tick(FPS)

    pygame.quit()
    oled("R2N2 Control", "Stopped", "")


if __name__ == "__main__":
    main()
