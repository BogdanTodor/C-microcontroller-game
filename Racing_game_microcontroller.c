#include <avr/io.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <lcd.h>
#include "lcd_model.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <sprite.h>
#include <string.h>
#include "cab202_adc.h"
#include "usb_serial.h"


#define w 84
#define h 48
#define THRESHOLD 512

void setup(void);
void process(void);
typedef enum { false, true } bool;
void draw_double(uint8_t x, uint8_t y, double value, colour_t colour);
void draw_int(uint8_t x, uint8_t y, int value, colour_t colour);
void draw_formatted(int x, int y, char * buffer, int buffer_size, const char * format, ...);
void send_formatted(char * buffer, int buffer_size, const char * format, ...);
void usb_serial_send(char * message);

#define BIT(x) (1 << (x))
#define OVERFLOW_TOP (1023)

uint8_t race_car_image[] = {
  0b11111111,
  0b11111111,
  0b00111100,
  0b00111100,
  0b00111100,
  0b11111111,
  0b11111111,
};

Sprite race_car;

uint8_t tree_image[] = {
  0b00111100,
  0b01111110,
  0b11111111,
  0b01111110,
  0b00011000,
  0b00011000,
  0b00011000,
};

#define Max_trees (2)
Sprite tree[Max_trees];
int tree_count = 2;

uint8_t debris_image[] = {
  0b11100000,
  0b11100000,
  0b11100000,
};

#define Max_debris (6)
Sprite debris[Max_debris];
int debris_count = 6;


uint8_t fuel_depot_image[] = {
  0b11111111,
  0b11111111,
  0b10000001,
  0b10000001,
  0b10111101,
  0b10100001,
  0b10111101,
  0b10100001,
  0b10100001,
  0b10100001,
  0b10000001,
  0b10000001,
  0b11111111,
  0b11111111,
};

Sprite fuel_depot;


uint8_t finish_line_image[] = {
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
  0b10101010,0b10101010,0b10101010,0b10101010,0b10101010,0b10101010,
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
};

Sprite finish_line;

bool game_over = false;
int FD_x;
int on_road_left;
int on_road_right;
double speed = 0.0;
double fuel_total = 100.0;
double fuel_display;
int health = 100;
double distance_travelled = 0;
bool paused = false;
double time;
double refuel_start;
double refuel_end;
bool button_clicked();
int NEW_FD_y;
double elapsed_time;


// Start of game code.

// Collision between 2 objects
bool collision( Sprite object1, Sprite object2 ) {
  int top_object1 = round(object1.y);
  int left_object1 = round(object1.x);
  int right_object1 = left_object1 + round(object1.width) - 1;
  int bottom_object1 = top_object1 + round(object1.height) - 1;

  int top_object2 = round(object2.y);
  int left_object2 = round(object2.x);
  int right_object2 = left_object2 + round(object2.width) - 1 ;
  int bottom_object2 = top_object2 + round(object2.height) - 1;

  if ( top_object2 > bottom_object1 ) return false;
  if ( top_object1 > bottom_object2 ) return false;
  if ( left_object1 > right_object2 ) return false;
  if ( left_object2 > right_object1 ) return false;

  return true;
}

// Creating the timer
void timer(void){
  TCCR0A = 0;
  TCCR0B = 4;
  TIMSK0 = 1;
  sei();
}

void timer1(void){
  TCCR1A = 0;
  TCCR1B = 3;
  TIMSK1 = 1;
  sei();
}

// counter for get current time function
volatile uint32_t counter = 0;

// right button
volatile uint8_t bit_count_rb = 0;
volatile uint8_t pressed_rb;

// left button
volatile uint8_t bit_count_lb = 0;
volatile uint8_t pressed_lb;
//
// up joystick
volatile uint8_t bit_count_uj = 0;
volatile uint8_t pressed_uj;

// down joystick
volatile uint8_t bit_count_dj = 0;
volatile uint8_t pressed_dj;
//
// left joystick
volatile uint8_t bit_count_lj = 0;
volatile uint8_t pressed_lj;

