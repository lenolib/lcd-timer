#include "Wire.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>

using namespace std;

LiquidCrystal lcd(8, 11, 4, 5, 6, 7);           // select the pins used on the LCD panel

// define some values used by the panel and buttons
int lcd_key     = 0;
int adc_key_in  = 0;
int current_menu = 0;
int cursor_x = 0;
int cursor_y = 0;

bool is_triggered = false;

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
#define DS3231_I2C_ADDRESS 0x68
#define MAX_X 16
#define MAX_Y 2
#define MODE_TIMER_1 0
#define MODE_TIMER_2 1
#define MODE_CLOCKSET 2

#define RELAYSWITCH_PIN 12
#define BACKLIGHT_DIGITAL_PIN 10

#define ONE_SPACE 1

#define POS_HOUR_10 0
#define POS_HOUR_1 1
#define POS_MINUTE_10 3
#define POS_MINUTE_1 4
#define POS_STATE 6

#define LOOP_WORK_FREQ 10000

#define TIMER_1_EEPROM_ADDRESS 0
#define TIMER_2_EEPROM_ADDRESS 32

#define EEPROM_OFFSET_HOUR 0
#define EEPROM_OFFSET_MINUTE 1
#define EEPROM_OFFSET_ONOFF_STATE 2

#define CLOCKSETMENU_TIMEOUT 300000 //about half a minute @ 16Mhz
#define BACKLIGHT_TIMEOUT 50000 //about 6 seconds @ 16Mhz

String active_text(bool state){
  if (state) {
    return "ON ";
  } else {
    return "OFF";
  }
}

String pad_number(unsigned int number, String padding, byte npads){
  String empty_string = "";
  String padded = empty_string + number;
  while (padded.length() < npads) {
    padded = padding + padded;
  }
  return padded;
}

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return( (val/16*10) + (val%16) );
}

int modulo (int a, int b) {
  return a >= 0 ? a % b : ( b - abs ( a%b ) ) % b;
}

void set_cursor(int x, int y) {
  cursor_x = x;
  cursor_y = y;
  lcd.setCursor(cursor_x, cursor_y);
}

void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte
dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}

class Menu {
  public:
    unsigned int* _allowed_positions;
    virtual unsigned int get_n_allowed() = 0;
    unsigned int _row;
    unsigned int cursor_idx = 0;
    unsigned int _hour;
    unsigned int _minute;
    virtual void show() = 0;
    // virtual void set_cursor_start_position() = 0;
    // void set_text(String uppers, String lowers);
    virtual void up() = 0;
    virtual void down() = 0;
    void left();
    void right();
    void set_cursor_start_position();
    unsigned int get_min();
    unsigned int get_hr();
    bool set;
    virtual void set_time_from_rtc() = 0;
};

void Menu::left() {
  cursor_idx = modulo((int)cursor_idx - 1, get_n_allowed());
  int new_position = _allowed_positions[cursor_idx];
  set_cursor(new_position, cursor_y);
};
void Menu::right() {
  cursor_idx = modulo((int)cursor_idx + 1, get_n_allowed());
  int new_position = _allowed_positions[cursor_idx];
  set_cursor(new_position, cursor_y);
};

void Menu::set_cursor_start_position() {
  cursor_idx = 0;
  set_cursor(_allowed_positions[cursor_idx], this->_row);
}

unsigned int Menu::get_min() {
  return _minute;
}

unsigned int Menu::get_hr() {
  return _hour;
}

class ClockMode : public Menu {
  String _text = "Set current time";
  public:
    ClockMode();
    unsigned int get_n_allowed() { return 4; };
    void show();
    void up();
    void down();
    void set_time_from_rtc();
};

ClockMode::ClockMode() {
  _row = 1;
  cursor_idx = 0;
  _hour = 0;
  _minute = 0;
  set = false;
  _allowed_positions = new unsigned int[get_n_allowed()];
  _allowed_positions[0] = 0;
  _allowed_positions[1] = 1;
  _allowed_positions[2] = 3;
  _allowed_positions[3] = 4;
}

