#include "FEHServo.h"
#include <FEHLCD.h>
#include <FEHIO.h>
#include <FEHUtility.h>
#include <FEHMotor.h>
#include <FEHRPS.h>
#include <cmath>
#include <vector>
#include <string>


const int LCD_WIDTH = 320;
const int LCD_HEIGHT = 240;
const int RPS_GET_TIMES = 10;
const int CHECK_TIMES = 3;
const double CHECK_POWER = 25;

const double HEADING_DOWN = 180;
const double HEADING_RIGHT = 270;
const double HEADING_UP = 0;
const double HEADING_LEFT = 90;

double leftMultiplier = 1;


double nextShowGuiTime = 0;


//Declarations for encoders & motors
DigitalEncoder right_encoder(FEHIO::P0_0);
DigitalEncoder left_encoder(FEHIO::P0_1);
FEHMotor right_motor(FEHMotor::Motor0,9.0);
FEHMotor left_motor(FEHMotor::Motor1,9.0);
FEHServo arm_servo(FEHServo::Servo0);


//Declaration for analog input pin
AnalogInputPin cdsCell(FEHIO::P3_7);






// Fuel lever number.
int fuel_lever = 0;


constexpr double WHEEL_RADIUS = 2.5 / 2;
constexpr double PI = 3.14159;
constexpr int ONE_REVOLUTION_COUNTS = 318;


void textLine(std::string s, int row) {
    int width = 26;
    if (s.length() < width) {
        s.append(width - s.length(), ' ');
    }
    LCD.WriteRC(s.c_str(), row, 0);
}


void textLine(std::string s, double value, int row) {
    textLine((s + ": " + std::to_string(value)), row);
}

std::string colorString = "color: ?";


void updateGui() {
    if (TimeNow() > nextShowGuiTime) {
        textLine("x", RPS.X(), 9);
        textLine("y", RPS.Y(), 10);
        textLine("h", RPS.Heading(), 11);
        textLine(colorString, 12);
        textLine("cds", cdsCell.Value(), 13);
        nextShowGuiTime = TimeNow() + 0.25;
        if (RPS.X() < 0) {
            LCD.SetBackgroundColor(RED);
        } else {
            LCD.SetBackgroundColor(BLACK);
        }
    }
}

void resetCounts() {
    left_encoder.ResetCounts();
    right_encoder.ResetCounts();
}

int getCounts() {
    return (left_encoder.Counts() + right_encoder.Counts())/2;
}


void wait_for_light() {
    double nextTime = 0;
    textLine("waiting for light", 0);
    while (cdsCell.Value() >= 1.0) {
        if (TimeNow() > nextTime) {
            textLine("cds: ", cdsCell.Value(), 1);
            textLine("expected: ", 1.5, 2);
            nextTime = TimeNow() + .1;
        }
    }
}



const double TIME_OUT = 10.0;


void move_forward(int percent, double inches)
{
    if (inches < 0) {
        move_forward(-percent, -inches);
        return;
    }
    LCD.Clear();
    textLine(percent > 0 ? "move forward" : "move backward", 0);
    // Sleep(1.0);
    double startTime = TimeNow();
    int expectedCounts = (ONE_REVOLUTION_COUNTS * inches) / (2 * PI * WHEEL_RADIUS);


    resetCounts();


    right_motor.SetPercent(percent);
    left_motor.SetPercent(leftMultiplier*percent);


    double nextTime = 0;
    int oldCounts = -10;
    int numberLowChanges = 0;
    while(true) {
        updateGui();
        if (TimeNow() > startTime+TIME_OUT) {
            break;
        }
        int counts = getCounts();
        if (counts >= expectedCounts) {
            break;
        }
        if (counts - oldCounts < 2) {
            numberLowChanges++;
        }
        if (numberLowChanges > 16) {
            break;
        }
        textLine("expected counts", expectedCounts, 4);
        if (TimeNow() > nextTime) {
            textLine("counts", counts, 1);
            textLine("distance", ((double)counts / expectedCounts) * inches, 2);
            oldCounts = counts;
            textLine("time", TimeNow() - startTime, 3);
            nextTime = TimeNow() + .25;
        }
    }


    right_motor.Stop();
    left_motor.Stop();
}


void move_backward(int percent, double inches) {
    move_forward(-percent, inches);
}




