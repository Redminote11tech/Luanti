// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2018 stujones11, Stuart Jones <stujones111@gmail.com>

#include <IEventReceiver.h>
#include <IGUIComboBox.h>
#include <IGUIEditBox.h>
#include "client/renderingengine.h"
#include "modalMenu.h"
#include "gui/guiInventoryList.h"
#include "porting.h"
#include "settings.h"
#ifdef __vita__
#include "porting_vita.h"
#endif

PointerAction PointerAction::fromEvent(const SEvent &event) {
	switch (event.EventType) {
	case EET_MOUSE_INPUT_EVENT:
		return {v2s32(event.MouseInput.X, event.MouseInput.Y), porting::getTimeMs()};
	case EET_TOUCH_INPUT_EVENT:
		return {v2s32(event.TouchInput.X, event.TouchInput.Y), porting::getTimeMs()};
	default:
		FATAL_ERROR("SEvent given to PointerAction::fromEvent has wrong EventType");
	}
}

bool PointerAction::isRelated(PointerAction previous) {
	u64 time_delta = porting::getDeltaMs(previous.time, time);
	v2s32 pos_delta = pos - previous.pos;
	f32 distance_sq = (f32)pos_delta.X * pos_delta.X + (f32)pos_delta.Y * pos_delta.Y;

	return time_delta < 400 && distance_sq < (30.0f * 30.0f);
}

GUIModalMenu::GUIModalMenu(gui::IGUIEnvironment* env, gui::IGUIElement* parent,
	s32 id, IMenuManager *menumgr, bool remap_click_outside) :
		IGUIElement(gui::EGUIET_ELEMENT, env, parent, id,
				core::rect<s32>(0, 0, 100, 100)),
#ifdef __ANDROID__
		m_jni_field_name(""),
#endif
		m_menumgr(menumgr),
		m_remap_click_outside(remap_click_outside)
{
	m_gui_scale = g_settings->getFloat("gui_scaling", 0.5f, 20.0f) *
			RenderingEngine::getDisplayDensity();

	setVisible(true);
#ifdef __vita__
	m_pointer = v2s32(480, 272);
#endif
	m_menumgr->createdMenu(this);
}

GUIModalMenu::~GUIModalMenu()
{
#ifdef __vita__
	if (m_vita_edit)
		porting::cancelVitaTextInput();
#endif
	m_menumgr->deletingMenu(this);
}

void GUIModalMenu::allowFocusRemoval(bool allow)
{
	m_allow_focus_removal = allow;
}

bool GUIModalMenu::canTakeFocus(gui::IGUIElement *e)
{
	return (e && (e == this || isMyDescendant(e))) || m_allow_focus_removal;
}

void GUIModalMenu::draw()
{
	if (!IsVisible)
		return;
#ifdef __vita__
	irr_ptr<GUIModalMenu> holder;
	holder.grab(this);
#endif

	m_is_window_focused = RenderingEngine::get_raw_device()->isWindowFocused();

	video::IVideoDriver *driver = Environment->getVideoDriver();
	v2u32 screensize = driver->getScreenSize();
	if (screensize != m_screensize_old) {
		m_screensize_old = screensize;
		regenerateGui(screensize);
	}

#ifdef __vita__
	updateVitaInput();
	if (!IsVisible || !getParent())
		return;
#endif
	drawMenu();
#ifdef __vita__
	if (m_vita_show_cursor && !m_vita_edit) {
		video::SColor color(255, 255, 255, 255);
		driver->draw2DLine(m_pointer - v2s32(7, 0), m_pointer + v2s32(7, 0), color);
		driver->draw2DLine(m_pointer - v2s32(0, 7), m_pointer + v2s32(0, 7), color);
	}
#endif
}

/*
	This should be called when the menu wants to quit.

	WARNING: THIS DEALLOCATES THE MENU FROM MEMORY. Return
	immediately if you call this from the menu itself.

	(More precisely, this decrements the reference count.)
*/
void GUIModalMenu::quitMenu()
{
	allowFocusRemoval(true);
	m_menumgr->deletingMenu(this);
	this->remove();
}

static bool isChild(gui::IGUIElement *tocheck, gui::IGUIElement *parent)
{
	while (tocheck) {
		if (tocheck == parent) {
			return true;
		}
		tocheck = tocheck->getParent();
	}
	return false;
}

