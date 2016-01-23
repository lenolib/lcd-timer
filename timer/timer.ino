#include "Wire.h"
#include <LiquidCrystal.h>

using namespace std;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);           // select the pins used on the LCD panel

// define some values used by the panel and buttons
int lcd_key     = 0;
int adc_key_in  = 0;
int current_menu = 0;
bool timer1_on = false;
bool timer2_on = false;
int cursor_x = 0;
int cursor_y = 0;

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
#define DS3231_I2C_ADDRESS 0x68
#define MAX_X 16
#define MAX_Y 2

void set_cursor(int x, int y) {
  cursor_x = x;
  cursor_y = y;
  lcd.setCursor(cursor_x, cursor_y);
}

void increment_x() {
  set_cursor((cursor_x + 1) % MAX_X, cursor_y);
}
void decrement_x() {
  set_cursor((cursor_x - 1) % MAX_X, cursor_y);
}
void increment_y() {
  set_cursor(cursor_x, (cursor_y + 1) % MAX_Y);
}
void decrement_y() {
  set_cursor(cursor_x, (cursor_y - 1) % MAX_Y);
}

class Menu {
  String upper;
  String lower;
  public:
//    Menu(String uppers, String lowers);
    void show();
    void set_text(String uppers, String lowers);
    void up();
    void down();
    void left();
    void right();
};

// Menu::Menu(String uppers, String lowers) {
//   upper = uppers;
//   lower = lowers;
// };

void Menu::show() {
  set_cursor(0, 0);
  lcd.print(upper);
  set_cursor(0, 1);
  lcd.print(lower);
};

void Menu::up() {
  increment_y();
};
void Menu::down() {
  decrement_y();
};
void Menu::left() {
  decrement_x();
};
void Menu::right() {
  increment_x();
};


class TimerMode : public Menu{
  byte _row;
  String _name;
  byte _hour;
  byte _minute;
  bool _active;
  byte _cursor_start;

  public:
    TimerMode(String name, byte row);
    void show();
};

TimerMode::TimerMode(String name, byte row) {
  _row = row;
  _name = name;
  _cursor_start = _name.length() + 2;
  _active = false;
  _hour = 0;
  _minute = 0;
};

void TimerMode::show() {
  set_cursor(0, _row);
  String out_str = _name + " " +
    pad_number(_hour, "0", 2) + ":" +
    pad_number(_minute, "0", 2) + active_text(_active);
  Serial.println(out_str);
  lcd.print(out_str);
}

String active_text(bool state){
  if (state) {
    return "ON";
  } else {
    return "OFF";
  }
}

String pad_number(byte number, String padding, byte npads){
  String padded = "" + number;
  while (padded.length() < npads) {
    padded = padding + padded;
  }
  return padded;
}


//Menu main_menu = TimerMode("Hej", 0);
Menu timer1 = TimerMode("Timer 1", 0);
Menu timer2 = TimerMode("Timer 2", 1);
Menu menus[] = {timer1, timer2};


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

void displayTime()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);
  // send it to the serial monitor
  Serial.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute<10)
  {
    Serial.print("0");
  }
  Serial.print(minute, DEC);
  Serial.print(":");
  if (second<10)
  {
    Serial.print("0");
  }
  Serial.print(second, DEC);
  Serial.print(" ");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(year, DEC);
  Serial.print(" Day of week: ");
  switch(dayOfWeek){
  case 1:
    Serial.println("Sunday");
    break;
  case 2:
    Serial.println("Monday");
    break;
  case 3:
    Serial.println("Tuesday");
    break;
  case 4:
    Serial.println("Wednesday");
    break;
  case 5:
    Serial.println("Thursday");
    break;
  case 6:
    Serial.println("Friday");
    break;
  case 7:
    Serial.println("Saturday");
    break;
  }
}

int read_LCD_buttons(){               // read the buttons
    adc_key_in = analogRead(0);       // read the value from the sensor
//    lcd.clear();
//    lcd.print(adc_key_in);
//    delay(100);
    // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
    // we add approx 50 to those values and check to see if we are close
    // We make this the 1st option for speed reasons since it will be the most likely result

    if (adc_key_in > 1000) return btnNONE;

    // For V1.1 us this threshold
    if (adc_key_in < 50)   return btnRIGHT;
    if (adc_key_in < 150)  return btnUP;
    if (adc_key_in < 300)  return btnDOWN;
    if (adc_key_in < 450)  return btnLEFT;
    if (adc_key_in < 700)  return btnSELECT;

   // For V1.0 comment the other threshold and use the one below:
   /*
     if (adc_key_in < 50)   return btnRIGHT;
     if (adc_key_in < 195)  return btnUP;
     if (adc_key_in < 380)  return btnDOWN;
     if (adc_key_in < 555)  return btnLEFT;
     if (adc_key_in < 790)  return btnSELECT;
   */

    return btnNONE;                // when all others fail, return this.
}

void handle_key_press(int lcd_key)
{
  switch (lcd_key){               // depending on which button was pushed, we perform an action
    case btnRIGHT:{             //  push button "RIGHT" and show the word on the screen
      menus[current_menu].right();
      debounce();
      break;
    }
    case btnLEFT:{
      menus[current_menu].left();
      debounce();
      break;
    }
    case btnUP:{
      menus[current_menu].up();
      debounce();
      //lcd.print("Hej Lennart");  //  push button "UP" and show the word on the screen
      break;
    }
    case btnDOWN:{
      menus[current_menu].down();
      debounce();
      break;
    }
    case btnSELECT:{
      switch_menu();
      debounce();
      break;
    }
    case btnNONE:{
//      lcd.print("NONE  ");  //  No action  will show "None" on the screen
      break;
    }
  }
}

void switch_menu() {
  current_menu = ++current_menu % 2;
  lcd.clear();
  menus[current_menu].show();
}

void debounce() {
  while(analogRead(0) < 1000) {
    //keep looping until released
  }
}

void setup()
{
  Wire.begin();
  Serial.begin(9600);
  lcd.begin(16, 2);               // start the library
  set_cursor(0,0);             // set the LCD cursor   position
  timer1.show();
  // set the initial time here:
  // DS3231 seconds, minutes, hours, day, date, month, year
  setDS3231time(30,18,16,7,23,1,16);
}

void loop()
{

//  displayTime(); // display the real-time clock data on the Serial Monitor,
//  delay(1000); // every second
//  lcd.setCursor(0,1);
  lcd_key = read_LCD_buttons();
  handle_key_press(lcd_key);
//  lcd.noCursor();
  //delay(300);
  // Turn on the cursor:
  lcd.cursor();
//  delay(300);
}








