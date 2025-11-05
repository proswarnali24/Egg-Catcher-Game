# EGG CATCHER:

## 🥚 Project Overview
I built Egg Catcher, a challenging grid-based game where the goal is to successfully manage two types of falling eggs with a movable basket. The game's mechanics require quick decision-making under pressure!

## 💡 Key Design and Implementation Choices

### 1. Game Entities and Logic
* **Basket Controls:** I implemented user control using the **Arrow Keys** (← and →) for intuitive movement across the screen.
* **Basket Appearance:** The basket uses its original blocky structure, rendered in a distinct **Brown** color to stand out against the dark grid.
* **Dueling Eggs System:** The core challenge revolves around two distinct egg types:
    * **Good Egg (White):** Successfully catching one grants **+1 Score**. Missing one results in a harsh penalty: **-1 Score and -1 Life**.
    * **Bad Egg (Burgundy):** Catching a bad egg incurs a penalty of **-1 Score**, but mercifully, the player's life count remains unchanged.
* **Lives:** The player starts with **5 lives**.

### 2. Low-Level Graphics Implementation
* **Custom Ellipse Algorithm:** To avoid relying on high-level built-in drawing functions (like `QPainter::drawEllipse`), I implemented a low-level, **implicit equation-based algorithm** to render the elliptical egg shapes.
* **Grid Simulation:** The algorithm checks pixel locations against the mathematical formula for an ellipse and draws visible $4 \times 4$ pixel blocks (`fillRect`) to simulate the shape within the $20 \times 20$ grid cells. This gives the eggs a deliberately stylized, retro look.

## 🚀 How to Run
The project is built using **Qt (C++)** and CMake. Compile and run the `EggCatcher` executable after configuring your build environment. 