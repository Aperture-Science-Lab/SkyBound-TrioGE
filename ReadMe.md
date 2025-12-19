# SkyBound - Flight Simulator

SkyBound is a 3D flight simulation application built with C++ and OpenGL. It features realistic flight physics, multiple aircraft selection, day/night cycles, and dynamic camera systems.

## Features

- **Advanced Flight Physics**: Simulation of lift, drag, gravity, and thrust, including angle-of-attack calculations and ground handling.
- **Aircraft Selection**: Choose from three distinct aircraft types (Zero Fighter, Interceptor, Stealth Jet) with unique visual styles.
- **Dynamic Environment**:
  - Sky system with day/night transitions.
  - Volumetric cloud rendering.
  - Infinite ocean generation.
- **Visual Effects**:
  - Particle systems for smoke and crash effects.
  - Dynamic lighting including navigation and strobe lights.
  - View bobbing and camera shake based on speed and turbulence.
- **Multiple Camera Modes**: Toggle between Cockpit view and 3rd Person chase camera.

## Controls

### Menu / Plane Selection
- **Left / Right Arrow** or **A / D**: Cycle through available aircraft.
- **Enter**: Confirm selection and start flight.
- **Esc**: Exit application.

### Flight Controls
- **W**: Increase Throttle (Accelerate).
- **S**: Decrease Throttle (Decelerate).
- **A / D**: 
  - **In Flight**: Yaw (Rudder) control.
  - **On Ground**: Steering (Taxiing).
- **Mouse X**: Roll (Bank left/right) - *Active only when airborne*.
- **Mouse Y**: Pitch (Nose up/down).
- **C**: Toggle Camera Mode (Cockpit / 3rd Person).
- **R**: Reset Simulation (useful after crashing).
- **Esc**: Return to main menu (from pause/options if available) or Exit.

## Getting Started

### Prerequisites
- Windows OS
- Visual Studio (Solution files included for VS)
- OpenGL libraries (GLUT/GLEW included in project dependencies)

### Building and Running
1. Open `OpenGLMeshLoader.sln` in Visual Studio.
2. Select the **Debug** or **Release** configuration.
3. Build the solution (**Ctrl+Shift+B**).
4. Run the application (**F5**).

## Project Structure
- **OpenGLMeshLoader.cpp**: Main entry point and window management.
- **FlightController.cpp**: Handles all aircraft physics, input processing, and movement logic.
- **GameManager.cpp**: Manages game states and level transitions.
- **Level1.cpp / Level2.cpp**: Game levels (Carrier deck and Free flight).
- **Model_3DS.cpp**: Custom 3DS model loader.
- **SkySystem.cpp**: Renders the atmospheric sky and clouds.
## Pictures 

![WhatsApp Image 2025-12-09 at 23 16 06_ca46cb12](https://github.com/user-attachments/assets/5d9b365d-f81d-4b33-95e8-f2bd5da377b9)
![WhatsApp Image 2025-12-09 at 22 03 28_7d77a91d](https://github.com/user-attachments/assets/53386fa2-3a7f-4208-891c-7a696b975ebe)
![WhatsApp Image 2025-12-09 at 01 43 35_8ea9275a](https://github.com/user-attachments/assets/83eb1583-5df4-4f61-9647-f1dde7147c57)
![WhatsApp Image 2025-12-09 at 01 22 02_d79e86c9](https://github.com/user-attachments/assets/87444c18-558a-4435-a103-302e1b4086b7)