bool GUIModalMenu::remapClickOutside(const SEvent &event)
{
	if (!m_remap_click_outside || event.EventType != EET_MOUSE_INPUT_EVENT ||
			(event.MouseInput.Event != EMIE_LMOUSE_PRESSED_DOWN &&
					event.MouseInput.Event != EMIE_LMOUSE_LEFT_UP))
		return false;

	// The formspec must only be closed if both the EMIE_LMOUSE_PRESSED_DOWN and
	// the EMIE_LMOUSE_LEFT_UP event haven't been absorbed by something else.

	PointerAction last = m_last_click_outside;
	m_last_click_outside = {}; // always reset
	PointerAction current = PointerAction::fromEvent(event);

	gui::IGUIElement *hovered =
			Environment->getRootGUIElement()->getElementFromPoint(current.pos);
	if (isChild(hovered, this))
		return false;

	if (!m_is_window_focused)
		return false;

	if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
		m_last_click_outside = current;
		return true;
	}

	if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP &&
			current.isRelated(last)) {
		SEvent translated{};
		translated.EventType              = EET_KEY_INPUT_EVENT;
		translated.KeyInput.Key           = KEY_ESCAPE;
		translated.KeyInput.SystemKeyCode = EscapeKey.getScancode();
		translated.KeyInput.Control       = false;
		translated.KeyInput.Shift         = false;
		translated.KeyInput.PressedDown   = true;
		translated.KeyInput.Char          = 0;
		OnEvent(translated);
		return true;
	}

	return false;
}

bool GUIModalMenu::simulateMouseEvent(ETOUCH_INPUT_EVENT touch_event, bool second_try)
{
	IGUIElement *target;
	if (!second_try)
		target = Environment->getFocus();
	else
		target = m_touch_hovered.get();

	SEvent mouse_event{}; // value-initialized, not uninitialized
	mouse_event.EventType = EET_MOUSE_INPUT_EVENT;
	mouse_event.MouseInput.X = m_pointer.X;
	mouse_event.MouseInput.Y = m_pointer.Y;
	mouse_event.MouseInput.Simulated = true;
	switch (touch_event) {
	case ETIE_PRESSED_DOWN:
		mouse_event.MouseInput.Event = EMIE_LMOUSE_PRESSED_DOWN;
		mouse_event.MouseInput.ButtonStates = SDL_BUTTON_MASK(SDL_BUTTON_LEFT);
		break;
	case ETIE_MOVED:
		mouse_event.MouseInput.Event = EMIE_MOUSE_MOVED;
		mouse_event.MouseInput.ButtonStates = SDL_BUTTON_MASK(SDL_BUTTON_LEFT);
		break;
	case ETIE_LEFT_UP:
		mouse_event.MouseInput.Event = EMIE_LMOUSE_LEFT_UP;
		mouse_event.MouseInput.ButtonStates = 0;
		break;
	case ETIE_COUNT:
		// ETIE_COUNT is used for double-tap events.
		mouse_event.MouseInput.Event = EMIE_LMOUSE_DOUBLE_CLICK;
		mouse_event.MouseInput.ButtonStates = SDL_BUTTON_MASK(SDL_BUTTON_LEFT);
		break;
	default:
		return false;
	}

	bool retval;
	do {
		if (preprocessEvent(mouse_event)) {
			retval = true;
			break;
		}
		if (!target) {
			retval = false;
			break;
		}
		retval = target->OnEvent(mouse_event);
	} while (false);

	if (!retval && !second_try)
		return simulateMouseEvent(touch_event, true);

	return retval;
}

void GUIModalMenu::enter(gui::IGUIElement *hovered)
{
	if (!hovered)
		return;
	sanity_check(!m_touch_hovered);
	m_touch_hovered.grab(hovered);
	SEvent gui_event{};
	gui_event.EventType = EET_GUI_EVENT;
	gui_event.GUIEvent.Caller = m_touch_hovered.get();
	gui_event.GUIEvent.EventType = gui::EGET_ELEMENT_HOVERED;
	gui_event.GUIEvent.Element = gui_event.GUIEvent.Caller;
	m_touch_hovered->OnEvent(gui_event);
}

