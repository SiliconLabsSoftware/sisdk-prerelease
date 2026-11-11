"""Unit tests for ruff_report_generator.py."""

import shutil
import tempfile
from pathlib import Path
from unittest.mock import patch

import pytest
from ruff_report_generator import (
    _clean_code_snippet,
    _generate_content_body,
    _generate_error_section,
    _generate_violation_item,
    _get_default_template,
    generate_html_report,
    load_ruff_text,
    organize_by_error_type,
    parse_text_violation,
)


class TestParseTextViolation:
    """Test cases for parse_text_violation function."""

    def test_parse_standard_violation_with_fix(self):
        """Test parsing a standard violation with auto-fix marker."""
        lines = [
            "E501 [*] Line too long (125 > 120)",
            "--> internal/dolly.py:452:121",
            "|",
            "450 |     def some_function():",
            "451 |         pass",
            "452 |         print('This is a very long line that exceeds the maximum allowed length')",
            (
                "    |                                                                                "
                "                     ^^^^"
            ),
            "help: Consider breaking this line into multiple lines",
            "",
        ]
        violation, next_idx = parse_text_violation(lines, 0)

        assert violation is not None
        assert violation["code"] == "E501"
        assert violation["message"] == "Line too long (125 > 120)"
        assert violation["filename"] == "internal/dolly.py"
        assert violation["location"]["row"] == 452
        assert violation["location"]["column"] == 121
        assert violation["fixable"] is True
        assert "print('This is a very long line" in violation["code_snippet"]
        assert violation["help"] == "Consider breaking this line into multiple lines"

    def test_parse_standard_violation_without_fix(self):
        """Test parsing a standard violation without auto-fix."""
        lines = [
            "B023 Function definition does not bind loop variable",
            "--> internal/dolly.py:612:25",
            "|",
            "610 |     for item in items:",
            "611 |         functions.append(",
            "612 |             lambda x: process(item, x)",
            "    |                         ^^^^",
            "613 |         )",
            "",
        ]
        violation, next_idx = parse_text_violation(lines, 0)

        assert violation is not None
        assert violation["code"] == "B023"
        assert violation["fixable"] is False
        assert violation["filename"] == "internal/dolly.py"
        assert "lambda x: process(item, x)" in violation["code_snippet"]

    def test_parse_alternative_format_violation(self):
        """Test parsing alternative format like 'invalid-syntax:'."""
        lines = [
            "invalid-syntax: Expected statement",
            "--> file.js:10:5",
            "|",
            "8  | function test() {",
            "9  |     const x = 10;",
            "10 |     if (x > 5",
            "   |          ^",
            "",
        ]
        violation, next_idx = parse_text_violation(lines, 0)

        assert violation is not None
        assert violation["code"] == "INVALID_SYNTAX"
        assert violation["message"] == "Expected statement"
        assert violation["fixable"] is False

    def test_parse_violation_no_location(self):
        """Test parsing violation without location info."""
        lines = [
            "E999 SyntaxError: invalid syntax",
            "",
        ]
        violation, next_idx = parse_text_violation(lines, 0)

        assert violation is not None
        assert violation["code"] == "E999"
        assert violation["filename"] == "Unknown"
        assert violation["location"]["row"] == 0

    def test_parse_empty_lines(self):
        """Test parsing empty lines returns None."""
        lines = ["", "  ", "\n"]
        violation, next_idx = parse_text_violation(lines, 0)
        assert violation is None

    def test_parse_multiple_violations_sequentially(self):
        """Test parsing multiple violations in sequence."""
        lines = [
            "E501 [*] Line too long",
            "--> file.py:1:121",
            "",
            "I001 Import block is un-sorted",
            "--> file.py:10:1",
            "",
        ]

        violation1, idx1 = parse_text_violation(lines, 0)
        assert violation1["code"] == "E501"

        violation2, idx2 = parse_text_violation(lines, idx1)
        assert violation2["code"] == "I001"

    def test_skip_summary_lines(self):
        """Test that summary lines are properly skipped."""
        lines = [
            "E501 Line too long",
            "--> file.py:1:1",
            "",
            "Found 5 errors.",
            "[*] 3 fixable with the `--fix` option.",
            "",
            "E502 Another error",
        ]

        violation1, idx1 = parse_text_violation(lines, 0)
        assert violation1["code"] == "E501"

        violation2, idx2 = parse_text_violation(lines, idx1)
        assert violation2["code"] == "E502"


