#include <U8g2lib.h>

// Nano Pong

/**
 * Constants
 */

const int ball_radius = 3;

const int paddle_w = 20;
const int paddle_h = 4;
const int paddle_vel = 6;

const int score_h = 10;

/** Buffer into which we can copy strings from PROGMEM */
char buf[20];


/**
 * Display driver. We need this at various points, so we just
 * initialize it first.
 *
 * Requires SDA=A4, SCL=A5
 */
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);


/**
 * At a high level, the game is a finite state machine whose current state is
 * kept in `game_state`.
 */

typedef enum {
    S_SPLASH,      /** Splash screen */
    S_READY,       /** Get ready */
    S_SET,         /** Get set */
    S_PLAYING,     /** Playing. Ball is moving and paddle is enabled. */
    S_DOH,         /** Paddle missed the ball, pause before using next ball. */
    S_GAME_OVER    /** Game over screen */
} GameState;

GameState game_state;

int score;

int balls_remaining;


/**
 * Events things like button presses and timers. The game runs in an infinite
 * loop as the Arduino runtime repeatedly calls the `loop()` function. We call
 * each iteration of the loop a "tick".
 *
 * On each tick, we check to see if a button was pressed, if a timer fired, or
 * if an entity of interest collided with another one. We put these events in a
 * queue to be handled by the next step of the game loop.
 *
 * The event queue is an array of events. `event_count` points to the next
 * available slot.
 */

typedef enum {
    EV_BUTTON_DOWN,
    EV_BUTTON_UP,
    EV_TIMER,
    EV_COLLISION
} EventType;

typedef struct {
    EventType event_type;  /** Type of event */
    uint8_t subject;          /** Button that was pushed, timer that fired, or entity that collided. */
    uint8_t object;           /** In a collision, the "other" entity. */
} Event;

const short EVENT_QUEUE_SIZE = 5;

Event event_queue[EVENT_QUEUE_SIZE];

short event_count = 0;

void enqueue_event(EventType event_type, short subject, short object) {
    event_queue[event_count].event_type = event_type;
    event_queue[event_count].subject = subject;
    event_queue[event_count].object = object;
    event_count++;
}


/**
 * The player interacts with the game by pressing buttons. Here we configure
 * which Arduino pin each button is connected to. On each tick we call
 * `check_buttons`, which updates the `down` field for each button and also
 * queues button events to the event queue when their state changes.
 */

typedef struct {
    uint8_t pin;
    boolean down;
} Button;

typedef enum {
    BUTTON_A,
    BUTTON_B,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_COUNT
} ButtonId;

Button buttons[] = {
    {7, false},           // A
    {6, false},           // B
    {4, false},           // UP
    {3, false},           // DOWN
    {5, false},           // LEFT
    {2, false}            // RIGHT
};

void setup_buttons() {
    for (short i = 0; i < BUTTON_COUNT; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }
}

void check_buttons() {
    for (short i = 0; i < BUTTON_COUNT; i++) {
        int val = digitalRead(buttons[i].pin);
        if (val == LOW && !buttons[i].down) {
            enqueue_event(EV_BUTTON_DOWN, i, 0);
        } else if (val == HIGH && buttons[i].down) {
            enqueue_event(EV_BUTTON_UP, i, 0);
        }
        buttons[i].down = (val == LOW);
    }
}


/**
 * We can set a timer so that the game loop receives a timer event
 * some fixed time in the future. Each timer is just an unsigned
 * long value that we compare with `millis()` on each tick.
 */

const short TIMER_COUNT = 1;

unsigned long timers[TIMER_COUNT];

void set_timer(short timer_id, unsigned long ms) {
    timers[timer_id] = millis() + ms;
}

void check_timers() {
    unsigned long now = millis();
    for (short i = 0; i < TIMER_COUNT; i++) {
        if (timers[i] > 0 && now > timers[i]) {
            enqueue_event(EV_TIMER, i, 0);
            timers[i] = 0;
        }
    }
}


/**
 * Entity is an object on the screen that is subject to collisions.
 * The system also updates x and y by dx and dy on every tick.
 *
 * For pong there are a fixed number of entities, all stored in the
 * `entities` array and identified by their index in the array.
 *
 * We don't automatically check for all object collisions as this would
 * be slow and unnecessary. After all, you don't need to check if your
 * walls collide with each other! Instead, in the game loop we check for
 * collisions with a particular object, primarily the ball.
 *
 * `check_collisions` checks for a collision between a given entity and
 * all other entities, queueing collision events as required.
 */
