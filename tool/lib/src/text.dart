import 'dart:convert';

extension StringExtensions on String {
  /// Use nicer HTML entities and special characters.
  String get pretty {
    return this
        .replaceAll("à", "&agrave;")
        .replaceAll("ï", "&iuml;")
        .replaceAll("ø", "&oslash;")
        .replaceAll("æ", "&aelig;");
  }

  String get escapeHtml =>
      const HtmlEscape(HtmlEscapeMode.attribute).convert(this);
}
