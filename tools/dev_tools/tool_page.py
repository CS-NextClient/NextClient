import tkinter as tk
from tkinter import ttk

from catalog import Tool_Get, kTools


class ToolPage:
    def __init__(self, panel, tabs, group):
        self.panel = panel
        self.group = group
        self.tools = [tool for tool in kTools if tool.group == group]
        self.frame = panel.MakeTab(tabs, group + "_tab")
        self.frame.columnconfigure(1, weight=1)
        row = 0
        if group == "studio":
            panel.AddPath(self.frame, row, "game_dir", panel.game_dir, "directory")
            panel.AddOperation(self.frame, "fill_game_paths", panel.FillGamePaths).grid(
                row=row + 1, column=1, sticky="w", pady=(2, 10)
            )
            row += 2
        elif group == "materials":
            panel.AddPath(self.frame, row, "matgen_executable", panel.matgen_executable, "input_file", ".exe")
            ttk.Label(self.frame, text=panel.Text("matgen_hint"), wraplength=920).grid(
                row=row + 1, column=0, columnspan=3, sticky="w", pady=(2, 10)
            )
            row += 2

        ttk.Label(self.frame, text=panel.Text("tool_selector")).grid(row=row, column=0, sticky="w", padx=(0, 8))
        self.choice = tk.StringVar(master=panel.window)
        self.selector = ttk.Combobox(
            self.frame, textvariable=self.choice, state="readonly",
            values=[panel.Text(tool.label) for tool in self.tools],
        )
        self.selector.grid(row=row, column=1, columnspan=2, sticky="ew", pady=4)
        self.selector.bind("<<ComboboxSelected>>", self.Changed)
        panel.controls.append(self.selector)
        self.form = ttk.Frame(self.frame)
        self.form.grid(row=row + 1, column=0, columnspan=3, sticky="ew", pady=(8, 0))
        self.form.columnconfigure(1, weight=1)
        self.Select(panel.selected_tools[group])

    def Changed(self, event=None):
        self.Select(self.tools[self.selector.current()].key)

    def Select(self, key):
        tool = Tool_Get(key)
        if tool.group != self.group:
            raise ValueError(key)
        panel = self.panel
        panel.selected_tools[self.group] = key
        self.choice.set(panel.Text(tool.label))
        for child in self.form.winfo_children():
            child.destroy()
        panel.controls = [control for control in panel.controls if control.winfo_exists()]
        panel.operation_buttons = [button for button in panel.operation_buttons if button.winfo_exists()]
        ttk.Label(self.form, text=panel.Text(tool.description), wraplength=920).grid(
            row=0, column=0, columnspan=3, sticky="w", pady=(0, 8)
        )
        for row, field in enumerate(tool.fields, start=1):
            variable = panel.tool_values[key][field.key]
            if field.kind == "bool":
                control = ttk.Checkbutton(self.form, text=panel.Text(field.label), variable=variable)
                control.grid(row=row, column=1, columnspan=2, sticky="w", pady=4)
                panel.controls.append(control)
            elif field.kind in {"input_dir", "output_dir", "input_file", "output_file"}:
                panel.AddPath(self.form, row, field.label, variable, field.kind, field.extension)
            else:
                ttk.Label(self.form, text=panel.Text(field.label)).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=4)
                control = ttk.Entry(self.form, textvariable=variable)
                control.grid(row=row, column=1, columnspan=2, sticky="ew", pady=4)
                panel.controls.append(control)
        panel.AddOperation(self.form, "execute", lambda: panel.RunTool(key)).grid(
            row=len(tool.fields) + 1, column=1, sticky="w", pady=(12, 8)
        )
        guide = "masks" if key == "masks" else ("panel" if self.group == "assets" else self.group)
        panel.AddDocumentation(self.form, guide, "tool_guide").grid(
            row=len(tool.fields) + 1, column=2, sticky="e", pady=(12, 8)
        )
