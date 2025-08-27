# Simulation-Driven Game Engine Project

A Defold game engine project with C++ native extensions implementing a simulation-driven architecture.

## Getting Started

### Install Defold

1. Download Defold from [defold.com/download](https://defold.com/download/)
2. Install and launch the Defold editor

### Setup Project

1. Clone this repository
2. Open the project in Defold (`File → Open Project`)
3. **Important**: Go to `Project → Fetch Libraries` to download dependencies
4. Build the project (`F5` or `Project → Build`)

### Building

- The C++ native extension is automatically built by Defold's cloud build servers
- No local C++ compiler setup required
- First build may take longer as the extension compiles remotely

## Architecture

- C++ simulation handles all game logic and state
- Lua manages commands, rendering, and user input
- See `docs/sim_architecture.md` for detailed architecture information
