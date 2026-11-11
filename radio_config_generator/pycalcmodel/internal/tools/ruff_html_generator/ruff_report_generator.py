"""Convert ruff text output to an organized HTML report.

This script parses ruff's text output and generates an HTML report
organized by error type (rule code).
"""

import argparse
import html
import re
import sys
from collections import defaultdict
from pathlib import Path


def parse_text_violation(lines: list[str], start_idx: int) -> tuple[dict | None, int]:
    """Parse a single violation from text format.

    Returns (violation_dict, next_index) or (None, next_index) if no violation found.
    """
    if start_idx >= len(lines):
        return None, start_idx

    first_line = lines[start_idx].strip()
    if not first_line:
        return None, start_idx + 1

    match = re.match(r"^([A-Z]+\d+)\s*(\[\*\])?\s*(.+)$", first_line)
    if match:
        code = match.group(1)
        fixable = match.group(2) is not None
        message = match.group(3)
    else:
        match = re.match(r"^([a-z\-]+):\s*(.+)$", first_line)
        if not match:
            return None, start_idx + 1
        code = match.group(1).upper().replace("-", "_")
        fixable = False
        message = match.group(2)

    idx = start_idx + 1

    filename, line_num, col_num, idx = _parse_location_line(lines, idx)

    while idx < len(lines) and lines[idx].strip() in ["|", ""]:
        idx += 1

    code_snippet, help_text, idx = _collect_code_snippet_lines(lines, idx)

    while idx < len(lines) and not lines[idx].strip():
        idx += 1

    if idx < len(lines) and re.match(r"^Found \d+ error", lines[idx]):
        idx += 1
        if idx < len(lines) and "[*]" in lines[idx] and "fixable" in lines[idx]:
            idx += 1
        while idx < len(lines) and not lines[idx].strip():
            idx += 1

    cleaned_snippet = _clean_code_snippet(code_snippet)

    violation = {
        "code": code,
        "message": message,
        "filename": filename or "Unknown",
        "location": {"row": line_num or 0, "column": col_num or 0},
        "end_location": {"row": line_num or 0, "column": col_num or 0},
        "code_snippet": cleaned_snippet if cleaned_snippet else None,
        "help": help_text,
        "fixable": fixable,
        "url": f"https://docs.astral.sh/ruff/rules/{code.lower()}",
    }

    return violation, idx


def _parse_location_line(lines: list[str], idx: int) -> tuple[str | None, int | None, int | None, int]:
    """Parse location line in format: --> filename:line:col."""
    filename = None
    line_num = None
    col_num = None

    if idx >= len(lines):
        return filename, line_num, col_num, idx

    if "-->" not in lines[idx]:
        return filename, line_num, col_num, idx

    line = lines[idx].strip()
    match = re.match(r"^-->\s+(.+?):(\d+):(\d+)", line)
    if match:
        filename = match.group(1)
        line_num = int(match.group(2))
        col_num = int(match.group(3))
        return filename, line_num, col_num, idx + 1

    return filename, line_num, col_num, idx


def _collect_code_snippet_lines(lines: list[str], idx: int) -> tuple[list[str], str, int]:
    """Collect code snippet lines until 'help:' or next violation."""
    code_snippet = []
    help_text = ""

    while idx < len(lines):
        line = lines[idx]

        if line.strip().startswith("help:"):
            help_text = line.strip()[5:].strip()
            idx += 1
            break

        stripped = line.strip()
        if stripped and not line.startswith(" "):
            if re.match(r"^[A-Z]+\d+", stripped) or re.match(r"^[a-z\-]+:", stripped):
                break

        if line.strip() and line.strip() != "|":
            code_snippet.append(line.rstrip())

        idx += 1

    return code_snippet, help_text, idx


def _clean_code_snippet(raw_lines: list[str]) -> str | None:
    """Clean up code snippet by removing line numbers and pipe characters.

    Ruff output format:
    24 | class PeripheralInfo:
    25 |     '''Docstring'''
    26 |
       | ^^^^
    """
    if not raw_lines:
        return None

    cleaned = []
    for line in raw_lines:
        match = re.match(r"^\s*(\d+\s*\||\|)\s?(.*)", line)
        if match:
            content = match.group(2)
            cleaned.append(content)
        else:
            cleaned.append(line)

    return "\n".join(cleaned) if cleaned else None


def load_ruff_text(file_path: Path) -> list[dict]:
    """Load and parse ruff text output."""
    try:
        content = None
        for encoding in ["utf-8-sig", "utf-16-le", "utf-16", "utf-8"]:
            try:
                with open(file_path, encoding=encoding) as f:
                    content = f.read()
                break
            except (UnicodeDecodeError, UnicodeError):
                continue

        if content is None:
            raise ValueError("Unable to decode file")

        lines = content.split("\n")
        violations = []
        idx = 0

        while idx < len(lines):
            violation, idx = parse_text_violation(lines, idx)
            if violation:
                violations.append(violation)

        return violations

    except FileNotFoundError:
        print(f"Error: File not found: {file_path}")
        sys.exit(1)
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)


def organize_by_error_type(violations: list[dict]) -> dict[str, list[dict]]:
    """Organize violations by error code."""
    organized = defaultdict(list)
    for violation in violations:
        code = violation.get("code", "UNKNOWN")
        organized[code].append(violation)
    return dict(organized)


