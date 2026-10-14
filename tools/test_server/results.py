from datetime import datetime
from html import escape
import json
import os
from pathlib import Path
import tempfile
from urllib.parse import quote
import uuid

from common import Json_Read, Json_Write, Path_Inside, Path_Resolve, Result_Make

kStatuses = {"pass": "Пройдено", "fail": "Ошибка", "skip": "Пропущено", "not_run": "Не проверено"}
kScopes = {"build": "Сборка", "server": "Сервер", "visual": "Визуальная оценка",
           "operations": "Игровые операции", "pvs": "PVS клиента", "execution": "Выполнение команды"}
kSmokeCodes = {name + "_smoke" for name in ("stack", "gameplay", "core", "review", "hostage", "bomb", "players", "lifecycle")}
kStyle = """
:root{color-scheme:light;--ink:#182b3b;--muted:#536778;--line:#dce4eb;--blue:#155baf}
*{box-sizing:border-box}body{margin:0;background:#f3f6fa;color:var(--ink);font:15px/1.55 system-ui,Segoe UI,sans-serif}
main{max-width:1440px;margin:auto;padding:32px}a{color:var(--blue);text-underline-offset:3px}
header{margin-bottom:24px}h1{font-size:30px;line-height:1.2;margin:12px 0}h2{font-size:20px;margin:0 0 16px}
h3{font-size:16px}p{margin:8px 0}small,.muted{color:var(--muted)}nav{display:flex;gap:20px;flex-wrap:wrap}
section,.panel{background:white;border:1px solid var(--line);border-radius:12px;padding:22px;margin:18px 0}
.cards{display:flex;gap:12px;flex-wrap:wrap;margin:20px 0}.card{background:white;border:1px solid var(--line);border-radius:10px;padding:16px 20px;min-width:175px}
.card strong{display:block;font-size:25px;margin-top:4px}.badge{display:inline-block;border-radius:5px;padding:3px 9px;font-size:13px;font-weight:600;white-space:nowrap}
.pass{background:#e2f2e9;color:#175a37}.fail{background:#fde5e5;color:#9d2626}.skip{background:#fdf0d7;color:#7f5311}.not_run{background:#eaf0f5;color:#4d6172}
.warning{background:#fff6e3;border-color:#e7c573}.note{white-space:pre-wrap;overflow-wrap:anywhere}
.table-wrap{overflow-x:auto}table{width:100%;border-collapse:collapse;font-size:14px}th{text-align:left;color:var(--muted);font-size:12px;letter-spacing:.03em;background:#f7f9fc}
th,td{padding:12px;vertical-align:top;border-bottom:1px solid var(--line)}td{overflow-wrap:anywhere}tr:last-child td{border-bottom:0}
td code{font-size:12px}code,pre{font-family:Consolas,monospace}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#f7f9fc;padding:14px;border-radius:6px;font-size:12px;max-height:520px;overflow:auto}
details{margin:12px 0}summary{cursor:pointer;color:var(--blue);font-weight:600}dl{display:grid;grid-template-columns:180px minmax(0,1fr);gap:8px 16px}dt{color:var(--muted)}dd{margin:0;overflow-wrap:anywhere}
.controls{display:flex;gap:14px;flex-wrap:wrap;align-items:end;margin-bottom:16px}label{display:grid;gap:5px;font-size:13px;color:var(--muted)}input,select{font:inherit;border:1px solid #b8c9d9;border-radius:6px;padding:9px;background:white;color:var(--ink)}input{width:min(420px,80vw)}
[hidden]{display:none!important}.empty{padding:18px;color:var(--muted)}.count{font-size:13px;color:var(--muted)}footer{color:var(--muted);font-size:13px;margin-top:28px}
@media(max-width:700px){main{padding:16px}section{padding:14px}.card{flex:1;min-width:135px}h1{font-size:25px}dl{grid-template-columns:1fr;gap:3px}dd{margin-bottom:10px}}
@media print{body{background:white}main{padding:0}input,select,.controls{display:none}section{break-inside:avoid}a{color:inherit}pre{max-height:none}}
"""
kScript = """
document.querySelectorAll('[data-list]').forEach(section => {
  const search = section.querySelector('input');
  const filter = section.querySelector('select');
  const rows = [...section.querySelectorAll('tbody tr')];
  const update = () => {
    const query = search.value.toLocaleLowerCase();
    let count = 0;
    rows.forEach(row => {
      row.hidden = !row.textContent.toLocaleLowerCase().includes(query) ||
        (filter.value !== 'all' && row.dataset[filter.value] !== 'true');
      if (!row.hidden) count++;
    });
    section.querySelector('.count').textContent = `Показано: ${count} из ${rows.length}`;
    section.querySelector('.empty').hidden = count !== 0;
  };
  search.addEventListener('input', update);
  filter.addEventListener('change', update);
  update();
});
"""


