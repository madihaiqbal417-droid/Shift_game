import math
from collections import deque

# Constants from main.cpp
W, H = 800, 600
PS = 40.0
PLAT_TOP = 290.0
PLAT_BOT = 310.0
GRAVITY = 1500.0
JUMP_SPEED = -650.0
MOVE_SPEED = 300.0
DRAIN_TIME = 3.5

def overlaps(p1, s1, p2, s2):
    return p1[0] < p2[0] + s2[0] and p1[0] + s1[0] > p2[0] and \
           p1[1] < p2[1] + s2[1] and p1[1] + s1[1] > p2[1]

class Solver:
    def __init__(self, obstacles, monsters, fill_time):
        self.obstacles = obstacles # List of (topWorld, deadly, x, width, height)
        self.monsters = monsters   # List of (startX, minX, maxX, speed)
        self.fill_time = fill_time

    def is_blocked(self, pos, top):
        # Middle platform
        if top:
            if pos[1] + PS > PLAT_TOP: return True
        else:
            if pos[1] < PLAT_BOT: return True

        # Solid obstacles
        for o_top, deadly, ox, ow, oh in self.obstacles:
            if o_top == top and not deadly:
                oy = PLAT_TOP - oh if o_top else PLAT_BOT
                if overlaps(pos, (PS, PS), (ox, oy), (ow, oh)):
                    return True
        return False

    def is_deadly(self, pos, top, time):
        # Deadly obstacles (forgiving hitbox: 4px inset)
        f_pos = (pos[0] + 4, pos[1] + 4)
        f_size = PS - 8
        for o_top, deadly, ox, ow, oh in self.obstacles:
            if o_top == top and deadly:
                oy = PLAT_TOP - oh if o_top else PLAT_BOT
                if overlaps(f_pos, (f_size, f_size), (ox, oy), (ow, oh)):
                    return True

        # Monsters (only in bottom world)
        if not top:
            for start_x, min_x, max_x, speed in self.monsters:
                # Approximate monster position at time
                # Monster oscillates: pos = min + (range * (1 + sin(...))) / 2?
                # No, code says: mp.x += m.direction * m.speed * dt
                # It's a linear patrol.
                dist = max_x - min_x
                if dist <= 0:
                    mx = start_x
                else:
                    # Total cycle distance is 2 * dist
                    # Time to complete cycle is 2 * dist / speed
                    cycle_time = 2 * dist / speed
                    t = time % cycle_time
                    if t < dist / speed:
                        mx = min_x + speed * t
                    else:
                        mx = max_x - speed * (t - dist/speed)

                if overlaps(f_pos, (f_size, f_size), (mx, PLAT_BOT), (36, 36)):
                    return True
        return False

    def solve(self):
        # state: (x, y, vy, top, nightmare, time)
        # We discretize for BFS
        # Use a coarse grid for x, y to keep state space manageable
        start = (60.0, PLAT_TOP - PS, 0.0, True, 0.0, 0.0)
        q = deque([start])
        visited = set()

        dt = 0.1 # 100ms steps

        while q:
            x, y, vy, top, nightmare, t = q.popleft()

            # Goal check
            if x >= 740 and top:
                return True

            # Discretize state for visiting
            # (x_grid, y_grid, vy_grid, top, night_grid)
            state = (int(x/5), int(y/5), int(vy/50), top, int(nightmare*20))
            if state in visited: continue
            visited.add(state)

            if t > 30.0: continue # Timeout

            # Possible actions: Move Left, Move Right, Idle, Jump, Shift
            for move in [-1, 0, 1]:
                # 1. Horizontal movement
                nx = x + move * MOVE_SPEED * dt
                nx = max(0, min(W - PS, nx))

                # Check collision (simple check)
                if self.is_blocked((nx, y), top):
                    # Slide against wall logic from main.cpp
                    # If moving right, stop at left edge of obstacle
                    # For simplicity in solver, we just don't move there if blocked
                    nx = x

                # 2. Gravity and Jump
                # Check if on ground
                on_ground = False
                if top:
                    if y + PS >= PLAT_TOP - 0.1: on_ground = True
                else:
                    if y <= PLAT_BOT + 0.1: on_ground = True

                # Check obstacles for ground
                for o_top, deadly, ox, ow, oh in self.obstacles:
                    if o_top == top and not deadly:
                        oy = PLAT_TOP - oh if o_top else PLAT_BOT
                        if top: # Landing on top of it
                            if overlaps((nx, y + 1), (PS, PS), (ox, oy), (ow, oh)):
                                on_ground = True
                        else: # Hanging from bottom of it
                            if overlaps((nx, y - 1), (PS, PS), (ox, oy), (ow, oh)):
                                on_ground = True

                jump_vy = vy
                if on_ground:
                    # Option to jump
                    jump_vys = [0.0, JUMP_SPEED * (1 if top else -1)]
                else:
                    jump_vys = [vy]

                for nvy in jump_vys:
                    # Apply gravity
                    f_vy = nvy + GRAVITY * (1 if top else -1) * dt
                    ny = y + f_vy * dt

                    # Clamp to floor/ceiling
                    if top:
                        if ny + PS > PLAT_TOP:
                            ny = PLAT_TOP - PS
                            f_vy = 0
                        if ny < 0:
                            ny = 0
                            f_vy = 0
                    else:
                        if ny < PLAT_BOT:
                            ny = PLAT_BOT
                            f_vy = 0
                        if ny + PS > H:
                            ny = H - PS
                            f_vy = 0

                    # Nightmare meter
                    n_night = nightmare + (dt/self.fill_time if not top else -dt/DRAIN_TIME)
                    n_night = max(0, min(1.0, n_night))

                    if n_night >= 1.0: continue # Died to nightmare

                    if self.is_deadly((nx, ny), top, t + dt): continue
                    if self.is_blocked((nx, ny), top): continue

                    q.append((nx, ny, f_vy, top, n_night, t + dt))

                    # Try shifting
                    my = H - ny - PS
                    if not self.is_blocked((nx, my), not top):
                        if not self.is_deadly((nx, my), not top, t + dt):
                            q.append((nx, my, -f_vy, not top, n_night, t + dt))

        return False

