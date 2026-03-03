#define DIR_LEFT 4
#define SPEED_LEFT 5

#define DIR_RIGHT 7
#define SPEED_RIGHT 6

#define FORWARD_LEFT LOW
#define BACKWARD_LEFT HIGH
#define FORWARD_RIGHT HIGH
#define BACKWARD_RIGHT LOW


void move(bool left_dir, int left_speed, bool right_dir, int right_speed) {
  digitalWrite(DIR_LEFT, left_dir);
  digitalWrite(DIR_RIGHT, right_dir);
  analogWrite(SPEED_LEFT, left_speed);
  analogWrite(SPEED_RIGHT, right_speed);
}


void forward(int speed){
    move(FORWARD_LEFT, speed, FORWARD_RIGHT, speed);
}

void backward(int speed){
    move(BACKWARD_LEFT, speed, BACKWARD_RIGHT, speed);
}

void turn_left(int steepness){ //speepness not a speed
    int left_speed = 255 - steepness;
    int right_speed = 255;

    move(FORWARD_LEFT, left_speed, FORWARD_RIGHT, right_speed);
}

void turn_right(int steepness){
    int left_speed = 255;
    int right_speed = 255 - steepness;

    move(FORWARD_LEFT, left_speed, FORWARD_RIGHT, right_speed);
}

void rotate_left(int speed){
  move(BACKWARD_LEFT, speed, FORWARD_RIGHT, speed);
}

void rotate_right(int speed){
  move(FORWARD_LEFT, speed, BACKWARD_RIGHT, speed);
}



void setup(){
  pinMode(DIR_LEFT, OUTPUT);
  pinMode(DIR_RIGHT, OUTPUT);
  pinMode(SPEED_LEFT, OUTPUT);
  pinMode(SPEED_RIGHT, OUTPUT);
}


void loop(){
  backward(100);
  delay(2000);
  forward(100);
  delay(2000);
  turn_left(255);
  delay(2000);
  turn_right(255);
  delay(2000);
  rotate_left(255);
  delay(2000);
  rotate_right(255);
  delay(2000);
}