typedef struct {
    short x;
    short y;
    uint8_t w;
    uint8_t h;
    int8_t dx;
    int8_t dy;
} Entity;

typedef enum {
    E_BALL,
    E_PADDLE,
    E_WALL_N,
    E_WALL_S,
    E_WALL_E,
    E_WALL_W,
    E_COUNT
} EntityId;

Entity entities[E_COUNT];

void check_collisions(short entity_index) {
    Entity *p = &entities[entity_index];
    for (short i = 0; i < E_COUNT; i++) {
        Entity *q = &entities[i];
        if (i != entity_index
                && p->x <= q->x + q->w
                && p->x + p->w >= q->x
                && p->y <= q->y + q->h
                && p->y + p->h >= q->y) {
            enqueue_event(EV_COLLISION, entity_index, i);
        }
    }
}

void update_entities() {
    for (short i = 0; i < E_COUNT; i++) {
        entities[i].x += entities[i].dx;
        entities[i].y += entities[i].dy;
    }
}

void init_entity(
        short entity_id,
        short x,
        short y,
        short w,
        short h,
        short dx,
        short dy) {
    Entity *p = &entities[entity_id];
    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;
    p->dx = dx;
    p->dy = dy;
}

void setup_entities() {
    u8g2_uint_t disp_h = u8g2.getDisplayHeight();
    u8g2_uint_t disp_w = u8g2.getDisplayWidth();
    init_entity(E_BALL, 0, 0, 2 * ball_radius, 2 * ball_radius, 0, 0);
    init_entity(E_PADDLE, 0, disp_h - paddle_h - score_h, paddle_w, paddle_h, 0, 0);
    init_entity(E_WALL_N, 0, 0, disp_w, 0, 0, 0);
    init_entity(E_WALL_S, 0, disp_h, disp_w, 0, 0, 0);
    init_entity(E_WALL_E, 0, 0, 0, disp_h, 0, 0);
    init_entity(E_WALL_W, disp_w, 0, 0, disp_h, 0, 0);
}


/**
 * The paddle is under user control, so we handle its position updates
 * explicitly. Entity updates are more for autonomous movement.
 */

void update_paddle() {
    u8g2_uint_t disp_h = u8g2.getDisplayHeight();
    u8g2_uint_t disp_w = u8g2.getDisplayWidth();
    Entity *p = &entities[E_PADDLE];
    if (buttons[BUTTON_RIGHT].down && p->x + p->w < disp_w) {
        p->x += paddle_vel;
    } else if (buttons[BUTTON_LEFT].down && p->x > 0) {
        p->x -= paddle_vel;
    }
}



/**
 * Drawing utilities
 */

void draw_center(int y, const __FlashStringHelper *s) {
    strcpy_P(buf, (char*) s);
    int w = u8g2.getStrWidth(buf);
    int disp_w = u8g2.getDisplayWidth();
    int x = (disp_w - w) / 2;
    u8g2.drawStr(x, y, buf);
}

void draw_ball() {
    Entity *p = &entities[E_BALL];
    u8g2.drawDisc(p->x + ball_radius, p->y + ball_radius, ball_radius, U8G2_DRAW_ALL);
}

void draw_paddle() {
    Entity *p = &entities[E_PADDLE];
    u8g2.drawRBox(p->x, p->y, p->w, p->h, 2);
}

void draw_score() {

    u8g2_uint_t disp_h = u8g2.getDisplayHeight();
    u8g2_uint_t disp_w = u8g2.getDisplayWidth();

    for (int i = 0; i < balls_remaining; i++) {
        u8g2.drawDisc(ball_radius + i * ball_radius * 2 + i * 2, disp_h - ball_radius - 1, ball_radius);
    }

    sprintf(buf, "%d", score);
    //strcpy(buf, "blah"); // testing
    u8g2.setFont(u8g2_font_7x13_tr);
    int w = u8g2.getStrWidth(buf);
    u8g2.drawStr(disp_w - w, disp_h, buf);
}


/**
 * Functions to draw each game state.
 */

void draw_splash() {

    u8g2.setFont(u8g2_font_7x13_tr);
    draw_center(20, F("nano"));

    u8g2.setFont(u8g2_font_michaelmouse_tu);
    draw_center(35, F("PONG"));

    u8g2.setFont(u8g2_font_6x12_tr);
    draw_center(60, F("Press A to begin"));

}

