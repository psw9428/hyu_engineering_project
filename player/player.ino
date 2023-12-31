#include <Servo.h>
#include <Stepper.h>
#include <DistanceSensor.h>
#include <StaticThreadController.h>
#include <Thread.h>
#include <ThreadController.h>

#define LEFT_PIN 2
#define RIGHT_PIN 3
#define PUNCH_PIN 4
#define GUARD_PIN 12
#define PUNCH_SIGNAL 12
#define GUARD_SIGNAL 13
#define DAMAGED_SIGNAL 0

#define MOVE_SPEED 10
#define DASH_SPEED 50
#define DASH_DISTANCE 40

#define ECHOPIN 5
#define TRIGPIN 6

#define READY_POSE 0
#define ON_PUNCHING 1
#define ON_PUNCHBACK 2

#define OUT_OF_RANGE 0
#define IN_PUNCH_RANGE 1
#define TOO_CLOSE 2
#define NO_DASH 4

#define PUNCH_DELAY 1000

const int stepsPerRevolution = 200;  
ThreadController controll = ThreadController();

Thread left_right_thread = Thread();
Thread punch_thread = Thread();
Thread distance_thread = Thread();
Thread guard_thread = Thread();
Thread damaged_thread = Thread();

Stepper BigStepper(stepsPerRevolution, 8, 9, 10, 11);
Servo myservo;
DistanceSensor sensor(TRIGPIN, ECHOPIN);

int distance_status;
bool guard_status;
bool damaged_status;

void setup() {
  // initialize function
  initial();

  // initialize the serial port:
  Serial.begin(9600);
}

void initial() {
  // set stepper speed
  BigStepper.setSpeed(MOVE_SPEED);

  // servo reset (punch motor)
  myservo.attach(7, 444, 2500);
  myservo.write(10);

  // left, right moving and punch pin setting
  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);
  pinMode(PUNCH_PIN, INPUT_PULLUP);
  pinMode(GUARD_PIN, INPUT_PULLUP);
  pinMode(PUNCH_SIGNAL, OUTPUT);

  // thread onRun setting
  left_right_thread.onRun(left_right_func);
  punch_thread.onRun(punch_func);
  distance_thread.onRun(distance_func);
  guard_thread.onRun(guard_func);
  //damaged_thread.onRun();

  // set thread's interval
  distance_thread.setInterval(50);

  // thread controller setting
  controll.add(&left_right_thread);
  controll.add(&punch_thread);
  controll.add(&distance_thread);
  controll.add(&guard_thread);

  digitalWrite(PUNCH_SIGNAL, LOW);

  // initialize global var
  distance_status = 0;
  guard_status = false;
  damaged_status = false;
}

void left_right_func() {
  int left;
  int right;
  static int status = 0;
  static int dash_status = 0;
  static unsigned long tmp = 0;
  static unsigned long prev_touch = 0;
  left = digitalRead(LEFT_PIN);
  right = digitalRead(RIGHT_PIN);

  if (guard_status)
    return;
  if (!left && right) {
    BigStepper.step(1);
    if (!tmp)
      tmp = millis();
    status = LEFT_PIN;
  }
  else if (left && !right && !(distance_status & TOO_CLOSE)) {
    BigStepper.step(-1);
    status = RIGHT_PIN;
    if (!tmp)
      tmp = millis();
  }
  else if (status) {
    if (millis() - tmp <= 150) { // touch very short
      Serial.println("SHORT_TOUCH");
      if (status == RIGHT_PIN && dash_status == RIGHT_PIN && millis() - prev_touch <= 400) {
        Serial.println("DASH_RIGHT!");
        BigStepper.setSpeed(DASH_SPEED);
        BigStepper.step(-DASH_DISTANCE);
        dash_status = DEFAULT;
        prev_touch = 0;
        BigStepper.setSpeed(MOVE_SPEED);
      }
      else if (status == LEFT_PIN && dash_status == LEFT_PIN && millis() - prev_touch <= 400) {
        Serial.println("DASH_LEFT!!");
        BigStepper.setSpeed(DASH_SPEED);
        BigStepper.step(DASH_DISTANCE);
        dash_status = DEFAULT;
        prev_touch = 0;
        BigStepper.setSpeed(MOVE_SPEED);
      }
      else if (dash_status == DEFAULT) {
        dash_status = status;
        prev_touch = millis();
      }
      else {
        dash_status = DEFAULT;
        prev_touch = 0;
      }
    }
    status = DEFAULT;
    tmp = 0;
  }
}

void punch_func() {
  static unsigned long time = 0;
  static int status = 0;

  if (!digitalRead(PUNCH_PIN) && !status) {
    if (guard_status)
      return;
    Serial.println("punch");
    myservo.write(140);
    time = millis();
    status = ON_PUNCHING;
  }
  if (time + 500 <= millis() && status & ON_PUNCHING) {
    status = ON_PUNCHBACK;
    if(distance_status & IN_PUNCH_RANGE)
      digitalWrite(PUNCH_SIGNAL, HIGH);
    myservo.write(10);
    time = millis();
  }
  if (time + 100 <= millis() && status & ON_PUNCHBACK)
    digitalWrite(PUNCH_SIGNAL, LOW);
  if (time + PUNCH_DELAY <= millis() && status & ON_PUNCHBACK) {
    digitalWrite(PUNCH_SIGNAL, LOW);
    status = READY_POSE;
  }
}

void distance_func() {
  float distance = sensor.getCM();

  if (distance <= 3.0 && distance > 0 && !(distance_status & TOO_CLOSE)) {
    //Serial.print(distance);
    Serial.println("TOO_CLOSE!");
    distance_status = TOO_CLOSE | IN_PUNCH_RANGE;
  }
  else if (distance > 3.0 && distance <= 4.0 && !(distance_status & IN_PUNCH_RANGE)) {
    //Serial.print(distance);
    Serial.println("IN_PUNCH_RANGE!");
    distance_status = IN_PUNCH_RANGE;
  }
  else if (distance > 4.0 && distance_status) {
    //Serial.print(distance);
    Serial.println("OUT_RANGE");
    distance_status = OUT_OF_RANGE;
  }
}

void guard_func() {
  // if (!digitalRead(GUARD_PIN)) {
  //   if (!guard_status) {
  //     Serial.println("GUARD!");
  //     guard_status = true;
  //     digitalWrite(GUARD_SIGNAL, HIGH);
  //   }
  // }
  // else if (guard_status) {
  //   Serial.println("NO_GUARD");
  //   guard_status = false;
  //   digitalWrite(GUARD_SIGNAL, LOW);
  // }
}

void damaged_func() {
  static unsigned long tmp = 0;
  if (digitalRead(DAMAGED_SIGNAL) && !damaged_status) {
    damaged_status = true;
    tmp = millis();
    myservo.write(0);
  }
  if (damaged_status && tmp + 500 <= millis()) {
    damaged_status = false;
    tmp = 0;
    myservo.write(10);
  }
}

void loop() {
  controll.run();
}