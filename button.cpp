/*******************************************************************************
 ******************************* Copyright 2015 ********************************
 *******************************************************************************
 *
 * Button class
 *
 * Created on: 		2015-11-05
 * Modified on:
 * Author:			Mikhail Soloviev
 *
 *******************************************************************************
 */

#include "Arduino.h"
#include "button.h"

Button::Button(int pin) : mState(idle), mPin(pin), mButtonState(LOW), lastDebounceTime(0), lastPressAndHoldTime(0),
						  lastButtonState(LOW)
{
}



Button::State Button::getState()
{
	unsigned long ms = millis();

	// provide for the wrap around of millis()

	// if the button entered press_hold state, it shall remain in that
	// until the state is reset by the owner. Until that the pin readings are
	// not important

	if(lastPressAndHoldTime && (ms < lastPressAndHoldTime || (ms - lastPressAndHoldTime) > pressAndHoldDelay))
	{
		mState = press_hold;
		return mState;
	}

	int reading = digitalRead(mPin);

	if (reading != lastButtonState)
	{
		// reset the debouncing timer
		lastDebounceTime = millis();
	}

	if (lastDebounceTime && ((millis() - lastDebounceTime) > debounceDelay))
	{
		lastDebounceTime = 0;

		// whatever the reading is at, it's been there for longer
		// than the debounce delay, so take it as the actual current state:

		// if the button state has changed:
		if (reading != mButtonState)
		{
			mButtonState = reading;

			if(reading == HIGH)
			{
				mState = pressed;
				lastPressAndHoldTime = millis();
			}
			else if (reading == LOW)
			{
				if(mState == pressed)
				{
					mState = released;
				}
				lastPressAndHoldTime = 0;
			}
		}
	}


	// save the reading.  Next time through the loop,
	// it'll be the lastButtonState:
	lastButtonState = reading;
	return mState;
}



