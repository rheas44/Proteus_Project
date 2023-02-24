#include <FEHLCD.h>
#include <FEHIO.h>
#include <FEHUtility.h>
#include <FEHMotor.h>
#include <FEHRPS.h>
#include <vector>
#include <string>
// Function prototypes
void move_forward(int a, int b);
void turn_right(int a, int b);
void turn_left(int a, int b);

//Declarations for encoders & motors
DigitalEncoder right_encoder(FEHIO::P0_0);
DigitalEncoder left_encoder(FEHIO::P0_1);
FEHMotor right_motor(FEHMotor::Motor0,9.0);
FEHMotor left_motor(FEHMotor::Motor1,9.0);

const double WHEEL_RADIUS = 2.5;
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
    gui.textLine("move forward", 0);
    int counts = (ONE_REVOLUTION_COUNTS * inches) / (2 * PI * WHEEL_RADIUS);

    right_encoder.ResetCounts();
    left_encoder.ResetCounts();

    right_motor.SetPercent(percent);
    left_motor.SetPercent(1.05*percent);

    while((left_encoder.Counts() + right_encoder.Counts()) / 2. < counts);

    right_motor.Stop();
    left_motor.Stop();
}


void turn_right(int percent, double degrees)
{
    gui.textLine("turn right", 0);
    int counts = 222 * (degrees/90);
    right_encoder.ResetCounts();
    left_encoder.ResetCounts();

    right_motor.SetPercent(-percent);
    left_motor.SetPercent(1.05*percent);

    while((left_encoder.Counts()+right_encoder.Counts())/2. < counts);

    right_motor.Stop();
    left_motor.Stop();
}


void turn_left(int percent, double degrees)
{
    gui.textLine("turn left", 0);
    int counts = 222 * (degrees/90);
    right_encoder.ResetCounts();
    left_encoder.ResetCounts();

    right_motor.SetPercent(percent);
    left_motor.SetPercent(-1.05*percent);

    while((left_encoder.Counts()+right_encoder.Counts())/2. < counts);

    right_motor.Stop();
    left_motor.Stop();
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
    LCD.WriteLine("Starting");    

    // go toward ramp
    move_forward(25, 14.0);
    // go up ramp
    move_forward(40, 5.0+12.31+4.0);

    // go to kiosk
    turn_left(25, 90);
    move_forward(25, 8.8);
    turn_right(25, 90);
    move_forward(25, 30);


    return 0;
}



