import 'dart:convert';
import 'dart:io';

import 'package:args/args.dart';
import 'package:glob/glob.dart';
import 'package:glob/list_local_fs.dart';
import 'package:path/path.dart' as p;

import 'package:tool/src/term.dart' as term;

/// Runs the tests.

final _expectedOutputPattern = RegExp(r"// expect: ?(.*)");
final _expectedErrorPattern = RegExp(r"// (Error.*)");
final _errorLinePattern = RegExp(r"// \[((java|c) )?line (\d+)\] (Error.*)");
final _expectedRuntimeErrorPattern = RegExp(r"// expect runtime error: (.+)");
final _syntaxErrorPattern = RegExp(r"\[.*line (\d+)\] (Error.+)");
final _stackTracePattern = RegExp(r"\[line (\d+)\]");
final _nonTestPattern = RegExp(r"// nontest");

var _passed = 0;
var _failed = 0;
var _skipped = 0;
var _expectations = 0;

Suite? _suite ;
String? _filterPath;
String? _customInterpreter;
List<String>? _customArguments;

final _allSuites = <String, Suite>{};
final _cSuites = <String>[];
final _javaSuites = <String>[];

class Suite {
  final String name;
  final String language;
  final String executable;
  final List<String> args;
  final Map<String, String> tests;

  Suite(this.name, this.language, this.executable, this.args, this.tests);
}

void main(List<String> arguments) {
  _defineTestSuites();

  var parser = ArgParser();

  parser.addOption("interpreter", abbr: "i", help: "Path to interpreter.");
  parser.addMultiOption("arguments",
      abbr: "a", help: "Additional interpreter arguments.");

  var options = parser.parse(arguments);
  String? suite;
  if (options.rest.isEmpty) {
    suite = "none";
  } else if (options.rest.length > 2) {
    _usageError(
        parser, "Unexpected arguments '${options.rest.skip(2).join(' ')}'.");
  }else {
    var suite = options.rest[0];
  }

  if (options.rest.length == 2) _filterPath = arguments[1];

  if (options.wasParsed("interpreter")) {
    _customInterpreter = options["interpreter"] as String;
  }

  if (options.wasParsed("arguments")) {
    _customArguments = options["arguments"] as List<String>;

    if (_customInterpreter == null) {
      _usageError(parser,
          "Must pass an interpreter path if providing custom arguments.");
    }
  }

  if (suite == "all") {
    _runSuites(_allSuites.keys.toList());
  } else if (suite == "c") {
    _runSuites(_cSuites);
  } else if (!_runTests()) {
    exit(1);
  }
}

void _usageError(ArgParser parser, String message) {
  print(message);
  print("");
  print("Usage: test.dart <suites> [filter] [custom interpreter...]");
  print("");
  print("Optional custom interpreter options:");
  print(parser.usage);
  exit(1);
}

void _runSuites(List<String> names) {
  var anyFailed = false;
  for (var name in names) {
    print("=== $name ===");
    if (!_runSuite(name)) anyFailed = true;
  }

  if (anyFailed) exit(1);
}

bool _runSuite(String name) {
  _suite = _allSuites[name];

  _passed = 0;
  _failed = 0;
  _skipped = 0;
  _expectations = 0;

  for (var file in Glob("language-tests/**.lox").listSync()) {
    _runTest(file.path);
  }

  term.clearLine();

  if (_failed == 0) {
    print("All ${term.green(_passed)} tests passed "
        "($_expectations expectations).");
  } else {
    print("${term.green(_passed)} tests passed. "
        "${term.red(_failed)} tests failed.");
  }

  return _failed == 0;
}

bool _runTests() {
  _passed = 0;
  _failed = 0;
  _skipped = 0;
  _expectations = 0;

  for (var file in Glob("language-tests/**.lox").listSync()) {
    _runTest(file.path);
  }

  term.clearLine();

   if (_failed == 0) {
    print("All ${term.green(_passed)} tests passed "
        "($_expectations expectations).");
  } else {
    print("${term.green(_passed)} tests passed. "
        "${term.red(_failed)} tests failed.");
  }

  return _failed == 0;
}