void draw_ready() {
    u8g2.setFont(u8g2_font_7x13_tr);
    draw_center(35, F("Ready"));
    draw_ball();
    draw_paddle();
    draw_score();
}

void draw_set() {
    u8g2.setFont(u8g2_font_7x13_tr);
    draw_center(35, F("Set"));
    draw_ball();
    draw_paddle();
    draw_score();
}

void draw_playing() {
    draw_ball();
    draw_paddle();
    draw_score();
}

void draw_doh() {
    u8g2.setFont(u8g2_font_7x13_tr);
    draw_center(35, F("Doh!"));
    draw_ball();
    draw_paddle();
    draw_score();
}

void draw_game_over() {
    u8g2.setFont(u8g2_font_7x13_tr);
    draw_center(35, F("Game Over"));
    draw_ball();
    draw_paddle();
    draw_score();
}

void draw_state() {
    switch (game_state) {
        case S_SPLASH:
            draw_splash();
            break;
        case S_READY:
            draw_ready();
            break;
        case S_SET:
            draw_set();
            break;
        case S_PLAYING:
            draw_playing();
            break;
        case S_DOH:
            draw_doh();
            break;
        case S_GAME_OVER:
            draw_game_over();
            break;
    }
}



/**
 * Event handling. This is where the game dynamics come together.
 * Here we describe handlers for important combinations of
 * game state + event.
 */

boolean event_matches(short event_index, GameState gs, EventType event_type, short subject, short object) {
    Event *p = &event_queue[event_index];
    return gs == game_state
        && p->event_type == event_type
        && p->subject == subject
        && p->object == object;
}

void start_playing() {
    Entity *p = &entities[E_BALL];
    p->x = ball_radius;
    p->y = ball_radius;
    p = &entities[E_PADDLE];
    p->x = 0;
    game_state = S_READY;
    set_timer(0, 1000);
}

void handle_event(short event_index) {
    if (event_matches(event_index, S_SPLASH, EV_BUTTON_DOWN, BUTTON_A, 0)) {
        score = 0;
        balls_remaining = 2;
        start_playing();
    } else if (event_matches(event_index, S_READY, EV_TIMER, 0, 0)) {
        game_state = S_SET;
        set_timer(0, 1000);
    } else if (event_matches(event_index, S_SET, EV_TIMER, 0, 0)) {
        Entity *p = &entities[E_BALL];
        p->dx = 2;
        p->dy = 3;
        game_state = S_PLAYING;
    } else if (event_matches(event_index, S_PLAYING, EV_COLLISION, E_BALL, E_WALL_N)) {
        Entity *p = &entities[E_BALL];
        p->dy = - p->dy;
    } else if (event_matches(event_index, S_PLAYING, EV_COLLISION, E_BALL, E_WALL_E)) {
        Entity *p = &entities[E_BALL];
        p->dx = - p->dx;
    } else if (event_matches(event_index, S_PLAYING, EV_COLLISION, E_BALL, E_WALL_W)) {
        Entity *p = &entities[E_BALL];
        p->dx = - p->dx;
    } else if (event_matches(event_index, S_PLAYING, EV_COLLISION, E_BALL, E_WALL_S)) {
        Entity *p = &entities[E_BALL];
        p->dx = 0;
        p->dy = 0;
        if (balls_remaining == 0) {
            game_state = S_GAME_OVER;
        } else {
            game_state = S_DOH;
            set_timer(0, 3000);
        }
    } else if (event_matches(event_index, S_PLAYING, EV_COLLISION, E_BALL, E_PADDLE)) {
        Entity *p = &entities[E_BALL];
        p->dy = - p->dy;
        score += 10;
    } else if (event_matches(event_index, S_DOH, EV_TIMER, 0, 0)) {
        balls_remaining--;
        start_playing();
    } else if (event_matches(event_index, S_GAME_OVER, EV_BUTTON_DOWN, BUTTON_A, 0)) {
        score = 0;
        balls_remaining = 2;
        start_playing();
    }
}

void handle_events() {
    for (short i = 0; i < event_count; i++) {
        handle_event(i);
    }
    event_count = 0;
}


/**
 * Entry points. Here are the standard `setup` and `loop` required
 * by the Arduino runtime.
 */

void setup() {
    u8g2.begin();
    setup_buttons();
    setup_entities();
}

void loop() {

    update_entities();
    update_paddle();

    check_buttons();
    check_timers();
    check_collisions(E_BALL);

    handle_events();

    u8g2.clearBuffer();
    draw_state();
    u8g2.sendBuffer();

}