void turn_right(int percent, double degrees)
{
    if (degrees < 0) {
        turn_right(-percent, -degrees);
        return;
    }

    LCD.Clear();
    textLine(percent < 0 ? "turn left" : "turn right", 0);
    // Sleep(1.0);


    double radians = degrees * PI / 180;
    int expectedCounts = (double)ONE_REVOLUTION_COUNTS * (3.5 * radians) / 2 / PI / WHEEL_RADIUS;


    resetCounts();


    right_motor.SetPercent(-percent);
    left_motor.SetPercent(leftMultiplier*percent);


    double nextTime = 0;
    double startTime = TimeNow();
    while(true) {
        updateGui();
        int counts = getCounts();
        if (TimeNow() > nextTime) {
            textLine("counts", counts, 1);
            textLine("expected", expectedCounts, 2);
            textLine("time", TimeNow() - startTime, 3);
            if (TimeNow() > startTime + TIME_OUT) {
                break;
            }
            if (counts >= expectedCounts) {
                break;
            }
        }
    }


    right_motor.Stop();
    left_motor.Stop();
}


void turn_left(int percent, double degrees)
{
    turn_right(-percent, degrees);
}

void sleep(double sec) {
    double start = TimeNow();
    while (TimeNow() < start + sec) {
        updateGui();
    }
}

double rps_heading() {
    textLine("", 6);
    for (int i = 0; i < RPS_GET_TIMES; i++) {
        sleep(.3);
        double heading = RPS.Heading();
        if (heading >= 0) {
            return heading;
        }
    }
    textLine("rps fail", 6);
    return -1;
}

double rps_x() {
    textLine("", 6);
    for (int i = 0; i < RPS_GET_TIMES; i++) {
        sleep(.3);
        double x = RPS.X();
        if (x >= 0) {
            return x;
        }
    }
    textLine("rps fail", 6);
    return -1;
}

double rps_y() {
    textLine("", 6);
    for (int i = 0; i < RPS_GET_TIMES; i++) {
        sleep(.3);
        double y = RPS.Y();
        if (y >= 0) {
            return y;
        }
    }
    textLine("rps fail", 6);
    return -1;
}

void check_heading(double targetHeading) {
    for (int i = 0; i < CHECK_TIMES; i++) {
        double currentHeading = rps_heading();
        textLine("target h", targetHeading, 8);
        textLine("current h", currentHeading, 7);
        if (currentHeading < 0) {
            return;
        }
        double difference = currentHeading - targetHeading;
        if (difference < -180) {
            difference += 360;
        }
        if (difference > 180) {
            difference -= 360;
        }
        if (std::abs(difference) > 20) {
            // assume RPS made an error
            return;
        }
        sleep(0.5);
        turn_right(CHECK_POWER, difference);
    }
}


/* Defines for how long each pulse should be and at what motor power.
These value will normally be small, but you should play around with the values to find what works best */
#define PULSE_TIME 0.25
#define PULSE_POWER 25

/*
 * Pulse forward a short distance using time
 */
void pulse_forward(int percent, float seconds)
{
    // Set both motors to desired percent
    right_motor.SetPercent(percent);
    left_motor.SetPercent(percent);

    // Wait for the correct number of seconds
    Sleep(seconds);

    // Turn off motors
    right_motor.Stop();
    left_motor.Stop();
}

void first_performance_checkpoint() {
    // go forward
    move_forward(25, 9.5);


    // align with right wall
    turn_left(25, 45);
    move_backward(25, 25);


    // go up ramp
    move_forward(25, 3.5);
    turn_right(35, 90);
    // move_forward(40, 5+5.0+12.31+4.0);
    move_forward(40, 6 + 12.31 + 10);


    // align with kiosk
    turn_left(35, 90);
    // move_forward(25, 8.8);
    move_forward(25, 10);
    turn_right(35, 90);


    // go to kisok
    move_forward(25, 12 + 12 + 3);


    // move back
    move_backward(25, 4);


    // align with left wall
    turn_left(35, 90);
    move_forward(40, 18);


    // move_backward(25, .15);
    turn_left(35, 90);


    // go down ramp
    move_forward(25, 35);
}