def generate_html_report(organized_violations: dict[str, list[dict]], output_path: Path) -> None:
    """Generate HTML report from organized violations."""

    total_violations = sum(len(v) for v in organized_violations.values())

    script_dir = Path(__file__).parent
    css_file = script_dir / "report_style.css"
    js_file = script_dir / "report_script.js"
    template_file = script_dir / "report_template.html"

    try:
        css_content = css_file.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"Warning: CSS file not found at {css_file}")
        css_content = "/* CSS file not found */"

    try:
        js_content = js_file.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"Warning: JavaScript file not found at {js_file}")
        js_content = "// JavaScript file not found"

    try:
        html_template = template_file.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"Warning: HTML template not found at {template_file}")
        html_template = _get_default_template()

    content_body = _generate_content_body(organized_violations)

    html_content = html_template.replace("{{CSS_CONTENT}}", css_content)
    html_content = html_content.replace("{{JS_CONTENT}}", js_content)
    html_content = html_content.replace("{{TOTAL_VIOLATIONS}}", str(total_violations))
    html_content = html_content.replace("{{ERROR_TYPES}}", str(len(organized_violations)))
    html_content = html_content.replace("{{CONTENT}}", content_body)

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(html_content)

    print(f"✅ HTML report generated: {output_path}")
    print(f"   Total violations: {total_violations}")
    print(f"   Error types: {len(organized_violations)}")


def _get_default_template() -> str:
    """Return a minimal HTML template if the template file is not found."""
    return """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ruff Linting Report</title>
    <style>{{CSS_CONTENT}}</style>
</head>
<body>
    <div class="container">
        <h1>Ruff Linting Report</h1>
        <div class="summary">
            <h2>Summary</h2>
            <p><strong>Total Violations:</strong> {{TOTAL_VIOLATIONS}}</p>
            <p><strong>Error Types:</strong> {{ERROR_TYPES}}</p>
        </div>
{{CONTENT}}
    </div>
    <script>{{JS_CONTENT}}</script>
</body>
</html>
"""


def _generate_content_body(organized_violations: dict[str, list[dict]]) -> str:
    """Generate the main content body HTML."""

    if not organized_violations:
        return """
        <div class="no-violations">
            ✅ No violations found! Your code is clean.
        </div>
"""

    content = """
        <div class="toggle-all">
            <button onclick="expandAll()">Expand All</button>
            <button onclick="collapseAll()">Collapse All</button>
        </div>
"""

    sorted_errors = sorted(organized_violations.items(), key=lambda x: len(x[1]), reverse=True)

    for error_code, violations in sorted_errors:
        content += _generate_error_section(error_code, violations)

    return content


def _generate_error_section(error_code: str, violations: list[dict]) -> str:
    """Generate HTML for a single error section."""

    error_message = violations[0].get("message", "").split(":")[0]
    violation_count = len(violations)
    plural = "s" if violation_count > 1 else ""

    section_html = f"""
        <div class="error-section">
            <div class="error-header" onclick="toggleSection(this)">
                <h3>{html.escape(error_code)}: {html.escape(error_message)}</h3>
                <span class="error-count">{violation_count} violation{plural}</span>
            </div>
            <div class="violations-list">
                <div class="violations-content">
"""

    for violation in violations:
        section_html += _generate_violation_item(violation)

    section_html += """
                </div>
            </div>
        </div>
"""

    return section_html


def _generate_violation_item(violation: dict) -> str:
    """Generate HTML for a single violation item."""

    filename = violation.get("filename", "Unknown")
    try:
        filename = str(Path(filename).relative_to(Path.cwd()))
    except ValueError:
        pass

    location = violation.get("location", {})
    end_location = violation.get("end_location", {})
    row = location.get("row", "?")
    col = location.get("column", "?")
    end_row = end_location.get("row", row)
    end_col = end_location.get("column", "?")

    message = violation.get("message", "No message")
    url = violation.get("url", "#")
    code_snippet = violation.get("code_snippet")
    help_text = violation.get("help")
    fixable = violation.get("fixable", False)

    location_str = f"Line {row}:{col}"
    if end_row != row:
        location_str += f" - {end_row}:{end_col}"
    elif end_col != "?":
        location_str += f"-{end_col}"

    fix_badge = '<span class="fix-badge">Auto-fixable</span>' if fixable else ""

    item_html = f"""
                    <div class="violation-item">
                        <div class="violation-file">📄 {html.escape(filename)}{fix_badge}</div>
                        <div class="violation-location">📍 {html.escape(location_str)}</div>
                        <div class="violation-message">{html.escape(message)}</div>
"""

    if code_snippet:
        escaped_code = html.escape(code_snippet)
        item_html += f"""
                        <div class="code-snippet">
                            <pre><code>{escaped_code}</code></pre>
                        </div>
"""

    if help_text:
        item_html += f"""
                        <div class="help-text">💡 {html.escape(help_text)}</div>
"""

    item_html += f"""
                        <div class="violation-url">
                            <a href="{html.escape(url)}" target="_blank">📖 View Rule Documentation →</a>
                        </div>
                    </div>
"""

    return item_html


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description="Convert ruff text output to organized HTML report")
    parser.add_argument("input", type=Path, help="Input text file from ruff (use: ruff check . > report.txt)")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("ruff_report.html"),
        help="Output HTML file path (default: ruff_report.html)",
    )

    args = parser.parse_args()

    print(f"📋 Loading ruff output from: {args.input}")
    violations = load_ruff_text(args.input)

    organized = organize_by_error_type(violations)

    generate_html_report(organized, args.output)


if __name__ == "__main__":
    main()