void _runTest(String path) {
  if (path.contains("benchmark")) return;

  // Make a nice short path relative to the working directory. Normalize it to
  // use "/" since the interpreters expect the argument to use that.
  path = p.posix.normalize(path);

  // Check if we are just running a subset of the tests.
  if (_filterPath != null) {
    var thisTest = p.posix.relative(path, from: "test");
    if (!thisTest.startsWith(_filterPath as String)) return;
  }

  // Update the status line.
  var grayPath = term.gray("($path)");
  term.writeLine("Passed: ${term.green(_passed)} "
      "Failed: ${term.red(_failed)} "
      "Skipped: ${term.yellow(_skipped)} $grayPath");

  // Read the test and parse out the expectations.
  var test = Test(path);

  // See if it's a skipped or non-test file.
  if (!test.parse()) return;

  var failures = test.run();

  // Display the results.
  if (failures.isEmpty) {
    _passed++;
  } else {
    _failed++;
    term.writeLine("${term.red("FAIL")} $path");
    print("");
    for (var failure in failures) {
      print("     ${term.pink(failure)}");
    }
    print("");
  }
}

class ExpectedOutput {
  final int line;
  final String output;

  ExpectedOutput(this.line, this.output);
}

class Test {
  final String _path;

  final _expectedOutput = <ExpectedOutput>[];

  /// The set of expected compile error messages.
  final _expectedErrors = <String>{};

  /// The expected runtime error message or `null` if there should not be one.
  String? _expectedRuntimeError;

  /// If there is an expected runtime error, the line it should occur on.
  int _runtimeErrorLine = 0;

  int _expectedExitCode = 0;

  /// The list of failure message lines.
  final _failures = <String>[];

  Test(this._path);

  bool parse() {
    // Get the path components.
    var parts = _path.split("/");
    var subpath = "";
    String? state = "pass";

    // Figure out the state of the test. We don't break out of this loop because
    // we want lines for more specific paths to override more general ones.
    for (var part in parts) {
      if (subpath.isNotEmpty) subpath += "/";
      subpath += part;

      if (_suite != null && (_suite as Suite).tests.containsKey(subpath)) {
        state = _suite?.tests[subpath];
      }
    }

    if (state == null) {
      throw "Unknown test state for '$_path'.";
    } else if (state == "skip") {
      _skipped++;
      return false;
    }

    var lines = File(_path).readAsLinesSync();
    for (var lineNum = 1; lineNum <= lines.length; lineNum++) {
      var line = lines[lineNum - 1];

      // Not a test file at all, so ignore it.
      var match = _nonTestPattern.firstMatch(line);
      if (match != null) return false;

      match = _expectedOutputPattern.firstMatch(line);
      if (match != null) {
        _expectedOutput.add(ExpectedOutput(lineNum, match[1] as String));
        _expectations++;
        continue;
      }

      match = _expectedErrorPattern.firstMatch(line);
      if (match != null) {
        _expectedErrors.add("[$lineNum] ${match[1]}");

        // If we expect a compile error, it should exit with EX_DATAERR.
        _expectedExitCode = 65;
        _expectations++;
        continue;
      }

      match = _errorLinePattern.firstMatch(line);
      if (match != null) {
        // The two interpreters are slightly different in terms of which
        // cascaded errors may appear after an initial compile error because
        // their panic mode recovery is a little different. To handle that,
        // the tests can indicate if an error line should only appear for a
        // certain interpreter.
        var language = match[2];
        if (language == null || language == _suite?.language) {
          _expectedErrors.add("[${match[3]}] ${match[4]}");

          // If we expect a compile error, it should exit with EX_DATAERR.
          _expectedExitCode = 65;
          _expectations++;
        }
        continue;
      }

      match = _expectedRuntimeErrorPattern.firstMatch(line);
      if (match != null) {
        _runtimeErrorLine = lineNum;
        _expectedRuntimeError = match[1];
        // If we expect a runtime error, it should exit with EX_SOFTWARE.
        _expectedExitCode = 70;
        _expectations++;
      }
    }

    if (_expectedErrors.isNotEmpty && _expectedRuntimeError != null) {
      print("${term.magenta('TEST ERROR')} $_path");
      print("     Cannot expect both compile and runtime errors.");
      print("");
      return false;
    }

    // If we got here, it's a valid test.
    return true;
  }

