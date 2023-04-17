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


// size of Proteus screen
const int LCD_WIDTH = 320;
const int LCD_HEIGHT = 240;

// if RPS gives -1 this many times, give up
const int RPS_GET_TIMES = 10;

// stop doing check_{x,y,heading} after this many pulses
const int CHECK_TIMES = 20;

// time out for calls to move_forward, turn_right, etc.
const double TIME_OUT = 10.0;

// RPS heading values
const double HEADING_DOWN = 180;
const double HEADING_RIGHT = 270;
const double HEADING_UP = 0;
const double HEADING_LEFT = 90;

// the servo value to be lowest
// (if it goes more than this, it pushes up on the chassis and goes back up)
const double ALL_THE_WAY_DOWN = 122.0;

// radius of wheel, in inches
constexpr double WHEEL_RADIUS = 2.5 / 2;

// wheel distance, in inches
constexpr double WHEEL_DISTANCE = 7;

// the ratio of a circles circumphrence to its diameter
constexpr double PI = 3.14159;

// how many counts in one revoltion of an Igwan motor
constexpr int ONE_REVOLUTION_COUNTS = 318;

// motor power for check heading without luggage
double regular_check_heading_power = 25.0;

// multiply left motor percent by this to callibrate the motors
double leftMultiplier = 1;

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

// is set to true in update if the CDS cell sees a value lower than a certain threshold
bool red = false;

// file for logging to the SD card ("log.csv")
FEHFile *log_file;

// write `s` to the screen at row `row`
void textLine(std::string s, int row) {
    int width = 26;
    if (s.length() < width) {
        s.append(width - s.length(), ' ');
    }
    LCD.WriteRC(s.c_str(), row, 0);
}

// write `s: value` to the screen at row `row`
void textLine(std::string s, double value, int row) {
    textLine((s + ": " + std::to_string(value)), row);
}

// gets shown on the screen. is set when the robot reads the color
std::string colorString = "color: ?";

// next time to draw to the screen
// (if you draw to the screen too fast it will be unreadable)
double nextUpdateGuiTime = 0;

// this should be called as fast as possible in every loop
// it reads the cds cell, updating the global variable `red`
// it stores the fuel lever index in the global variable fuel_lever
// it also logs the current position, heading, and cds cell to log_file
// it also updates the gui if enough time has passed
// returns `true` if it updated the gui
bool update() {
    if (cdsCell.Value() < 1.0) {
        red = true;
    }
    int rps_lever = RPS.GetCorrectLever();
    if (rps_lever >= 0) {
        fuel_lever = rps_lever;
    }
    if (TimeNow() > nextUpdateGuiTime) {
        SD.FPrintf(log_file, "%f,%f,%f,%f,%f\n", TimeNow(), RPS.X(), RPS.Y(), RPS.Heading(), cdsCell.Value());
        // if RPS isn't working put red on the screen
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
        nextUpdateGuiTime = TimeNow() + 0.25;
        return true;
    }
    return false;
}

// reset counts on both encoders
void resetCounts() {
    left_encoder.ResetCounts();
    right_encoder.ResetCounts();
}

// get the encoder counts
// if both encoders work return the average
// otherwise return the one that works
// bool bothEncodersWork = false;

int getCounts() {
    bool bothEncodersWork = false;
    if (left_encoder.Counts() > 0 && right_encoder.Counts() > 0) {
        bothEncodersWork = true;
    }
    if (bothEncodersWork) {
        return (left_encoder.Counts() + right_encoder.Counts())/2;
    } else {
        textLine("encoder failure", 8);
        return (left_encoder.Counts() + right_encoder.Counts());
    }
}


// wait for the light to turn on
void wait_for_light() {
    double timeOut = TimeNow() + 30.0;
    textLine("waiting for light", 0);
    while (cdsCell.Value() >= 1.5 && TimeNow() < timeOut) {
        if (update()) {
            textLine("timeout", timeOut - TimeNow(), 1);
        }
    }
}