// right joystick
volatile uint8_t bit_count_rj = 0;
volatile uint8_t pressed_rj;

ISR(TIMER0_OVF_vect){
  uint8_t mask = 0b00011111;
  // debouncing down joystick
  bit_count_dj = (bit_count_dj << 1);

  bit_count_dj = bit_count_dj & mask;

  if (BIT_IS_SET(PINB, 7)){
    bit_count_dj = bit_count_dj | 1;
  } else bit_count_dj = bit_count_dj | 0;

  if (bit_count_dj == mask){
    pressed_dj = 1;
  }

  if (bit_count_dj == 0){
    pressed_dj = 0;
  }

  // debouncing right button
  bit_count_rb = (bit_count_rb << 1);


  bit_count_rb = bit_count_rb & mask;

  if (BIT_IS_SET(PINF, 5)){
    bit_count_rb = bit_count_rb | 1;
  } else (bit_count_rb = bit_count_rb | 0);

  if (bit_count_rb == mask){
    pressed_rb = 1;
  }

  if (bit_count_rb == 0){
    pressed_rb = 0;
  }
  // debouncing left button
  bit_count_lb = (bit_count_lb << 1);

  bit_count_lb = bit_count_lb & mask;

  if (BIT_IS_SET(PINF, 6)){
    bit_count_lb = bit_count_lb | 1;
  } else bit_count_lb = bit_count_lb | 0;

  if (bit_count_lb == mask){
    pressed_lb = 1;
  }

  if (bit_count_lb == 0){
    pressed_lb = 0;
  }
  // debouncing up joystick
  bit_count_uj = (bit_count_uj << 1);

  bit_count_uj = bit_count_uj & mask;

  if (BIT_IS_SET(PIND, 1)){
    bit_count_uj = bit_count_uj | 1;
  } else bit_count_uj = bit_count_uj | 0;

  if (bit_count_uj == mask){
    pressed_uj = 1;
  }

  if (bit_count_uj == 0){
    pressed_uj = 0;
  }


  // // debouncing left joystick
  bit_count_lj = (bit_count_lj << 1);

  bit_count_lj = bit_count_lj & mask;

  if (BIT_IS_SET(PINB, 1)){
    bit_count_lj = bit_count_lj | 1;
  } else bit_count_lj = bit_count_lj | 0;

  if (bit_count_lj == mask){
    pressed_lj = 1;
  }

  if (bit_count_lj == 0){
    pressed_lj = 0;
  }
  // debouncing right joystick
  bit_count_rj = (bit_count_rj << 1);

  bit_count_rj = bit_count_rj & mask;

  if (BIT_IS_SET(PIND, 0)){
    bit_count_rj = bit_count_rj | 1;
  } else bit_count_rj = bit_count_rj | 0;

  if (bit_count_rj == mask){
    pressed_rj = 1;
  }

  if (bit_count_rj == 0){
    pressed_rj = 0;
  }

}

ISR(TIMER1_OVF_vect){
  if(!paused){
    counter++;
  }
}

double get_current_time(){
  double time = ( counter * 65536.0 + TCNT1) * 64.0/8000000.0;
  return time;
}

void draw_char_direct(int x, int y, char ch, colour_t colour) {
  if (x < 0 || x > LCD_X - CHAR_WIDTH || y < 0 || y > LCD_Y - CHAR_HEIGHT) {
    return;
  }

  LCD_CMD(lcd_set_function, lcd_instr_basic | lcd_addr_horizontal);
  LCD_CMD(lcd_set_x_addr, (x & 0x7f));
  LCD_CMD(lcd_set_y_addr, (y & 0x7f) / 8);

  for (int i = 0; i < CHAR_WIDTH; i++) {
    uint8_t pixelBlock = pgm_read_byte(&(ASCII[ch - ' '][i]));

    if (colour == BG_COLOUR) {
      pixelBlock = ~pixelBlock;
    }

    LCD_DATA(pixelBlock);
  }
}

void part_c_draw_string(int x, int y, char * str, colour_t colour) {
  for (int i = 0; str[i] != 0; i++, x += CHAR_WIDTH) {
    draw_char_direct(x, y, str[i], colour);
  }
}