  /// Invoke the interpreter and run the test.
  List<String> run() {
    var args = [
      if (_customInterpreter != null) ...?_customArguments else ..._suite!.args,
      _path
    ];
    var result = Process.runSync(_customInterpreter ?? _suite!.executable, args);

    // Normalize Windows line endings.
    var outputLines = const LineSplitter().convert(result.stdout as String);
    var errorLines = const LineSplitter().convert(result.stderr as String);

    // Validate that an expected runtime error occurred.
    if (_expectedRuntimeError != null) {
      _validateRuntimeError(errorLines);
    } else {
      _validateCompileErrors(errorLines);
    }

    _validateExitCode(result.exitCode, errorLines);
    _validateOutput(outputLines);
    return _failures;
  }

  void _validateRuntimeError(List<String> errorLines) {
    if (errorLines.length < 2) {
      fail("Expected runtime error '$_expectedRuntimeError' and got none.");
      return;
    }

    if (errorLines[0] != _expectedRuntimeError) {
      fail("Expected runtime error '$_expectedRuntimeError' and got:");
      fail(errorLines[0]);
    }

    // Make sure the stack trace has the right line.
    RegExpMatch? match;
    var stackLines = errorLines.sublist(1);
    for (var line in stackLines) {
      match = _stackTracePattern.firstMatch(line);
      if (match != null) break;
    }

    if (match == null) {
      fail("Expected stack trace and got:", stackLines);
    } else {
      var stackLine = int.parse(match[1]!);
      if (stackLine != _runtimeErrorLine) {
        fail("Expected runtime error on line $_runtimeErrorLine "
            "but was on line $stackLine.");
      }
    }
  }

  void _validateCompileErrors(List<String> error_lines) {
    // Validate that every compile error was expected.
    var foundErrors = <String>{};
    var unexpectedCount = 0;
    for (var line in error_lines) {
      var match = _syntaxErrorPattern.firstMatch(line);
      if (match != null) {
        var error = "[${match[1]}] ${match[2]}";
        if (_expectedErrors.contains(error)) {
          foundErrors.add(error);
        } else {
          if (unexpectedCount < 10) {
            fail("Unexpected error:");
            fail(line);
          }
          unexpectedCount++;
        }
      } else if (line != "") {
        if (unexpectedCount < 10) {
          fail("Unexpected output on stderr:");
          fail(line);
        }
        unexpectedCount++;
      }
    }

    if (unexpectedCount > 10) {
      fail("(truncated ${unexpectedCount - 10} more...)");
    }

    // Validate that every expected error occurred.
    for (var error in _expectedErrors.difference(foundErrors)) {
      fail("Missing expected error: $error");
    }
  }

  void _validateExitCode(int exitCode, List<String> errorLines) {
    if (exitCode == _expectedExitCode) return;

    if (errorLines.length > 10) {
      errorLines = errorLines.sublist(0, 10);
      errorLines.add("(truncated...)");
    }

    fail("Expected return code $_expectedExitCode and got $exitCode. Stderr:",
        errorLines);
  }

  void _validateOutput(List<String> outputLines) {
    // Remove the trailing last empty line.
    if (outputLines.isNotEmpty && outputLines.last == "") {
      outputLines.removeLast();
    }

    var index = 0;
    for (; index < outputLines.length; index++) {
      var line = outputLines[index];
      if (index >= _expectedOutput.length) {
        fail("Got output '$line' when none was expected.");
        continue;
      }

      var expected = _expectedOutput[index];
      if (expected.output != line) {
        fail("Expected output '${expected.output}' on line ${expected.line} "
            " and got '$line'.");
      }
    }

    while (index < _expectedOutput.length) {
      var expected = _expectedOutput[index];
      fail("Missing expected output '${expected.output}' on line "
          "${expected.line}.");
      index++;
    }
  }

