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

#ifndef BUTTON_H_
#define BUTTON_H_

/*! @brief Implements debounce button.
 *
 * This class implements a button with software debouncing. The button
 * currently recognizes the pressed, released and press and hold states.
 *
 * The button implements SW debouncing algorithm (50ms).
 *
 * The button shall be in the pull-down configuration. The pressed state is
 * level high.
 */
class Button
{
public:

	/*! @brief Enumerates possible states of the button. */
	enum State {
		idle,				/*!< initial state or state after resetState */
		pressed,			/*!< pressed. Ignore this state if press_hold is used too */
		released,			/*!< released */
		press_hold,			/*!< held pressed for at least 1000ms. The button should be acknowledged (reset) by the owner, not released */
		double_press		/*!< not supported */
	};

public:

	/*!
	 * @brief      Constructor
	 *
	 * @param[in]  The digital pin for use
	 */
	Button(int pin);
	virtual ~Button() {};

	/*!
	 * @brief      Detects the current state of the button
	 *
	 * Notice when the button is released its state transfers from
	 * pressed to released, not Idle.
	 *
	 * If the button is in released or press_hold state the owner of the button
	 * shall call resetState in order to reset the state machine
	 */
	State getState();

	/*!
	 * @brief      Resets state of the button to Idle
	 *
	 * The state of the button should be reset by the owner in order to
	 * acknowledge that the released or press-and-hold state is processed
	 */
	void resetState()
	{
		mState = idle;
		lastPressAndHoldTime = 0;
	}

	/*!
	 * @brief      Configures the digital pin
	 *
	 * Has to be called in the setup by the owner
	 */
	void begin()
	{
		pinMode(mPin, INPUT);
	}

private:

	State mState;					//!< current State
	int mPin;						//!< digital inpit pin connecting the button
	int mButtonState;				//!< the physical state, currently pressed or depressed
	long lastDebounceTime;  		//!< the last time the output pin was toggled (ms)
	long lastPressAndHoldTime;
	int lastButtonState;

	static const long debounceDelay = 50;
	static const long pressAndHoldDelay = 1000;

};



#endif /* BUTTON_H_ */