// move forward at motor percent `percent` for `inches` inches using shaft encoding
void move_forward(int percent, double inches)
{
    // to move backward, percent should be negative but inches should be positive
    if (inches < 0) {
        move_forward(-percent, -inches);
        return;
    }
    LCD.Clear();
    textLine(percent > 0 ? "move forward" : "move backward", 0);
    // sleep(1.0);
    double startTime = TimeNow();

    // calculate expected motor counts using math
    int expectedCounts = (ONE_REVOLUTION_COUNTS * inches) / (2 * PI * WHEEL_RADIUS);

    resetCounts();

    // start motors
    right_motor.SetPercent(percent);
    left_motor.SetPercent(leftMultiplier*percent);


    double nextTime = 0;
    int numberLowChanges = 0;

    while(true) {
        update();
        // stop if timeout occurs
        if (TimeNow() > startTime+TIME_OUT) {
            break;
        }
        int counts = getCounts();
        if (counts >= expectedCounts) {
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


// move backward with motor power `percent` for `inches` inches using shaft encoding
void move_backward(int percent, double inches) {
    move_forward(-percent, inches);
}

// turn right `degrees` degrees with motor power `percent` using shaft encoding
void turn_right(int percent, double degrees)
{
    // to turn left, percent should be negative but degrees positive
    if (degrees < 0) {
        turn_right(-percent, -degrees);
        return;
    }

    LCD.Clear();
    textLine(percent < 0 ? "turn left" : "turn right", 0);
    // sleep(1.0);


    // calculate expected encoder counts using math
    double radians = degrees * PI / 180;
    int expectedCounts = (double)ONE_REVOLUTION_COUNTS * ((WHEEL_DISTANCE / 2) * radians) / 2 / PI / WHEEL_RADIUS;


    resetCounts();


    right_motor.SetPercent(-percent);
    left_motor.SetPercent(leftMultiplier*percent);


    double nextTime = 0;
    double startTime = TimeNow();
    while(true) {
        update();
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

// turn left `degrees` degrees with motor power `percent` using shaft encoding
void turn_left(int percent, double degrees)
{
    turn_right(-percent, degrees);
}

// sleep for `sec` seconds. call update during this
void sleep(double sec) {
    double start = TimeNow();
    while (TimeNow() < start + sec) {
        update();
    }
}

// get the rps heading
// sleep first to get an accurate result
// retry up to RPS_GET_TIMES if negative result is returned
double rps_heading() {
    textLine("", 6);
    for (int i = 0; i < RPS_GET_TIMES; i++) {
        sleep(.60);
        double heading = RPS.Heading();
        if (heading >= 0) {
            return heading;
        }
    }
    textLine("rps fail", 6);
    return -1;
}

// get the rps x
// sleep first to get an accurate result
// retry up to RPS_GET_TIMES if negative result is returned
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

// get the rps y
// sleep first to get an accurate result
// retry up to RPS_GET_TIMES if negative result is returned
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

// Make sure that heading is correct by calculating the difference between the current and target headings. Pulse until the current heading is less than 2 degrees away from the target heading.
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

// Set the threshold for RPS check x and check y.
const double threshold = 0.5

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

// Deposit the luggage into the top bin and prepare for the next task.
void luggage() {
    // Wait for the light.
    wait_for_light();
    // Move forward.
    move_forward(80, 8.25);

    // Set the luggage turn and check heading powers.
    double luggage_turn_power = 55.0;
    double luggage_check_heading_power = 30.0;

    // align with right wall
    sleep(0.1);
    turn_left(luggage_turn_power, 45);
    check_heading(HEADING_LEFT, luggage_check_heading_power);
    move_backward(40, 18);

    sleep(0.25);
    // go up ramp
    if (RPS.CurrentRegionLetter() == 'C') {
        move_forward(40, 3);
        sleep(0.1);
        turn_right(80.0, 90*1.65);
    } else {
        move_forward(40, 2);
        sleep(0.1);
        turn_right(80.0, 90);
    }
    check_heading(HEADING_UP, luggage_check_heading_power);
    move_forward(80, 6 + 12.31 + 3 + 2 + 2 + 2 + 2);
    check_y(45.3 + 3 - 2 + 2 - 1 - .75 - 1.0, PLUS);
    sleep(0.1);

    // Turn left and make sure the robot has the left heading
    turn_left(luggage_turn_power, 90);
    check_heading(HEADING_LEFT, 50);


    // Get next to luggage bin
    double difference = -1.0;
    move_forward(luggage_turn_power, 5 + 9 - .5 - 1.5 - 1.5 - 1);
    check_x(16+difference+2+1.5-1-.25, MINUS);

    // Make sure to position properly and accurately in front of the luggage deposit
    turn_left(60, 90);
    check_heading((HEADING_DOWN + HEADING_LEFT)/2, luggage_check_heading_power);
    check_x(16+difference+2-1-1-.25, MINUS);
    check_heading(HEADING_DOWN, luggage_check_heading_power * 1.5);

    // Move forward slightly
    move_forward(80, 2.25);

    // Gradually move arm down to deposit luggage
    for (int i = 0; i < 140; i += 140/5) {
        arm_servo.SetDegree(i);
        sleep(.25);
    }
}

// Flip the passport stamp
void passport_flip() {
    // Move backward and put servo arm all the way up
    move_backward(40, 4);
    arm_servo.SetDegree(0);

    // Move backward and check y
    move_backward(40, 5 + 4.4 - 3);
    check_y(59.09 - 2.5, MINUS);

    // Make sure the arm servo's been set to all the way down for passport stamp flipping
    arm_servo.SetDegree(ALL_THE_WAY_DOWN);
    sleep(0.5);

    // Turn left and make sure the heading is set to heading right.
    turn_left(25, 90);
    check_heading(HEADING_RIGHT, regular_check_heading_power);
    // move_forward(25, 0.5);
    sleep(0.5);

    // Move forward
    move_forward(40, 7.5+0.5);

    // Prepare for kiosk button selection
    sleep(0.5);
    arm_servo.SetDegree(0);
    move_forward(25, 1);
    sleep(1.5);
    move_backward(40, 3);
    arm_servo.SetDegree(0);
}

// Press the correct kiosk button based on the correct CdS cell reading
void kiosk_buttons() {
    // Move backward toward the kiosk.
    check_heading(HEADING_RIGHT, regular_check_heading_power);
    move_backward(40, 1 + 1 + 0.5 + 0.5);

    // Correctly position to go over the light
    check_x(11.4 + 1.5 - 1.5 + 0.75-0.5 - 0.5 - 0.5, PLUS);

    // Turn left and make sure heading is up
    turn_left(60, 90);
    check_heading(HEADING_UP, regular_check_heading_power);

    // Set the red boolean to false
    red = false;

    // Move forward to position properly for kiosk light button pushing
    move_forward(10, 4);

    if (red) {
        // red light case - approach the red button on the kiosk and press it by gently running into it
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
        // blue light case - approach the blue button on the kiosk and press it by gently running into it
        colorString = "color: BLUE";
        move_backward(40, 5);
        turn_right(35, 90);
        check_heading(HEADING_RIGHT, regular_check_heading_power);
        move_forward(25, 4);
        turn_left(35, 90);
        check_heading(HEADING_UP, regular_check_heading_power);

        move_forward(40, 7);
        move_backward(40, 4);
    }


    // move back
    move_backward(25, 4);


    // align with left wall
    sleep(0.25);
    turn_left(35, 90);
    check_heading(HEADING_LEFT, regular_check_heading_power);
    move_forward(40, 18);

    // Face the downward direction to go down the ramp
    sleep(0.25);
    turn_left(25, 70);
    move_forward(25, 2);
    turn_left(25, 20);
    check_heading(HEADING_DOWN, regular_check_heading_power);


    // Go down the ramp
    move_forward(60, 12+3+3+2.5-.5);
    check_y(21+.5, MINUS);

    // Turn left and check heading to prepare for the fuel lever task
    turn_left(25, 90);
    check_heading(HEADING_RIGHT, regular_check_heading_power);
}

void fuel_levers() {
    // Determine the correct distance to move based on the correct fuel lever
    double distance;
    if (fuel_lever == 2) {
        distance = 3.5;
    } else if (fuel_lever == 1) {
        distance = 7.0;
    } else if (fuel_lever == 0) {
        distance = 10.5;
    }
    // Approach the correct fuel lever
    move_forward(25, distance-5);
    check_x(2.5+distance, PLUS);
    sleep(0.5);
    turn_right(25, 90);
    check_heading(HEADING_DOWN, regular_check_heading_power);
    move_backward(25, 1.5);
    // Flipping the correct lever down
    arm_servo.SetDegree(100);
    sleep(0.5);
    move_backward(25, 3);
    arm_servo.SetDegree(100);
    // Wait five seconds for the airplane to be fueled, and flip the fuel lever back up
    double startTime = TimeNow();
    arm_servo.SetDegree(ALL_THE_WAY_DOWN);
    check_heading(HEADING_DOWN, regular_check_heading_power);
    sleep(5.0 - (TimeNow() - startTime));
    move_forward(25, 2.15);
    arm_servo.SetDegree(15);
    sleep(.5);
    // Approach the ramp on the right side of the course
    arm_servo.SetDegree(ALL_THE_WAY_DOWN);
    turn_left(25, 90);
    check_heading(HEADING_RIGHT, regular_check_heading_power);
    move_forward(60, 10-distance);
    // Put up the servo arm
    arm_servo.SetDegree(0);

    // Get into the x position sufficient for turning and approaching the final button.
    check_x(11.65+3-1, PLUS);
    turn_right(25, 45);
    // Check heading making the robot face the final button
    check_heading((HEADING_DOWN+HEADING_RIGHT)/2, regular_check_heading_power);
    // Move forward into the final button
    move_forward(80, 30);
}

// Course traversal function
void course() {
    // Complete luggage task
    luggage();
    // Complete passport flip task
    passport_flip();
    // Complete kiosk button task
    kiosk_buttons();
    // Complete fuel lever task
    fuel_levers();
}

// Motor calibration function
void calibrate_motors() {
    // Set the left multiplier to 1.0
    leftMultiplier = 1.0;
    // Write that the motors are being calibrated.
    LCD.WriteLine("Calibrating motors");
    // Move forward 8 inches
    move_forward(25, 8);
    // Clear the screen
    LCD.Clear();
    // Write the left and right encoder counts
    LCD.Write("Left: ");
    LCD.WriteLine(left_encoder.Counts());
    LCD.Write("Right: ");
    LCD.WriteLine(right_encoder.Counts());
    // Write the quotient of the left encoder counts divided by the right encoder counts
    LCD.WriteLine((double)left_encoder.Counts() / right_encoder.Counts());
}



// Main function
int main(void)
{
    // Set up floats for touch screen values
    float touchX, touchY;

    // Open a log file
    log_file = SD.FOpen("log.csv", "w");

    // Clear the screen, setting the screen color to black and setting the font color to white
    LCD.Clear(BLACK);
    LCD.SetFontColor(WHITE);

    // Set the arm servo's maximum and minimum values
    arm_servo.SetMin(750);
    arm_servo.SetMax(1800);

    // Set the arm servo's degree to 0, putting it all the way up
    arm_servo.SetDegree(0);

    // Initialize RPS.
    RPS.InitializeTouchMenu();

    // Print a message saying to touch the screen and wait for screen touch
    textLine("Touch the screen", 0);
    while(LCD.Touch(&touchX,&touchY)); //Wait for screen to be unpressed
    while(!LCD.Touch(&touchX,&touchY)) {
        update();
    };// Wait for screen to be pressed
    while(LCD.Touch(&touchX,&touchY)); //Wait for screen to be unpressed

    // Clear the screen.
    LCD.Clear();

    // Wait for the red start light to turn on.
    wait_for_light();

//    fifth_performance_checkpoint();
    calibrate_motors();
    // course();

    SD.FClose(log_file);

    // don't turn off screen until power button pressed
    while (true);
}