# Levels from main.cpp
levels = [
    # Level 7: The Needle
    { "fill": 4.0, "monsters": [(190, 190, 240, 120), (390, 390, 440, 120)],
      "obs": [(True, False, 150, 40, 220), (True, False, 250, 40, 220), (True, False, 350, 40, 220), (True, False, 450, 40, 220), (True, False, 550, 40, 220)] },
    # Level 8: Monster Alley
    { "fill": 5.0, "monsters": [(150, 100, 190, 150), (350, 300, 390, 150), (550, 500, 590, 150)],
      "obs": [(True, True, 100, 600, 30), (False, False, 200, 40, 100), (False, False, 400, 40, 100), (False, False, 600, 40, 100)] },
    # Level 9: Tight Fit
    { "fill": 4.0, "monsters": [],
      "obs": [(True, False, 200, 400, 150), (False, False, 250, 50, 100), (False, False, 400, 50, 100), (False, False, 550, 50, 100), (True, True, 300, 40, 30), (True, True, 450, 40, 30)] },
    # Level 10: The Grinder
    { "fill": 3.5, "monsters": [(150, 120, 180, 180), (350, 320, 380, 180), (550, 520, 580, 180)],
      "obs": [(True, True, 100, 40, 30), (True, True, 200, 40, 30), (True, True, 300, 40, 30), (True, True, 400, 40, 30), (True, True, 500, 40, 30), (True, True, 600, 40, 30)] },
    # Level 11: Deep Breath
    { "fill": 6.0, "monsters": [(200, 150, 600, 130), (400, 150, 600, 140)],
      "obs": [(True, False, 100, 550, 250), (False, True, 300, 100, 30)] },
    # Level 12: Mirror Maze
    { "fill": 4.0, "monsters": [],
      "obs": [(i%2==0, False, 150 + i*100, 30, 180) for i in range(6)] },
    # Level 13: Leap of Faith
    { "fill": 5.0, "monsters": [],
      "obs": [(True, True, 200, 150, 30), (False, False, 200, 150, 100), (True, False, 450, 150, 100), (False, True, 450, 150, 30)] },
    # Level 14: The Gauntlet
    { "fill": 4.5, "monsters": [(100, 80, 200, 160), (350, 300, 400, 170), (550, 500, 600, 180)],
      "obs": [(True, True, 250, 50, 30), (True, True, 450, 50, 30), (True, True, 650, 50, 30)] },
    # Level 15: Pressure
    { "fill": 3.0, "monsters": [],
      "obs": [(True, False, 200, 50, 200), (True, False, 400, 50, 200), (True, False, 600, 50, 200)] },
    # Level 16: Final Shift
    { "fill": 3.8, "monsters": [(150, 100, 250, 140), (650, 600, 720, 200)],
      "obs": [(True, True, 100, 200, 30), (False, False, 350, 40, 150), (True, False, 450, 40, 150), (False, False, 550, 40, 150), (True, True, 600, 140, 30)] },
]

for i, lvl in enumerate(levels):
    s = Solver(lvl["obs"], lvl["monsters"], lvl["fill"])
    print(f"Level {i+7}: {'Solvable' if s.solve() else 'IMPOSSIBLE'}")