void GUIModalMenu::leave()
{
	if (!m_touch_hovered)
		return;
	SEvent gui_event{};
	gui_event.EventType = EET_GUI_EVENT;
	gui_event.GUIEvent.Caller = m_touch_hovered.get();
	gui_event.GUIEvent.EventType = gui::EGET_ELEMENT_LEFT;
	m_touch_hovered->OnEvent(gui_event);
	m_touch_hovered.reset();
}

bool GUIModalMenu::preprocessEvent(const SEvent &event)
{
#ifdef __vita__
	if (preprocessVitaEvent(event))
		return true;
#endif
#ifdef __ANDROID__
	// display software keyboard when clicking edit boxes
	if (event.EventType == EET_MOUSE_INPUT_EVENT &&
			((event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN &&
			!porting::hasPhysicalKeyboardAndroid()) ||
			event.MouseInput.Event == EMIE_LMOUSE_DOUBLE_CLICK)) {
		gui::IGUIElement *hovered =
			Environment->getRootGUIElement()->getElementFromPoint(
				core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y));
		if ((hovered) && (hovered->getType() == gui::EGUIET_EDIT_BOX)) {
			bool retval = hovered->OnEvent(event);
			if (retval)
				Environment->setFocus(hovered);

			std::string field_name = getNameByID(hovered->getID());
			// read-only field
			if (field_name.empty())
				return retval;

			m_jni_field_name = field_name;

			// single line text input
			int type = 2;

			// multi line text input
			if (((gui::IGUIEditBox *)hovered)->isMultiLineEnabled())
				type = 1;

			// passwords are always single line
			if (((gui::IGUIEditBox *)hovered)->isPasswordBox())
				type = 3;

			porting::showTextInputDialog("",
					wide_to_utf8(((gui::IGUIEditBox *) hovered)->getText()), type);
			// Since we have opened the dialog, we have to return true to mark
			// the event as handled (avoids double-opening).
			return true;
		}
	}

	if (event.EventType == EET_GUI_EVENT) {
		if (event.GUIEvent.EventType == gui::EGET_LISTBOX_OPENED) {
			gui::IGUIComboBox *dropdown = (gui::IGUIComboBox *) event.GUIEvent.Caller;

			std::string field_name = getNameByID(dropdown->getID());
			if (field_name.empty())
				return false;

			m_jni_field_name = field_name;

			s32 selected_idx = dropdown->getSelected();
			s32 option_size = dropdown->getItemCount();
			std::vector<std::string> list_of_options;

			for (s32 i = 0; i < option_size; i++) {
				list_of_options.push_back(wide_to_utf8(dropdown->getItem(i)));
			}

			porting::showComboBoxDialog(list_of_options.data(), option_size, selected_idx);
			return true; // Prevent the Irrlicht dropdown from opening.
		}
	}
#endif

	// If the second touch arrives here again, that means nobody handled it.
	// Abort to avoid infinite recursion.
	if (m_second_touch)
		return true;

	// Convert touch events into mouse events.
	if (event.EventType == EET_TOUCH_INPUT_EVENT) {
		irr_ptr<GUIModalMenu> holder;
		holder.grab(this); // keep this alive until return (it might be dropped downstream [?])

		if (event.TouchInput.touchedCount == 1) {
			m_pointer = v2s32(event.TouchInput.X, event.TouchInput.Y);

			gui::IGUIElement *hovered = Environment->getRootGUIElement()->getElementFromPoint(core::position2d<s32>(m_pointer));
			if (event.TouchInput.Event == ETIE_PRESSED_DOWN)
				Environment->setFocus(hovered);
			if (m_touch_hovered != hovered) {
				leave();
				enter(hovered);
			}
			bool ret = simulateMouseEvent(event.TouchInput.Event);
			if (event.TouchInput.Event == ETIE_LEFT_UP)
				leave();

			// Detect double-taps and convert them into double-click events.
			if (event.TouchInput.Event == ETIE_PRESSED_DOWN) {
				PointerAction current = PointerAction::fromEvent(event);
				if (current.isRelated(m_last_touch)) {
					// ETIE_COUNT is used for double-tap events.
					simulateMouseEvent(ETIE_COUNT);
				}
				m_last_touch = current;
			}

			return ret;
		} else if (event.TouchInput.touchedCount == 2) {
			if (event.TouchInput.Event != ETIE_LEFT_UP)
				return true; // ignore
			auto focused = Environment->getFocus();
			if (!focused)
				return true;
			// The second-touch event is propagated as is (not converted).
			m_second_touch = true;
			focused->OnEvent(event);
			m_second_touch = false;
			return true;
		} else {
			// Any other touch after the second touch is ignored.
			return true;
		}
	}

	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		if (!event.MouseInput.Simulated) {
			// Only process if this is a real mouse event.
			m_pointer = v2s32(event.MouseInput.X, event.MouseInput.Y);
			m_touch_hovered.reset();
		}

		if (remapClickOutside(event))
			return true;
	}

	return false;
}