class TestCleanCodeSnippet:
    """Test cases for _clean_code_snippet function."""

    def test_clean_snippet_with_line_numbers(self):
        """Test cleaning snippet with line numbers."""
        raw_lines = [
            "24 | class PeripheralInfo:",
            "25 |     '''Docstring'''",
            "26 |     ",
            "   | ^^^^",
        ]
        result = _clean_code_snippet(raw_lines)
        expected = "class PeripheralInfo:\n    '''Docstring'''\n    \n^^^^"
        assert result == expected

    def test_clean_snippet_without_line_numbers(self):
        """Test cleaning snippet with only pipes."""
        raw_lines = [
            "| def function():",
            "|     pass",
        ]
        result = _clean_code_snippet(raw_lines)
        expected = "def function():\n    pass"
        assert result == expected

    def test_clean_empty_snippet(self):
        """Test cleaning empty snippet returns None."""
        result = _clean_code_snippet([])
        assert result is None

    def test_clean_snippet_preserves_indentation(self):
        """Test that indentation is preserved."""
        raw_lines = [
            "10 |     def method(self):",
            "11 |         return True",
        ]
        result = _clean_code_snippet(raw_lines)
        expected = "    def method(self):\n        return True"
        assert result == expected


class TestLoadRuffText:
    """Test cases for load_ruff_text function."""

    def test_load_valid_text_file(self):
        """Test loading a valid ruff text output file."""
        content = """E501 [*] Line too long
--> file.py:1:121
|
1 | print('hello world')
  | ^^^^

I001 Import block is un-sorted
--> file.py:5:1
"""

        with tempfile.NamedTemporaryFile(mode="w", delete=False, encoding="utf-8", suffix=".txt") as f:
            f.write(content)
            temp_path = Path(f.name)

        try:
            violations = load_ruff_text(temp_path)
            assert len(violations) == 2
            assert violations[0]["code"] == "E501"
            assert violations[1]["code"] == "I001"
        finally:
            temp_path.unlink()

    def test_load_file_with_utf8_sig(self):
        """Test loading file with UTF-8 BOM."""
        content = "E501 Line too long\n--> file.py:1:1\n"

        with tempfile.NamedTemporaryFile(mode="w", delete=False, encoding="utf-8-sig", suffix=".txt") as f:
            f.write(content)
            temp_path = Path(f.name)

        try:
            violations = load_ruff_text(temp_path)
            assert len(violations) > 0
        finally:
            temp_path.unlink()

    def test_load_nonexistent_file(self):
        """Test loading non-existent file exits with error."""
        with pytest.raises(SystemExit):
            load_ruff_text(Path("nonexistent_file.txt"))

    def test_load_empty_file(self):
        """Test loading empty file returns empty list."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, encoding="utf-8", suffix=".txt") as f:
            f.write("")
            temp_path = Path(f.name)

        try:
            violations = load_ruff_text(temp_path)
            assert len(violations) == 0
        finally:
            temp_path.unlink()


class TestOrganizeByErrorType:
    """Test cases for organize_by_error_type function."""

    def test_organize_single_error_type(self):
        """Test organizing violations of a single error type."""
        violations = [
            {"code": "E501", "message": "Line 1"},
            {"code": "E501", "message": "Line 2"},
            {"code": "E501", "message": "Line 3"},
        ]
        result = organize_by_error_type(violations)

        assert len(result) == 1
        assert "E501" in result
        assert len(result["E501"]) == 3

    def test_organize_multiple_error_types(self):
        """Test organizing violations of multiple error types."""
        violations = [
            {"code": "E501", "message": "Line too long"},
            {"code": "I001", "message": "Import unsorted"},
            {"code": "E501", "message": "Another long line"},
            {"code": "B023", "message": "Loop variable"},
        ]
        result = organize_by_error_type(violations)

        assert len(result) == 3
        assert len(result["E501"]) == 2
        assert len(result["I001"]) == 1
        assert len(result["B023"]) == 1

    def test_organize_empty_violations(self):
        """Test organizing empty violations list."""
        result = organize_by_error_type([])
        assert result == {}

    def test_organize_missing_code(self):
        """Test organizing violations with missing code uses UNKNOWN."""
        violations = [
            {"message": "No code"},
        ]
        result = organize_by_error_type(violations)
        assert "UNKNOWN" in result


class TestGenerateHTMLReport:
    """Test cases for generate_html_report function."""

    @pytest.fixture
    def temp_dir(self):
        """Set up temporary directory for test files."""
        temp_dir = tempfile.mkdtemp()
        yield temp_dir
        shutil.rmtree(temp_dir)

    @pytest.fixture
    def script_dir(self, temp_dir):
        """Create mock template files in temp directory."""
        script_dir = Path(temp_dir) / "templates"
        script_dir.mkdir()

        (script_dir / "report_style.css").write_text("/* Test CSS */", encoding="utf-8")
        (script_dir / "report_script.js").write_text("// Test JS", encoding="utf-8")
        (script_dir / "report_template.html").write_text(
            "<!DOCTYPE html><html><head><style>{{CSS_CONTENT}}</style></head>"
            "<body><h1>{{TOTAL_VIOLATIONS}} violations</h1>{{CONTENT}}"
            "<script>{{JS_CONTENT}}</script></body></html>",
            encoding="utf-8",
        )
        return script_dir

    def test_generate_html_with_violations(self, temp_dir, script_dir):
        """Test generating HTML report with violations."""
        output_path = Path(temp_dir) / "test_report.html"

        with patch("ruff_report_generator.__file__", str(script_dir / "ruff_report_generator.py")):
            organized = {
                "E501": [
                    {
                        "code": "E501",
                        "message": "Line too long",
                        "filename": "test.py",
                        "location": {"row": 1, "column": 121},
                        "end_location": {"row": 1, "column": 125},
                        "fixable": True,
                        "url": "https://docs.astral.sh/ruff/rules/e501",
                    }
                ]
            }

            generate_html_report(organized, output_path)

            assert output_path.exists()
            content = output_path.read_text(encoding="utf-8")
            assert "1 violations" in content
            assert "E501" in content

    def test_generate_html_empty_violations(self, temp_dir, script_dir):
        """Test generating HTML report with no violations."""
        output_path = Path(temp_dir) / "test_report.html"

        with patch("ruff_report_generator.__file__", str(script_dir / "ruff_report_generator.py")):
            generate_html_report({}, output_path)

            assert output_path.exists()
            content = output_path.read_text(encoding="utf-8")
            assert "0 violations" in content


class TestGetDefaultTemplate:
    """Test cases for _get_default_template function."""

    def test_default_template_contains_placeholders(self):
        """Test that default template contains required placeholders."""
        template = _get_default_template()

        assert "{{CSS_CONTENT}}" in template
        assert "{{JS_CONTENT}}" in template
        assert "{{TOTAL_VIOLATIONS}}" in template
        assert "{{ERROR_TYPES}}" in template
        assert "{{CONTENT}}" in template

    def test_default_template_is_valid_html(self):
        """Test that default template starts with DOCTYPE."""
        template = _get_default_template()
        assert template.strip().startswith("<!DOCTYPE html>")


class TestGenerateContentBody:
    """Test cases for _generate_content_body function."""

    def test_generate_body_with_violations(self):
        """Test generating content body with violations."""
        organized = {
            "E501": [
                {
                    "code": "E501",
                    "message": "Line too long",
                    "filename": "test.py",
                    "location": {"row": 1, "column": 121},
                    "end_location": {"row": 1, "column": 125},
                    "fixable": True,
                }
            ]
        }

        content = _generate_content_body(organized)

        assert "Expand All" in content
        assert "Collapse All" in content
        assert "E501" in content

    def test_generate_body_empty_violations(self):
        """Test generating content body with no violations."""
        content = _generate_content_body({})
        assert "No violations found" in content
        assert "✅" in content

    def test_generate_body_sorts_by_count(self):
        """Test that errors are sorted by violation count."""
        organized = {
            "E501": [{"code": "E501"}],  # 1 violation
            "I001": [{"code": "I001"}, {"code": "I001"}, {"code": "I001"}],  # 3 violations
            "B023": [{"code": "B023"}, {"code": "B023"}],  # 2 violations
        }

        content = _generate_content_body(organized)

        pos_i001 = content.find("I001")
        pos_b023 = content.find("B023")
        pos_e501 = content.find("E501")

        assert pos_i001 < pos_b023 < pos_e501


class TestGenerateErrorSection:
    """Test cases for _generate_error_section function."""

    def test_generate_section_single_violation(self):
        """Test generating error section with single violation."""
        violations = [
            {
                "code": "E501",
                "message": "Line too long (125 > 120)",
                "filename": "test.py",
                "location": {"row": 1, "column": 121},
                "end_location": {"row": 1, "column": 125},
                "fixable": True,
                "url": "https://docs.astral.sh/ruff/rules/e501",
            }
        ]

        section = _generate_error_section("E501", violations)

        assert "E501" in section
        assert "1 violation" in section

    def test_generate_section_multiple_violations(self):
        """Test generating error section with multiple violations."""
        violations = [
            {
                "code": "E501",
                "message": "Line too long",
                "filename": "test1.py",
                "location": {"row": 1, "column": 121},
                "end_location": {"row": 1, "column": 125},
            },
            {
                "code": "E501",
                "message": "Line too long",
                "filename": "test2.py",
                "location": {"row": 5, "column": 121},
                "end_location": {"row": 5, "column": 125},
            },
        ]

        section = _generate_error_section("E501", violations)

        assert "2 violations" in section
        assert "test1.py" in section
        assert "test2.py" in section


class TestGenerateViolationItem:
    """Test cases for _generate_violation_item function."""

    def test_generate_item_with_all_fields(self):
        """Test generating violation item with all fields present."""
        violation = {
            "code": "E501",
            "message": "Line too long (125 > 120)",
            "filename": "internal/test.py",
            "location": {"row": 10, "column": 121},
            "end_location": {"row": 10, "column": 125},
            "fixable": True,
            "code_snippet": "print('This is a very long line')",
            "help": "Consider breaking this line",
            "url": "https://docs.astral.sh/ruff/rules/e501",
        }

        item = _generate_violation_item(violation)

        assert "test.py" in item
        assert "Line 10:121" in item
        assert "Auto-fixable" in item
        assert "Consider breaking this line" in item
        assert "print(&#x27;This is a very long line&#x27;)" in item  # HTML escaped
        assert "View Rule Documentation" in item

    def test_generate_item_without_optional_fields(self):
        """Test generating violation item without optional fields."""
        violation = {
            "code": "E999",
            "message": "SyntaxError",
            "filename": "test.py",
            "location": {"row": 1, "column": 1},
            "end_location": {"row": 1, "column": 1},
            "fixable": False,
            "url": "https://docs.astral.sh/ruff/rules/e999",
        }

        item = _generate_violation_item(violation)

        assert "test.py" in item
        assert "Auto-fixable" not in item
        assert "code-snippet" not in item
        assert "help-text" not in item

    def test_generate_item_escapes_html(self):
        """Test that HTML special characters are properly escaped."""
        violation = {
            "message": "Error in <script>alert('xss')</script>",
            "filename": "test.py",
            "location": {"row": 1, "column": 1},
            "end_location": {"row": 1, "column": 1},
            "code_snippet": "<div>malicious</div>",
            "help": "Use & instead of &amp;",
            "url": "#",
        }

        item = _generate_violation_item(violation)

        assert "<script>" not in item
        assert "<div>" not in item
        assert "&lt;script&gt;" in item
        assert "&lt;div&gt;" in item

    def test_generate_item_multiline_location(self):
        """Test generating item with multi-line violation."""
        violation = {
            "message": "Multi-line error",
            "filename": "test.py",
            "location": {"row": 10, "column": 1},
            "end_location": {"row": 15, "column": 10},
            "url": "#",
        }

        item = _generate_violation_item(violation)
        assert "Line 10:1 - 15:10" in item
