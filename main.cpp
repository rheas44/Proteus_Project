#include "FEHServo.h"
#include <FEHLCD.h>
#include <FEHIO.h>
#include <FEHUtility.h>
#include <FEHMotor.h>
#include <FEHRPS.h>
#include <FEHSD.h>
#include <cmath>
#include <vector>
#include <string>


const int LCD_WIDTH = 320;
const int LCD_HEIGHT = 240;
const int RPS_GET_TIMES = 10;
const int CHECK_TIMES = 10;

const double HEADING_DOWN = 180;
const double HEADING_RIGHT = 270;
const double HEADING_UP = 0;
const double HEADING_LEFT = 90;

const double ALL_THE_WAY_DOWN = 122.0;

double regular_check_heading_power = 25.0;

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

FEHFile *log_file;

bool red = false;

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


bool updateGui() {
    if (cdsCell.Value() < .8) {
        red = true;
    }
    int rps_lever = RPS.GetCorrectLever();
    if (rps_lever >= 0) {
        fuel_lever = rps_lever;
    }
    if (TimeNow() > nextShowGuiTime) {
        SD.FPrintf(log_file, "%f,%f,%f,%f,%f\n", TimeNow(), RPS.X(), RPS.Y(), RPS.Heading(), cdsCell.Value());
        if (RPS.X() < 0) {
            LCD.SetBackgroundColor(RED);
        } else {
            LCD.SetBackgroundColor(BLACK);
        }
        textLine("x", RPS.X(), 9);
        textLine("y", RPS.Y(), 10);
        textLine("h", RPS.Heading(), 11);
        textLine(colorString, 12);
        textLine("cds", cdsCell.Value(), 13);
        // textLine("lever", fuel_lever, 13);
        nextShowGuiTime = TimeNow() + 0.25;
        return true;
    }
    return false;
}

void resetCounts() {
    left_encoder.ResetCounts();
    right_encoder.ResetCounts();
}

bool bothEncodersWork = false;

int getCounts() {
    if (left_encoder.Counts() > 0 && right_encoder.Counts() > 0) {
        bothEncodersWork = true;
    }
    if (bothEncodersWork) {
        return (left_encoder.Counts() + right_encoder.Counts())/2;
    } else {
        return (left_encoder.Counts() + right_encoder.Counts());
    }
}


void wait_for_light() {
    double timeOut = TimeNow() + 30.0;
    textLine("waiting for light", 0);
    while (cdsCell.Value() >= 1.0 && TimeNow() < timeOut) {
        if (updateGui()) {
            textLine("timeout", timeOut - TimeNow(), 1);
        }
    }
}



const double TIME_OUT = 5.0;


void move_forward(int percent, double inches)
{
    if (inches < 0) {
        move_forward(-percent, -inches);
        return;
    }
    LCD.Clear();
    textLine(percent > 0 ? "move forward" : "move backward", 0);
    // sleep(1.0);
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
    // sleep(1.0);


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


void check_heading(double targetHeading, int percent) {
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
        if (std::abs(difference) < 2) {
            return;
        }
        turn_right(percent, difference);
    }
}



/* Defines for how long each pulse should be and at what motor power.
These value will normally be small, but you should play around with the values to find what works best */
#define PULSE_TIME 0.15
#define PULSE_POWER 25
#define PLUS 1
#define MINUS -1

/*
 * Pulse forward a short distance using time
 */
void pulse_forward(int percent, float seconds)
{
    // Set both motors to desired percent
    right_motor.SetPercent(percent);
    left_motor.SetPercent(percent);

    // Wait for the correct number of seconds
    sleep(seconds);

    // Turn off motors
    right_motor.Stop();
    left_motor.Stop();
}


const double threshold = 0.5;