def Results_Text(value):
    if value is None:
        return "Не указано"
    if isinstance(value, (dict, list)):
        value = json.dumps(value, ensure_ascii=False, indent=2)
    return escape(str(value), quote=True)


def Results_Badge(status):
    status = status if isinstance(status, str) and status in kStatuses else "not_run"
    return f'<span class="badge {status}">{kStatuses[status]}</span>'


def Results_Cards(statuses):
    return '<div class="cards">' + "".join(
        f'<div class="card" data-scope="{scope}" data-status="{status if status in kStatuses else "not_run"}">'
        f'<p>{kScopes[scope]}</p>{Results_Badge(status)}</div>' for scope, status in statuses.items()) + '</div>'


def Results_Page(title, body):
    return ('<!doctype html><html lang="ru"><head><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width,initial-scale=1">'
            f'<title>{Results_Text(title)} | NextClient</title><style>{kStyle}</style></head>'
            f'<body><main>{body}<footer>NextClient TestLab · Локальный снимок результатов. '
            'Для обновления выполните команду results и перезагрузите страницу.</footer></main>'
            f'<script>{kScript}</script></body></html>')


def Results_Table(headers, rows):
    return '<div class="table-wrap"><table><thead><tr>' + ''.join(f'<th>{name}</th>' for name in headers) + \
           '</tr></thead><tbody>' + ''.join(rows) + '</tbody></table></div>'


def Results_Fields(fields):
    return '<dl>' + ''.join(f'<dt>{name}</dt><dd>{Results_Text(value)}</dd>' for name, value in fields) + '</dl>'


def Results_Checks(events):
    rows = []
    for event in events:
        data = event.get("data", {}) if "event" in event else event
        if "event" in event and event.get("event") != "check":
            continue
        stage = event.get("stage")
        step = stage + 1 if type(stage) is int and stage >= 0 else "Подготовка"
        rows.append('<tr>' + ''.join(f'<td>{cell}</td>' for cell in (
            Results_Text(data.get("id")), Results_Badge(data.get("status")),
            Results_Text(kScopes.get(data.get("scope"), data.get("scope"))),
            Results_Text(step), Results_Text(data.get("expected")), Results_Text(data.get("actual")))) + '</tr>')
    if not rows:
        return '<p class="muted">В этом отчёте нет отдельных проверок.</p>'
    return Results_Table(["Проверка", "Результат", "Область", "Шаг", "Ожидается", "Наблюдалось"], rows)


def Results_Details(title, value):
    return f'<details><summary>{title}</summary><pre>{Results_Text(value)}</pre></details>'


def Results_Header(title, source, root):
    back = quote(os.path.relpath(root / "index.html", source.parent).replace("\\", "/"), safe="/.")
    return f'<header><nav><a href="{back}">Все результаты</a></nav><h1>{Results_Text(title)}</h1>' \
           f'<p class="muted">Источник: {Results_Text(source.relative_to(root).as_posix())}</p></header>'


