# Gomoku

A feature-rich **Gomoku** (五目並べ) game written in **C++20** from scratch using **SFML 3** for graphics and input.  

Two players alternate placing stones on a 19×19 board, aiming to form an **uninterrupted line of five stones** or to **capture ten of the opponent's stones** by flanking pairs. The game supports **local hotseat** multiplayer and **versus AI** mode, with a strong search-based AI opponent powered by alpha-beta pruning.

> **Note:** Stones are displayed as **Blue** (Black) and **Red** (White) for visual clarity.

---

## Features

- **Standard Gomoku Rules** on a traditional 19×19 grid
- **Capture System** – capture two opponent stones by sandwiching them with your own (flank capture)
- **Five-in-a-Row Win** – align five (or more) of your stones in any direction
- **Capture Win** – first player to capture 10 stones wins
- **Five Deferred by Capture** – a five-in-a-row does not immediately win if the opponent can break every winning run with a single capture move
- **Double-Three (Free-Three) Forbidden Move** – Black cannot place a stone that creates two separate "open three" patterns simultaneously (standard Gomoku anti-cheating rule)
- **Advanced AI** with:
  - **Iterative-deepening negamax** with **principal variation search** (PVS)
  - **Transposition table** (Zobrist hashing) for position caching
  - **Killer-move heuristic** for move ordering
  - **Null-move pruning** and **late-move reductions** (LMR)
  - **Aspiration windows** for tighter search bounds
  - **Tactical hot-spot detection** and capture-race awareness
  - Time-managed search (~465 ms per move)
  - Multi-threaded asynchronous AI computation (does not block the UI)
