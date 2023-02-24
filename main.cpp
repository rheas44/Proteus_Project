#include <FEHLCD.h>
#include <FEHIO.h>
#include <FEHUtility.h>
#include <FEHMotor.h>
#include <FEHRPS.h>
#include <vector>
#include <string>

const int LCD_WIDTH = 320;
const int LCD_HEIGHT = 240;

const double leftMultiplier = 0.99;

//Declarations for encoders & motors
DigitalEncoder right_encoder(FEHIO::P0_0);
DigitalEncoder left_encoder(FEHIO::P0_1);
FEHMotor right_motor(FEHMotor::Motor0,9.0);
FEHMotor left_motor(FEHMotor::Motor1,9.0);

const double WHEEL_RADIUS = 2.5 / 2;
const double PI = 3.14159;
const int ONE_REVOLUTION_COUNTS = 318;

struct Gui {
    void textLine(std::string s, int row) {
        LCD.WriteRC((s + "                 ").c_str(), row, 0);
    }
};

Gui gui;

void move_forward(int percent, double inches)
{
    double timeOut = TimeNow() + 10.0;
    int expectedCounts = (ONE_REVOLUTION_COUNTS * inches) / (2 * PI * WHEEL_RADIUS);
    LCD.Write("Expected counts ");
    LCD.WriteLine(expectedCounts);

    right_encoder.ResetCounts();
    left_encoder.ResetCounts();

    right_motor.SetPercent(percent);
    left_motor.SetPercent(leftMultiplier*percent);

    double nextWriteLineTime = 0;
    while(true) {
        if (TimeNow() > timeOut) {
            break;
        }
        int counts = (left_encoder.Counts() + right_encoder.Counts()) / 2.;
        if (counts > expectedCounts) {
            break;
        }
        if(TimeNow() > nextWriteLineTime) {
            // LCD.WriteLine(counts);
            nextWriteLineTime = TimeNow()+.1;
        }
    }

    right_motor.Stop();
    left_motor.Stop();
}


void turn_left(int percent, double degrees)
{
    gui.textLine("turn left", 0);
    int counts = 222 * (degrees/90);
    right_encoder.ResetCounts();
    left_encoder.ResetCounts();

    right_motor.SetPercent(-percent);
    left_motor.SetPercent(leftMultiplier*percent);

    while((left_encoder.Counts()+right_encoder.Counts())/2. < counts);

    right_motor.Stop();
    left_motor.Stop();
}


void turn_right(int percent, double degrees)
{
    gui.textLine("turn right", 0);
    int counts = 222 * (degrees/90);
    right_encoder.ResetCounts();
    left_encoder.ResetCounts();

    right_motor.SetPercent(percent);
    left_motor.SetPercent(-leftMultiplier*percent);

    while((left_encoder.Counts()+right_encoder.Counts())/2. < counts);

    right_motor.Stop();
    left_motor.Stop();
}

void try_course() {
    // go toward ramp
    move_forward(25, 14.0);
    // go up ramp
    move_forward(40, 5.0+12.31+4.0);

    // go to kiosk
    turn_left(25, 90);
    move_forward(25, 8.8);
    turn_right(25, 90);
    move_forward(25, 30);
    turn_left(25, 90);
    move_forward(25, 8.8);
    turn_left(25, 90);
    move_forward(25, 30);
}

void calibrate_motors() {
    LCD.WriteLine("Calibrating motors");
    move_forward(25, 8.0);
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


    LCD.WriteLine("Go up ramp test");
    LCD.WriteLine("Touch the screen");
    while(!LCD.Touch(&touchX,&touchY)); //Wait for screen to be pressed
    while(LCD.Touch(&touchX,&touchY)); //Wait for screen to be unpressed
        
    // calibrate_motors();
    try_course();

    return 0;
}