def Results_Run(report, source, root):
    body = Results_Header(report.get("scenario_id") or "Ручной прогон", source, root)
    body += Results_Cards({scope: report["statuses"].get(scope, "not_run") for scope in ("build", "server", "visual")})
    complete = report.get("complete") is True
    body += f'<section class="{"panel" if complete else "warning"}" data-complete="{str(complete).lower()}">'
    body += '<h2>' + ('Журнал полный' if complete else 'Отчёт неполный') + '</h2>'
    body += '<p>Полнота журнала не означает успешную визуальную приёмку.</p>'
    if report.get("trace_truncated"):
        body += '<p>Достигнут лимит диагностических событий. Часть трассировки отсутствует.</p>'
    body += '<ul>' + ''.join(f'<li>{Results_Text(issue)}</li>' for issue in report.get("issues", [])) + '</ul></section>'
    terminal = next((event for event in reversed(report["events"]) if event.get("event") in
                     ("run_finished", "run_failed", "run_canceled")), None)
    states = {"run_finished": "Завершён", "run_failed": "Завершён с ошибкой", "run_canceled": "Отменён"}
    body += '<section><h2>Прогон</h2>' + Results_Fields([
        ("Идентификатор", report["run_id"]), ("Карта", report.get("map")), ("Платформа", report.get("platform")),
        ("Состояние", states.get(terminal["event"]) if terminal else "Завершение не зафиксировано"),
        ("Источник снимка", "Работающий сервер" if report.get("live_snapshot") else "Сохранённый журнал")])
    if terminal and terminal.get("event") != "run_finished":
        body += '<p class="note">' + Results_Text(terminal.get("data")) + '</p>'
    body += '<h3>Примечание оператора</h3><p class="note">' + Results_Text(report.get("note")) + '</p></section>'
    body += '<section><h2>Проверки по шагам</h2>' + Results_Checks(report["events"]) + '</section>'
    body += '<section><h2>Диагностика и файлы</h2>'
    body += f'<nav><a href="{quote(source.name)}">Исходный JSON</a>'
    for name in report.get("logs", []):
        path = source.parent / name if isinstance(name, str) else None
        if name in ("events.clean.jsonl", "server.clean.log") and path.is_file() and not path.is_symlink():
            body += f'<a href="{name}">{name}</a>'
    body += '</nav>'
    body += Results_Details("Версии и хеши", {key: report.get(key) for key in (
        "versions", "build", "plugin_hash", "map_hash", "map_profile_hashes", "lock_hash", "valve_base", "steamcmd", "client")})
    body += Results_Details("Состояние сцены", {key: report.get(key) for key in ("actors", "objectives", "cvars")})
    body += Results_Details(f'События ({len(report["events"])})', report["events"]) + '</section>'
    return Results_Page(report.get("scenario_id") or "Ручной прогон", body)


def Results_Smoke(report, source, root):
    details = report["details"]
    body = Results_Header(report["code"], source, root)
    statuses = {"execution": "pass" if report["ok"] else "fail"}
    statuses.update({scope: details.get(scope, "not_run") for scope in ("build", "operations", "server", "pvs", "visual")})
    body += Results_Cards(statuses)
    body += '<p>Успешное выполнение команды подтверждает только перечисленные автоматические проверки.</p>'
    body += '<section><h2>Запуск</h2>' + Results_Fields([
        ("Сессия", details.get("session_id")), ("Вариант", details.get("case")), ("Повторы", details.get("repetitions"))])
    for name in ("error", "failed", "failures", "cleanup", "pending"):
        if details.get(name):
            body += Results_Details({"error": "Ошибка", "failed": "Ошибки проверки", "failures": "Ошибки",
                                     "cleanup": "Остановка сервера", "pending": "Продолжение после смены карты"}[name], details[name])
    body += '</section><section><h2>Проверки</h2>' + Results_Checks(details.get("checks", []))
    if details.get("pending_checks"):
        body += '<h3>Продолжение после смены карты</h3>' + Results_Checks(details["pending_checks"])
    for item in details.get("results", []):
        body += '<h3>' + Results_Text(item.get("map")) + ' / ' + Results_Text(item.get("probe")) + '</h3>'
        body += Results_Badge(item.get("status")) + Results_Checks(item.get("checks", []))
        if item.get("error"):
            body += '<p class="note">' + Results_Text(item["error"]) + '</p>'
    body += '</section><section><h2>Диагностика</h2>'
    body += f'<a href="{quote(source.name)}">Исходный JSON</a>'
    body += Results_Details("Полные данные запуска", details) + '</section>'
    return Results_Page(report["code"], body)