void second_performance_checkpoint() {
    wait_for_light();
    // go forward
    move_forward(25, 9.5);


    // align with right wall
    Sleep(0.25);
    turn_left(35, 45);
    move_backward(35, 15);


    // go up ramp
    move_forward(25, 3.5);
    Sleep(0.25);
    turn_right(35, 90);
    // move_forward(40, 5+5.0+12.31+4.0);
    move_forward(40, 6 + 12.31 + 10);
    Sleep(0.25);


    // drive near light
    Sleep(0.25);
    turn_left(35, 90);
    move_backward(50, 10);
    // move_forward(25, 8.8);
    move_forward(25, 22.75);
    Sleep(0.25);
    turn_right(35, 90);
    left_motor.SetPercent(25);
    right_motor.SetPercent(25);
    bool red = false;
    resetCounts();
    int inches = 18;
    int expectedCounts = (ONE_REVOLUTION_COUNTS * inches) / (2 * PI * WHEEL_RADIUS);
    while (getCounts() < expectedCounts) {
        while (cdsCell.Value() < 1.05) {
            left_motor.SetPercent(15);
            right_motor.SetPercent(15);
        }
        left_motor.SetPercent(25);
        right_motor.SetPercent(25);
        updateGui();
        textLine("cds", cdsCell.Value(), 1);
        if (cdsCell.Value() < 1.05) {
            red = true;
        }
    }
    left_motor.SetPercent(0);
    right_motor.SetPercent(0);
    // go to light
    // while (cdsCell.Value() >= 1.0) {
    //     left_motor.SetPercent(25);
    //     right_motor.SetPercent(25);
    // }
    //     left_motor.SetPercent(0);
    //     right_motor.SetPercent(0);
    //     // red light case (need to check cds cell values)

        if (red) {
            // red light case
            colorString = "color: RED";
            move_backward(25, 15);
            Sleep(0.25);
            turn_right(35, 90);
            move_forward(25, 10.5);
            Sleep(0.25);
            turn_left(35, 90);
            move_forward(25, 20);
            move_backward(25, 4);
        } else {
            // blue light case
            colorString = "color: BLUE";
            move_backward(25, 5);
            Sleep(0.25);
            turn_right(35, 90);
            move_forward(25, 4);
            Sleep(0.25);
            turn_left(35, 90);
            move_forward(25, 14);
            move_backward(25, 4);
        }


    // move back
    move_backward(25, 4);


    // align with left wall
    Sleep(0.25);
    turn_left(35, 90);
    move_forward(40, 18);
    move_forward(60, 5);


    // move_backward(25, .15);
    Sleep(0.25);
    turn_left(25, 90);


    // go down ramp
    move_forward(25, 29);
}

void raiseArm() {
   arm_servo.SetDegree(0);
}

void lowerArm() {
    arm_servo.SetDegree(60);
}

void third_performance_checkpoint() {
    // wait_for_light();

    // move_backward(35, 15);


    // // go up ramp
    // move_forward(25, 3.5);
    // Sleep(0.5);
    // turn_right(35, 90);
    // check_heading(HEADING_UP);
    // // move_forward(40, 5+5.0+12.31+4.0);
    // move_forward(40, 6 + 12.31 + 11);
    // Sleep(0.5);
    // turn_left(40, 90);
    // check_heading(HEADING_LEFT);
    // move_forward(35, 35);
    // // move_backward(25, .15);
    // Sleep(0.5);
    // turn_left(25, 90);
    // check_heading(HEADING_DOWN);



    // // go down ramp
    // move_forward(25, 20);
    // move_backward(25, 2);
    // Sleep(0.5);
    // turn_left(25, 90);
    // check_heading(HEADING_RIGHT);


    arm_servo.SetDegree(0);

    move_forward(25, 15);

    turn_right(35, 135);

    move_forward(40, 20);
    move_backward(40, 30);
    int value = RPS.GetCorrectLever();

    double distance;
    // if (value == 0) {
    //     distance = 3.5;
    // } else if (value == 1) {
    //     distance = 5.0;
    // } else if (value == 2) {
    //     distance = 6.5;
    // }
    distance = 3.5;
    move_forward(25, distance);
    Sleep(0.5);
    turn_right(25, 90);
    // check_heading(HEADING_DOWN);
    move_forward(25, 1);
    lowerArm();
    Sleep(0.5);
    move_backward(25, 7);
    arm_servo.SetDegree(100);
    Sleep(5.0);
    move_forward(25, 3.5);
    arm_servo.SetDegree(15);
    move_backward(25, 5);
}

void fourth_performance_checkpoint() {
    wait_for_light();
    arm_servo.SetDegree(100);
    // go forward
    move_forward(25, 9.5);


    // align with right wall
    Sleep(0.25);
    turn_left(35, 45);
    move_backward(35, 15);


    // go up ramp
    move_forward(25, 3.5);
    Sleep(0.25);
    turn_right(35, 90);
    // move_forward(40, 5+5.0+12.31+4.0);
    move_forward(40, 6 + 12.31 + 10);
    Sleep(0.25);
    turn_left(25, 90);
    check_heading(HEADING_LEFT);
    move_forward(25, 3);
    turn_right(25, 90);
    check_heading(HEADING_UP);
    move_forward(25, 5);
    turn_right(25, 90);
    check_heading(HEADING_RIGHT);
    arm_servo.SetDegree(40);
    turn_left(25, 90);
    move_backward(25, 4);
    turn_left(25, 90);
    move_forward(25, 1.5);
    turn_right(25, 90);
    move_forward(25, 2.5);
    turn_right(25, 90);



}