void splash_screen(void){
  lcd_clear();
  while(!BIT_IS_SET(PINF, 5) && !BIT_IS_SET(PINF, 6)){
    part_c_draw_string(2, 1, "Welcome To:", FG_COLOUR);
    part_c_draw_string(2, 11, "Race To Zombie", FG_COLOUR);
    part_c_draw_string(2, 21, "Mountain:", FG_COLOUR);
    part_c_draw_string(2, 40, "By: Bogdan Todor:", FG_COLOUR);
  }
  if (pressed_rb || pressed_lb){
    clear_screen();
  }
}

void dashboard(void){
  // drawing the road
  draw_line(on_road_left, h-1, on_road_left, 1, '#');
  draw_line(on_road_right, h-1, on_road_right, 1, '#');

  //Draw label
  draw_string(1, 40, "                  ", BG_COLOUR);
  draw_string(1, 40, "S:", BG_COLOUR);
  draw_string(21, 40, "H:", BG_COLOUR);
  draw_string(48, 40, "F:", BG_COLOUR);

  draw_int(10, 40, speed, BG_COLOUR);
  draw_int(30, 40, health, BG_COLOUR);
  fuel_display = round(fuel_total * 100.0) / 100.0;
  draw_double(57, 40, fuel_display, BG_COLOUR);
}

void create_tree(void){
  for(int i = 0; i < tree_count; i++){
    int tree_y = rand() % (h + 1 - 1) + 1 ;
    int tree_x = rand() % (w - 12 + 1);

    while (tree_x <= on_road_right && (tree_x + 12) >= on_road_left ){
      tree_x = rand() % (w - 12);
    }

    sprite_init(&tree[i], tree_x, tree_y, 8, 7, tree_image);
  }
}

void move_tree(){
  for(int i = 0; i < tree_count; i++){
    tree[i].y = tree[i].y + 0.08*speed;
  }
}

void respawn_trees(bool fromCollision){

  for (int i = 0; i < tree_count; i++){
    int tree_x = rand() % (w - 12);

    while (tree_x <= on_road_right && (tree_x + 12) >= on_road_left ){
      tree_x = rand() % (w - 12);
    }

    if (tree[i].y > h-5){
      sprite_init(&tree[i], tree_x, -10, 8, 7, tree_image);
    }else if(fromCollision){
      if (collision(tree[i], fuel_depot) && tree_x > on_road_right){
        sprite_init(&tree[i], tree_x + 1 ,tree[i].y, 8, 7, tree_image);
      }
      if (collision(tree[i], fuel_depot) && tree_x < on_road_left){
        sprite_init(&tree[i], tree_x - 1 ,tree[i].y, 8, 7, tree_image);
      }
    }
  }
}

void create_debris(void){
  for(int i = 0; i < debris_count; i++){
    int debris_x = rand() % ((on_road_right - 10) + 1 - (on_road_left + 2)) + (on_road_left + 2);
    int debris_y = rand() % ((h/4 * 3) + 1 - 1) + 1 ;

    sprite_init(&debris[i], debris_x, debris_y, 3, 3, debris_image);
  }
}

void move_debris(){
  for(int i = 0; i < debris_count; i++){
    debris[i].y = debris[i].y + 0.08*speed;
  }
}

void respawn_debris(bool fromCollision){
  for (int i = 0; i < debris_count; i++){
    int debris_x = rand() % ((on_road_right - 10) + 1 - (on_road_left + 2)) + (on_road_left + 2);
    if (round(debris[i].y) > h-5){
      sprite_init(&debris[i], debris_x, -5, 3, 3, debris_image);
    }else if(fromCollision){
      sprite_init(&debris[i], debris_x ,(debris[i].y), 3, 3, debris_image);
    }
  }
}

// creating the finish line
void create_finish_line(void){
  sprite_init(&finish_line, on_road_left, -10, on_road_right - on_road_left, 3, finish_line_image);
}

void move_finish_line(){
  finish_line.y = finish_line.y + 0.08*speed;
}