void ClockMode::show() {
  lcd.clear();
  set_cursor(0, 0);
  lcd.print(_text);
  String out_str = pad_number(_hour, "0", 2) + ":" +
    pad_number(_minute, "0", 2) + " (set=exit)";
  set_cursor(0, 1);
  lcd.print(out_str);
  // set_cursor(_allowed_positions[cursor_idx], _row);
  set_cursor(_allowed_positions[cursor_idx], _row);
};

void ClockMode::up() {
  switch (cursor_idx) {
    case 0: {
      increment_value_10(&_hour, 24);
      break;
    }
    case 1: {
      increment_value_1(&_hour, 24);
      break;
    }
    case 2: {
      increment_value_10(&_minute, 60);
      break;
    }
    case 3: {
      increment_value_1(&_minute, 60);
      break;
    }
  }
}

void ClockMode::down() {
  switch (cursor_idx) {
    case 0: {
      decrement_value_10(&_hour, 24);
      break;
    }
    case 1: {
      decrement_value_1(&_hour, 24);
      break;
    }
    case 2: {
      decrement_value_10(&_minute, 60);
      break;
    }
    case 3: {
      decrement_value_1(&_minute, 60);
      break;
    }
  }
}

void ClockMode::set_time_from_rtc() {
  readHourAndMinute(&_hour, &_minute);
}

void write_t_to_rtc(unsigned int min, unsigned int hr) {
  setDS3231time(0, min, hr, 0, 0, 0, 0);
}

class TimerMode : public Menu{
  // unsigned int _row;
  String _name;
  // unsigned int _hour;
  // unsigned int _minute;
  // unsigned int _cursor_start;
  //unsigned int _allowed_positions[5];

  public:
    TimerMode(String name, unsigned int row, unsigned int eeprom_addr);
    unsigned int _eeprom_addr;
    unsigned int get_n_allowed() { return 5; };
    void read_alarm_from_eeprom();
    void write_alarm_to_eeprom();
    void show();
    // void set_cursor_start_position();
    void up();
    void down();
    void set_time_from_rtc() {};
    bool active;

  private:
    // void increment_value_10(unsigned int *var, byte max_val);
    // void increment_value_1(unsigned int *var, byte max_val);
    // void decrement_value_10(unsigned int *var, byte max_val);
    // void decrement_value_1(unsigned int *var, byte max_val);
    void toggle_timer();
    //void decrement_value(&var);
};

TimerMode::TimerMode(String name, unsigned int row, unsigned int eeprom_addr) {
  _row = row;
  _name = name;
  unsigned int _cursor_start = _name.length() + ONE_SPACE;
  _eeprom_addr = eeprom_addr;
  active = false;
  _hour = 0;
  _minute = 0;
  _allowed_positions = new unsigned int[get_n_allowed()];
  _allowed_positions[0] = _cursor_start + POS_HOUR_10;
  _allowed_positions[1] = _cursor_start + POS_HOUR_1;
  _allowed_positions[2] = _cursor_start + POS_MINUTE_10;
  _allowed_positions[3] = _cursor_start + POS_MINUTE_1;
  _allowed_positions[4] = _cursor_start + POS_STATE;
};


void TimerMode::read_alarm_from_eeprom() {
  _hour = EEPROM.read(_eeprom_addr + EEPROM_OFFSET_HOUR);
  _minute = EEPROM.read(_eeprom_addr + EEPROM_OFFSET_MINUTE);
  active = EEPROM.read(_eeprom_addr + EEPROM_OFFSET_ONOFF_STATE);
}

void TimerMode::write_alarm_to_eeprom() {
  EEPROM.write(_eeprom_addr + EEPROM_OFFSET_HOUR,         _hour);
  EEPROM.write(_eeprom_addr + EEPROM_OFFSET_MINUTE,       _minute);
  EEPROM.write(_eeprom_addr + EEPROM_OFFSET_ONOFF_STATE,  active);
}


void TimerMode::show() {
  set_cursor(0, _row);
  String out_str = _name + " " +
    pad_number(_hour, "0", 2) + ":" +
    pad_number(_minute, "0", 2) + " " + active_text(active);
  lcd.print(out_str);
  set_cursor(_allowed_positions[cursor_idx], _row);
}

