#pragma once 
#include <vector>
#include <array>
#include <bitset>
#include <GLFW/glfw3.h>
#include <variant>
#include <glm/vec2.hpp>

// Full Input system flow 
// Operating System generates events internally  -> GLFW handles events and calls callbacks -> Callbacks 
// again generate events which when consumed update in engine input state  -> Game queries InputState for input events


struct KeyEvent {
	int  key;
	int  action; 
};

// double because glfw cursor position is double and we want to avoid precision loss
struct MouseMoveEvent {
	glm::vec2 position{};
};

struct MouseButtonEvent {
	int button;
	int action;
};

using Event = std::variant<KeyEvent, MouseMoveEvent, MouseButtonEvent>; 

// in input system most state is binary so bitset > array as uses less memory 
// GLFW supports up to 512 keys and 8 mouse buttons 
constexpr uint16_t MAX_KEYS = 512;
constexpr uint8_t MAX_MOUSE_BUTTONS = 8;

// run time polymorphism using std::variant and std::visit 

struct InputState {

	// query state 

	InputState() = default; 

	void beginFrame() {
		keyPressed.reset();
		keyReleased.reset();
		mouseButtonPressed.reset();
		mouseButtonReleased.reset();
		events.clear();
	}

	[[nodiscard]] bool isHeld(int key) const {return keyHeld[key];}
	[[nodiscard]] bool wasPressed(int key) const { return keyPressed[key];}
	[[nodiscard]] bool wasReleased(int key) const { return keyReleased[key]; }
	[[nodiscard]] bool isMouseButtonHeld(int button) const { return mouseButtonHeld[button]; }
	[[nodiscard]] bool wasMouseButtonPressed(int button) const { return mouseButtonPressed[button]; }
	[[nodiscard]] bool wasMouseButtonReleased(int button) const { return mouseButtonReleased[button]; }
	[[nodiscard]] glm::vec2 getMousePosition() const { return mousePosition; }

	// events when processed update state 
	void processEvents() {
		for (const auto& event : events) {
			std::visit([this](const auto& event) { process(event);},event);
		}
		events.clear();
	}

	std::vector<Event> events;

private : 
	std::bitset<MAX_KEYS> keyPressed; 
	std::bitset<MAX_KEYS> keyReleased; 
	std::bitset<MAX_KEYS> keyHeld;

	std::bitset<MAX_MOUSE_BUTTONS> mouseButtonPressed;
	std::bitset<MAX_MOUSE_BUTTONS> mouseButtonReleased;
	std::bitset<MAX_MOUSE_BUTTONS> mouseButtonHeld;

	glm::vec2 mousePosition{};

	//mouse move
	void process(const MouseMoveEvent& event) {
		mousePosition = event.position;
	}
	
	//key events
	void process(const KeyEvent& event) {
		if (event.action == GLFW_PRESS) {
			keyPressed.set(event.key);
			keyHeld.set(event.key);
		}
		else if (event.action == GLFW_RELEASE) {
			keyReleased.set(event.key);
			keyHeld.reset(event.key);
		}
	}

	void process(const MouseButtonEvent& event) {
		if (event.action == GLFW_PRESS) {
			mouseButtonPressed.set(event.button);
			mouseButtonHeld.set(event.button);
		}
		else if (event.action == GLFW_RELEASE) {
			mouseButtonReleased.set(event.button);
			mouseButtonHeld.reset(event.button);
		}
	}

};


class GlfwInputAdapter {
public:
	GlfwInputAdapter(GLFWwindow* window, InputState& input) : inputState(input) {
		glfwSetWindowUserPointer(window, this);
	};

	InputState& inputState;
	
};

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto adapter = static_cast<GlfwInputAdapter*>(glfwGetWindowUserPointer(window));
	adapter->inputState.events.push_back(KeyEvent(key, action));
}

static void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto adapter = static_cast<GlfwInputAdapter*>(glfwGetWindowUserPointer(window));
	adapter->inputState.events.push_back(MouseMoveEvent{ glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos)) });
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto adapter = static_cast<GlfwInputAdapter*>(glfwGetWindowUserPointer(window));
	adapter->inputState.events.push_back(MouseButtonEvent{ button, action });
}