void check_x(float x_coordinate, int orientation)
{
    textLine("check_x", 0);
    // Determine the direction of the motors based on the orientation of the QR code
    int power = PULSE_POWER;
    if (orientation == MINUS)
    {
        power = -PULSE_POWER;
    }



    // Check if receiving proper RPS coordinates and whether the robot is within an acceptable range
    double current_x;
    int i = 0;
    while (current_x = rps_x(), x_coordinate >= 0 && (current_x < x_coordinate - threshold || current_x > x_coordinate + threshold) && i < CHECK_TIMES)
    {
        i++;
        if (current_x > x_coordinate)
        {
            textLine("moving backward", 2);
            // Pulse the motors for a short duration in the correct direction
            pulse_forward(-power, PULSE_TIME);
        }
        else if (current_x < x_coordinate)
        {
            textLine("moving forward", 2);
            // Pulse the motors for a short duration in the correct direction
            pulse_forward(power, PULSE_TIME);
        }
    }
}


/*
 * Use RPS to move to the desired y_coordinate based on the orientation of the QR code
 */
void check_y(float y_coordinate, int orientation)
{
    // Determine the direction of the motors based on the orientation of the QR code
    int power = PULSE_POWER;
    if (orientation == MINUS)
    {
        power = -PULSE_POWER;
    }




    // Check if receiving proper RPS coordinates and whether the robot is within an acceptable range
    double current_y;
    int i = 0;
    while (current_y = rps_y(), y_coordinate >= 0 && (current_y < y_coordinate - threshold || current_y > y_coordinate + threshold) && i < CHECK_TIMES) {
        i++;
        if (current_y > y_coordinate)
        {
            textLine("moving backward", 2);
            // LCD.WriteLine(rps_y());
            // Pulse the motors for a short duration in the correct direction
            pulse_forward(-power, PULSE_TIME);
        }
        else if (current_y < y_coordinate)
        {
            textLine("moving forward", 2);
            // Pulse the motors for a short duration in the correct direction
            pulse_forward(power, PULSE_TIME);
        }
    }
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
    // sleep(0.5);
    // turn_right(35, 90);
    // check_heading(HEADING_UP);
    // // move_forward(40, 5+5.0+12.31+4.0);
    // move_forward(40, 6 + 12.31 + 11);
    // sleep(0.5);
    // turn_left(40, 90);
    // check_heading(HEADING_LEFT);
    // move_forward(35, 35);
    // // move_backward(25, .15);
    // sleep(0.5);
    // turn_left(25, 90);
    // check_heading(HEADING_DOWN);






    // // go down ramp
    // move_forward(25, 20);
    // move_backward(25, 2);
    // sleep(0.5);
    // turn_left(25, 90);
    // check_heading(HEADING_RIGHT);




    arm_servo.SetDegree(0);


    move_forward(25, 21.5);


    turn_right(35, 135);


    // move_forward(40, 20);
    check_heading(HEADING_RIGHT, regular_check_heading_power);
    move_backward(40, 15);




    double distance;
    if (fuel_lever == 2) {
        distance = 3.5;
    } else if (fuel_lever == 1) {
        distance = 7.0;
    } else if (fuel_lever == 0) {
        distance = 10.5;
    }
    // distance = 3.5;
    move_forward(25, distance);
    sleep(0.5);
    turn_right(25, 90);
    check_heading(HEADING_DOWN, regular_check_heading_power);
    // move_forward(25, 1);
    move_backward(25, 1.5);
    arm_servo.SetDegree(100);
    sleep(0.5);
    move_backward(25, 3);
    arm_servo.SetDegree(100);
    sleep(5.0);
    check_heading(HEADING_DOWN, regular_check_heading_power);
    move_forward(25, 2.5);
    arm_servo.SetDegree(15);
    sleep(.5);
    arm_servo.SetDegree(100);
    move_backward(25, 5);
    arm_servo.SetDegree(15);
}