// Function that creates the fuel depot
void create_fuel_depot (void){

  int FD_y = rand() % (h + 1 - 1) + 1;

  int FD_left_or_right = rand() % (2 + 1 -1) + 1;

  if(FD_left_or_right == 1){ // 1 = left
    FD_x = on_road_left - 8 + 1; //
  }

  if(FD_left_or_right == 2){ // 2 = right
    FD_x = on_road_right;
  }

  sprite_init(&fuel_depot, FD_x, FD_y, 8, 14, fuel_depot_image);
}

// Function states which conditions must be met for the fuel depot to start
// moving and the speed at which the fuel depot will move relative to the car
void move_fuel_depot(){
  fuel_depot.y = fuel_depot.y + 0.08 * speed;
}

void respawn_fuel_depot(void){
  // Drawing the fuel depot at randomised position
  int FD_left_or_right = rand() % (2 + 1 -1) + 1;

  if(FD_left_or_right == 1){ // 1 = left
    FD_x = on_road_left - 8 + 1; //
  }

  if(FD_left_or_right == 2){ // 2 = right
    FD_x = on_road_right;
  }


  if (fuel_depot.y > h){
    NEW_FD_y = rand() % (100 + 1 - 50) + 50;
    if (FD_left_or_right == 1){
      sprite_init(&fuel_depot, FD_x, -NEW_FD_y, 8, 14, fuel_depot_image);
    }
    if (FD_left_or_right == 2){
      sprite_init(&fuel_depot, FD_x, -NEW_FD_y, 8, 14, fuel_depot_image);
    }
  }
}


void draw_sprites(void){
  for(int i = 0; i < tree_count; i++){
    sprite_draw(&tree[i]);
  }
  for(int i = 0; i < debris_count; i++){
    sprite_draw(&debris[i]);
  }
  sprite_draw(&fuel_depot);
  sprite_draw(&finish_line);
  sprite_draw(&race_car);
}


void setup( void ) {
  set_clock_speed(CPU_8MHz);
  usb_init();
  lcd_init(LCD_LOW_CONTRAST);
  CLEAR_BIT(DDRB, 7); // down joystick activated
  CLEAR_BIT(DDRD, 1); // up joystick activated
  CLEAR_BIT(DDRD, 0); // right joystic
  CLEAR_BIT(DDRB, 1); // left joystick
  CLEAR_BIT(DDRB, 0); // centre joystick
  CLEAR_BIT(DDRF, 6); // left button
  CLEAR_BIT(DDRF, 5); // right button

  on_road_left = (w - 1) / 4 + 1;
  on_road_right = 3 * on_road_left;

  splash_screen();
  sprite_init(&race_car, (w/2) - 6, h-17 /* -17 to account for dashboard*/, 8, 7, race_car_image);
  create_tree();
  create_debris();
  create_finish_line();
  create_fuel_depot();
  sprite_draw(&race_car);
}

double tempTime1;
double newTime1;

double tempTime2;
double newTime2;

double tempTime3;
double newTime3;

double tempTime4;
double newTime4;

double tempTime5;
double newTime5;

double tempTime6;
double newTime6;

