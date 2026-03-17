"""
LiDAR Escape Map Editor
A simple 2D top-down editor for making maps creation easier and quicker.
"""

import tkinter as tk
from tkinter import filedialog, messagebox
import os

# constants
GRID_SIZE = 40 #size of grid cells in pixels (world units per cell)
DEFAULT_MAP_FILE = "map.txt" # default file name for saving/loading

# color mapping for different objects (only walls for now)
BOX_COLORS = {
    "wall": "#bababa" #grayish
}


class MapEditor:
    """Main class"""

    def __init__(self, root):
        """Initialize the editor window, set up variables, build UI, and load default map"""
        self.root = root
        self.root.title("LiDAR Escape - Simple Editor")
        self.root.geometry("1500x900")
        self.root.configure(bg="#181818")

        # view transformation
        self.zoom = 1.0
        self.min_zoom = 0.25
        self.max_zoom = 4.0
        self.offset_x = 0
        self.offset_y = 0

        # tool and geometry settings
        self.current_tool = "wall"
        self.wall_height = 2.15
        self.floor_y = 0.0
        self.floor_thickness = 0.5
        self.ceiling_thickness = 0.30
        self.spawn_height = 1.55
        self.auto_padding = 0.0 # extra space around walls for auto floor/ceiling

        self.boxes = []
        self.spawn = [0.0, 1.55, 7.0] # [x, y, z] spawn point

        # different states
        self.selected_index = None
        self.dragging = False
        self.drag_mode = None
        self.drag_start_world = None
        self.drag_start_box = None

        # wall creation preview
        self.creating = False
        self.create_start = None
        self.preview_rect = None

        # panning state
        self.panning = False
        self.pan_last = (0, 0)

        self.status_var = tk.StringVar()
        self.file_var = tk.StringVar(value=f"File: {DEFAULT_MAP_FILE}")

        self.build_ui()
        self.bind_events()
        self.load_default_map()
        self.redraw()

    # ui
    def build_ui(self):
        """Create whole GUI"""
        # left side
        self.left_panel = tk.Frame(self.root, bg="#202225", width=360)
        self.left_panel.pack(side=tk.LEFT, fill=tk.Y)

        # right side
        self.right_panel = tk.Frame(self.root, bg="#111111")
        self.right_panel.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

        title = tk.Label(
            self.left_panel,
            text="LiDAR Escape Map Editor",
            bg="#202225",
            fg="white",
            font=("Segoe UI", 16, "bold")
        )
        title.pack(anchor="w", padx=12, pady=(12, 8))

        # tool selection
        tool_frame = tk.LabelFrame(
            self.left_panel, text="Tools",
            bg="#202225", fg="white", padx=8, pady=8
        )
        tool_frame.pack(fill=tk.X, padx=12, pady=8)

        self.tool_buttons = {}
        for tool in ["wall", "spawn", "select"]:
            btn = tk.Button(
                tool_frame,
                text=tool,
                command=lambda t=tool: self.set_tool(t),
                bg="#2f3136",
                fg="white",
                activebackground="#5865f2",
                activeforeground="white",
                relief=tk.FLAT
            )
            btn.pack(fill=tk.X, pady=2)
            self.tool_buttons[tool] = btn

        # geometry settings (auto floor/ceiling parameters)
        settings_frame = tk.LabelFrame(
            self.left_panel, text="Auto geometry settings",
            bg="#202225", fg="white", padx=8, pady=8
        )
        settings_frame.pack(fill=tk.X, padx=12, pady=8)

        def add_entry(label, value):
            tk.Label(settings_frame, text=label, bg="#202225", fg="white").pack(anchor="w")
            e = tk.Entry(settings_frame)
            e.insert(0, value)
            e.pack(fill=tk.X, pady=(0, 8))
            return e

        self.wall_height_entry = add_entry("Wall height", "2.15")
        self.floor_y_entry = add_entry("Floor top Y", "0.0")
        self.floor_thickness_entry = add_entry("Floor thickness", "0.5")
        self.ceiling_thickness_entry = add_entry("Ceiling thickness", "0.30")
        self.spawn_height_entry = add_entry("Spawn height", "1.55")
        self.padding_entry = add_entry("Auto floor/ceiling padding", "0.0")

        # file operations
        actions_frame = tk.LabelFrame(
            self.left_panel, text="File",
            bg="#202225", fg="white", padx=8, pady=8
        )
        actions_frame.pack(fill=tk.X, padx=12, pady=8)

        tk.Button(actions_frame, text="New map", command=self.new_map, bg="#2f3136", fg="white", relief=tk.FLAT).pack(fill=tk.X, pady=2)
        tk.Button(actions_frame, text="Open map.txt", command=self.open_map, bg="#2f3136", fg="white", relief=tk.FLAT).pack(fill=tk.X, pady=2)
        tk.Button(actions_frame, text="Save map.txt", command=self.save_map, bg="#2f3136", fg="white", relief=tk.FLAT).pack(fill=tk.X, pady=2)
        tk.Button(actions_frame, text="Save as...", command=self.save_map_as, bg="#2f3136", fg="white", relief=tk.FLAT).pack(fill=tk.X, pady=2)

        # selected wall info
        selected_frame = tk.LabelFrame(
            self.left_panel, text="Selected wall",
            bg="#202225", fg="white", padx=8, pady=8
        )
        selected_frame.pack(fill=tk.X, padx=12, pady=8)

        self.selected_info = tk.Text(
            selected_frame, height=8,
            bg="#111111", fg="white",
            insertbackground="white", relief=tk.FLAT
        )
        self.selected_info.pack(fill=tk.X)

        # just for comfort of double checking controls
        instructions = (
            "- drawing only walls for now\n"
            "- floor and celining are generated automatically on save\n"
            "- spawn is player's start point\n\n"
            "Mouse:\n"
            "LMB drag = create wall\n"
            "RMB = delete wall under cursor\n"
            "MMB drag = move screen\n"
            "Wheel = zoom\n\n"
            "Keyboard:\n"
            "1 = wall tool\n"
            "2 = spawn tool\n"
            "Q = select tool\n"
            "Delete = delete selected wall\n"
            "Ctrl+S = save\n"
            "Ctrl+O = open\n"
            "Ctrl+N = new map\n"
            "G = toggle grid size\n\n"
            "Select mode:\n"
            "- click wall to select\n"
            "- drag inside = move\n"
            "- drag near edge = resize\n\n"
            "Export format:\n"
            "spawn x y z\n"
            "floor ... (auto)\n"
            "ceiling ... (auto)\n"
            "wall ...\n"
        )

        # top bar on right
        top_bar = tk.Frame(self.right_panel, bg="#181818", height=32)
        top_bar.pack(fill=tk.X)

        self.file_label = tk.Label(top_bar, textvariable=self.file_var, bg="#181818", fg="white")
        self.file_label.pack(side=tk.LEFT, padx=10)

        self.status_label = tk.Label(top_bar, textvariable=self.status_var, bg="#181818", fg="#bfbfbf")
        self.status_label.pack(side=tk.RIGHT, padx=10)

        self.canvas = tk.Canvas(self.right_panel, bg="#0b0b0b", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True)

        self.set_tool("wall")
        self.set_status("Ready")

    # binding controls
    def bind_events(self):
        """Bind keyboard shortcuts and mouse events"""

        # keyboard shortcuts
        self.root.bind("<KeyPress-1>", lambda e: self.set_tool("wall"))
        self.root.bind("<KeyPress-2>", lambda e: self.set_tool("spawn"))
        self.root.bind("<KeyPress-q>", lambda e: self.set_tool("select"))
        self.root.bind("<Delete>", self.delete_selected)
        self.root.bind("<Control-s>", lambda e: self.save_map())
        self.root.bind("<Control-o>", lambda e: self.open_map())
        self.root.bind("<Control-n>", lambda e: self.new_map())
        self.root.bind("<KeyPress-g>", self.toggle_grid_size)

        # mouse events on canvas
        self.canvas.bind("<Button-1>", self.on_left_down)
        self.canvas.bind("<B1-Motion>", self.on_left_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_left_up)
        self.canvas.bind("<Button-3>", self.on_right_click)

        self.canvas.bind("<Button-2>", self.on_middle_down)
        self.canvas.bind("<B2-Motion>", self.on_middle_drag)
        self.canvas.bind("<ButtonRelease-2>", self.on_middle_up)

        self.canvas.bind("<MouseWheel>", self.on_mousewheel)

    # helpers
    def set_status(self, text):
        """Update the status bar message"""
        self.status_var.set(text)

    def set_tool(self, tool):
        """Change the current editing tool and update UI"""
        self.current_tool = tool
        for name, btn in self.tool_buttons.items():
            btn.configure(bg="#5865f2" if name == tool else "#2f3136")
        self.set_status(f"Tool: {tool}")
        self.selected_index = None # clear selection when changing tool
        self.update_selected_info()
        self.redraw()

    def read_settings(self):
        """Default settings"""
        def get_float(entry, default):
            try:
                return float(entry.get())
            except ValueError:
                entry.delete(0, tk.END)
                entry.insert(0, str(default))
                return default

        self.wall_height = get_float(self.wall_height_entry, 2.15)
        self.floor_y = get_float(self.floor_y_entry, 0.0)
        self.floor_thickness = get_float(self.floor_thickness_entry, 0.5)
        self.ceiling_thickness = get_float(self.ceiling_thickness_entry, 0.30)
        self.spawn_height = get_float(self.spawn_height_entry, 1.55)
        self.auto_padding = get_float(self.padding_entry, 0.0)

    # conversion of stuff
    def world_to_screen(self, x, z):
        """Convert world coordinates (x,z) to screen pixel coordinates"""
        s = GRID_SIZE * self.zoom
        sx = x * s + self.offset_x + self.canvas.winfo_width() / 2
        sy = z * s + self.offset_y + self.canvas.winfo_height() / 2
        return sx, sy

    def screen_to_world(self, sx, sy):
        """Convert screen pixel coordinates to world coordinates (x,z)"""
        s = GRID_SIZE * self.zoom
        x = (sx - self.offset_x - self.canvas.winfo_width() / 2) / s
        z = (sy - self.offset_y - self.canvas.winfo_height() / 2) / s
        return x, z

    def snap(self, value):
        """Snap a world coordinate to the nearest grid cell"""
        return round(value)

    # drawing
    def draw_grid(self):
        """Draw the background grid based on current zoom and offset"""
        w = self.canvas.winfo_width()
        h = self.canvas.winfo_height()
        if w <= 1 or h <= 1:
            return

        step = GRID_SIZE * self.zoom
        if step < 8: # don't draw if too small
            return

        # calculate starting positions to center grid
        start_x = (self.offset_x + w / 2) % step
        start_y = (self.offset_y + h / 2) % step

        # draw vertical lines
        for x in range(int(start_x), w, int(step) if step >= 1 else 1):
            self.canvas.create_line(x, 0, x, h, fill="#1d1d1d")

        # draw horizontal lines
        for y in range(int(start_y), h, int(step) if step >= 1 else 1):
            self.canvas.create_line(0, y, w, y, fill="#1d1d1d")

    def get_auto_bounds(self):
        """Calculate the bounding box of all walls"""
        if not self.boxes:
            return None
        min_x = min(b["minX"] for b in self.boxes) - self.auto_padding
        min_z = min(b["minZ"] for b in self.boxes) - self.auto_padding
        max_x = max(b["maxX"] for b in self.boxes) + self.auto_padding
        max_z = max(b["maxZ"] for b in self.boxes) + self.auto_padding
        return min_x, min_z, max_x, max_z

    def redraw(self):
        """Clear and redraw the entire canvas (to reset/update stuff)"""
        self.canvas.delete("all")
        self.draw_grid()

        # Draw auto floor/ceiling bounds
        bounds = self.get_auto_bounds()
        if bounds:
            min_x, min_z, max_x, max_z = bounds
            sx1, sy1 = self.world_to_screen(min_x, min_z)
            sx2, sy2 = self.world_to_screen(max_x, max_z)
            self.canvas.create_rectangle(
                sx1, sy1, sx2, sy2,
                outline="#2ecc71",
                dash=(6, 4),
                width=2
            )
            self.canvas.create_text(
                sx1 + 8, sy1 + 8,
                text="auto floor/ceiling",
                fill="#2ecc71",
                anchor="nw",
                font=("Segoe UI", 10, "bold")
            )

        # draw all walls
        for i, obj in enumerate(self.boxes):
            sx1, sy1 = self.world_to_screen(obj["minX"], obj["minZ"])
            sx2, sy2 = self.world_to_screen(obj["maxX"], obj["maxZ"])

            color = BOX_COLORS.get(obj["type"], "#aaaaaa")
            outline = "#ffffff" if i == self.selected_index else color
            width = 3 if i == self.selected_index else 2

            self.canvas.create_rectangle(
                sx1, sy1, sx2, sy2,
                fill=color,
                outline=outline,
                width=width,
                stipple="gray25"
            )

            # Label each wall
            self.canvas.create_text(
                (sx1 + sx2) / 2,
                (sy1 + sy2) / 2,
                text="wall",
                fill="white",
                font=("Segoe UI", 9, "bold")
            )

        # draw spawn
        spawn_sx, spawn_sy = self.world_to_screen(self.spawn[0], self.spawn[2])
        self.canvas.create_oval(spawn_sx - 7, spawn_sy - 7, spawn_sx + 7, spawn_sy + 7, fill="#ffcc00", outline="white", width=2)
        self.canvas.create_text(spawn_sx + 16, spawn_sy - 12, text="SPAWN", fill="#ffcc00", anchor="w", font=("Segoe UI", 10, "bold"))

        # preview on top of the layers
        if self.preview_rect:
            self.canvas.tag_raise(self.preview_rect)

    # object selection + INFO
    def canvas_pick_box(self, sx, sy):
        """Return the index of the wall under screen coordinates (sx, sy)"""
        wx, wz = self.screen_to_world(sx, sy)
        for i in range(len(self.boxes) - 1, -1, -1):
            b = self.boxes[i]
            if b["minX"] <= wx <= b["maxX"] and b["minZ"] <= wz <= b["maxZ"]:
                return i
        return None

    def update_selected_info(self):
        """Update the text widget based on the currently selected wall"""
        self.selected_info.delete("1.0", tk.END)
        if self.selected_index is None:
            self.selected_info.insert(tk.END, "No selection")
            return

        obj = self.boxes[self.selected_index]
        text = (
            f'Type: wall\n'
            f'minX: {obj["minX"]:.2f}\n'
            f'minY: {obj["minY"]:.2f}\n'
            f'minZ: {obj["minZ"]:.2f}\n'
            f'maxX: {obj["maxX"]:.2f}\n'
            f'maxY: {obj["maxY"]:.2f}\n'
            f'maxZ: {obj["maxZ"]:.2f}\n'
        )
        self.selected_info.insert(tk.END, text)

    # mouse event handlers
    def on_left_down(self, event):
        """Handle left mouse button press"""
        self.read_settings()
        wx, wz = self.screen_to_world(event.x, event.y)
        wx, wz = self.snap(wx), self.snap(wz)

        if self.current_tool == "spawn":
            # place spawn point
            self.spawn = [wx, self.spawn_height, wz]
            self.set_status(f"Spawn moved to ({wx}, {self.spawn_height}, {wz})")
            self.redraw()
            return

        if self.current_tool == "select":
            # try to select a wall
            idx = self.canvas_pick_box(event.x, event.y)
            self.selected_index = idx
            self.update_selected_info()
            self.redraw()

            if idx is not None:
                # start dragging or resizing
                self.dragging = True
                self.drag_start_world = (wx, wz)
                self.drag_start_box = self.boxes[idx].copy()
                self.drag_mode = self.detect_drag_mode(idx, wx, wz)
            return

        # create a wall
        self.creating = True
        self.create_start = (wx, wz)
        sx, sy = self.world_to_screen(wx, wz)
        self.preview_rect = self.canvas.create_rectangle(
            sx, sy, sx, sy,
            outline="white",
            width=2,
            dash=(5, 3)
        )

    def detect_drag_mode(self, idx, wx, wz):
        """Determine whether the user is about to move or resize a selected wall"""
        b = self.boxes[idx]
        margin = 0.25 # tolerance for edge detection

        if abs(wx - b["minX"]) <= margin:
            return "resize_left"
        if abs(wx - b["maxX"]) <= margin:
            return "resize_right"
        if abs(wz - b["minZ"]) <= margin:
            return "resize_top"
        if abs(wz - b["maxZ"]) <= margin:
            return "resize_bottom"
        return "move"

    def on_left_drag(self, event):
        """Handle mouse drag with left button"""
        wx, wz = self.screen_to_world(event.x, event.y)
        wx, wz = self.snap(wx), self.snap(wz)

        # update preview rectangle dynamically
        if self.creating and self.preview_rect:
            x0, z0 = self.create_start
            sx1, sy1 = self.world_to_screen(x0, z0)
            sx2, sy2 = self.world_to_screen(wx, wz)
            self.canvas.coords(self.preview_rect, sx1, sy1, sx2, sy2)
            return

        # move or resize selected wall
        if self.dragging and self.selected_index is not None:
            b = self.boxes[self.selected_index]
            start_box = self.drag_start_box
            sx, sz = self.drag_start_world
            dx = wx - sx
            dz = wz - sz

            if self.drag_mode == "move":
                # Move entire wall, preserving dimensions
                width = start_box["maxX"] - start_box["minX"]
                depth = start_box["maxZ"] - start_box["minZ"]
                b["minX"] = start_box["minX"] + dx
                b["maxX"] = b["minX"] + width
                b["minZ"] = start_box["minZ"] + dz
                b["maxZ"] = b["minZ"] + depth
            elif self.drag_mode == "resize_left":
                b["minX"] = min(wx, b["maxX"] - 1)   # Keep at least 1 unit wide
            elif self.drag_mode == "resize_right":
                b["maxX"] = max(wx, b["minX"] + 1)
            elif self.drag_mode == "resize_top":
                b["minZ"] = min(wz, b["maxZ"] - 1)
            elif self.drag_mode == "resize_bottom":
                b["maxZ"] = max(wz, b["minZ"] + 1)

            self.update_selected_info()
            self.redraw()

    def on_left_up(self, event):
        """Handle left button release"""
        wx, wz = self.screen_to_world(event.x, event.y)
        wx, wz = self.snap(wx), self.snap(wz)

        if self.creating:
            self.creating = False
            if self.preview_rect:
                self.canvas.delete(self.preview_rect)
                self.preview_rect = None

            # create a new wall from start to current position
            x0, z0 = self.create_start
            min_x = min(x0, wx)
            max_x = max(x0, wx)
            min_z = min(z0, wz)
            max_z = max(z0, wz)

            # reject bad walls
            if abs(max_x - min_x) < 1 or abs(max_z - min_z) < 1:
                self.set_status("Wall too small")
                self.redraw()
                return

            obj = {
                "type": "wall",
                "minX": float(min_x),
                "minY": self.floor_y,
                "minZ": float(min_z),
                "maxX": float(max_x),
                "maxY": self.floor_y + self.wall_height,
                "maxZ": float(max_z),
            }

            self.boxes.append(obj)
            self.selected_index = len(self.boxes) - 1
            self.update_selected_info()
            self.set_status("Created wall")
            self.redraw()

        if self.dragging:
            self.dragging = False
            self.drag_mode = None
            self.drag_start_world = None
            self.drag_start_box = None
            self.redraw()

    def on_right_click(self, event):
        """Delete wall under cursor on right click"""
        idx = self.canvas_pick_box(event.x, event.y)
        if idx is not None:
            del self.boxes[idx]
            if self.selected_index == idx:
                self.selected_index = None
            elif self.selected_index is not None and self.selected_index > idx:
                self.selected_index -= 1
            self.update_selected_info()
            self.set_status("Deleted wall")
            self.redraw()

    def delete_selected(self, event=None):
        """Delete the currently selected wall (delete key option)"""
        if self.selected_index is None:
            return
        del self.boxes[self.selected_index]
        self.selected_index = None
        self.update_selected_info()
        self.set_status("Deleted wall")
        self.redraw()

    def on_middle_down(self, event):
        """Start moving screen with middle mouse button"""
        self.panning = True
        self.pan_last = (event.x, event.y)

    def on_middle_drag(self, event):
        """Update pan offset while dragging middle button"""
        if not self.panning:
            return
        dx = event.x - self.pan_last[0]
        dy = event.y - self.pan_last[1]
        self.offset_x += dx
        self.offset_y += dy
        self.pan_last = (event.x, event.y)
        self.redraw()

    def on_middle_up(self, event):
        """Stop panning"""
        self.panning = False

    def on_mousewheel(self, event):
        """Zoom in/out centered on mouse position"""
        if event.delta > 0:
            self.zoom *= 1.1
        else:
            self.zoom *= 0.9

        self.zoom = max(self.min_zoom, min(self.max_zoom, self.zoom))

        # adjust offset
        mx, my = event.x, event.y
        wx_before, wz_before = self.screen_to_world(mx, my)

        s = GRID_SIZE * self.zoom
        self.offset_x = mx - wx_before * s - self.canvas.winfo_width() / 2
        self.offset_y = my - wz_before * s - self.canvas.winfo_height() / 2

        self.set_status(f"Zoom: {self.zoom:.2f}x")
        self.redraw()

    def toggle_grid_size(self, event=None):
        """Toggle between two grid sizes (40 and 20)"""
        global GRID_SIZE
        GRID_SIZE = 20 if GRID_SIZE == 40 else 40
        self.set_status(f"Grid size: {GRID_SIZE}")
        self.redraw()

    # file operations
    def new_map(self):
        """Clear all walls and reset spawn to default"""
        self.boxes.clear()
        self.spawn = [0.0, 1.55, 7.0]
        self.selected_index = None
        self.update_selected_info()
        self.set_status("New map")
        self.redraw()

    def load_default_map(self):
        """Load map from DEFAULT_MAP_FILE if it exists, otherwise start new"""
        if os.path.exists(DEFAULT_MAP_FILE):
            self.load_map_file(DEFAULT_MAP_FILE)
        else:
            self.new_map()

    def open_map(self):
        """Open a file dialog and load selected map"""
        path = filedialog.askopenfilename(
            title="Open map",
            filetypes=[("Text map", "*.txt"), ("All files", "*.*")]
        )
        if not path:
            return
        self.load_map_file(path)

    def load_map_file(self, path):
        """Parse a map file and load its data"""
        try:
            self.boxes.clear()
            with open(path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    parts = line.split()
                    if parts[0] == "spawn" and len(parts) >= 4:
                        self.spawn = [float(parts[1]), float(parts[2]), float(parts[3])]
                    elif parts[0] == "wall" and len(parts) >= 7:
                        self.boxes.append({
                            "type": "wall",
                            "minX": float(parts[1]),
                            "minY": float(parts[2]),
                            "minZ": float(parts[3]),
                            "maxX": float(parts[4]),
                            "maxY": float(parts[5]),
                            "maxZ": float(parts[6]),
                        })
            self.selected_index = None
            self.update_selected_info()
            self.file_var.set(f"File: {path}")
            self.set_status(f"Loaded {os.path.basename(path)}")
            self.redraw()
        except Exception as e:
            messagebox.showerror("Error", f"Could not load map:\n{e}")

    def build_auto_floor_and_ceiling(self):
        """Create floor and ceiling boxes based on current wall bounds"""
        bounds = self.get_auto_bounds()
        if not bounds:
            return []

        min_x, min_z, max_x, max_z = bounds

        floor = {
            "type": "floor",
            "minX": min_x,
            "minY": self.floor_y - self.floor_thickness,
            "minZ": min_z,
            "maxX": max_x,
            "maxY": self.floor_y,
            "maxZ": max_z,
        }

        ceiling = {
            "type": "ceiling",
            "minX": min_x,
            "minY": self.floor_y + self.wall_height,
            "minZ": min_z,
            "maxX": max_x,
            "maxY": self.floor_y + self.wall_height + self.ceiling_thickness,
            "maxZ": max_z,
        }

        return [floor, ceiling]

    def save_map(self):
        """Save to the current file"""
        current = self.file_var.get().replace("File: ", "").strip()
        if not current or current == DEFAULT_MAP_FILE and not os.path.isabs(current):
            self.save_map_file(DEFAULT_MAP_FILE)
        else:
            self.save_map_file(current)

    def save_map_as(self):
        """Open a file dialog to choose a save location"""
        path = filedialog.asksaveasfilename(
            title="Save map as",
            defaultextension=".txt",
            filetypes=[("Text map", "*.txt"), ("All files", "*.*")]
        )
        if not path:
            return
        self.save_map_file(path)

    def save_map_file(self, path):
        """Write map data to a file"""
        self.read_settings()
        try:
            auto_parts = self.build_auto_floor_and_ceiling()
            with open(path, "w", encoding="utf-8") as f:
                f.write(f"spawn {self.spawn[0]:.2f} {self.spawn[1]:.2f} {self.spawn[2]:.2f}\n")

                for obj in auto_parts:
                    f.write(
                        f'{obj["type"]} '
                        f'{obj["minX"]:.2f} {obj["minY"]:.2f} {obj["minZ"]:.2f} '
                        f'{obj["maxX"]:.2f} {obj["maxY"]:.2f} {obj["maxZ"]:.2f}\n'
                    )

                for obj in self.boxes:
                    f.write(
                        f'{obj["type"]} '
                        f'{obj["minX"]:.2f} {obj["minY"]:.2f} {obj["minZ"]:.2f} '
                        f'{obj["maxX"]:.2f} {obj["maxY"]:.2f} {obj["maxZ"]:.2f}\n'
                    )

            self.file_var.set(f"File: {path}")
            self.set_status(f"Saved {os.path.basename(path)}")
        except Exception as e:
            messagebox.showerror("Error", f"Could not save map:\n{e}")

root = tk.Tk()
app = MapEditor(root)
root.mainloop()