void TimerMode::toggle_timer() {
  active = !active;
}

void TimerMode::up() {
  switch (cursor_idx) {
    case 0: {
      increment_value_10(&_hour, 24);
      break;
    }
    case 1: {
      increment_value_1(&_hour, 24);
      break;
    }
    case 2: {
      increment_value_10(&_minute, 60);
      break;
    }
    case 3: {
      increment_value_1(&_minute, 60);
      break;
    }
    case 4: {
      toggle_timer();
      break;
    }
  }
  this->write_alarm_to_eeprom();
}

void TimerMode::down() {
  switch (cursor_idx) {
    case 0: {
      decrement_value_10(&_hour, 24);
      break;
    }
    case 1: {
      decrement_value_1(&_hour, 24);
      break;
    }
    case 2: {
      decrement_value_10(&_minute, 60);
      break;
    }
    case 3: {
      decrement_value_1(&_minute, 60);
      break;
    }
    case 4: {
      toggle_timer();
      break;
    }
  }
  this->write_alarm_to_eeprom();
}

void readDS3231time(
    byte *second,
    byte *minute,
    byte *hour,
    byte *dayOfWeek,
    byte *dayOfMonth,
    byte *month,
    byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}

int readSecond() {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 1);
  int second = bcdToDec(Wire.read() & 0x7f);
  return second;
}

void readHourAndMinute(unsigned int *hour, unsigned int *minute) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 3);
  int second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
}

int read_LCD_buttons(){               // read the buttons
    adc_key_in = analogRead(0);       // read the value from the sensor
//    lcd.clear();
//    lcd.print(adc_key_in);
//    delay(100);
    // my buttons when read are centered at these valies: x,x,x,x,x,x
    // we add approx 50 to those values and check to see if we are close
    // We make this the 1st option for speed reasons since it will be the most likely result

    if (adc_key_in > 1000) return btnNONE;

    // For V1.1 us this threshold
    if (adc_key_in < 50)   return btnRIGHT;
    if (adc_key_in < 150)  return btnUP;
    if (adc_key_in < 300)  return btnDOWN;
    if (adc_key_in < 450)  return btnLEFT;
    if (adc_key_in < 700)  return btnSELECT;

    return btnNONE;                // when all others fail, return this.
}

void debounce() {
  while(analogRead(0) < 1000) {
    //keep looping until released
  }
}

void increment_value_10(unsigned int *var, byte max_val) {
  unsigned int temp_var = *var;
  temp_var = (temp_var + 10);
  if (temp_var >= max_val) {
    *var = temp_var % 10;
  }
  else {
    *var = temp_var;
  }
}

void increment_value_1(unsigned int *var, byte max_val) {
  *var = (*var + 1) % max_val;
}

void decrement_value_10(unsigned int *var, byte max_val) {
  int temp_var = (*var - 10);
  if (temp_var < 0) {
    *var = 0;
  }
  else {
    *var = temp_var;
  }
}

void decrement_value_1(unsigned int *var, byte max_val) {
  *var = modulo((*var - 1), max_val);
}

Menu* timer1 = new TimerMode("1:", 0, TIMER_1_EEPROM_ADDRESS);
Menu* timer2 = new TimerMode("2:", 1, TIMER_2_EEPROM_ADDRESS);
Menu* clock_ = new ClockMode();
Menu *menus[] = {timer1, timer2, clock_};

void switch_menu() {
  int t0 = readSecond();
  while (analogRead(0) < 1000) {
    if (abs(t0 - readSecond()) > 2) {
      current_menu = 2;
      lcd.clear();
      menus[current_menu]->show();
      return;
    }
  }
  if (current_menu == 2) {
    lcd.clear();
    menus[MODE_TIMER_1]->show();
    menus[MODE_TIMER_2]->show();
    current_menu = 0;
    menus[current_menu]->set_cursor_start_position();
    write_t_to_rtc(menus[MODE_CLOCKSET]->get_min(), menus[MODE_CLOCKSET]->get_hr());
  }
  else {
    current_menu = ++current_menu % 2;
    menus[current_menu]->show();
  }
}

