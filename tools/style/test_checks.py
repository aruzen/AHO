#!/usr/bin/env python3
"""規約の分類、配置の例外、既存違反との差分検出を実コンパイラで確認する。"""

import argparse
from collections import Counter
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


HERE = Path(__file__).resolve().parent
PARSER = argparse.ArgumentParser()
PARSER.add_argument('--ast-tool', type=Path, required=True)
PARSER.add_argument('--clang-tidy', default='clang-tidy')
PARSER.add_argument('--clang-format', default='clang-format')
ARGS, TEST_ARGS = PARSER.parse_known_args()
SPEC = importlib.util.spec_from_file_location('style_check', HERE / 'check.py')
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class CheckerContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()

    def ast(self, header, min_lines=0):
        (self.root / 'probe.hpp').write_text(header)
        source = self.root / 'probe.cpp'
        source.write_text('#include "probe.hpp"\n')
        command = [str(ARGS.ast_tool), '--root=' + str(self.root)]
        if min_lines is not None:
            command.append('--in-class-min-lines=' + str(min_lines))
        result = subprocess.run([*command, str(source), '--', '-std=c++23'], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return [item for line in result.stdout.splitlines() if 'coverage' not in (item := json.loads(line))]

    def tidy(self, source):
        path = self.root / 'probe.cpp'
        path.write_text(source)
        result = subprocess.run([ARGS.clang_tidy, '--config-file=' + str(HERE.parents[1] / '.clang-tidy'),
                                 str(path), '--', '-std=c++23'], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout + result.stderr

    def test_static_members_and_free_functions_and_const_locals_are_distinguished(self):
        output = self.tidy('''
namespace good_scope {
int GLOBAL_VALUE;
static int STATIC_VALUE;
constexpr int GLOBAL_CONSTANT = 1;
class Example {
    int _value;
    const int _constant = 1;
    static int CLASS_VALUE;
    static constexpr int CLASS_CONSTANT = 1;
public:
    using ValueType = int;
    using value_type = int;
    static void MakeValue();
    static constexpr int MakeConstant() { return 1; }
    void set_value();
    constexpr int value() const { return 1; }
};
static void free_function() {
    const int local_constant = 1;
    constexpr int local_constexpr = 2;
    static int LOCAL_STATIC;
    static constexpr int LOCAL_STATIC_CONSTANT = 1;
}
}
''')
        self.assertNotIn('[readability-identifier-naming]', output)

    def test_naming_violations_are_detected_for_each_category(self):
        output = self.tidy('''
namespace BadScope {
int globalValue;
enum class Mode { BadValue };
struct bad_type {};
class Example {
    int privateValue;
    static int staticValue;
public:
    static void make_value();
    void SetValue();
};
static void FreeFunction(int BadParameter) { constexpr int LocalValue = 1; }
}
''')
        for name in ['BadScope', 'globalValue', 'BadValue', 'bad_type', 'privateValue', 'staticValue',
                     'make_value', 'SetValue', 'FreeFunction', 'BadParameter', 'LocalValue']:
            with self.subTest(name=name):
                self.assertIn("'" + name + "'", output)

    def test_collision_suffix_does_not_allow_an_incorrect_case_or_private_prefix(self):
        output = self.tidy('''
struct Point_ {};
int GLOBAL_VALUE_;
class Example {
    int _value_;
public:
    static void MakeValue_();
    void set_value_();
    void badCase_();
};
void free_function_(int value_);
''')
        self.assertIn("'badCase_'", output)
        for name in ['Point_', 'GLOBAL_VALUE_', '_value_', 'MakeValue_', 'set_value_', 'free_function_', 'value_']:
            self.assertNotIn("'" + name + "'", output)

    def test_out_of_class_templates_default_delete_lambdas_and_local_classes_are_allowed(self):
        items = self.ast('''
namespace sample {
class Example {
public:
    Example() = default;
    Example(const Example&) = delete;
    inline int value() const;
    template<class T> T convert(T value) const;
};
inline int Example::value() const { return 1; }
template<class T> T Example::convert(T value) const { return value; }
inline int use_local_class() {
    struct Local { int value() { return 1; } };
    auto lambda = [] { return 2; };
    return Local{}.value() + lambda();
}
}
''')
        self.assertEqual(items, [])

    def test_constructor_getter_header_definition_and_declaration_order_are_detected(self):
        items = self.ast('''
namespace sample {
class Example {
public:
    Example() {}
    int value() const { return 1; }
};
int header_function() { return 0; }
struct Later {};
void declared_later();
}
''')
        counts = Counter(item['rule'] for item in items)
        self.assertEqual(counts['aho-in-class-definition'], 2)
        self.assertEqual(counts['aho-header-definition'], 1)
        self.assertEqual(counts['aho-declaration-order'], 2)

    def test_code_lines_exclude_signature_comments_outer_braces_and_initializers(self):
        items = self.ast('''
class Example {
    int a, b, c, d, e;
public:
    Example()
        : a(1),
          b(2),
          c(3),
          d(4),
          e(5) {}
    int empty() {
        // コメントは数えない。
        /* 複数行の
           コメントも除外する。 */
        return 0;
    }
    int four(
        int value,
        int other
    ) {
        value += other;

        value += 2; // 行末コメント
        /* comment */ value += 3;
        return value;
    }
    int literals() {
        const char* url = "https://example.test";
        const char* comment = "/* not a comment */";
        return url[0] + comment[0];
    }
    int one_line() { int value = 1; value += 2; value += 3; value += 4; return value; }
};
''')
        counts = {item['symbol']: item['code_lines'] for item in items if item['rule'] == 'aho-in-class-definition'}
        self.assertEqual(counts, {'Example::Example': 0, 'Example::empty': 1, 'Example::four': 4,
                                  'Example::literals': 3, 'Example::one_line': 1})

    def test_in_class_threshold_includes_five_lines_and_excludes_four(self):
        source = '''
struct Example {
    int four() {
        int value = 1;
        value += 2;
        value += 3;
        return value;
    }
    int five() {
        int value = 1;
        value += 2;
        value += 3;
        value += 4;
        return value;
    }
};
'''
        items = self.ast(source, min_lines=5)
        relevant = [item for item in items if item['rule'] == 'aho-in-class-definition']
        self.assertEqual([(item['symbol'], item['code_lines']) for item in relevant], [('Example::five', 5)])
        defaults = self.ast(source, min_lines=None)
        self.assertEqual([(item['symbol'], item['code_lines']) for item in defaults], [('Example::five', 5)])
        self.assertEqual(self.ast(source, min_lines=6), [])

    def test_function_try_block_keeps_a_diagnostic_when_count_is_unavailable(self):
        items = self.ast('''
struct Example {
    int value;
    Example() try : value(1) {} catch (...) { throw; }
};
''', min_lines=100)
        self.assertEqual([item['rule'] for item in items], ['aho-in-class-definition'])
        self.assertNotIn('code_lines', items[0])

    def test_default_cli_detects_growth_from_four_to_five_lines(self):
        directory = self.root / 'AHOLi/AHO'
        directory.mkdir(parents=True)
        source = directory / 'probe.cpp'
        original = '''
struct Example {
    int value() {
        int result = 1;
        result += 2;
        result += 3;
        return result;
    }
};
'''
        source.write_text(original)
        database = self.root / 'compile_commands.json'
        database.write_text(json.dumps([dict(directory=str(self.root), file=str(source),
                                             arguments=['clang++', '-std=c++23', '-c', str(source)])]))
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', 'add', '.'], cwd=self.root, check=True)
        baseline = self.root / 'baseline.json'
        baseline.write_text(json.dumps({'version': 1, 'llvm_major': 20, 'in_class_min_lines': 5, 'diagnostics': []}))
        command = [sys.executable, str(HERE / 'check.py'), '--root', str(self.root), '--checks=layout',
                   '--database', str(database), '--baseline', str(baseline), '--ast-tool', str(ARGS.ast_tool.resolve())]
        first = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(first.returncode, 0, first.stderr)
        source.write_text(original.replace('        return result;', '        result += 4;\n        return result;'))
        changed = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(changed.returncode, 1, changed.stderr)
        report = json.loads((self.root / 'build/style-report/report.json').read_text())
        self.assertEqual(report['in_class_min_lines'], 5)
        self.assertEqual([(item['rule'], item['code_lines']) for item in report['new']], [('aho-in-class-definition', 5)])

    def test_repository_baseline_has_no_short_function_allowances(self):
        baseline = json.loads((HERE / 'baseline.json').read_text())
        self.assertEqual(baseline['in_class_min_lines'], 5)
        functions = [item for item in baseline['diagnostics'] if item['rule'] == 'aho-in-class-definition']
        self.assertTrue(functions)
        self.assertTrue(all(item['code_lines'] >= 5 for item in functions))

    def test_reopened_namespace_does_not_bypass_declaration_order(self):
        items = self.ast('''
namespace sample { inline int first() { return 1; } }
namespace sample { class Later {}; }
''')
        self.assertEqual([item['rule'] for item in items], ['aho-declaration-order'])

    def test_acronyms_and_local_and_global_structured_bindings_are_checked(self):
        items = self.ast('''
struct GpuBuffer {};
int VALUES[2] = {};
auto [BAD, ALSO_GOOD] = VALUES;
auto [bad_global, ANOTHER_GOOD] = VALUES;
inline int probe() {
    auto [badLocal, good_local] = VALUES;
    static auto [bad_static, GOOD_STATIC] = VALUES;
    return good_local;
}
''')
        counts = Counter(item['rule'] for item in items)
        self.assertEqual(counts['aho-acronym'], 1)
        self.assertEqual(counts['aho-binding-name'], 3)

    def test_parse_errors_fail_instead_of_producing_a_clean_result(self):
        source = self.root / 'invalid.cpp'
        source.write_text('class Broken { missing_type value; };')
        result = subprocess.run([str(ARGS.ast_tool), '--root=' + str(self.root), str(source), '--', '-std=c++23'],
                                text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)

    def test_coverage_owns_paths_and_normalizes_relative_includes(self):
        directory = self.root / 'include'
        directory.mkdir()
        headers = [f'header_{index}_' + 'long_' * index + '.hpp' for index in range(6)]
        for name in headers:
            (directory / name).write_text('#pragma once\n')
        source = self.root / 'probe.cpp'
        source.write_text(''.join(f'#include "include/./{name}"\n' for name in headers))
        result = subprocess.run([str(ARGS.ast_tool), '--root=' + str(self.root), str(source), '--', '-std=c++23'],
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        coverage = next(json.loads(line)['coverage'] for line in result.stdout.splitlines() if '"coverage"' in line)
        self.assertEqual(set(coverage), {'probe.cpp', *(f'include/{name}' for name in headers)})

    def test_baseline_allows_only_recorded_occurrences_and_source_content(self):
        source = self.root / 'probe.cpp'
        source.write_text('int badName;\nint badName;\n')
        old = CHECK.diagnostic(self.root, 'probe.cpp', 1, 1, 'name', 'badName', 'bad name')
        baseline = {'diagnostics': [{**old, 'count': 1}]}
        self.assertEqual(CHECK.compare([old], baseline), [])
        duplicate = CHECK.diagnostic(self.root, 'probe.cpp', 2, 1, 'name', 'badName', 'bad name')
        self.assertEqual(CHECK.compare([old, duplicate], baseline), [duplicate])
        source.write_text('const int badName = 1;\n')
        changed = CHECK.diagnostic(self.root, 'probe.cpp', 1, 1, 'name', 'badName', 'bad name')
        self.assertEqual(CHECK.compare([changed], baseline), [changed])

    def test_cli_format_baseline_and_new_violation_exit_status(self):
        directory = self.root / 'AHOLi/AHO/src'
        directory.mkdir(parents=True)
        source = directory / 'probe.cpp'
        source.write_text('int   value=1;\n')
        shutil.copy2(HERE.parents[1] / '.clang-format', self.root / '.clang-format')
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', 'add', '.'], cwd=self.root, check=True)
        command = [sys.executable, str(HERE / 'check.py'), '--root', str(self.root), '--checks=format',
                   '--clang-format', ARGS.clang_format]
        first = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(first.returncode, 1, first.stderr)
        report = json.loads((self.root / 'build/style-report/report.json').read_text())
        baseline = {'diagnostics': [{**item, 'count': 1} for item in report['diagnostics']]}
        (self.root / 'baseline.json').write_text(json.dumps(baseline))
        same = subprocess.run(command + ['--baseline', str(self.root / 'baseline.json')], text=True, capture_output=True)
        self.assertEqual(same.returncode, 0, same.stderr)
        source.write_text('int   value=2;\n')
        changed = subprocess.run(command + ['--baseline', str(self.root / 'baseline.json')], text=True, capture_output=True)
        self.assertEqual(changed.returncode, 1, changed.stderr)

    def test_unused_headers_and_unregistered_sources_are_reported(self):
        directory = self.root / 'AHOLi/AHO'
        directory.mkdir(parents=True)
        source = directory / 'probe.cpp'
        source.write_text('#include "used.hpp"\n')
        (directory / 'used.hpp').write_text('#pragma once\nstruct Used {};\n')
        (directory / 'unused.hpp').write_text('#pragma once\nstruct Unused {};\n')
        (directory / 'unregistered.cpp').write_text('int VALUE;\n')
        database = self.root / 'compile_commands.json'
        database.write_text(json.dumps([dict(directory=str(self.root), file=str(source),
                                             arguments=['clang++', '-std=c++23', '-c', str(source)])]))
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', 'add', '.'], cwd=self.root, check=True)
        command = [sys.executable, str(HERE / 'check.py'), '--root', str(self.root), '--checks=layout',
                   '--database', str(database), '--ast-tool', str(ARGS.ast_tool.resolve())]
        result = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        report = json.loads((self.root / 'build/style-report/report.json').read_text())
        self.assertEqual(report['unparsed_headers'], ['AHOLi/AHO/unused.hpp'])
        self.assertEqual(report['unmapped_sources'], ['AHOLi/AHO/unregistered.cpp'])
        self.assertEqual({item['rule'] for item in report['new']}, {'aho-unparsed-header', 'aho-missing-command'})
        focused = subprocess.run(command + ['AHOLi/AHO/probe.cpp'], text=True, capture_output=True)
        self.assertEqual(focused.returncode, 0, focused.stderr)
        focused_report = json.loads((self.root / 'build/style-report/report.json').read_text())
        self.assertEqual(focused_report['translation_units'], 1)
        old = next(item for item in report['new'] if item['rule'] == 'aho-unparsed-header')
        (directory / 'unused.hpp').write_text('#pragma once\nstruct Unused { int another_value; };\n')
        subprocess.run(command, text=True, capture_output=True, check=False)
        report = json.loads((self.root / 'build/style-report/report.json').read_text())
        changed = next(item for item in report['new'] if item['rule'] == 'aho-unparsed-header')
        self.assertNotEqual(old['key'], changed['key'])


if __name__ == '__main__':
    unittest.main(argv=[__file__, *TEST_ARGS])
