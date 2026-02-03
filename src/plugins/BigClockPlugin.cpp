#include "plugins/BigClockPlugin.h"

void BigClockPlugin::setup()
{
  // loading screen
  Screen.setPixel(4, 7, 1);
  Screen.setPixel(5, 7, 1);
  Screen.setPixel(7, 7, 1);
  Screen.setPixel(8, 7, 1);
  Screen.setPixel(10, 7, 1);
  Screen.setPixel(11, 7, 1);
  delay(10);
  previousMinutes = -1;
  previousHour = -1;
  previousHH.clear();
  previousMM.clear();
  previousLeadingZero = false;
}

void BigClockPlugin::loop()
{
  if (getLocalTime(&timeinfo))
  {
    if (previousHour != timeinfo.tm_hour || previousMinutes != timeinfo.tm_min)
    {
      std::vector<int> hh = {(timeinfo.tm_hour - timeinfo.tm_hour % 10) / 10, timeinfo.tm_hour % 10};
      std::vector<int> mm = {(timeinfo.tm_min - timeinfo.tm_min % 10) / 10, timeinfo.tm_min % 10};
      bool leadingZero = (hh.at(0) == 0);

      bool layoutChanged = (previousHH.empty() || previousLeadingZero != leadingZero);

      if (layoutChanged)
      {

        Screen.clear();
        delay(10);
        if (leadingZero)
        {
          hh.erase(hh.begin());
          Screen.drawBigNumbers(COLS / 2, 0, hh);
          delayMicroseconds(1000);
          Screen.drawBigNumbers(0, ROWS / 2, mm);
        }
        else
        {
          Screen.drawBigNumbers(0, 0, hh);
          delayMicroseconds(1000);
          Screen.drawBigNumbers(0, ROWS / 2, mm);
          delay(10);
        }
      }
      else
      {

        std::vector<int> displayHH = hh;
        if (leadingZero)
        {
          displayHH.erase(displayHH.begin());
          delay(10);
        }

        if (displayHH != previousHH)
        {
          int startX = leadingZero ? COLS / 2 : 0;
          Screen.drawBigNumbers(startX, 0, displayHH);
          delay(10);
        }

        if (mm != previousMM)
        {
          Screen.drawBigNumbers(0, ROWS / 2, mm);
          delay(10);
        }

        previousHH = displayHH;
        previousMM = mm;
      }

      if (layoutChanged)
      {

        std::vector<int> displayHH = hh;
        if (leadingZero)
        {
          displayHH.erase(displayHH.begin());
          delay(10);
        }
        previousHH = displayHH;
        previousMM = mm;
      }

      previousMinutes = timeinfo.tm_min;
      delay(10);
      previousHour = timeinfo.tm_hour;
      delay(10);
      previousLeadingZero = leadingZero;
      delay(10);
    }
  }
}

const char *BigClockPlugin::getName() const
{
  return "Big Clock";
}