void move_car( void ){

  static uint8_t prevState_rb = 0;
  if(pressed_rb){
    tempTime1 = get_current_time();
    while(pressed_rb){
      newTime1 = get_current_time();
      if (newTime1 >= (tempTime1 + 0.03 )){
        break;
      }
      prevState_rb = pressed_rb;
      if(race_car.x >= on_road_left-1 && race_car.x <= on_road_right){
        if (speed >= 0.0 && speed < 10.0){
          speed += 0.105 * (newTime1 - tempTime1);
        }
      }

      if (race_car.x < on_road_left || race_car.x > on_road_right){
        if (speed >= 0 && speed < 3){
          speed += (0.105/3) * (newTime1 - tempTime1);
        }
      }
    }
    if(race_car.y > 40-(40/4)){
      race_car.y = race_car.y - 0.2;
    }

  } else if(!pressed_rb || newTime1 > 10.0){
    newTime1 = 0;
  }


  // brake button
  static uint8_t prevState_lb = 0;
  if(pressed_lb){
    tempTime2 = get_current_time();
    while(pressed_lb){
      newTime2 = get_current_time();
      if (newTime2 >= (tempTime2 + 0.03 )){
        break;
      }
      prevState_lb = pressed_lb;
      if ((speed > 0.0 && speed < 11.0)){ // since its a double, the 11 represents fractional values of 10.
        speed -= (0.105*1.7) * (newTime2 - tempTime2);
        if(speed < 0.0){
          speed = 0;
        }
      }
    }
  }


  // natural deceleration - 10 to 0 in 3 seconds - on road
  if ((!pressed_lb && !pressed_rb) && speed > 1.1){
    if(race_car.x >= on_road_left-1 && race_car.x <= on_road_right){
      tempTime3 = get_current_time();
      while ((!pressed_lb && !pressed_rb) && speed > 1.1){
        newTime3 = get_current_time();
        if (newTime3 >= (tempTime3 + 0.03 )){
          break;
        }
        speed -= (0.105*1.2) * (newTime3 - tempTime3);
      }
    }
  }
  // natural deceleration - 3 to 0 in 3 seconds - offroad
  if ((!pressed_lb && !pressed_rb) && speed > 1.1){
    tempTime4 = get_current_time();
    while ((!pressed_lb && !pressed_rb) && speed > 1.1){
      newTime4 = get_current_time();
      if (newTime4 >= (tempTime4 + 0.03 )){
        break;
      }
      speed -= (0.105*0.15) * (newTime4 - tempTime4);
    }
  }


  // natural acceleration - 0 to 1 in 2 seconds on road
  if ((!pressed_lb && !pressed_rb) && (speed < 1)){
    if(race_car.x >= on_road_left-1 && race_car.x <= on_road_right){
      tempTime5 = get_current_time();
      while ((!pressed_lb && !pressed_rb) && (speed < 1)){
        newTime5 = get_current_time();
        if (newTime5 >= (tempTime5 + 0.03 )){
          break;
        }
        speed += (0.105*0.24) * (newTime5 - tempTime5); // change
      }
    }
  }

  // natural acceleration - 0 to 1 in 3 seconds off road
  if ((!pressed_lb && !pressed_rb) && (speed < 1)){
    if (race_car.x < on_road_left || race_car.x > on_road_right){
      tempTime6 = get_current_time();
      while ((!pressed_lb && !pressed_rb) && (speed < 1)){
        newTime6 = get_current_time();
        if (newTime6 >= (tempTime6 + 0.03 )){
          break;
        }
        speed += (0.105*0.15) * (newTime6 - tempTime6);; // change
      }
    }
  }

  // regulating offroad maximum speed
  if((race_car.x+2 < on_road_left)||(race_car.x + race_car.width - 1 > on_road_right)){
    if(speed > 3){
      speed = 3;
    }
  }

  int left_adc = adc_read(0);
  // potentiometer
  if((race_car.x < (w - 1.000 - race_car.width)) && speed > 0 ){
    if (left_adc >= THRESHOLD){
      race_car.x += 0.001 * speed * (512 - (1024 - left_adc));
    }
  }

  // potentiometer
  if(race_car.x > 1.000 && speed > 0){
    if (left_adc <= THRESHOLD){
      race_car.x -= 0.001 * speed * (512 - left_adc);
    }
  }
}




// Fuel consumed - explain number choices
void fuel(void){
  if (fuel_total > 1){
    fuel_total -= 0.03 * speed;
  }
}

//Distance travelled
void distance(void){
  distance_travelled = 0.01 * speed + distance_travelled;
}