def Results_Classify(report):
    if isinstance(report, dict) and "run_id" in report and "events" in report:
        if type(report.get("schema")) is not int or report["schema"] != 1:
            raise ValueError("Неизвестная версия схемы отчёта")
        if not isinstance(report.get("statuses"), dict) or not isinstance(report["events"], list):
            raise ValueError("Неверная структура отчёта")
        return "run"
    if isinstance(report, dict) and isinstance(report.get("code"), str) and report["code"] in kSmokeCodes:
        if type(report.get("ok")) is not bool or not isinstance(report.get("details"), dict):
            raise ValueError("Неверная структура автоматической проверки")
        return "smoke"
    return None


def Results_Index(entries, issues):
    runs = sum(item["kind"] == "run" for item in entries)
    body = '<header><p class="muted">NEXTCLIENT / TESTLAB</p><h1>Результаты тестирования</h1>' \
           '<p>Сохранённые прогоны сценариев и автоматические серверные проверки.</p></header>'
    body += f'<div class="cards"><div class="card">Прогоны сценариев<strong>{runs}</strong></div>' \
            f'<div class="card">Автоматические проверки<strong>{len(entries) - runs}</strong></div>' \
            f'<div class="card">Требуют внимания<strong>{sum(item["attention"] for item in entries)}</strong></div></div>'
    body += '<p class="muted">«Не проверено» означает отсутствие оценки. Дата в списке соответствует изменению исходного файла.</p>'
    for kind, title in (("run", "Прогоны сценариев"), ("smoke", "Автоматические проверки")):
        body += f'<section data-list><h2>{title}</h2><div class="controls">' \
                '<label>Поиск<input type="search" placeholder="Сценарий, карта, идентификатор"></label>' \
                '<label>Показывать<select><option value="all">Все</option>' \
                '<option value="attention">Требуют внимания</option>' \
                '<option value="unverified">Без визуальной оценки</option></select></label><span class="count"></span></div>'
        rows = []
        for entry in entries:
            if entry["kind"] != kind:
                continue
            report = entry["report"]
            statuses = report["statuses"] if kind == "run" else report["details"]
            title = report.get("scenario_id") or report.get("code") or "Прогон"
            description = report.get("run_id") if kind == "run" else report["details"].get("session_id")
            link = f'<a href="{quote(entry["html"], safe="/.")}">{Results_Text(title)}</a>'
            link += f'<br><small>{Results_Text(description)}</small><br><small>{Results_Text(entry["source"])}</small>'
            result = Results_Badge("pass" if report.get("ok") else "fail") if kind == "smoke" else \
                ("Полный журнал" if report.get("complete") is True else '<span class="badge fail">Неполный журнал</span>')
            cells = [link, Results_Text(entry["modified"]), Results_Text(report.get("map")), result,
                     Results_Badge(statuses.get("server")), Results_Badge(statuses.get("visual"))]
            rows.append(f'<tr data-attention="{str(entry["attention"]).lower()}" '
                        f'data-unverified="{str(statuses.get("visual") not in ("pass", "fail", "skip")).lower()}">' +
                        ''.join(f'<td>{cell}</td>' for cell in cells) + '</tr>')
        body += Results_Table(["Сценарий / источник", "Изменён", "Карта", "Журнал" if kind == "run" else "Команда", "Сервер", "Визуальная оценка"], rows)
        body += '<p class="empty"' + (' hidden' if rows else '') + '>Нет результатов для выбранных условий.</p></section>'
    if issues:
        body += '<section class="warning"><h2>Проблемы чтения</h2><ul>'
        body += ''.join(f'<li>{Results_Text(item)}</li>' for item in issues) + '</ul></section>'
    return Results_Page("Результаты тестирования", body)