void fourth_performance_checkpoint() {
    wait_for_light();
    arm_servo.SetDegree(20);
    // go forward.
    move_forward(40, 9.5);


    // align with right wall
    sleep(0.25);
    turn_left(40, 45);
    check_heading(HEADING_LEFT, regular_check_heading_power);
    move_backward(35, 15);

    // go up ramp
    move_forward(25, 3.5);
    sleep(0.25);
    turn_right(40, 90);
    check_heading(HEADING_UP, regular_check_heading_power);
    // move_forward(40, 5+5.0+12.31+4.0);
    move_forward(40, 6 + 12.31 + 10 + 1);
    sleep(0.25);

    // align with passport flip
    turn_left(40, 90);
    check_heading(HEADING_LEFT, regular_check_heading_power);
    move_forward(25, 15);
    turn_right(40, 90);
    check_heading(HEADING_UP, regular_check_heading_power);
    move_forward(25, 12.0);
    turn_right(40, 90);
    check_heading(HEADING_RIGHT, regular_check_heading_power);
    arm_servo.SetDegree(140);
    move_forward(25, 5);


    // raise arm
    LCD.SetBackgroundColor(BLUE);
    LCD.Clear();
    sleep(1.0);
    arm_servo.SetDegree(40);
    sleep(1.0);


    // turn left to flip passport flip
    // turn_left(25, 90);


    // // move to other side of passport flip
    // move_backward(25, 10);
    // turn_left(25, 90);
    // check_heading(HEADING_LEFT);
    // move_forward(25, 15);
    // turn_right(25, 90);
    // check_heading(HEADING_UP);
    // move_forward(25, 15.0);
    // turn_right(25, 90);
    // check_heading(HEADING_RIGHT);
    // move_forward(25, 20.0);


    // // turn right to flip passport flip back down
    // LCD.SetBackgroundColor(GREEN);
    // LCD.Clear();
    // sleep(4.0);
    // turn_right(25, 90);
}

// start at start position.
// wait for light.
// end facing luggage deposit.
void luggage() {
    wait_for_light();
    // go forward.
    move_forward(40, 8.25);

    double luggage_turn_power = 55.0;
    double luggage_check_heading_power = 30.0;

    // align with right wall
    sleep(0.1);
    turn_left(luggage_turn_power, 45);
    check_heading(HEADING_LEFT, luggage_check_heading_power);
    move_backward(40, 15);

    // go up ramp
    move_forward(40, 2.5);
    sleep(0.1);
    turn_right(luggage_turn_power, 90);
    check_heading(HEADING_UP, luggage_check_heading_power);
    // move_forward(40, 5+5.0+12.31+4.0);
    move_forward(40, 6 + 12.31 + 3 + 2 + 2 + 2 + 2);
    check_y(45.3 + 3 - 2 + 2 - 1 - .75, PLUS);
    sleep(0.1);


    turn_left(luggage_turn_power, 90);
    check_heading(HEADING_LEFT, 50);


    // get next to luggage
    double difference = -1.0;
    move_forward(luggage_turn_power, 5 + 9 - .5 - 1.5 - 1.5);
    check_x(16+difference+2+1.5, MINUS);

    turn_left(60, 90);
    // check_heading((HEADING_DOWN + HEADING_LEFT)/2, luggage_check_heading_power);
    check_x(16+difference+2-1, MINUS);
    check_heading(HEADING_DOWN, luggage_check_heading_power);


    move_forward(50, 2.25);
    check_y(43.6, MINUS);
    for (int i = 0; i < 140; i += 140/3) {
        arm_servo.SetDegree(i);
        sleep(.25);
    }
}

// start after luggage
// end at passport flip
void passport_flip() {
    move_backward(40, 4);
    arm_servo.SetDegree(0);

    move_backward(40, 5 + 4.4 - 3);
    check_y(59.09 - 2.5, MINUS);

    turn_left(25, 90);
    check_heading(HEADING_RIGHT, regular_check_heading_power);
    // move_forward(25, 0.5);

    arm_servo.SetDegree(ALL_THE_WAY_DOWN);
    sleep(1.0);

    move_forward(40, 7.5+0.5);

    sleep(0.5);
    arm_servo.SetDegree(0);
    move_forward(25, 1);
    sleep(1.5);
    move_backward(40, 3);
    arm_servo.SetDegree(0);
}