// // function to check collision between objects
void if_collided() {
  if (collision(fuel_depot, race_car ) ) {
    health = 0;
    speed = 0;
  }
  //
  // prevent overlapping of trees
  for ( int i = 0; i < tree_count; i++ ) {
    for ( int j = 0; j < tree_count; j++ ) {
      int d_x = tree[i].x;
      int d_y = tree[i].y;
      if(i != j){
        while ( collision( tree[i], tree[j] ) ) {
          sprite_init(&tree[i], d_x, d_y, 8, 7, tree_image);
        }
      }
    }
  }
  //
  for ( int i = 0; i < tree_count; i++ ) {
    while ( collision( fuel_depot, tree[i] ) ) {
      respawn_trees(true);
    }
  }
  //
  for ( int i = 0; i < debris_count; i++ ) {
    if ( collision( race_car, debris[i] ) ) {
      health = health - 20;
      speed = 0; // to account for left, right, top and bottom collision and the two layers of the roadblock
      int rcx = rand() % ((on_road_right - 6) + 1 - (on_road_left + 2)) + (on_road_left + 2);
      if(collision(debris[i], race_car)){
        sprite_init(&race_car, rcx, h-17, 8, 7, race_car_image);
      }
    }
  }

  for ( int i = 0; i < tree_count; i++ ) {
    if ( collision( race_car, tree[i] ) ) {
      health = health - 20;
      speed = 0; // to account for left, right, top and bottom collision and the two layers of the roadblock
      sprite_init(&race_car, (w-5)/2, h-17, 8, 7, race_car_image);
    }
  }

  // prevent overlapping of debris
  for ( int i = 0; i < debris_count; i++ ) {
    for ( int j = 0; j < debris_count; j++ ) {
      int debris_x = rand() % ((on_road_right - 6) + 1 - (on_road_left + 2)) + (on_road_left + 2);
      int debris_y = -2 ;
      if(i != j){
        while ( collision( debris[i], debris[j] ) ) {
          sprite_init(&debris[i], debris_x, debris_y, 3, 3, debris_image);
        }
      }
    }
  }
}

// Pause functionality
void pause_screen(void){
  static uint8_t prevState_dj = 0;
  if(pressed_dj){
    double tempTime7 = get_current_time();
    while(pressed_dj){
      double newTime7 = get_current_time();
      if (newTime7 >= (tempTime7 + 0.03 )){
        break;
      }
    }
    prevState_dj = pressed_dj;
    paused = false;
  }

  static uint8_t prevState_uj = 0;
  if(pressed_uj){
    double tempTime8 = get_current_time();
    while(pressed_uj){
      double newTime8 = get_current_time();
      if (newTime8 >= (tempTime8 + 0.03)){
        break;
      }
    }
    prevState_uj = pressed_uj;
    paused = true;
    clear_screen();
    draw_string(0, 1,"GAME IS PAUSED", FG_COLOUR );
    draw_string(5, 11,"Time:", FG_COLOUR );
    draw_string(5, 21,"Distance:", FG_COLOUR );
    draw_double(55, 21, distance_travelled, FG_COLOUR);
    draw_double(55, 11, elapsed_time, FG_COLOUR);
    show_screen();
  }
  if(game_over){
    if(pressed_rj){
      paused = false;
      game_over = false;
      clear_screen();
      splash_screen();
      setup ( );
      speed = 0;
      distance_travelled = 0;
      fuel_total = 100;
      health = 100;
      counter = 0;
      timer1();
    }
  }
}

// refuelling function
void refuel(void){

  if(race_car.y < fuel_depot.y || race_car.y > fuel_depot.y + fuel_depot.height - 2){ // dunno bout the 8
    refuel_start = 0;
    refuel_end = 0;
  }

  if ((race_car.y >= fuel_depot.y && race_car.y <= fuel_depot.y + fuel_depot.height - 2)
  && (((race_car.x >= on_road_left && race_car.x <= on_road_left + 5) || (round(race_car.x) + race_car.width - 1) >= fuel_depot.x -6)) ){// change screen_width term to sprite
    if(speed > 0){
      refuel_start = get_current_time();
    }
  }
  if ((race_car.y >= fuel_depot.y && race_car.y <= fuel_depot.y + fuel_depot.height - 2)
  && (((race_car.x >= on_road_left && race_car.x <= on_road_left + 5) || (round(race_car.x) + race_car.width - 1) >= fuel_depot.x-6))){// change screen_width term to sprite
    if(speed == 0){
      refuel_end = get_current_time();
    }
  }

  double refuel_time = refuel_end - refuel_start;

  // Part B fuel
  if(refuel_time <= 3 && refuel_time > 0){
    if(fuel_total < 100){
      fuel_total += (33 * refuel_time);
    }
    if (fuel_total >= 100){
      fuel_total = 100;
    }
  }
  // end of part B fuel
  if(refuel_time >=3){
    fuel_total = 100;
  }

}