- **Suggestion feature** – ask the AI for a recommended move at any time (Blue's perspective)
- **Interactive UI** with clearly-defined board, panel with status, captures, and debug information
- **Polished visual design** – oval stones with gloss highlights and drop shadows, board coordinate indices, move markers, and star points

---

## Screenshot

![Gomoku Game](screenshots/game.png)

*(A screenshot of the game in progress.)*

---

## How to Play

### Game Modes

- **Hotseat** – two players take turns on the same machine
- **Vs AI** – play as **Blue** (Black) against the **Red AI** opponent

### Controls

| Action          | Input                  |
|-----------------|------------------------|
| Place a stone   | Left-click on an empty intersection |
| Restart         | Click **Restart** button (keeps current mode) |
| Full Reset      | Click **Full Reset** (resets to Hotseat) |
| Toggle Mode     | Click **Switch to Vs AI** / **Switch to Hotseat** |
| Get Suggestion  | Click **Suggestion** to highlight the AI's recommended move |

### Win Conditions

1. **Five in a Row** – align five consecutive stones in a horizontal, vertical, or diagonal line.  
   ⚠️ *If the opponent can break every such line by capturing a pair in a single move, the game continues.*
2. **Capture Win** – capture a total of **10 opponent stones**.  
   A capture occurs when you place a stone that sandwiches two adjacent opponent stones between your stones on any axis (e.g., `X O O X` → the two `O` stones are removed).

---

## Building & Running

### Dependencies

- **C++20** compiler (`g++` or `clang++` with C++20 support)
- **SFML 3** (Simple and Fast Multimedia Library): `libsfml-graphics`, `libsfml-window`, `libsfml-system`
- **CMake** (optional) / `make`

### Build

```bash
make
```

This compiles the project using the provided `Makefile` with `-Wall -Wextra -Werror` and C++20 standard.

### Run

```bash
make run
# or directly:
./Gomoku
```

### Clean

```bash
make clean        # remove object files
make fclean       # remove object files and binary
```

---

## Project Structure

```
├── Makefile              # Build system
├── include/              # Header files
│   ├── AI.hpp            # AI search interface
│   ├── App.hpp           # Application main loop
│   ├── Board.hpp         # 19×19 board representation
│   ├── Controller.hpp    # Input handling, UI state, button logic
│   ├── engine_api.hpp    # Engine API (facade over game state + rules)
│   ├── GameState.hpp     # Complete game state (board, captures, debug info)
│   ├── Move.hpp          # Position struct and coordinate conversion
│   ├── Renderer.hpp      # SFML rendering (board, stones, panel, text)
│   ├── Rules.hpp         # Game rules (win detection, captures, forbidden moves)
│   └── Zobrist.hpp       # Zobrist hashing for transposition table
├── src/                  # Source files
│   ├── main.cpp          # Entry point
│   ├── App.cpp           # Application initialization and event loop
│   ├── AI.cpp            # AI search: negamax, PVS, alpha-beta, heuristics (~1400 lines)
│   ├── Board.cpp         # Board operations and cell management
│   ├── Controller.cpp    # Mouse/button handling, AI turn management
│   ├── GameState.cpp     # Game state initialization and reset
│   ├── Move.cpp          # Coordinate conversion (e.g., A1, J10)
│   ├── Renderer.cpp      # All drawing routines
│   ├── Rules.cpp         # Capture, win, and forbidden-move logic
│   └── Zobrist.cpp       # Zobrist hash computation
├── assets/               # Font files for rendering
│   ├── arial.ttf
│   ├── DejaVuSans.ttf
│   └── font.ttf
└── screenshots/          # (optional) Screenshot directory
```

---

## AI Overview

The AI (in `src/AI.cpp`) implements a competitive Gomoku playing engine using:

| Technique | Description |
|-----------|-------------|
| **Negamax with PVS** | Principal Variation Search (a variant of alpha-beta) with full-window and zero-window search |
| **Iterative Deepening** | Searches progressively deeper, up to depth 20, within a ~465 ms time budget |
| **Transposition Table** | Zobrist hashing to cache evaluated positions across branches |
| **Killer Moves** | Refutation moves stored per ply to improve move ordering |
| **Null-Move Pruning** | Skip a move for the side to play when the position is strong enough |
| **Late-Move Reductions** | Reduce search depth for moves unlikely to cause a cutoff |
| **Aspiration Windows** | Narrow the initial alpha-beta window around the previous iteration's score |
| **Capture Race Awareness** | Dynamically widens search when either player approaches 10 captures |
| **Multi-threaded** | AI search runs on a background thread, allowing the UI to remain responsive |

### Evaluation Function

The evaluation considers:

- **Material balance** (captured stones × 3200)
- **Sequence scoring** – patterns of 2–5 stones with open/semi-open ends
- **Capture potential** – immediate and latent capture threats
- **Open pairs** (OO patterns) – double-open and semi-open adjacent pairs
- **Vulnerable pairs** – pairs exposed to opponent flank capture
- **Last-move pressure** – threat intensity from recent moves
- **Proximity to capture win** – scaling bonuses when either side is close to 10 captures

---

## Rules Details

### Captures

A capture is made by placing a stone that creates the pattern `X O O X` along any axis. The two opponent stones between your stones are removed from the board and added to your capture count. Captures can happen in both directions simultaneously from a single placement.

### Five-in-a-Row Resolution

When a player forms five or more stones in a row:
1. The game checks if the opponent can break **every** winning line with a **single flank capture**.
2. If so, the five-in-a-row is considered **deferred** – play continues.
3. If the opponent has no capture that breaks all winning runs, the mover wins immediately.



### Forbidden Move (Double-Three)

Black (Blue) is prohibited from making a move that simultaneously creates two separate "open three" patterns (a free-three that could become an open-four). If a move creates both a capture and a double-three, the capture takes precedence, and the move is allowed. This rule applies only to Black (the first player) per standard Gomoku conventions.

<img width="1495" height="709" alt="Screenshot 2026-04-21 155612" src="https://github.com/user-attachments/assets/489c11a9-2ac9-4a3c-b6ff-281b6fde6181" />

<img width="1090" height="817" alt="Screenshot 2026-04-19 202546" src="https://github.com/user-attachments/assets/c0d17deb-f961-49bd-9326-40cb83479ebe" />

<img width="1117" height="842" alt="Screenshot 2026-04-18 225221" src="https://github.com/user-attachments/assets/1d961c57-039e-42e5-8337-1847fc160060" />

<img width="1094" height="807" alt="Screenshot 2026-04-19 193419" src="https://github.com/user-attachments/assets/4f70b0b3-6365-4802-8539-23f8314269c7" />





