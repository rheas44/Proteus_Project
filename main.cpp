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

//Declarations for encoders & motors
DigitalEncoder right_encoder(FEHIO::P0_0);
DigitalEncoder left_encoder(FEHIO::P0_1);
FEHMotor right_motor(FEHMotor::Motor0,9.0);
FEHMotor left_motor(FEHMotor::Motor1,9.0);

//Declaration for analog input pin
AnalogInputPin cdsCell(FEHIO::P3_7);

const double WHEEL_RADIUS = 2.5 / 2;
const double PI = 3.14159;
const int ONE_REVOLUTION_COUNTS = 318;

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

int getCounts() {
    return (left_encoder.Counts() + right_encoder.Counts())/2;
}

void wait_for_start_light() {
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
    double startTime = TimeNow();
    int expectedCounts = (ONE_REVOLUTION_COUNTS * inches) / (2 * PI * WHEEL_RADIUS);

    resetCounts();

    right_motor.SetPercent(percent);
    left_motor.SetPercent(leftMultiplier*percent);

    double nextTime = 0;
    int oldCounts = -10;
    int numberLowChanges = 0;
    while(true) {
        if (TimeNow() > startTime+TIME_OUT) {
            break;
        }
        int counts = getCounts();
        if (counts >= expectedCounts) {
            break;
        }
        if (counts - oldCounts < 4) {
            numberLowChanges++;
        }
        // if (numberLowChanges > 8) {
        //     break;
        // }
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


void turn_right(int percent, double degrees)
{
    LCD.Clear();
    gui.textLine(percent < 0 ? "turn left" : "turn right", 0);
    int expectedCounts = std::abs(222 * (degrees/90));
    resetCounts();

    right_motor.SetPercent(percent);
    left_motor.SetPercent(-leftMultiplier*percent);

    double nextTime = 0;
    double startTime = TimeNow();
    while(true) {
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

void try_course() {
    // go forward
    move_forward(25, 8);

    // align with right wall
    turn_left(25, 45);
    move_forward(-25, 10);
    move_forward(-15, 10);

    // go up ramp
    move_forward(25, 1);
    turn_right(25, 90);
    // move_forward(40, 5+5.0+12.31+4.0);
    move_forward(40, 6 + 12.31 + 10);

    // align with kiosk
    turn_left(25, 90);
    // move_forward(25, 8.8);
    move_forward(25, 10);
    turn_right(25, 90);

    // go to kisok
    move_forward(15, 12 + 12);

    // move back
    move_forward(-25, 4);

    // align with left wall
    turn_left(25, 90+15);
    move_forward(25, 10);
    turn_right(25, 15);
    move_forward(15, 10);
    move_forward(-15, .5);
    turn_left(15, 90-15); // it would be better to not do this

    // go down ramp
    move_forward(25, 35);
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


    gui.textLine("Touch the screen", 0);
    while(!LCD.Touch(&touchX,&touchY)) {
        gui.textLine("cds", cdsCell.Value(), 1);
        Sleep(.1);
    }; //Wait for screen to be pressed
    while(LCD.Touch(&touchX,&touchY)); //Wait for screen to be unpressed
    LCD.Clear();

    left_motor.SetPercent(25);
    right_motor.SetPercent(25);

    // move_forward(25, 8);
    // calibrate_motors();

    wait_for_start_light();
    try_course();

    return 0;
}