#ifdef __ANDROID__
porting::AndroidDialogState GUIModalMenu::getAndroidUIInputState()
{
	// No dialog is shown
	if (m_jni_field_name.empty())
		return porting::DIALOG_CANCELED;

	return porting::getInputDialogState();
}
#endif

GUIModalMenu::ScalingInfo GUIModalMenu::getScalingInfo(v2u32 screensize, v2u32 base_size)
{
	f32 scale = m_gui_scale;
	scale = std::min(scale, (f32)screensize.X / (f32)base_size.X);
	scale = std::min(scale, (f32)screensize.Y / (f32)base_size.Y);
	s32 w = base_size.X * scale, h = base_size.Y * scale;
	return {scale, core::rect<s32>(
		screensize.X / 2 - w / 2,
		screensize.Y / 2 - h / 2,
		screensize.X / 2 + w / 2,
		screensize.Y / 2 + h / 2
	)};
}

#ifdef __vita__
void GUIModalMenu::setVisible(bool visible)
{
	IGUIElement::setVisible(visible);
	m_vita_axis = v2f(0, 0);
	std::fill(std::begin(m_vita_dpad), std::end(m_vita_dpad), false);
	m_vita_buttons = 0;
	m_vita_shift = false;
	m_vita_last_frame = 0;
	if (!visible && m_vita_edit) {
		porting::cancelVitaTextInput();
		m_vita_edit.reset();
	}
}

void GUIModalMenu::updateVitaInput()
{
	const u64 now = porting::getTimeMs();
	const float dt = m_vita_last_frame ? std::min(0.05f, (now - m_vita_last_frame) / 1000.f) : 0.f;
	m_vita_last_frame = now;
	if (m_vita_edit) {
		std::wstring text;
		int result = porting::pollVitaTextInput(text);
		if (!result)
			return;
		auto edit = std::move(m_vita_edit);
		// A server may replace a formspec while its keyboard is open.
		if (result > 0 && isMyDescendant(edit.get())) {
			edit->setText(text.c_str());
			onVitaTextAccepted(edit.get());
		}
		return;
	}
	v2f direction = m_vita_axis;
	direction.X += int(m_vita_dpad[3]) - int(m_vita_dpad[2]);
	direction.Y += int(m_vita_dpad[1]) - int(m_vita_dpad[0]);
	if (direction.getLengthSQ() == 0)
		return;
	m_vita_cursor += direction * (360.f * dt);
	m_vita_cursor.X = std::clamp(m_vita_cursor.X, 0.f, float(m_screensize_old.X - 1));
	m_vita_cursor.Y = std::clamp(m_vita_cursor.Y, 0.f, float(m_screensize_old.Y - 1));
	m_pointer = v2s32(m_vita_cursor.X, m_vita_cursor.Y);
	SEvent mouse{};
	mouse.EventType = EET_MOUSE_INPUT_EVENT;
	mouse.MouseInput.Event = EMIE_MOUSE_MOVED;
	mouse.MouseInput.X = m_pointer.X;
	mouse.MouseInput.Y = m_pointer.Y;
	mouse.MouseInput.ButtonStates = m_vita_buttons;
	mouse.MouseInput.Shift = m_vita_shift;
	mouse.MouseInput.Simulated = true;
	RenderingEngine::get_raw_device()->postEventFromUser(mouse);
}