// start after passport flip
// end down the ramp
void kiosk_buttons() {
    move_backward(40, 1 + 1);

    check_x(11.4 + 1.5 - 1.5 + 0.75-0.5, PLUS);

    turn_left(60, 90);
    check_heading(HEADING_UP, regular_check_heading_power);

    red = false;

    move_forward(10, 3);

    if (red) {
        // red light case
        colorString = "color: RED";
        move_backward(40, 15);
        turn_right(35, 90);
        check_heading(HEADING_RIGHT, regular_check_heading_power);
        move_forward(40, 10.5 - 2 - 1);
        check_x(23, PLUS);
        turn_left(35, 90);
        check_heading(HEADING_UP, regular_check_heading_power);
        move_forward(40, 20);
        move_backward(40, 4);
    } else {
        // blue light case
        colorString = "color: BLUE";
        move_backward(40, 5);
        turn_right(35, 90);
        check_heading(HEADING_RIGHT, regular_check_heading_power);
        move_forward(25, 4);
        turn_left(35, 90);
        check_heading(HEADING_UP, regular_check_heading_power);

        move_forward(40, 14);
        move_backward(40, 4);
    }

    // move back
    move_backward(25, 4);


    // align with left wall
    sleep(0.25);
    turn_left(35, 90);
    check_heading(HEADING_LEFT, regular_check_heading_power);
    move_forward(40, 18);

    sleep(0.25);
    turn_left(25, 70);
    move_forward(25, 2);
    turn_left(25, 20);
    check_heading(HEADING_DOWN, regular_check_heading_power);


    // go down ramp
    move_forward(40, 12+3+3+2.5);
    check_y(21, MINUS);
    turn_left(25, 90);
    check_heading(HEADING_RIGHT, regular_check_heading_power);
    // move_backward(40, 6);
}

void fuel_levers() {
    double distance;
    if (fuel_lever == 2) {
        distance = 3.5;
    } else if (fuel_lever == 1) {
        distance = 7.0;
    } else if (fuel_lever == 0) {
        distance = 10.5;
    }
    // distance = 3.5;
    move_forward(25, distance-6);
    check_x(2.5+distance, PLUS);
    sleep(0.5);
    turn_right(25, 90);
    check_heading(HEADING_DOWN, regular_check_heading_power);
    // move_forward(25, 1);
    move_backward(25, 1.5);
    arm_servo.SetDegree(100);
    sleep(0.5);
    move_backward(25, 3);
    arm_servo.SetDegree(100);
    sleep(5.0);
    arm_servo.SetDegree(ALL_THE_WAY_DOWN);
    check_heading(HEADING_DOWN, regular_check_heading_power);
    move_forward(25, 2);
    arm_servo.SetDegree(15);
    sleep(.5);
    arm_servo.SetDegree(ALL_THE_WAY_DOWN);

    turn_left(25, 90);
    check_heading(HEADING_RIGHT, regular_check_heading_power);

    move_forward(40, 10-distance);

    arm_servo.SetDegree(0);

    check_x(11.65+3-1, PLUS);
    turn_right(25, 45);
    check_heading((HEADING_DOWN+HEADING_RIGHT)/2, regular_check_heading_power);

    move_forward(80, 18);
    move_forward(40, 20);
}

void course() {
    luggage();
    passport_flip();
    kiosk_buttons();
    fuel_levers();
}

void fifth_performance_checkpoint() {
    luggage();

    move_backward(40, 4);


    arm_servo.SetDegree(0);


    turn_left(25, 90);


    check_heading(HEADING_RIGHT, 15);
    move_forward(40, 10 + 5 + 5);


    turn_right(25, 90);
    check_heading(HEADING_DOWN, 15);


    move_forward(40, 39.31 / 2);
    check_heading(HEADING_DOWN, 15);
    move_forward(40, 39.31 / 2);
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

    log_file = SD.FOpen("log.csv", "w");

    LCD.Clear(BLACK);
    LCD.SetFontColor(WHITE);

    arm_servo.SetMin(750);
    arm_servo.SetMax(1800);

    arm_servo.SetDegree(0);

   RPS.InitializeTouchMenu();

    textLine("Touch the screen", 0);
    while(LCD.Touch(&touchX,&touchY)); //Wait for screen to be unpressed
    while(!LCD.Touch(&touchX,&touchY)) {
        updateGui();
    };// Wait for screen to be pressed
    while(LCD.Touch(&touchX,&touchY)); //Wait for screen to be unpressed
    LCD.Clear();

    wait_for_light();

//    fifth_performance_checkpoint();
    course();

    SD.FClose(log_file);

    // don't turn off screen until power button pressed
    while (true);
}