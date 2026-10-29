from datetime import datetime
from pathlib import Path
import queue
import subprocess
import sys
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

from catalog import kTools
from cli import ParseArguments
from commands import CommandBuilder, Result_Parse, ServerState, kRepoRoot, kSmokeSuites
from documentation import Document_Open
from i18n import Text
from runner import JobRunner
from tool_page import ToolPage

kLogLimit = 200000
kToolGroups = ("assets", "materials", "studio", "radar")


class Panel:
    def __init__(self, window, config="build/test-server/config.local.json", language="en"):
        self.window = window
        self.language = language
        self.builder = CommandBuilder(language=language)
        self.runner = JobRunner(kRepoRoot, language)
        self.server_state = ServerState()
        self.busy = False
        self.controls = []
        self.operation_buttons = []
        self.config = tk.StringVar(master=window, value=config)
        self.server_api = tk.StringVar(master=window)
        self.server_api_module = tk.StringVar(master=window)
        self.map_name = tk.StringVar(master=window)
        self.bind = tk.StringVar(master=window)
        self.port = tk.StringVar(master=window)
        self.repair_base = tk.BooleanVar(master=window)
        self.suite = tk.StringVar(master=window, value="stack")
        self.case = tk.StringVar(master=window)
        self.repetitions = tk.StringVar(master=window, value="5")
        self.run_id = tk.StringVar(master=window)
        self.game_dir = tk.StringVar(master=window)
        self.matgen_executable = tk.StringVar(master=window, value=self.builder.FindMatgen())
        self.language_choice = tk.StringVar(master=window, value=language.upper())
        self.server_status = tk.StringVar(master=window)
        self.operation_status = tk.StringVar(master=window)
        self.status_key = "ready"
        self.status_values = {}
        self.last_outcome = None
        self.last_server_outcome = None
        self.selected_tools = {group: next(tool.key for tool in kTools if tool.group == group) for group in kToolGroups}
        self.tool_values = {}
        for tool in kTools:
            self.tool_values[tool.key] = {}
            for field in tool.fields:
                value = field.initial_value
                variable = tk.BooleanVar(master=window, value=value) if field.kind == "bool" else tk.StringVar(master=window, value=value)
                self.tool_values[tool.key][field.key] = variable
        self.flags_source = self.tool_values["flags"]["source"]
        self.flags_output = self.tool_values["flags"]["output"]
        self.icon_output = self.tool_values["country_icon"]["output"]

        window.geometry("1060x900")
        window.minsize(900, 760)
        window.protocol("WM_DELETE_WINDOW", self.Close)
        window.columnconfigure(0, weight=1)
        window.rowconfigure(1, weight=3)
        window.rowconfigure(2, weight=2)
        style = ttk.Style(window)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        self.BuildUI()
        self.poll_id = window.after(75, self.Poll)

    def Text(self, key, **values):
        return Text(self.language, key, **values)

    def BuildUI(self):
        saved_log = self.log.get("1.0", "end-1c") if hasattr(self, "log") else ""
        selected_tab = self.tabs.index("current") if hasattr(self, "tabs") else 0
        for child in self.window.winfo_children():
            child.destroy()
        self.controls = []
        self.operation_buttons = []
        self.window.title(self.Text("title"))
        header = ttk.Frame(self.window, padding=(12, 8))
        header.grid(row=0, column=0, sticky="ew")
        header.columnconfigure(0, weight=1)
        ttk.Label(header, text=self.Text("title")).grid(row=0, column=0, sticky="w")
        ttk.Label(header, text=self.Text("language")).grid(row=0, column=1, padx=(8, 6))
        language_selector = ttk.Combobox(header, values=("RU", "EN"), textvariable=self.language_choice, state="readonly", width=5)
        language_selector.grid(row=0, column=2)
        language_selector.bind("<<ComboboxSelected>>", lambda event: self.SetLanguage(self.language_choice.get().lower()))
        self.controls.append(language_selector)
        documentation = ttk.Frame(header)
        documentation.grid(row=1, column=0, columnspan=3, sticky="w", pady=(8, 0))
        for key, label in [("index", "documentation"), ("testing", "testing_docs"), ("panel", "panel_guide")]:
            self.AddDocumentation(documentation, key, label).pack(side="left", padx=(0, 8))

        self.tabs = ttk.Notebook(self.window)
        self.tabs.grid(row=1, column=0, sticky="nsew", padx=12, pady=(0, 6))
        self.BuildServerTab(self.tabs)
        self.pages = {group: ToolPage(self, self.tabs, group) for group in kToolGroups}
        self.tabs.select(selected_tab)
        self.BuildLog()
        if saved_log:
            self.AppendLog(saved_log)
        ttk.Label(self.window, textvariable=self.operation_status, wraplength=1000).grid(
            row=3, column=0, sticky="w", padx=12, pady=(4, 10)
        )
        self.operation_status.set(self.Text(self.status_key, **self.status_values))
        self.UpdateServerStatus()

    def SetLanguage(self, language):
        if self.busy or language not in {"ru", "en"}:
            self.language_choice.set(self.language.upper())
            return
        self.language = language
        self.language_choice.set(language.upper())
        self.builder.language = language
        self.runner.language = language
        self.BuildUI()
        if self.last_outcome:
            self.Finish(self.last_outcome, append_log=False)

    def MakeTab(self, tabs, label):
        outer = ttk.Frame(tabs)
        tabs.add(outer, text=self.Text(label))
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(0, weight=1)
        canvas = tk.Canvas(outer, highlightthickness=0)
        scrollbar = ttk.Scrollbar(outer, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, sticky="ns")
        frame = ttk.Frame(canvas, padding=12)
        item = canvas.create_window(0, 0, window=frame, anchor="nw")
        frame.bind("<Configure>", lambda event: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.bind("<Configure>", lambda event: canvas.itemconfigure(item, width=event.width))
        canvas.bind("<MouseWheel>", lambda event: canvas.yview_scroll(-int(event.delta / 120), "units"))
        return frame

    def AddPath(self, parent, row, label, variable, kind, extension=""):
        ttk.Label(parent, text=self.Text(label)).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=4)
        entry = ttk.Entry(parent, textvariable=variable)
        entry.grid(row=row, column=1, sticky="ew", pady=4)
        button = ttk.Button(parent, text=self.Text("browse"), command=lambda: self.Browse(variable, kind, extension))
        button.grid(row=row, column=2, padx=(8, 0), pady=4)
        self.controls.extend([entry, button])

    def AddOperation(self, parent, label, callback):
        button = ttk.Button(parent, text=self.Text(label), command=callback)
        self.controls.append(button)
        self.operation_buttons.append(button)
        return button

    def AddDocumentation(self, parent, key, label):
        return ttk.Button(parent, text=self.Text(label), command=lambda: self.OpenDocumentation(key))

    def OpenDocumentation(self, key):
        try:
            Document_Open(key, self.language)
        except OSError as error:
            messagebox.showerror(self.Text("document_open_failed"), str(error), parent=self.window)

    def BuildServerTab(self, tabs):
        tab = self.MakeTab(tabs, "server_tab")
        tab.columnconfigure(1, weight=1)
        self.AddPath(tab, 0, "config", self.config, "config", ".json")
        self.AddPath(tab, 1, "server_api", self.server_api, "directory")
        self.AddPath(tab, 2, "server_api_module", self.server_api_module, "module")
        ttk.Label(tab, text=self.Text("api_hint"), wraplength=920).grid(row=3, column=0, columnspan=3, sticky="w", pady=4)
        repair = ttk.Checkbutton(tab, text=self.Text("repair_base"), variable=self.repair_base)
        repair.grid(row=4, column=0, columnspan=3, sticky="w", pady=4)
        self.controls.append(repair)
        setup = ttk.Frame(tab)
        setup.grid(row=5, column=0, columnspan=3, sticky="ew", pady=4)
        for index, action in enumerate(("prepare", "doctor", "build", "init", "compile-only", "results")):
            self.AddOperation(setup, "server." + action, lambda action=action: self.Server(action)).grid(
                row=index // 3, column=index % 3, sticky="ew", padx=(0, 8), pady=3
            )
            setup.columnconfigure(index % 3, weight=1)
        ttk.Label(tab, text=self.Text("prepare_hint"), wraplength=920).grid(row=6, column=0, columnspan=3, sticky="w", pady=(4, 8))

        options = ttk.LabelFrame(tab, text=self.Text("server_options"), padding=8)
        options.grid(row=7, column=0, columnspan=3, sticky="ew")
        for index, (label, variable) in enumerate((
            ("map", self.map_name), ("bind", self.bind), ("port", self.port),
            ("suite", self.suite), ("case", self.case), ("repetitions", self.repetitions),
        )):
            row, column = divmod(index, 3)
            ttk.Label(options, text=self.Text(label)).grid(row=row * 2, column=column, sticky="w", padx=(0, 8), pady=(4, 0))
            if label == "suite":
                entry = ttk.Combobox(options, textvariable=variable, values=kSmokeSuites, state="readonly")
            else:
                entry = ttk.Entry(options, textvariable=variable)
            entry.grid(row=row * 2 + 1, column=column, sticky="ew", padx=(0, 8), pady=(0, 4))
            options.columnconfigure(column, weight=1)
            self.controls.append(entry)
        ttk.Label(options, text=self.Text("run_id")).grid(row=4, column=0, columnspan=3, sticky="w", pady=(4, 0))
        run_entry = ttk.Entry(options, textvariable=self.run_id)
        run_entry.grid(row=5, column=0, columnspan=3, sticky="ew", pady=(0, 4))
        self.controls.append(run_entry)

        runtime = ttk.Frame(tab)
        runtime.grid(row=8, column=0, columnspan=3, sticky="w", pady=(8, 4))
        for action in ("start", "stop", "status", "smoke", "calibrate", "report"):
            self.AddOperation(runtime, "server." + action, lambda action=action: self.Server(action)).grid(
                row=0 if action in {"start", "stop", "status"} else 1,
                column=("start", "stop", "status", "smoke", "calibrate", "report").index(action) % 3,
                sticky="ew", padx=(0, 8), pady=3,
            )
        ttk.Label(tab, text=self.Text("cycle_hint"), wraplength=920).grid(row=9, column=0, columnspan=3, sticky="w", pady=4)
        ttk.Label(tab, textvariable=self.server_status, wraplength=920).grid(row=10, column=0, columnspan=3, sticky="w", pady=4)
        ttk.Label(tab, text=self.Text("server_stop_hint"), wraplength=920).grid(row=11, column=0, columnspan=3, sticky="w", pady=4)
        guides = ttk.Frame(tab)
        guides.grid(row=12, column=0, columnspan=3, sticky="w", pady=(8, 0))
        self.AddDocumentation(guides, "server", "server_guide").pack(side="left", padx=(0, 8))
        self.AddDocumentation(guides, "materials_test", "materials_test_guide").pack(side="left")

    def BuildLog(self):
        frame = ttk.LabelFrame(self.window, text=self.Text("journal"), padding=8)
        frame.grid(row=2, column=0, sticky="nsew", padx=12, pady=6)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(0, weight=1)
        self.log = scrolledtext.ScrolledText(frame, height=10, wrap="word", state="disabled", font=("Consolas", 10))
        self.log.grid(row=0, column=0, sticky="nsew")
        buttons = ttk.Frame(frame)
        buttons.grid(row=1, column=0, sticky="w", pady=(8, 0))
        ttk.Button(buttons, text=self.Text("save_log"), command=self.SaveLog).pack(side="left", padx=(0, 8))
        ttk.Button(buttons, text=self.Text("clear_log"), command=self.ClearLog).pack(side="left")

    def Browse(self, variable, kind, extension=""):
        try:
            current = self.builder.Path(variable.get()) if variable.get().strip() else kRepoRoot
        except (ValueError, OSError):
            current = kRepoRoot
        directory = current if current.is_dir() else current.parent
        while not directory.is_dir() and directory != directory.parent:
            directory = directory.parent
        options = {"parent": self.window, "initialdir": str(directory)}
        if kind in {"directory", "input_dir", "output_dir"}:
            selected = filedialog.askdirectory(title=self.Text("select_directory"), mustexist=kind != "output_dir", **options)
        else:
            filters = {
                ".json": ("filter_json", "*.json"), ".tga": ("filter_tga", "*.tga"),
                ".mdl": ("filter_mdl", "*.mdl"), ".wad": ("filter_wad", "*.wad"),
                ".mat": ("filter_mat", "*.mat"), ".png": ("filter_image", "*.png *.tga *.jpg *.bmp"),
            }
            filter_key, pattern = filters.get(extension, ("filter_all", "*"))
            if kind == "module":
                filter_key, pattern = "filter_module", "*.dll *.so"
            options["filetypes"] = [(self.Text(filter_key), pattern), (self.Text("filter_all"), "*")]
            if kind in {"config", "output_file", "icon"}:
                selected = filedialog.asksaveasfilename(
                    title=self.Text("select_config" if kind == "config" else "select_output"),
                    initialfile=current.name if not current.is_dir() else "", defaultextension=extension,
                    confirmoverwrite=kind != "config", **options,
                )
            else:
                selected = filedialog.askopenfilename(title=self.Text("select_input"), **options)
        if selected:
            variable.set(selected)

    def SelectTool(self, group, key):
        self.pages[group].Select(key)

    def FillGamePaths(self):
        try:
            game = self.builder.Path(self.game_dir.get())
            for tool in kTools:
                if tool.group == "studio":
                    for field in tool.fields:
                        if isinstance(field.default, str) and ("{game}" in field.default or "{parent}" in field.default):
                            self.tool_values[tool.key][field.key].set(field.default.replace("{game}", str(game)).replace("{parent}", str(game.parent)))
            self.last_outcome = None
            self.status_key, self.status_values = "paths_filled", {"path": game}
            self.operation_status.set(self.Text(self.status_key, **self.status_values))
        except (ValueError, OSError) as error:
            messagebox.showerror(self.Text("not_started"), str(error), parent=self.window)

    def Server(self, action):
        options = {}
        if action in {"prepare", "init"}:
            options["repair_base"] = self.repair_base.get()
        if action == "prepare":
            options.update(server_api=self.server_api.get(), server_api_module=self.server_api_module.get())
        if action in {"start", "smoke", "calibrate"}:
            options["map_name"] = self.map_name.get()
        if action in {"start", "smoke"}:
            options["port"] = self.port.get()
        if action == "start":
            options["bind"] = self.bind.get()
        if action == "smoke":
            options.update(suite=self.suite.get(), case=self.case.get(), repetitions=self.repetitions.get())
        if action == "report":
            options["run_id"] = self.run_id.get()
        self.BuildAndStart(lambda: self.builder.Server(action, self.config.get(), **options))

    def RunTool(self, key):
        values = {name: variable.get() for name, variable in self.tool_values[key].items()}
        self.BuildAndStart(lambda: self.builder.Tool(key, values, native_executable=self.matgen_executable.get()))

    def Flags(self):
        self.RunTool("flags")

    def CountryIcon(self):
        self.RunTool("country_icon")

    def BuildAndStart(self, build):
        if self.busy:
            return
        try:
            self.Start(build())
        except (ValueError, OSError, RuntimeError) as error:
            messagebox.showerror(self.Text("not_started"), str(error), parent=self.window)

    def Start(self, command):
        if self.busy:
            return
        self.runner.Start(command)
        self.busy = True
        for control in self.controls:
            control.state(["disabled"])
        self.operation_status.set(self.Text("running", label=command.label))
        self.AppendLog(f"\n[{datetime.now():%H:%M:%S}] {command.label}\n{subprocess.list2cmdline(command.args)}\n")

    def Poll(self):
        for _ in range(300):
            try:
                kind, value = self.runner.events.get_nowait()
            except queue.Empty:
                break
            if kind == "output":
                self.AppendLog(value)
            else:
                self.Finish(value)
        self.poll_id = self.window.after(75, self.Poll)

    def Finish(self, outcome, append_log=True):
        self.busy = False
        self.last_outcome = outcome
        for control in self.controls:
            control.state(["!disabled"])
        result = Result_Parse(outcome.output) if outcome.command.server_action else None
        self.server_state.Apply(outcome.command, outcome.returncode, result)
        label = self.Text(outcome.command.label_key) if outcome.command.label_key else outcome.command.label
        if outcome.error_detail:
            summary = self.Text("spawn_error", error=outcome.error_detail)
        elif outcome.error:
            summary = outcome.error
        elif outcome.command.server_action and result is None:
            summary = self.Text("json_missing", code=outcome.returncode)
        elif outcome.returncode == 0 and (result is None or result["ok"]):
            summary = self.Text("completed", label=label)
        else:
            detail = f", {result['code']}" if result else ""
            summary = self.Text("operation_failed", label=label, code=outcome.returncode, detail=detail)
        self.operation_status.set(summary)
        if append_log:
            self.AppendLog(f"\n{summary}\n")
        if outcome.command.config:
            self.last_server_outcome = outcome
        self.UpdateServerStatus()

    def UpdateServerStatus(self):
        outcome = self.last_server_outcome
        if outcome is None:
            self.server_status.set(self.Text("server_unchecked"))
            return
        result = Result_Parse(outcome.output)
        if result:
            address = result["details"].get("connect")
            connect = self.Text("connect", address=address) if address else ""
            self.server_status.set(self.Text("server_result", config=outcome.command.config, code=result["code"], connect=connect))
        else:
            self.server_status.set(self.Text("server_uncertain", config=outcome.command.config))

    def AppendLog(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text)
        length = self.log.count("1.0", "end-1c", "chars")[0]
        if length > kLogLimit:
            self.log.delete("1.0", f"1.0+{length - kLogLimit}c")
        self.log.see("end")
        self.log.configure(state="disabled")

    def ClearLog(self):
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    def SaveLog(self):
        selected = filedialog.asksaveasfilename(
            parent=self.window, title=self.Text("save_log_title"), defaultextension=".log",
            initialfile=f"nextclient-tools-{datetime.now():%Y%m%d-%H%M%S}.log",
            filetypes=[(self.Text("filter_log"), "*.log"), (self.Text("filter_all"), "*")],
        )
        if selected:
            try:
                Path(selected).write_text(self.log.get("1.0", "end-1c"), encoding="utf-8")
            except OSError as error:
                messagebox.showerror(self.Text("save_failed"), str(error), parent=self.window)

    def Close(self):
        if self.busy:
            messagebox.showinfo(self.Text("close_busy_title"), self.Text("close_busy"), parent=self.window)
            return
        active = self.server_state.running_configs | self.server_state.uncertain_configs
        if active and not messagebox.askokcancel(
            self.Text("close_server_title"), self.Text("close_server", configs="\n".join(sorted(active))),
            parent=self.window, default="cancel",
        ):
            return
        self.window.after_cancel(self.poll_id)
        self.window.destroy()


def Main():
    arguments = ParseArguments()
    try:
        window = tk.Tk()
    except tk.TclError as error:
        print(Text(arguments.language, "tk_error", error=error), file=sys.stderr)
        return 1
    if arguments.self_check:
        window.withdraw()
    panel = Panel(window, arguments.config, arguments.language)
    if arguments.self_check:
        try:
            window.update_idletasks()
            if panel.runner.thread is not None or panel.busy or len(panel.tabs.tabs()) != 5:
                raise RuntimeError(Text(arguments.language, "self_check_failed"))
            print(Text(arguments.language, "self_check_passed", version=window.tk.call("info", "patchlevel")))
        finally:
            window.after_cancel(panel.poll_id)
            window.destroy()
        return 0
    window.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(Main())
