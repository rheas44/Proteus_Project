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

FEHMotor arm_motor(FEHMotor::Motor2, 4.5);

int main(void)
{
    float touchX, touchY; //for touch screen

    LCD.Clear(BLACK);

    LCD.SetFontColor(GREEN);
    LCD.FillRectangle(0, 0, LCD_WIDTH/2, LCD_HEIGHT);

    LCD.SetFontColor(RED);
    LCD.FillRectangle(LCD_WIDTH/2, 0, LCD_WIDTH/2, LCD_HEIGHT);

    // RPS.InitializeTouchMenu();
    while (true) {
        while(!LCD.Touch(&touchX,&touchY));
        if (touchX < LCD_WIDTH/2.0) {
            arm_motor.SetPercent(25);
        }  else {
            arm_motor.SetPercent(-25);

        }
        while(LCD.Touch(&touchX,&touchY));
        arm_motor.SetPercent(0);
    }
}