bool GUIModalMenu::preprocessVitaEvent(const SEvent &event)
{
	if (m_vita_edit && (event.EventType == EET_GAMEPAD_BUTTON_EVENT ||
			event.EventType == EET_GAMEPAD_AXIS_EVENT ||
			event.EventType == EET_TOUCH_INPUT_EVENT ||
			event.EventType == EET_MOUSE_INPUT_EVENT))
		return true;
	if (event.EventType == EET_MOUSE_INPUT_EVENT &&
			event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP) {
		auto hovered = Environment->getRootGUIElement()->getElementFromPoint(
			v2s32(event.MouseInput.X, event.MouseInput.Y));
		if (hovered && hovered->getType() == gui::EGUIET_EDIT_BOX &&
				isMyDescendant(hovered) && hovered->acceptsIME()) {
			auto edit = static_cast<gui::IGUIEditBox *>(hovered);
			if (!getNameByID(edit->getID()).empty() &&
					porting::startVitaTextInput(edit->getText(), edit->isPasswordBox(),
						edit->isMultiLineEnabled())) {
				m_vita_edit.grab(edit);
				m_vita_axis = v2f(0, 0);
				std::fill(std::begin(m_vita_dpad), std::end(m_vita_dpad), false);
				m_vita_buttons = 0;
				m_vita_shift = false;
				return true;
			}
		}
	}
	if (event.EventType == EET_TOUCH_INPUT_EVENT) {
		m_vita_show_cursor = false;
		m_vita_cursor = v2f(event.TouchInput.X, event.TouchInput.Y);
	}
	if (event.EventType == EET_GAMEPAD_AXIS_EVENT) {
		auto axis = event.GamepadAxisEvent.Axis;
		if (axis == GamepadAxis::LEFTX || axis == GamepadAxis::LEFTY) {
			float value = event.GamepadAxisEvent.Value / 32768.f;
			value = std::abs(value) < 0.15f ? 0.f : value;
			(axis == GamepadAxis::LEFTX ? m_vita_axis.X : m_vita_axis.Y) = value;
			m_vita_show_cursor = true;
		}
		return true;
	}
	if (event.EventType != EET_GAMEPAD_BUTTON_EVENT)
		return false;
	m_vita_show_cursor = true;
	const auto button = event.GamepadButtonEvent.Button;
	const bool pressed = event.GamepadButtonEvent.PressedDown;
	if (button >= GamepadButton::DPAD_UP && button <= GamepadButton::DPAD_RIGHT) {
		m_vita_dpad[int(button) - int(GamepadButton::DPAD_UP)] = pressed;
		return true;
	}
	if (button == GamepadButton::EAST || button == GamepadButton::START) {
		if (pressed) {
			SEvent escape{};
			escape.EventType = EET_KEY_INPUT_EVENT;
			escape.KeyInput.Key = KEY_ESCAPE;
			escape.KeyInput.PressedDown = true;
			escape.KeyInput.SystemKeyCode = EscapeKey.getScancode();
			OnEvent(escape);
		}
		return true;
	}
	if (button != GamepadButton::SOUTH && button != GamepadButton::WEST &&
			button != GamepadButton::LEFT_SHOULDER && button != GamepadButton::RIGHT_SHOULDER)
		return false;
	const bool right = button == GamepadButton::LEFT_SHOULDER;
	const u32 mask = SDL_BUTTON_MASK(right ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT);
	if (pressed)
		m_vita_buttons |= mask;
	else
		m_vita_buttons &= ~mask;
	m_vita_shift = button == GamepadButton::WEST;
	SEvent mouse{};
	mouse.EventType = EET_MOUSE_INPUT_EVENT;
	mouse.MouseInput.Event = right ? (pressed ? EMIE_RMOUSE_PRESSED_DOWN : EMIE_RMOUSE_LEFT_UP) :
		(pressed ? EMIE_LMOUSE_PRESSED_DOWN : EMIE_LMOUSE_LEFT_UP);
	mouse.MouseInput.X = m_pointer.X;
	mouse.MouseInput.Y = m_pointer.Y;
	mouse.MouseInput.ButtonStates = m_vita_buttons;
	mouse.MouseInput.Shift = m_vita_shift;
	mouse.MouseInput.Simulated = true;
	irr_ptr<GUIModalMenu> holder;
	holder.grab(this);
	RenderingEngine::get_raw_device()->postEventFromUser(mouse);
	if (!pressed && button == GamepadButton::WEST)
		m_vita_shift = false;
	return true;
}

void GUIModalMenu::onVitaTextAccepted(gui::IGUIEditBox *edit)
{
	SEvent changed{};
	changed.EventType = EET_GUI_EVENT;
	changed.GUIEvent.Caller = edit;
	changed.GUIEvent.EventType = gui::EGET_EDITBOX_CHANGED;
	if (edit->getParent())
		edit->getParent()->OnEvent(changed);
}
#endif
