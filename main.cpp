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

double leftMultiplier = 1;

double nextShowCDSTime = 0;

//Declarations for encoders & motors
DigitalEncoder right_encoder(FEHIO::P0_0);
DigitalEncoder left_encoder(FEHIO::P0_1);
FEHMotor right_motor(FEHMotor::Motor0,9.0);
FEHMotor left_motor(FEHMotor::Motor1,9.0);
FEHMotor arm_motor(FEHMotor::Motor2, 4.5);

//Declaration for analog input pin
AnalogInputPin cdsCell(FEHIO::P3_7);



// Fuel lever number.
int fuel_lever = 0;

constexpr double WHEEL_RADIUS = 2.5 / 2;
constexpr double PI = 3.14159;
constexpr int ONE_REVOLUTION_COUNTS = 318;

struct Gui {
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
};

Gui gui;

void resetCounts() {
    left_encoder.ResetCounts();
    right_encoder.ResetCounts();
}

void showCDS() {
    if (TimeNow() > nextShowCDSTime) {
        gui.textLine("cds", cdsCell.Value(), 13);
        nextShowCDSTime = TimeNow() + 0.25;
    }
}

int getCounts() {
    return (left_encoder.Counts() + right_encoder.Counts())/2;
}

void wait_for_light() {
    double nextTime = 0;
    gui.textLine("waiting for light", 0);
    while (cdsCell.Value() >= 1.0) {
        if (TimeNow() > nextTime) {
            gui.textLine("cds: ", cdsCell.Value(), 1);
            gui.textLine("expected: ", 1.5, 2);
            nextTime = TimeNow() + .1;
        }
    }
}

const double TIME_OUT = 10.0;

void move_forward(int percent, double inches)
{
    LCD.Clear();
    gui.textLine(percent > 0 ? "move forward" : "move backward", 0);
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
        showCDS();
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
        gui.textLine("expected counts", expectedCounts, 4);
        if (TimeNow() > nextTime) {
            gui.textLine("counts", counts, 1);
            gui.textLine("distance", ((double)counts / expectedCounts) * inches, 2);
            oldCounts = counts;
            gui.textLine("time", TimeNow() - startTime, 3);
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
    LCD.Clear();
    gui.textLine(percent < 0 ? "turn left" : "turn right", 0);
    // Sleep(1.0);

    double radians = degrees * PI / 180;
    int expectedCounts = (double)ONE_REVOLUTION_COUNTS * (3.5 * radians) / 2 / PI / WHEEL_RADIUS;

    resetCounts();

    right_motor.SetPercent(-percent);
    left_motor.SetPercent(leftMultiplier*percent);

    double nextTime = 0;
    double startTime = TimeNow();
    while(true) {
        showCDS();
        int counts = getCounts();
        if (TimeNow() > nextTime) {
            gui.textLine("counts", counts, 1);
            gui.textLine("expected", expectedCounts, 2);
            gui.textLine("time", TimeNow() - startTime, 3);
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
    turn_left(25, 45);
    move_backward(35, 15);

    // go up ramp
    move_forward(25, 3.5);
    turn_right(35, 90);
    // move_forward(40, 5+5.0+12.31+4.0);
    move_forward(40, 6 + 12.31 + 10);

    // drive near light
    turn_left(25, 100);
    // move_forward(25, 8.8);
    move_forward(25, 17.5);
    turn_right(35, 90);
    left_motor.SetPercent(25);
    right_motor.SetPercent(25);
    bool red = false;
    resetCounts();
    int inches = 15;
    int expectedCounts = (ONE_REVOLUTION_COUNTS * inches) / (2 * PI * WHEEL_RADIUS);
    while (getCounts() < expectedCounts) {
        showCDS();
        gui.textLine("cds", cdsCell.Value(), 1);
        if (cdsCell.Value() < 0.5) {
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
    
        // red light case
        if (red) {
            move_backward(25, 10);
            turn_right(35, 90);
            move_forward(25, 11);
            turn_left(35, 100);
            move_forward(25, 20);
            move_backward(25, 4);
            // blue light case
        } else {
            move_backward(25, 2);
            turn_right(35, 90);
            move_forward(25, 4);
            turn_left(35, 90);
            move_forward(25, 14);
            move_backward(25, 4);
        }

    // move back
    move_backward(25, 4);

    // align with left wall
    turn_left(35, 90);
    move_forward(40, 18);

    // move_backward(25, .15);
    turn_left(35, 95);

    // go down ramp
    move_forward(25, 29);
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
        arm_motor.SetPercent(50);
        move_backward(25, 2);
        Sleep(5.0);
        arm_motor.SetPercent(-50);
    } else if (fuel_lever == 2) {
        turn_left(35, 90);
        move_forward(25, 1);
        turn_right(35, 90);
        arm_motor.SetPercent(50);
        move_backward(25, 2);
        arm_motor.SetPercent(50);
        move_backward(25, 2);
        Sleep(5.0);
        arm_motor.SetPercent(-50);
    } else {
        turn_left(35, 90);
        move_forward(25, 2);
        turn_right(35, 90);
        arm_motor.SetPercent(50);
        move_backward(25, 2);
        arm_motor.SetPercent(50);
        move_backward(25, 2);
        Sleep(5.0);
        arm_motor.SetPercent(-50);
    }
}

void luggage() {
    move_backward(25, 2);
    turn_right(35, 90);
    turn_right(35, 90);
    arm_motor.SetPercent(50);
    arm_motor.SetPercent(-50);
    move_backward(25, 4);
}

void passport_stamp() {
    turn_right(35, 90);
    move_forward(25, 17);
    turn_left(35, 90);
    move_forward(25, 30);
    arm_motor.SetPercent(50);
    move_forward(25, 1);
    arm_motor.SetPercent(-50);
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

    // RPS.InitializeTouchMenu();

    gui.textLine("Touch the screen", 0);
    while(!LCD.Touch(&touchX,&touchY)) {
        gui.textLine("cds", cdsCell.Value(), 1);
        Sleep(.1);
    }; //Wait for screen to be pressed
    while(LCD.Touch(&touchX,&touchY)); //Wait for screen to be unpressed
    LCD.Clear();

    // move_forward(25, 8);
    // calibrate_motors();

    // wait_for_red_light();
    // first_performance_checkpoint();

    second_performance_checkpoint();

    // arm_motor.SetPercent(-50);

    // Sleep(10.0);

    // arm_motor.Stop();

    // Sleep(1.0);

    // arm_motor.SetPercent(50);

    // Sleep(10.0);

    // arm_motor.Stop();


    return 0;
}