void handle_key_press(int lcd_key)
{
  switch (lcd_key){               // depending on which button was pushed, we perform an action
    case btnRIGHT:{             //  push button "RIGHT" and show the word on the screen
      menus[current_menu]->right();
      debounce();
      break;
    }
    case btnLEFT:{
      menus[current_menu]->left();
      debounce();
      break;
    }
    case btnUP:{
      menus[current_menu]->up();
      debounce();
      menus[current_menu]->show();
      break;
    }
    case btnDOWN:{
      menus[current_menu]->down();
      debounce();
      menus[current_menu]->show();
      break;
    }
    case btnSELECT:{
      switch_menu();
      debounce();
      break;
    }
    case btnNONE:{
      break;
    }
  }
}

void setup()
{
  // Timer1.initialize(10000000);         // initialize timer1, and set a 1/2 second period                // setup pwm on pin 9, 50% duty cycle
  // Timer1.attachInterrupt(checkClock);   // Set Timer ISR to update clock status
  pinMode(RELAYSWITCH_PIN, OUTPUT);
  //pinMode(BACKLIGHT_DIGITAL_PIN, OUTPUT); //TODO turn me on
  digitalWrite(RELAYSWITCH_PIN, LOW);
  Wire.begin();
  Serial.begin(9600);
  lcd.begin(16, 2);               // start the library

  set_cursor(0,0);             // set the LCD cursor   position

  // Read alarms from EEPROM
  static_cast<TimerMode*>(timer1)->read_alarm_from_eeprom();
  static_cast<TimerMode*>(timer2)->read_alarm_from_eeprom();

  timer1->show();
  timer2->show();
  timer1->set_cursor_start_position();

  //Retrieve time from RTC
  menus[MODE_CLOCKSET]->set_time_from_rtc();
  lcd.cursor();
}

bool should_alarm_trigger(unsigned int timer_idx) {
  Menu* timer_ = menus[timer_idx];
  bool active = static_cast<TimerMode*>(timer_)->active;
  bool same_hour = menus[MODE_CLOCKSET]->get_hr() == timer_->get_hr();
  bool same_minute = menus[MODE_CLOCKSET]->get_min() == timer_->get_min();
  if  ((same_hour and same_minute) and active){ return true; }
  else {return false;}
}


void loop_work() {
  if (current_menu != MODE_CLOCKSET) {
    menus[MODE_CLOCKSET]->set_time_from_rtc();
  }

  bool alarm_1 = should_alarm_trigger(MODE_TIMER_1);
  bool alarm_2 = should_alarm_trigger(MODE_TIMER_2);
  if (alarm_1 or alarm_2) {
    if (!is_triggered){
      trigger_relayswitch();
      is_triggered = true;
    }
  }
  else {
    is_triggered = false;
  }
}

void turn_backlight_off() {
  digitalWrite(BACKLIGHT_DIGITAL_PIN, LOW);
}

void turn_backlight_on() {
  digitalWrite(BACKLIGHT_DIGITAL_PIN, HIGH);
}

void trigger_relayswitch() {
  Serial.println("Triggered");
  digitalWrite(RELAYSWITCH_PIN, HIGH);
  delay(1000);
  digitalWrite(RELAYSWITCH_PIN, LOW);
}

unsigned long int clockset_timeout_counter = 0;
unsigned int counter = 0;
unsigned long int backlight_timeout_counter = 0;

void loop()
{
  counter ++;
  backlight_timeout_counter++;
  if ( backlight_timeout_counter > BACKLIGHT_TIMEOUT ) {
    turn_backlight_off();
  }

  if (current_menu == MODE_CLOCKSET) {
    clockset_timeout_counter++;
    if (clockset_timeout_counter > CLOCKSETMENU_TIMEOUT) {
      clockset_timeout_counter = 0;
      switch_menu();
    }

  } else {
    clockset_timeout_counter = 0;
  }


  lcd_key = read_LCD_buttons();
  if (lcd_key != btnNONE) {
    clockset_timeout_counter = 0;
    backlight_timeout_counter = 0;
    turn_backlight_on();
  }
  handle_key_press(lcd_key);

  if (counter >= LOOP_WORK_FREQ) {
    loop_work();
    counter = 0;
  }

}