def Results_Write(path, root, content):
    if path.is_symlink():
        raise ValueError("HTML output is a symbolic link")
    destination = Path_Inside(root, path.relative_to(root).as_posix())
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=destination.parent,
                                     prefix=path.name + ".", suffix=".tmp", delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(content)
        except OSError:
            stream.close()
            temporary.unlink(missing_ok=True)
            raise
    try:
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def Results_HtmlPath(source, root):
    output = source.with_suffix(".html")
    if output == root / "index.html":
        output = source.with_name(source.name + ".html")
        while output.with_suffix(".json").exists():
            output = output.with_name(output.stem + ".json.html")
    return output


def Results_Build(config):
    root = Path_Resolve(config["paths"]["reports"])
    root.mkdir(parents=True, exist_ok=True)
    entries, issues = [], []
    for directory, folders, files in os.walk(root, followlinks=False):
        folders[:] = [name for name in folders if not (Path(directory) / name).is_symlink()
                      and not (Path(directory) / name).is_junction()]
        for name in sorted(files):
            source = Path(directory) / name
            if source.suffix != ".json" or source.is_symlink():
                continue
            relative = source.relative_to(root).as_posix()
            try:
                report = Json_Read(source)
                kind = Results_Classify(report)
                if kind is None:
                    continue
                output = Results_HtmlPath(source, root)
                content = Results_Run(report, source, root) if kind == "run" else Results_Smoke(report, source, root)
                Results_Write(output, root, content)
                statuses = report["statuses"] if kind == "run" else report["details"]
                entries.append({"kind": kind, "report": report, "source": relative,
                                "html": output.relative_to(root).as_posix(), "mtime": source.stat().st_mtime,
                                "modified": datetime.fromtimestamp(source.stat().st_mtime).astimezone().isoformat(timespec="seconds"),
                                "attention": (kind == "run" and report.get("complete") is not True)
                                or (kind == "smoke" and not report["ok"])
                                or any(statuses.get(scope) == "fail" for scope in kScopes)})
            except (ValueError, OSError, TypeError, KeyError, AttributeError, RecursionError) as error:
                issues.append(f"{relative}: {error}")
    entries.sort(key=lambda item: (-item["mtime"], item["source"]))
    output = root / "index.html"
    Results_Write(output, root, Results_Index(entries, issues))
    return Result_Make(True, "results_built", path=str(output), runs=sum(item["kind"] == "run" for item in entries),
                       smoke=sum(item["kind"] == "smoke" for item in entries), issues=issues)


def Results_Present(config, source):
    try:
        root = Path_Resolve(config["paths"]["reports"])
        source = source.resolve()
        relative = source.relative_to(root).as_posix()

        presentation = Results_Build(config)["details"]
        html_path = Results_HtmlPath(source, root)
        available = html_path.is_file() and not any(issue.startswith(relative + ":") for issue in presentation["issues"])
        return {"html_path": str(html_path) if available else None, "index_path": presentation["path"],
                "presentation_issues": presentation["issues"]}
    except (ValueError, OSError, TypeError, KeyError) as error:
        return {"html_path": None, "index_path": None, "presentation_issues": [str(error)]}


def Results_SaveSmoke(config, result, suite):
    if suite + "_smoke" not in kSmokeCodes:
        raise ValueError("Unknown smoke suite")
    saved = result if result["code"] == suite + "_smoke" else Result_Make(result["ok"], suite + "_smoke", result=result)
    try:
        source = Path_Inside(Path_Resolve(config["paths"]["reports"]), f"smoke/{suite}-{uuid.uuid4().hex}.json")
        Json_Write(source, saved)
    except (ValueError, OSError) as error:
        return {"result_path": None, "html_path": None, "index_path": None, "presentation_issues": [str(error)]}
    return {"result_path": str(source), **Results_Present(config, source)}