void choose_airline() {
    // go forward
    move_forward(25, 9.5);


    // align with right wall
    turn_left(25, 45);
    move_backward(25, 10);


    // go up ramp
    move_forward(25, 3.5);
    turn_right(35, 90);
    // move_forward(40, 5+5.0+12.31+4.0);
    move_forward(40, 6 + 12.31 + 18);


    // drive near light
    turn_left(35, 90);
    // move_forward(25, 8.8);
    move_forward(25, 10);
    turn_right(35, 90);


    // go to light
    move_forward(25, 12 + 12);


    // move back
    move_backward(25, 4);


    // red light case (need to check cds cell values)
    if (cdsCell.Value() < 0.6) {
        turn_left(35, 90);
        move_forward(25, 15);
        turn_right(35, 90);
        move_forward(25, 14);
        move_backward(25, 4);
    }


    // blue light case (need to check cds cell values)
    if (cdsCell.Value() > 0.65 && cdsCell.Value() < 0.9) {
        turn_left(35, 90);
        move_forward(25, 6);
        turn_right(35, 90);
        move_forward(25, 14);
        move_backward(25, 4);
    }


    // arm_motor.SetPercent(50);
    // move_forward(25, 4);
    // move_backward(25, 4);
    // arm_motor.SetPercent(-50);


    // align with left wall
    turn_left(35, 90);
    move_forward(40, 18);


    // move_backward(25, .15);
    turn_left(35, 90);


    // go down ramp
    move_forward(25, 29);
}


void fuel_airplane() {
    if (fuel_lever == 1) {
        // arm_motor.SetPercent(50);
        move_backward(25, 2);
        Sleep(5.0);
        // arm_motor.SetPercent(-50);
    } else if (fuel_lever == 2) {
        turn_left(35, 90);
        move_forward(25, 1);
        turn_right(35, 90);
        // arm_motor.SetPercent(50);
        move_backward(25, 2);
        // arm_motor.SetPercent(50);
        move_backward(25, 2);
        Sleep(5.0);
        // arm_motor.SetPercent(-50);
    } else {
        turn_left(35, 90);
        move_forward(25, 2);
        turn_right(35, 90);
        // arm_motor.SetPercent(50);
        move_backward(25, 2);
        // arm_motor.SetPercent(50);
        move_backward(25, 2);
        Sleep(5.0);
        // arm_motor.SetPercent(-50);
    }
}


void luggage() {
    move_backward(25, 2);
    turn_right(35, 90);
    turn_right(35, 90);
    // arm_motor.SetPercent(50);
    // arm_motor.SetPercent(-50);
    move_backward(25, 4);
}


void passport_stamp() {
    turn_right(35, 90);
    move_forward(25, 17);
    turn_left(35, 90);
    move_forward(25, 30);
    // arm_motor.SetPercent(50);
    move_forward(25, 1);
    // arm_motor.SetPercent(-50);
}


void final_button() {
    move_backward(25, 4.7);
    turn_left(35, 90);
    move_forward(25, 19);
    turn_left(35, 90);
    move_forward(29, 19);
    turn_right(35, 90);
    move_forward(29, 21);
    turn_right(35, 90);
    move_forward(19, 21);
}


void course_traversal() {
    // Begin by choosing the correct airline.
    choose_airline();
    fuel_airplane();
    luggage();
    passport_stamp();
    final_button();
}


void calibrate_motors() {
    leftMultiplier = 1.0;
    LCD.WriteLine("Calibrating motors");
    move_forward(25, 8);
    LCD.Clear();
    LCD.Write("Left: ");
    LCD.WriteLine(left_encoder.Counts());
    LCD.Write("Right: ");
    LCD.WriteLine(right_encoder.Counts());
    LCD.WriteLine((double)left_encoder.Counts() / right_encoder.Counts());
}


int main(void)
{
    float touchX, touchY; //for touch screen


    LCD.Clear(BLACK);
    LCD.SetFontColor(WHITE);

    arm_servo.SetMin(750);
    arm_servo.SetMax(1800);

    //arm_servo.SetDegree(0);

    //textLine("Touch the screen", 0);
    //while(!LCD.Touch(&touchX,&touchY)) {
    //    Sleep(.1);
    //}; Wait for screen to be pressed

    //arm_servo.SetDegree(90);


     RPS.InitializeTouchMenu();


    textLine("Touch the screen", 0);
    while(!LCD.Touch(&touchX,&touchY)) {
        updateGui();
    };// Wait for screen to be pressed
    while(LCD.Touch(&touchX,&touchY)); //Wait for screen to be unpressed
    LCD.Clear();


    // move_forward(25, 8);
    // calibrate_motors();


    // wait_for_red_light();
    // first_performance_checkpoint();


   third_performance_checkpoint();

    // don't turn off screen until power button pressed
    while (true);
}