void setup_pwm(void){
  TC4H = OVERFLOW_TOP >> 8;
  OCR4C = OVERFLOW_TOP & 0xff;
  TCCR4A = BIT(COM4A1) | BIT(PWM4A);
  SET_BIT(DDRC, 7);
  TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
  TCCR4D = 0;
}

void set_duty_cycle(int duty_cycle){
  TC4H = duty_cycle >> 8;
  OCR4A = duty_cycle & 0xff;
}


void finish_line_proximity_indicator(void){
  setup_pwm();
  // scales the brightness of the light to the distance from the finish line
  set_duty_cycle(0 + 1023/220 * distance_travelled);
}

void lose_screen(){
  clear_screen();
  paused = true;
  draw_string(1, 1, "GAMEOVER LOSER", FG_COLOUR);
  draw_string(1, 11, "Time:", FG_COLOUR);
  draw_double(50, 11, elapsed_time, FG_COLOUR);
  draw_string(1, 21, "Distance:", FG_COLOUR);
  draw_double(50, 21, distance_travelled, FG_COLOUR);
  draw_string(1, 41, " -> to restart", FG_COLOUR);
  show_screen();
}

void win_screen(){
  clear_screen();
  paused = true;
  draw_string(1, 1, "WINNER", FG_COLOUR);
  draw_string(1, 11, "Time:", FG_COLOUR);
  draw_double(50, 11, elapsed_time, FG_COLOUR);
  draw_string(1, 21, "Distance:", FG_COLOUR);
  draw_double(50, 21, distance_travelled, FG_COLOUR);
  draw_string(1, 41, " -> to restart", FG_COLOUR);
  show_screen();
}

void if_game_over(){
  // if lost
  if(health <= 0){
    game_over = true;
    lose_screen();
  }
  if(fuel_total < 1){
    game_over = true;
    lose_screen();
  }
  // if won
  if (collision(race_car, finish_line)){
    game_over = true;
    win_screen();
  }

  pause_screen();
}



void process(void){
  clear_screen();
  elapsed_time = get_current_time();
  adc_init();
  finish_line_proximity_indicator();
  move_car();
  for (int i = 0; i < (tree_count + debris_count); i++){
    move_tree(&tree[i]);
    move_debris(&debris[i]);
    move_fuel_depot();
    if (distance_travelled >= 30){
      move_finish_line(finish_line);
    }
  }
  respawn_trees(false);
  respawn_debris(false);
  respawn_fuel_depot();
  if_collided();
  fuel();
  refuel();
  distance();
  draw_sprites();
  dashboard();
  if_game_over();
  show_screen();
}


int main(void) {
  setup();
  for ( ;; ) {
    while(!game_over){
      while(!paused){
        timer();
        timer1();
        pause_screen();
        process();
      }
      while(paused){
        pause_screen();
      }
    }
  }

  return 0;
}

char buffer[20];

void draw_double(uint8_t x, uint8_t y, double value, colour_t colour) {
  snprintf(buffer, sizeof(buffer), "%0.2f", value);
  draw_string(x, y, buffer, colour);
}

void draw_int(uint8_t x, uint8_t y, int value, colour_t colour) {
  snprintf(buffer, sizeof(buffer), "%d", value);
  draw_string(x, y, buffer, colour);
}

void draw_formatted(int x, int y, char * buffer, int buffer_size, const char * format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, buffer_size, format, args);
  draw_string(x, y, buffer, FG_COLOUR);
}

void send_formatted(char * buffer, int buffer_size, const char * format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, buffer_size, format, args);
  usb_serial_write((uint8_t *) buffer, strlen(buffer));
}
