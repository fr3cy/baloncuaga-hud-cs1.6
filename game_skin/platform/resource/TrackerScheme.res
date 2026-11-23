//
// TRACKER SCHEME RESOURCE FILE
//
// sections:
//		colors			- all the colors used by the scheme
//		basesettings	- contains settings for app to use to draw controls
//		fonts			- list of all the fonts used by app
//		borders			- description of all the borders
//
// notes:
// 		hit ctrl-alt-shift-R in the app to reload this file
//
Scheme
{
	//////////////////////// COLORS ///////////////////////////
	Colors
	{
		// base colors
		"BaseText"			"232 234 238 255"	// used in text windows, lists
		"BrightBaseText"	"255 255 255 255"	// brightest text
		"SelectedText"		"17 18 21 255"	// selected text
		"DimBaseText"		"158 162 172 255"	// dim base text
		"LabelDimText"		"158 162 172 255"	// used for info text
		"ControlText"		"232 234 238 255"	// used in all text controls
		"BrightControlText"	"247 248 250 255"	// use for selected controls
		"DisabledText1"		"106 110 120 255"	// disabled text
		"DisabledText2"		"17 18 21 0"		// overlay color for disabled text (to give that inset look)
		"DimListText"		"158 162 172 255"	// offline friends, unsubscribed games, etc.

		// background colors
		"ControlBG"			"22 23 28 255"		// background color of controls
		"ControlDarkBG"		"10 11 15 255"		// darker background color; used for background of scrollbars
		"WindowBG"			"14 15 20 255"		// background color of text edit panes (chat, text entries, etc.)
		"SelectionBG"		"221 224 232 255"	// background color of any selected text or menu item
		"SelectionBG2"		"166 170 181 255"		// selection background in window w/o focus
		"ListBG"			"12 13 17 255"		// background of server browser, buddy list, etc.

		// titlebar colors
		"TitleText"			"249 250 252 255"
		"TitleDimText"		"157 161 173 255"
		"TitleBG"			"24 25 31 255"
		"TitleDimBG"		"17 18 23 255"
		
		// slider tick colors
		"SliderTickColor"	"140 144 156 255"
		"SliderTrackColor"	"7 8 11 255"

		// border colors
		"BorderBright"		"81 86 101 255"	// the lit side of a control
		"BorderDark"		"43 46 56 255"		// the dark/unlit side of a control
		"BorderSelection"	"177 184 204 255"			// the additional border color for displaying the default/selected button
		"BrandPressedBG"		"65 69 82 255"
		"BrandSlider"		"194 201 220 255"
		"BrandFrameBorder"		"67 72 88 255"
	}

	///////////////////// BASE SETTINGS ////////////////////////
	// default settings for all panels
	// controls use these to determine their settings
	BaseSettings
	{
		"FgColor"			"ControlText"
		"BgColor"			"ControlBG"
		"LabelBgColor"		"ControlBG"
		"SubPanelBgColor"	"ControlBG"

		"DisabledFgColor1"		"DisabledText1" 
		"DisabledFgColor2"		"DisabledText2"			// set this to the BgColor if you don't want it to draw

		"TitleBarFgColor"			"TitleText"
		"TitleBarDisabledFgColor"	"TitleDimText"
		"TitleBarBgColor"			"TitleBG"
		"TitleBarDisabledBgColor"	"TitleDimBG"

		"TitleBarIcon"				"resource/icon_steam"
		"TitleBarDisabledIcon"		"resource/icon_steam_disabled"

		"TitleButtonFgColor"			"BrightBaseText"
		"TitleButtonBgColor"			"ControlBG"
		"TitleButtonDisabledFgColor"	"DimBaseText"
		"TitleButtonDisabledBgColor"	"TitleDimBG"

		"TextCursorColor"			"BaseText"			// color of the blinking text cursor in text entries
		"URLTextColor"				"BrightBaseText"	// color that URL's show up in chat window

		Menu
		{
			"FgColor"			"BaseText"
			"BgColor"			"WindowBG"
			"ArmedFgColor"		"SelectedText"
			"ArmedBgColor"		"SelectionBG"
			"DividerColor"		"BorderDark"

			"TextInset"			"6"
		}

		MenuButton	  // the little arrow on the side of boxes that triggers drop down menus
		{
			"ButtonArrowColor"	"BaseText"		// color of arrows
		   	"ButtonBgColor"		"WindowBG"			// bg color of button. same as background color of text edit panes 
			"ArmedArrowColor"	"SelectedText"	// color of arrow when mouse is over button
			"ArmedBgColor"		"SelectionBG"		// bg color of button when mouse is over button
		}

		Slider
		{
			"SliderFgColor"		"BrandSlider"			// handle with which the slider is grabbed
			"SliderBgColor"		"BrandSlider"		// area behind handle
		}

		ScrollBarSlider
		{
			"BgColor"					"ControlDarkBG"		// this isn't really used
			"ScrollBarSliderFgColor"	"BrandSlider"		// handle with which the slider is grabbed
			"ScrollBarSliderBgColor"	"BrandSlider"	// area behind handle
			"ButtonFgColor"				"BaseText"	// color of arrows
		}


		// text edit windows
		"WindowFgColor"				"BaseText"		// off-white
		"WindowBgColor"				"WindowBG"		// redundant. can we get rid of WindowBgColor and just use WindowBG?
		"WindowDisabledFgColor"		"DimBaseText"
		"WindowDisabledBgColor"		"ListBG"		// background of chat conversation
		"SelectionFgColor"			"SelectedText"	// fg color of selected text
		"SelectionBgColor"			"SelectionBG"
		"ListSelectionFgColor"		"SelectedText"
		"ListBgColor"				"ListBG"		// background of server browser control, etc
		"BuddyListBgColor"			"ListBG"		// background of buddy list pane
		
		// App-specific stuff
		"ChatBgColor"				"WindowBG"

		// status selection
		"StatusSelectFgColor"		"BrightBaseText"
		"StatusSelectFgColor2"		"BrightControlText"	// this is the color of the friends status

		// checkboxes
		"CheckButtonBorder1"   		"BorderDark"		// the left checkbutton border
		"CheckButtonBorder2"   		"BorderBright"		// the right checkbutton border
		"CheckButtonCheck"			"BrightBaseText"	// color of the check itself
		"CheckBgColor"				"ListBG"

		// buttons (default fg/bg colors are used if these are not set)
//		"ButtonArmedFgColor"
//		"ButtonArmedBgColor"
//		"ButtonDepressedFgColor"	"BrightControlText"
//		"ButtonDepressedBgColor"

		// buddy buttons
		BuddyButton
		{
			"FgColor1"				"ControlText"
			"FgColor2"				"DimListText"
			"ArmedFgColor1"			"SelectedText"
			"ArmedFgColor2"			"SelectedText"
			"ArmedBgColor"			"SelectionBG"
		}

		Chat
		{
			"TextColor"				"BrightBaseText"
			"SelfTextColor"			"BaseText"
			"SeperatorTextColor"	"DimBaseText"
		}

		InGameDesktop
		{
			"MenuColor"			"228 232 241 255"
			"ArmedMenuColor"	"255 255 255 255"
			"DepressedMenuColor" "165 175 198 255"
			"WidescreenBarColor" "0 0 0 0"
			"MenuItemVisibilityRate" "0.06"  // time it takes for one menu item to appear
			"MenuItemHeight"	"28"
			"GameMenuInset"		"32"
		}

		"SectionTextColor"		"BrightBaseText"	// text color for IN-GAME, ONLINE, OFFLINE sections of buddy list
		"SectionDividerColor"	"BorderDark"		// color of line that runs under section name in buddy list
		"ButtonArmedFgColor"		"SelectedText"
		"ButtonArmedBgColor"		"SelectionBG"
		"ButtonDepressedFgColor"		"BrightBaseText"
		"ButtonDepressedBgColor"		"BrandPressedBG"
	}

	//
	//////////////////////// FONTS /////////////////////////////
	//
	// describes all the fonts
	Fonts
	{
		// fonts are used in order that they are listed
		// fonts listed later in the order will only be used if they fulfill a range not already filled
		// if a font fails to load then the subsequent fonts will replace
		"Default"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"16"
				"weight"	"500"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
		}
		"DefaultBold"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"16"
				"weight"	"700"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
		}
		"DefaultUnderline"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"16"
				"weight"	"500"
				"underline" "1"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
		}
		"DefaultSmall"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"13"
				"weight"	"500"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
		}
		"DefaultSmallBold"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"700"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
		}
		"DefaultVerySmall"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"500"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
		}
		"MenuLarge"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"18"
				"weight"	"700"
				"antialias" "1"
				"blur"		"0"
				"scanlines"		"0"
			}
		}
		"UiHeadline"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"16"
				"weight"	"700"
				"antialias" "0"
				"blur"		"0"
				"scanlines"		"0"
			}
		}

		// this is the symbol font
		"Marlett"
		{
			"1"
			{
				"name"		"Marlett"
				"tall"		"14"
				"weight"	"0"
				"symbol"	"1"
			}
		}
		
		"EngineFont"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"600"
				"yres"	"480 599"
				"dropshadow"	"1"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
			"2"
			{
				"name"		"Tahoma"
				"tall"		"13"
				"weight"	"600"
				"yres"	"600 767"
				"dropshadow"	"1"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
			"3"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"600"
				"yres"	"768 1023"
				"dropshadow"	"1"
				"antialias"		"0"
				"blur"		"0"
				"scanlines"		"0"
			}
			"4"
			{
				"name"		"Tahoma"
				"tall"		"20"
				"weight"	"600"
				"yres"	"1024 1199"
				"dropshadow"	"1"
				"antialias"		"1"
				"blur"		"0"
				"scanlines"		"0"
			}
			"5"
			{
				"name"		"Tahoma"
				"tall"		"24"
				"weight"	"600"
				"yres"	"1200 6000"
				"dropshadow"	"1"
				"antialias"		"1"
				"blur"		"0"
				"scanlines"		"0"
			}
		}	
		
		"CreditsFont"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"18"
				"weight"	"600"
				"antialias"	"1"
				"blur"		"0"
				"scanlines"		"0"
			}
		}

		"Legacy_CreditsFont" // Added to accomodate 3rd party server plugins, etc. This version should not scale.
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"20"
				"weight"	"600"
				"antialias"	"1"
				"yres"	"1 10000"
				"blur"		"0"
				"scanlines"		"0"
			}
		}

		"GameConsole_Mono"
		{
			"1"
			{
				"name"		"Courier"
				"tall"		"14"
				"weight"	"500"
			}
		}
	}

	//
	//////////////////// BORDERS //////////////////////////////
	//
	// describes all the border types
	Borders
	{
		// references to other borders
		BaseBorder		"InsetBorder"
		ComboBoxBorder	"InsetBorder"
		BrowserBorder	"InsetBorder"
		ButtonBorder	"RaisedBorder"
		FrameBorder		"RaisedBorder"
		TabBorder		"RaisedBorder"
		MenuBorder		"RaisedBorder"
		
		// standard borders
		InsetBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 0"
				}
			}
		}

		RaisedBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		// special border types
		TitleButtonBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "1 0"
				}
			}

			Top
			{
				"4"
				{
					"color" "BorderBright"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		TitleButtonDisabledBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BgColor"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BgColor"
					"offset" "1 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BgColor"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BgColor"
					"offset" "0 0"
				}
			}
		}

		TitleButtonDepressedBorder
		{
			"inset" "1 1 1 1"
			Left
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 0"
				}
			}
		}

		ScrollBarButtonBorder
		{
			"inset" "1 0 0 0"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		ScrollBarButtonDepressedBorder
		{
			"inset" "2 2 0 0"
			Left
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 0"
				}
			}
		}
		
		TabActiveBorder
		{
			"inset" "0 0 1 0"
			Left
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 0"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "ControlBG"
					"offset" "6 2"
				}
			}
		}


		ToolTipBorder
		{
			"inset" "0 0 1 0"
			Left
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}
		}

		// this is the border used for default buttons (the button that gets pressed when you hit enter)
		ButtonKeyFocusBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BorderSelection"
					"offset" "0 0"
				}
				"2"
				{
					"color" "BorderBright"
					"offset" "0 1"
				}
			}
			Top
			{
				"1"
				{
					"color" "BorderSelection"
					"offset" "0 0"
				}
				"2"
				{
					"color" "BorderBright"
					"offset" "1 0"
				}
			}
			Right
			{
				"1"
				{
					"color" "BorderSelection"
					"offset" "0 0"
				}
				"2"
				{
					"color" "BorderDark"
					"offset" "1 0"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "BorderSelection"
					"offset" "0 0"
				}
				"2"
				{
					"color" "BorderDark"
					"offset" "1 1"
				}
			}
		}

		ButtonDepressedBorder
		{
			"inset" "2 1 1 1"
			Left
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "BorderDark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BorderBright"
					"offset" "0 0"
				}
			}
		}
	}
}