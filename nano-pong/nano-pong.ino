#include <U8g2lib.h>


enum ButtonId {
    BUTTON_A,
    BUTTON_B,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT
};

const byte BUTTON_COUNT = 6;

struct Button {
    byte pin;
    boolean down;
    boolean pressed;
};

Button buttons[BUTTON_COUNT];

// Requires SDA=A4, SCL=A5
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

enum Page {
    SPLASH,
    PLAY,
    GAME_OVER
};

Page page = SPLASH;

u8g2_uint_t ball_radius = 3;
u8g2_uint_t ball_x = ball_radius + 1;
u8g2_uint_t ball_y = ball_radius + 1;
u8g2_uint_t ball_dx = 2;
u8g2_uint_t ball_dy = 3;

u8g2_uint_t display_height = 0;
u8g2_uint_t display_width = 0;

int paddle_h = 4;
int paddle_w = 20;
int paddle_x = 0;
int paddle_r = 2;
int paddle_vel = 6;

void drawCenter(int y, const char *s) {
    int w = u8g2.getStrWidth(s);
    int x = (display_width - w) / 2;
    u8g2.drawStr(x, y, s);
}

void setup() {

    buttons[BUTTON_RIGHT].pin = 2;
    buttons[BUTTON_DOWN].pin = 3;
    buttons[BUTTON_UP].pin = 4;
    buttons[BUTTON_LEFT].pin = 5;
    buttons[BUTTON_B].pin = 6;
    buttons[BUTTON_A].pin = 7;

    for (byte i = 0; i < BUTTON_COUNT; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }

    u8g2.begin();

    display_height = u8g2.getDisplayHeight();
    display_width = u8g2.getDisplayWidth();

}


void detect_buttons() {
    for (byte i = 0; i < BUTTON_COUNT; i++) {
        int val = digitalRead(buttons[i].pin);
        buttons[i].pressed = (val == LOW && !buttons[i].down);
        buttons[i].down = (val == LOW);
    }
}


void update_ball() {
    ball_x += ball_dx;
    ball_y += ball_dy;

    if (ball_x - ball_radius <= 0 || ball_x + ball_radius >= display_width) {
        ball_dx = -ball_dx;
    }

    if (ball_y - ball_radius <= 0 || ball_y + ball_radius >= display_height) {
        ball_dy = -ball_dy;
    }
}


void draw_ball() {
    u8g2.drawDisc(ball_x, ball_y, ball_radius, U8G2_DRAW_ALL);
}


void update_paddle() {
    if (buttons[BUTTON_RIGHT].down && paddle_x + paddle_w < display_width) {
        paddle_x += paddle_vel;
    } else if (buttons[BUTTON_LEFT].down && paddle_x > 0) {
        paddle_x -= paddle_vel;
    }
}


void draw_paddle() {
    u8g2.drawRBox(paddle_x, display_height - paddle_h, paddle_w, paddle_h, paddle_r);
}

void draw_splash() {

    u8g2.setFont(u8g2_font_7x13_tr);
    drawCenter(20, "nano");

    u8g2.setFont(u8g2_font_michaelmouse_tu);
    drawCenter(35, "PONG");

    u8g2.setFont(u8g2_font_6x12_tr);
    drawCenter(60, "Press A to begin");

    if (buttons[BUTTON_A].pressed) {
        page = PLAY;
    }
}

void draw_play() {
    update_ball();
    update_paddle();
    draw_ball();
    draw_paddle();
}

void draw_game_over() {
}

void loop(void) {

    u8g2.clearBuffer();
    detect_buttons();

    if (page == SPLASH) {
        draw_splash();
    } else if (page == PLAY) {
        draw_play();
    } else {
        draw_game_over();
    }

    u8g2.sendBuffer();
    //delay(16);
}
