import 'package:markdown/markdown.dart';
import 'package:tool/src/text.dart';

import 'highlighter.dart';

/// Custom code block formatter that uses our syntax highlighter.
class HighlightedCodeBlockSyntax extends BlockSyntax {
  static final _codeFencePattern = RegExp(r'^(\s*)```(.*)$');

  RegExp get pattern => _codeFencePattern;

  HighlightedCodeBlockSyntax();

  bool canParse(BlockParser parser) =>
      pattern.firstMatch(parser.current.content) != null;

  List<Line?> parseChildLines(BlockParser parser) {
    var childLines = <Line?>[];
    parser.advance();

    while (!parser.isDone) {
      var match = pattern.firstMatch(parser.current.content);
      if (match == null) {
        childLines.add(parser.current);
        parser.advance();
      } else {
        parser.advance();
        break;
      }
    }

    return childLines;
  }

  Node parse(BlockParser parser) {
    // Get the syntax identifier, if there is one.
    var match = pattern.firstMatch(parser.current.content);
    var indent = match![1]!.length;
    var language = match[2];

    var childLines = parseChildLines(parser);

    String code;
    if (language == "text") {
      // Don't syntax highlight text.
      var buffer = StringBuffer();
      buffer.write("<pre>");

      // The HTML spec mandates that a leading newline after '<pre>' is
      // ignored.
      // https://html.spec.whatwg.org/#element-restrictions
      // Some snippets deliberately start with a newline which needs to be
      // preserved, so output an extra (discarded) newline in that case.
      if (childLines.first!.isBlankLine) buffer.writeln();

      for (var l in childLines) {
        var line = l!.content;
        // Strip off any leading indentation.
        if (line.length > indent) line = line.substring(indent);
        checkLineLength(line);

        buffer.write(line.escapeHtml);
        buffer.writeln();
      }
      buffer.write("</pre>");
      code = buffer.toString();
    } else {
      code = formatCode(language!,
          [childLines.map((Line? line) => line!.content).join("\n")]);
//      code = formatCode(
//          language!, childLines.map((Line? line) => line!.content).toList(),
//          indent: indent);
    }

    var element = Element.text("div", code);
    element.attributes["class"] = "codehilite";
    return element;
  }
}
