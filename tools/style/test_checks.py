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

    def write_master(self, file, diagnostics, directory=None, min_lines=5):
        directory = directory or self.root / 'tools/style/master'
        path = directory / (file + '.json')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(dict(version=1, llvm_major=20, in_class_min_lines=min_lines,
                                        diagnostics=diagnostics)))
        return path

    def prepare_cli(self, files, registered):
        for file, contents in files.items():
            path = self.root / file
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents)
        for config in ['.clang-format', '.clang-tidy']:
            shutil.copy2(HERE.parents[1] / config, self.root / config)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', 'add', '.'], cwd=self.root, check=True)
        database = self.root / 'compile_commands.json'
        database.write_text(json.dumps([
            dict(directory=str(self.root), file=str(self.root / file),
                 arguments=['clang++', '-std=c++23', '-c', str(self.root / file)])
            for file in registered]))
        return [sys.executable, str(HERE / 'check.py'), '--root', str(self.root),
                '--database', str(database), '--ast-tool', str(ARGS.ast_tool.resolve()),
                '--clang-format', ARGS.clang_format, '--clang-tidy', ARGS.clang_tidy]

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
        command = [sys.executable, str(HERE / 'check.py'), '--root', str(self.root), '--checks=layout',
                   '--database', str(database), '--ast-tool', str(ARGS.ast_tool.resolve())]
        first = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(first.returncode, 0, first.stderr)
        source.write_text(original.replace('        return result;', '        result += 4;\n        return result;'))
        changed = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(changed.returncode, 1, changed.stderr)
        report = json.loads((self.root / 'build/style-report/report.json').read_text())
        self.assertEqual(report['in_class_min_lines'], 5)
        self.assertEqual([(item['rule'], item['code_lines']) for item in report['new']], [('aho-in-class-definition', 5)])

    def test_repository_master_has_no_short_function_allowances(self):
        master = CHECK.load_master(HERE / 'master', CHECK.tracked_files(HERE.parents[1]), 5)
        functions = [item for item in master if item['rule'] == 'aho-in-class-definition']
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

    def test_master_allows_only_recorded_occurrences_and_source_content(self):
        source = self.root / 'probe.cpp'
        source.write_text('int badName;\nint badName;\n')
        old = CHECK.diagnostic(self.root, 'probe.cpp', 1, 1, 'name', 'badName', 'bad name')
        master = [{**old, 'count': 1}]
        self.assertEqual(CHECK.compare([old], master), [])
        duplicate = CHECK.diagnostic(self.root, 'probe.cpp', 2, 1, 'name', 'badName', 'bad name')
        self.assertEqual(CHECK.compare([old, duplicate], master), [duplicate])
        source.write_text('const int badName = 1;\n')
        changed = CHECK.diagnostic(self.root, 'probe.cpp', 1, 1, 'name', 'badName', 'bad name')
        self.assertEqual(CHECK.compare([changed], master), [changed])

    def test_cli_format_master_and_new_violation_exit_status(self):
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
        self.write_master('AHOLi/AHO/src/probe.cpp', [{**item, 'count': 1} for item in report['diagnostics']])
        same = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(same.returncode, 0, same.stderr)
        source.write_text('int   value=2;\n')
        changed = subprocess.run(command, text=True, capture_output=True)
        self.assertEqual(changed.returncode, 1, changed.stderr)

    def test_master_reads_only_the_selected_source_files(self):
        first = CHECK.diagnostic(self.root, 'AHOLi/AHO/first.cpp', 1, 1, 'name', 'bad', 'bad', source='int bad;')
        other = CHECK.diagnostic(self.root, 'VSLi/VSL/other.cpp', 1, 1, 'name', 'bad', 'bad', source='int bad;')
        directory = self.root / 'custom-master'
        self.write_master(first['file'], [{**first, 'count': 1}], directory)
        self.write_master(other['file'], [{**other, 'count': 1}], directory)
        master = CHECK.load_master(directory, {first['file']}, 5)
        self.assertEqual(CHECK.compare([first, other], master), [other])
        self.assertEqual(CHECK.load_master(self.root / 'absent', {first['file']}, 5), [])

    def test_master_rejects_invalid_metadata_paths_and_counts(self):
        file = 'AHOLi/AHO/probe.cpp'
        item = CHECK.diagnostic(self.root, file, 1, 1, 'name', 'bad', 'bad', source='int bad;')
        path = self.write_master(file, [{**item, 'count': 1}])
        original = json.loads(path.read_text())
        invalid = [dict(original, version=2), dict(original, llvm_major=19),
                   dict(original, in_class_min_lines=0), dict(original, diagnostics={}), []]
        for fields in [{'file': 'VSLi/VSL/other.cpp'}, {'count': 0}, {'count': -1}, {'count': True}, {'key': None}]:
            invalid.append(dict(original, diagnostics=[{**item, 'count': 1, **fields}]))
        for value in invalid:
            with self.subTest(value=value):
                path.write_text(json.dumps(value))
                with self.assertRaises(ValueError):
                    CHECK.load_master(path.parents[2], {file}, 5)

    def test_update_master_reduces_occurrences_without_adding_new_permissions(self):
        file = 'AHOLi/AHO/probe.cpp'
        old = CHECK.diagnostic(self.root, file, 1, 1, 'name', 'bad', 'bad', source='int bad;')
        other = CHECK.diagnostic(self.root, 'VSLi/VSL/other.cpp', 1, 1, 'name', 'bad', 'bad', source='int bad;')
        path = self.write_master(file, [{**old, 'count': 2}])
        untouched = self.write_master(other['file'], [{**other, 'count': 1}])
        before = untouched.read_bytes()
        directory = self.root / 'tools/style/master'
        new = CHECK.diagnostic(self.root, file, 2, 1, 'name', 'another', 'bad', source='int another;')
        CHECK.update_master(directory, {file}, [old, new], 5)
        self.assertEqual(CHECK.load_master(directory, {file}, 5), [{**old, 'count': 1}])
        self.assertEqual(untouched.read_bytes(), before)
        CHECK.update_master(directory, {file}, [new], 5)
        self.assertFalse(path.exists())
        self.assertEqual(untouched.read_bytes(), before)

    def test_cli_update_master_is_scoped_and_keeps_new_violations_failing(self):
        first, other = 'AHOLi/AHO/first.cpp', 'VSLi/VSL/other.cpp'
        command = self.prepare_cli({first: 'int   VALUE=1;\n', other: 'int   OTHER=1;\n'}, [first, other])
        old = CHECK.format_check(self.root, first, ARGS.clang_format)
        path = self.write_master(first, [{**item, 'count': 1} for item in old])
        untouched = self.write_master(other, [{**item, 'count': 1}
                                            for item in CHECK.format_check(self.root, other, ARGS.clang_format)])
        before = untouched.read_bytes()
        (self.root / first).write_text('int VALUE = 1;\n')
        result = subprocess.run(command + ['--update-master', first], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(path.exists())
        self.assertEqual(untouched.read_bytes(), before)
        self.write_master(first, [{**item, 'count': 1} for item in old])
        (self.root / first).write_text('int bad_value = 1;\n')
        result = subprocess.run(command + ['--update-master', first], text=True, capture_output=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertFalse(path.exists())
        self.assertEqual(untouched.read_bytes(), before)
        for options in [['--checks=format'], ['--strict']]:
            result = subprocess.run(command + ['--update-master', *options, first], text=True, capture_output=True)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertEqual(untouched.read_bytes(), before)

    def test_full_master_update_removes_records_for_deleted_sources(self):
        file = 'AHOLi/AHO/deleted.cpp'
        item = CHECK.diagnostic(self.root, file, 1, 1, 'name', 'bad', 'bad', source='int bad;')
        path = self.write_master(file, [{**item, 'count': 1}])
        directory = self.root / 'tools/style/master'
        CHECK.update_master(directory, set(), [], 5)
        self.assertTrue(path.exists())
        CHECK.update_master(directory, set(), [], 5, remove_deleted=True)
        self.assertFalse(path.exists())

    def test_cmake_targets_check_and_update_the_per_source_master(self):
        file = 'AHOLi/AHO/probe.cpp'
        self.prepare_cli({file: 'int   VALUE=1;\n'}, [file])
        path = self.write_master(file, [{**item, 'count': 1}
                                       for item in CHECK.format_check(self.root, file, ARGS.clang_format)], min_lines=0)
        shutil.copy2(HERE / 'check.py', self.root / 'tools/style/check.py')
        (self.root / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.20)
project(StyleMasterFixture LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(probe OBJECT {file})
add_executable(aho-style-ast IMPORTED GLOBAL)
set_property(TARGET aho-style-ast PROPERTY IMPORTED_LOCATION [==[{ARGS.ast_tool.resolve()}]==])
set(Python3_EXECUTABLE [==[{sys.executable}]==])
set(AHO_CLANG_FORMAT [==[{ARGS.clang_format}]==])
set(AHO_CLANG_TIDY [==[{ARGS.clang_tidy}]==])
include([==[{HERE.parents[1] / 'cmake/StyleChecks.cmake'}]==])
''')
        build = self.root / 'build/cmake'
        configure = subprocess.run(['cmake', '-S', str(self.root), '-B', str(build), '-G', 'Ninja',
                                    '-DAHO_STYLE_IN_CLASS_MIN_LINES=0'],
                                   text=True, capture_output=True)
        self.assertEqual(configure.returncode, 0, configure.stdout + configure.stderr)
        before = path.read_bytes()
        check = subprocess.run(['cmake', '--build', str(build), '--target', 'check-format'],
                               text=True, capture_output=True)
        self.assertEqual(check.returncode, 0, check.stdout + check.stderr)
        self.assertEqual(path.read_bytes(), before)
        (self.root / file).write_text('int VALUE = 1;\n')
        for target in ['update-style-master', 'check-style']:
            result = subprocess.run(['cmake', '--build', str(build), '--target', target],
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(path.exists())

    def test_cli_update_master_does_not_prune_when_files_cannot_be_analyzed(self):
        file = 'AHOLi/AHO/probe.cpp'
        command = self.prepare_cli({file: 'int VALUE;\n', 'AHOLi/AHO/unused.hpp': '#pragma once\n',
                                    'AHOLi/AHO/unregistered.cpp': 'int OTHER;\n'}, [file])
        old = CHECK.diagnostic(self.root, file, 1, 1, 'name', 'bad', 'bad', source='int bad;')
        path = self.write_master(file, [{**old, 'count': 1}])
        before = path.read_bytes()
        result = subprocess.run(command + ['--update-master'], text=True, capture_output=True)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn('masterを更新しません', result.stderr)
        self.assertEqual(path.read_bytes(), before)

    def test_cli_custom_master_and_strict_mode(self):
        file = 'AHOLi/AHO/probe.cpp'
        command = self.prepare_cli({file: 'int   VALUE=1;\n'}, [file])
        directory = self.root / 'custom-master'
        self.write_master(file, [{**item, 'count': 1}
                                for item in CHECK.format_check(self.root, file, ARGS.clang_format)], directory)
        for options, expected in [([], 1), (['--master', str(directory)], 0),
                                  (['--master', str(directory), '--strict'], 1)]:
            result = subprocess.run(command + options, text=True, capture_output=True)
            self.assertEqual(result.returncode, expected, result.stderr)

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