  void fail(String message, [List<String>? lines]) {
    _failures.add(message);
    if (lines != null) _failures.addAll(lines);
  }
}

void _defineTestSuites() {
  void c(String name, Map<String, String> tests) {
    var executable = name == "clox" ? "build/cloxd" : "build/$name";

    _allSuites[name] = Suite(name, "c", executable, [], tests);
    _cSuites.add(name);
  }

  // These are just for earlier chapters.
  var earlyChapters = {
    "language-tests/scanning": "skip",
    "language-tests/expressions": "skip",
  };

  // No control flow in C yet.
  var noCControlFlow = {
    "language-tests/block/empty.lox": "skip",
    "language-tests/for": "skip",
    "language-tests/if": "skip",
    "language-tests/limit/loop_too_large.lox": "skip",
    "language-tests/logical_operator": "skip",
    "language-tests/variable/unreached_undefined.lox": "skip",
    "language-tests/while": "skip",
  };

  // No functions in C yet.
  var noCFunctions = {
    "language-tests/call": "skip",
    "language-tests/closure": "skip",
    "language-tests/for/closure_in_body.lox": "skip",
    "language-tests/for/return_closure.lox": "skip",
    "language-tests/for/return_inside.lox": "skip",
    "language-tests/for/syntax.lox": "skip",
    "language-tests/function": "skip",
    "language-tests/limit/no_reuse_constants.lox": "skip",
    "language-tests/limit/stack_overflow.lox": "skip",
    "language-tests/limit/too_many_constants.lox": "skip",
    "language-tests/limit/too_many_locals.lox": "skip",
    "language-tests/limit/too_many_upvalues.lox": "skip",
    "language-tests/regression/40.lox": "skip",
    "language-tests/return": "skip",
    "language-tests/unexpected_character.lox": "skip",
    "language-tests/variable/collide_with_parameter.lox": "skip",
    "language-tests/variable/duplicate_parameter.lox": "skip",
    "language-tests/variable/early_bound.lox": "skip",
    "language-tests/while/closure_in_body.lox": "skip",
    "language-tests/while/return_closure.lox": "skip",
    "language-tests/while/return_inside.lox": "skip",
  };

  // No classes in C yet.
  var noCClasses = {
    "language-tests/assignment/to_this.lox": "skip",
    "language-tests/call/object.lox": "skip",
    "language-tests/class": "skip",
    "language-tests/closure/close_over_method_parameter.lox": "skip",
    "language-tests/constructor": "skip",
    "language-tests/field": "skip",
    "language-tests/inheritance": "skip",
    "language-tests/method": "skip",
    "language-tests/number/decimal_point_at_eof.lox": "skip",
    "language-tests/number/trailing_dot.lox": "skip",
    "language-tests/operator/equals_class.lox": "skip",
    "language-tests/operator/equals_method.lox": "skip",
    "language-tests/operator/not.lox": "skip",
    "language-tests/operator/not_class.lox": "skip",
    "language-tests/regression/394.lox": "skip",
    "language-tests/return/in_method.lox": "skip",
    "language-tests/super": "skip",
    "language-tests/this": "skip",
    "language-tests/variable/local_from_method.lox": "skip",
  };

  // No inheritance in C yet.
  var noCInheritance = {
    "language-tests/class/local_inherit_other.lox": "skip",
    "language-tests/class/local_inherit_self.lox": "skip",
    "language-tests/class/inherit_self.lox": "skip",
    "language-tests/class/inherited_method.lox": "skip",
    "language-tests/inheritance": "skip",
    "language-tests/regression/394.lox": "skip",
    "language-tests/super": "skip",
  };

  c("clox", {
    "language-tests": "pass",
    ...earlyChapters,
  });

  c("chap17_compiling", {
    // No real interpreter yet.
    "language-tests": "skip",
    "language-tests/expressions/evaluate.lox": "pass",
  });

  c("chap18_types", {
    // No real interpreter yet.
    "language-tests": "skip",
    "language-tests/expressions/evaluate.lox": "pass",
  });

  c("chap19_strings", {
    // No real interpreter yet.
    "language-tests": "skip",
    "language-tests/expressions/evaluate.lox": "pass",
  });

  c("chap20_hash", {
    // No real interpreter yet.
    "language-tests": "skip",
    "language-tests/expressions/evaluate.lox": "pass",
  });

  c("chap21_global", {
    "language-tests": "pass",
    ...earlyChapters,
    ...noCControlFlow,
    ...noCFunctions,
    ...noCClasses,

    // No blocks.
    "language-tests/assignment/local.lox": "skip",
    "language-tests/variable/in_middle_of_block.lox": "skip",
    "language-tests/variable/in_nested_block.lox": "skip",
    "language-tests/variable/scope_reuse_in_different_blocks.lox": "skip",
    "language-tests/variable/shadow_and_local.lox": "skip",
    "language-tests/variable/undefined_local.lox": "skip",

    // No local variables.
    "language-tests/block/scope.lox": "skip",
    "language-tests/variable/duplicate_local.lox": "skip",
    "language-tests/variable/shadow_global.lox": "skip",
    "language-tests/variable/shadow_local.lox": "skip",
    "language-tests/variable/use_local_in_initializer.lox": "skip",
  });

  c("chap22_local", {
    "language-tests": "pass",
    ...earlyChapters,
    ...noCControlFlow,
    ...noCFunctions,
    ...noCClasses,
  });

  c("chap23_jumping", {
    "language-tests": "pass",
    ...earlyChapters,
    ...noCFunctions,
    ...noCClasses,
  });

  c("chap24_calls", {
    "language-tests": "pass",
    ...earlyChapters,
    ...noCClasses,

    // No closures.
    "language-tests/closure": "skip",
    "language-tests/for/closure_in_body.lox": "skip",
    "language-tests/for/return_closure.lox": "skip",
    "language-tests/function/local_recursion.lox": "skip",
    "language-tests/limit/too_many_upvalues.lox": "skip",
    "language-tests/regression/40.lox": "skip",
    "language-tests/while/closure_in_body.lox": "skip",
    "language-tests/while/return_closure.lox": "skip",
  });

  c("chap25_closures", {
    "language-tests": "pass",
    ...earlyChapters,
    ...noCClasses,
  });

  c("chap26_garbage", {
    "language-tests": "pass",
    ...earlyChapters,
    ...noCClasses,
  });

  c("chap27_classes", {
    "language-tests": "pass",
    ...earlyChapters,
    ...noCInheritance,

    // No methods.
    "language-tests/assignment/to_this.lox": "skip",
    "language-tests/class/local_reference_self.lox": "skip",
    "language-tests/class/reference_self.lox": "skip",
    "language-tests/closure/close_over_method_parameter.lox": "skip",
    "language-tests/constructor": "skip",
    "language-tests/field/get_and_set_method.lox": "skip",
    "language-tests/field/method.lox": "skip",
    "language-tests/field/method_binds_this.lox": "skip",
    "language-tests/method": "skip",
    "language-tests/operator/equals_class.lox": "skip",
    "language-tests/operator/equals_method.lox": "skip",
    "language-tests/return/in_method.lox": "skip",
    "language-tests/this": "skip",
    "language-tests/variable/local_from_method.lox": "skip",
  });

  c("chap28_methods", {
    "language-tests": "pass",
    ...earlyChapters,
    ...noCInheritance,
  });

  c("chap29_superclasses", {
    "language-tests": "pass",
    ...earlyChapters,
  });

  c("chap30_optimization", {
    "language-tests": "pass",
    ...earlyChapters,
  });
}
